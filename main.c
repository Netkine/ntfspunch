/*
 * main.c - The NTFS Punch Driver to access files within
 *	    NTFS as block-devices.
 *
 * Copyright (c) 2014 Daniel Hiltgen @ Netkine Inc.
 *
 * This program/include file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program/include file is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in the main directory of the Linux-NTFS
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation,Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "ntfspunch.h"
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/blkdev.h>

int ntfspunch_major = 0;
module_param(ntfspunch_major, int, 0);

struct mapping_dev **dev_list = NULL;
spinlock_t dev_list_lock;
int num_devices = 0;

static void ntfspunch_make_request(struct request_queue *q, struct bio *bio);
static runlist_element *copy_runlist(runlist *runlist);

/*
 * Returns the calculated physical sector of the start if it
 * fits within one chunk
 *
 * If the IO stradles a chunk, then it will automatically be
 * split and re-submitted
 *
 * dev lock should NOT be held
 */
static unsigned long long
split_or_get_offset(struct request_queue *q,
		    struct mapping_dev *dev, struct bio *bio)
{
	runlist_element *rl;
	unsigned long long blocks_per_cluster, ret;
	sector_t start = bio->bi_sector;
	sector_t end = bio_end_sector(bio);

	spin_lock(&dev->lock);
	blocks_per_cluster = dev->cluster_size >> 9;
	for (rl = dev->rl; rl->length; rl++) {
		if (start >= rl->vcn * blocks_per_cluster &&
		    end <= rl->vcn * blocks_per_cluster +
		    rl->length * blocks_per_cluster) {
			/*
			 * This should never be zero,
			 * since NTFS has metadata up front
			 */
			ret = (start - rl->vcn * blocks_per_cluster +
			       rl->lcn * blocks_per_cluster);
			spin_unlock(&dev->lock);
			return ret;
		} else if (start < rl->vcn * blocks_per_cluster &&
			   end <= rl->vcn * blocks_per_cluster +
			   rl->length * blocks_per_cluster) {
			struct bio_pair *bp;
			printk(KERN_WARNING "ntfspunch: Split I/Os\n");
			printk(KERN_WARNING "ntfspunch: segments %d\n",
			       bio_segments(bio));
			printk(KERN_WARNING "ntfspunch: splitting on %lld\n",
			       (rl->vcn * blocks_per_cluster) - start);
			bp = bio_split(bio,
				       (rl->vcn * blocks_per_cluster) - start);
			printk(KERN_WARNING "ntfspunch: resubmitting requests\n");
			/* Release all locks to re-enter */
			spin_unlock(&dev->lock);
			ntfspunch_make_request(q, &bp->bio1);
			ntfspunch_make_request(q, &bp->bio2);
			bio_pair_release(bp);
			printk(KERN_WARNING "ntfspunch: all done with split I/O\n");
			return 0;
		}
	}
	spin_unlock(&dev->lock);

	printk(KERN_WARNING "ntfspunch: Couldn't map I/O\n");
	printk(KERN_WARNING "ntfspunch: bio start sec:%ld  bytes: %ld\n",
	       bio->bi_sector, bio->bi_sector * 512);
	printk(KERN_WARNING "ntfspunch: bio end	sec:%ld bytes: %ld\n",
	       bio->bi_sector + bio_sectors(bio),
	       (bio->bi_sector + bio_sectors(bio)) * 512);
	bio_io_error(bio);
	return 0;
}

static void
ntfspunch_make_request(struct request_queue *q, struct bio *bio)
{
	struct mapping_dev *dev = q->queuedata;
	unsigned long long disk_start;
	spin_lock(&dev->lock);
	bio_get(bio);
#if NP_DEBUG_IO
	printk(KERN_WARNING "ntfspunch: make_request called\n\n");
	if (bio->bi_rw & REQ_WRITE)
		printk(KERN_WARNING "ntfspunch: write request\n");
	else
		printk(KERN_WARNING "ntfspunch: read (or other) request\n");

	printk(KERN_WARNING "bi_next: %p\n", bio->bi_next);
	printk(KERN_WARNING "bi_bdev: %p\n", bio->bi_bdev);
	printk(KERN_WARNING "bi_flags: %lx\n", bio->bi_flags);
	printk(KERN_WARNING "bi_rw: %lx\n", bio->bi_rw);
	printk(KERN_WARNING "bi_sector: %ld\n", bio->bi_sector);
	printk(KERN_WARNING "bi_sector (*512): %ld\n", bio->bi_sector * 512);
	printk(KERN_WARNING "length: %u\n", bio_sectors(bio));
	printk(KERN_WARNING "length (*512): %u\n", bio_sectors(bio) * 512);
	printk(KERN_WARNING "bi_seg_front_size: %x\n", bio->bi_seg_front_size);
	printk(KERN_WARNING "bi_seg_back_size: %x\n", bio->bi_seg_back_size);
	printk(KERN_WARNING "bi_end_io: %p\n", bio->bi_end_io);
	printk(KERN_WARNING "bi_private: %p\n", bio->bi_private);
	printk(KERN_WARNING "bi_vcnt: %x\n", bio->bi_vcnt);
	printk(KERN_WARNING "bi_max_vecs: %x\n", bio->bi_max_vecs);
	printk(KERN_WARNING "bi_io_vec: %p\n", bio->bi_io_vec);
	printk(KERN_WARNING "bi_pool: %p\n", bio->bi_pool);
#endif
	spin_unlock(&dev->lock);
	disk_start = split_or_get_offset(q, dev, bio);
	if (disk_start > 0) {
		bio->bi_bdev = dev->block_dev;
		bio->bi_sector = disk_start;
		generic_make_request(bio);
#if NP_DEBUG_IO
		printk(KERN_WARNING "new sector %llu\n\n", disk_start);
#endif
	}
	bio_put(bio);
}

static int
ntfspunch_open(struct block_device *bdev, fmode_t mode)
{
	struct mapping_dev *dev = bdev->bd_disk->private_data;
	spin_lock(&dev->lock);
	dev->users++;
	spin_unlock(&dev->lock);
	return 0;
}

static void
ntfspunch_release(struct gendisk *disk, fmode_t mode)
{
	struct mapping_dev *dev = disk->private_data;
	spin_lock(&dev->lock);
	dev->users--;
	spin_unlock(&dev->lock);
}

int
ntfspunch_media_changed(struct gendisk *gd)
{
	return 0;
}

int
ntfspunch_revalidate(struct gendisk *gd)
{
	return 0;
}

int
ntfspunch_ioctl(struct block_device *bdev, fmode_t mode,
		unsigned int cmd, unsigned long arg)
{
	printk(KERN_WARNING "ntfspunch: TODO ioctl called with cmd 0x%x\n",
	       cmd);
	return -ENOTTY;
}

static struct block_device_operations ntfspunch_ops = {
	.owner = THIS_MODULE,
	.open = ntfspunch_open,
	.release = ntfspunch_release,
	.media_changed = ntfspunch_media_changed,
	.revalidate_disk = ntfspunch_revalidate,
	.ioctl = ntfspunch_ioctl
};

/*
 * Free an already locked device
 */
static void
ntfspunch_free_dev(struct mapping_dev *dev)
{
	if (dev->users > 0) {
		printk(KERN_DEBUG "ntfspunch: %s still in use %d\n",
		       dev->filename, dev->users);
		/* TODO - block freeing if it's inuse */
	}
	if (dev->img_fp) {
		filp_close(dev->img_fp, 0);
	}
	if (dev->gd) {
		del_gendisk(dev->gd);
		put_disk(dev->gd);
	}
	if (dev->queue) {
		blk_cleanup_queue(dev->queue);
	}
	kfree(dev->rl);
	kfree(dev);
}

/*
 * Perform as much validation as possible on the requetsed file
 */
int
validate(struct file *img_fp)
{
	runlist_element *rl;
	ntfs_inode *ni;
	s64 size = -1;
	int ret = 0;
        if (!img_fp->f_inode->i_sb->s_flags & MS_RDONLY) {
		printk(KERN_WARNING "ntfspunch: FS mounted read-write\n");
		ret = -EFAULT;
	}
	if (strcmp(img_fp->f_inode->i_sb->s_type->name, "ntfs") != 0) {
		printk(KERN_WARNING "ntfspunch: File must be on an NTFS volume\n");
		ret = -EFAULT;
		goto done;
	}

	ni = NTFS_I(img_fp->f_inode);
	if (ni->type != AT_DATA) {
		// XXX directory?  Better log message...
		printk(KERN_WARNING "ntfspunch: type not AT_DATA!\n");
		ret = -EFAULT;
	}
	if (ni->vol == NULL) {
		printk(KERN_WARNING "ntfspunch: ntfs inode has null vol!\n");
		ret = -EFAULT;
	}

	if (ni->allocated_size != ni->initialized_size) {
		printk(KERN_WARNING "ntfspunch: File must be fully allocated! (not sparse)\n");
		ret = -EFAULT;
	}

	/* TODO - try to read from the file to fix this scenario */
	if (ni->runlist.rl == NULL) {
		printk(KERN_WARNING "ntfspunch: inode null runlist!\n");
		ret = -EFAULT;
	}
	for (rl = ni->runlist.rl; rl->length; rl++) {
		if ((rl->vcn + rl->length) * ni->vol->cluster_size < size) {
			printk(KERN_WARNING "ntfspunch: runlist out of order!\n");
			ret = -EFAULT;
			break;
		}
		size = (rl->vcn + rl->length) * ni->vol->cluster_size;
	}
	if (size != ni->allocated_size) {
		printk(KERN_WARNING "ntfspunch: runlist size mismatch!\n");
		printk(KERN_WARNING "ntfspunch: inode size: %lld\n",
		       ni->allocated_size);
		printk(KERN_WARNING "ntfspunch: runlist size: %lld\n",
		       size);
		ret = -EFAULT;
	}

	/* TODO - Is there any more validation we can do? */

#if NP_DEBUG_SETUP
	printk(KERN_WARNING "ntfpunch: allocated_size %lld\n",
	       ni->allocated_size);
	printk(KERN_WARNING "ntfpunch: initialized_size %lld\n",
	       ni->initialized_size);
	printk(KERN_WARNING "ntfpunch: img_fp %p\n", img_fp);
	printk(KERN_WARNING "ntfpunch: inode %p\n", img_fp->f_inode);
	printk(KERN_WARNING "ntfpunch: i_sb %p\n", img_fp->f_inode->i_sb);
	printk(KERN_WARNING "ntfpunch: s_type %p\n",
	       img_fp->f_inode->i_sb->s_type);
	printk(KERN_WARNING "ntfpunch: name %s\n",
	       img_fp->f_inode->i_sb->s_type->name);
#endif
done:
	return ret;
}

/*
 * Allocate and copy over a runlist
 *
 * This fundamentally assumes the runlist isn't
 * changing out from under us
 */
static runlist_element *
copy_runlist(runlist *runlist)
{
	runlist_element *rl;
	int i = 1;
	for (rl = runlist->rl; rl->length; rl++, i++);
	rl = kcalloc(i, sizeof(*rl), GFP_KERNEL);
	memcpy(rl, runlist->rl, (i-1) * sizeof(*rl));
	return rl;
}

int
add_device(char *in_filename)
{
	struct mapping_dev *dev = NULL, **tmp = NULL;
	struct file *img_fp = NULL;
	int ret, device_num;
	char *filename = strim(in_filename);

	/* Before doing anything else, attempt to open the file */
	img_fp = filp_open(filename, O_RDONLY|O_LARGEFILE, 0600);
	if (IS_ERR(img_fp)) {
		printk(KERN_WARNING "ntfspunch: Failed to open file %s\n",
		       filename);
		return PTR_ERR(img_fp);
	}
	if ((ret = validate(img_fp))) {
		printk(KERN_WARNING "ntfspunch: file failed validation%s\n",
		       filename);
		filp_close(img_fp, 0);
		return ret;
	}

	spin_lock(&dev_list_lock);
	if (dev_list == NULL) {
		dev_list = kmalloc((num_devices+1) * sizeof(dev),
				   GFP_KERNEL);
		if (dev_list == NULL) {
			printk(KERN_WARNING "ntfspunch: unable to allocate device %s\n",
			       filename);
			filp_close(img_fp, 0);
			spin_unlock(&dev_list_lock);
			return -ENOMEM;
		}
	} else {
		tmp = krealloc(dev_list, (num_devices+1) *
			       sizeof(dev), GFP_KERNEL);
		if (tmp == NULL) {
			printk(KERN_WARNING "ntfspunch: unable to allocate device %s\n",
			       filename);
			spin_unlock(&dev_list_lock);
			filp_close(img_fp, 0);
			return -ENOMEM;
		}
		printk(KERN_WARNING "ntfspunch: realloc of dev_list - old:%p new %p\n",
		       dev_list, tmp);
		dev_list = tmp;

	}
	device_num = num_devices;
	dev = dev_list[device_num] = kmalloc(sizeof(*dev), GFP_KERNEL);
	if (dev == NULL) {
		printk(KERN_WARNING "ntfspunch: unable to allocate queue\n");
		spin_unlock(&dev_list_lock);
		return -ENOMEM;
	}

	spin_lock_init(&dev->lock);
	spin_lock(&dev->lock);
	dev->img_fp = img_fp;
	dev->users = 0;
	dev->gd = NULL;
	dev->queue = NULL;
	dev->rl = NULL;
	strncpy(dev->filename, filename, PATH_MAX);
	dev->ni = NTFS_I(img_fp->f_inode);
	dev->rl = copy_runlist(&dev->ni->runlist);
	if (dev->rl == NULL) {
		printk(KERN_WARNING "ntfspunch: unable to copy runlist\n");
		goto devfree;
	}
	dev->cluster_size = dev->ni->vol->cluster_size;
	dev->size = dev->ni->allocated_size;

	/* Queue setup */
	dev->queue = blk_alloc_queue(GFP_KERNEL);
	if (dev->queue == NULL) {
		printk(KERN_WARNING "ntfspunch: unable to allocate queue\n");
		goto devfree;
	}
	blk_queue_make_request(dev->queue, ntfspunch_make_request);
	blk_limits_max_hw_sectors(&dev->queue->limits, dev->cluster_size >> 9);
	dev->queue->queuedata = dev;

	/* Gendisk setup */
	dev->gd = alloc_disk(1);
	if (dev->gd == NULL) {
		printk(KERN_WARNING "ntfspunch: unable to allocate device\n");
		goto devfree;
	}

	dev->gd->major = ntfspunch_major;
	dev->gd->first_minor = device_num;
	dev->gd->fops = &ntfspunch_ops;
	dev->gd->queue = dev->queue;
	dev->gd->private_data = dev;
	snprintf(dev->gd->disk_name, 32, "ntfspunch%c", device_num + 'a');
	set_capacity(dev->gd, dev->size / 512);
	dev->block_dev = img_fp->f_inode->i_sb->s_bdev;

	disk_stack_limits(dev->gd, dev->block_dev,
			  dev->rl[0].lcn * (dev->cluster_size >> 9));

	blk_queue_flush(dev->queue, REQ_FLUSH | REQ_FUA);

	num_devices++;
	spin_unlock(&dev_list_lock);
	spin_unlock(&dev->lock);

	add_disk(dev->gd);
	proc_add_node(device_num);


	printk(KERN_DEBUG "ntfspunch: Added file %s\n", dev->filename);
#if NP_DEBUG_SETUP
	dump_unlocked_device(device_num);
#endif
	return 0;

devfree:
	ntfspunch_free_dev(dev);
	spin_unlock(&dev_list_lock);
	return -ENOMEM;

}

static int __init
ntfspunch_init(void)
{
	int ret = 0;
	printk(KERN_DEBUG "ntfspunch: initializing...\n");
	spin_lock_init(&dev_list_lock);
	ntfspunch_major = register_blkdev(ntfspunch_major, "ntfspunch");
	if (ntfspunch_major <= 0) {
		printk(KERN_WARNING "ntfspunch: unable to get major number\n");
		return -EBUSY;
	}

	ret = proc_init();
	if (ret != 0) {
		printk(KERN_WARNING "ntfspunch: unable to setup proc: %d\n",
		       ret);
		return ret;
	}

	printk(KERN_DEBUG "ntfspunch: initialized (lock %p)\n", &dev_list_lock);
	return 0;
}

static void
ntfspunch_exit(void)
{
	struct mapping_dev *dev;
	int i;
	printk(KERN_DEBUG "ntfspunch: Exiting...\n");

	spin_lock(&dev_list_lock);
	for (i = 0; i < num_devices; i++) {
		proc_remove_node(i);
		dev = dev_list[i];
		spin_lock(&dev->lock);
		ntfspunch_free_dev(dev);
	}
	if (dev_list != NULL) {
		kfree(dev_list);
	}
	unregister_blkdev(ntfspunch_major, "ntfspunch");
	proc_exit();
	printk(KERN_DEBUG "ntfspunch: exited.\n");
}

module_init(ntfspunch_init);
module_exit(ntfspunch_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Daniel Hiltgen");
MODULE_DESCRIPTION("Expose NTFS files as block devices");
