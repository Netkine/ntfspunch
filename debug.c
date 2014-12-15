/*
 * debug.c - Debugging output for the NTFS Punch Driver
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
#include <linux/blkdev.h>

/*
 * Dump out the device information for debugging purposes
 */
void
dump_unlocked_device(int device_num)
{
	runlist_element *rl;
	struct mapping_dev *dev = NULL;
	printk(KERN_DEBUG "ntfspunch: Dump for device_num [%d]\n", device_num);
	spin_lock(&dev_list_lock);
	printk(KERN_DEBUG "num_devices: %d\n", num_devices);
	if (device_num < 0 || device_num >= num_devices) {
		printk(KERN_WARNING "Requestsed device_num %d out of range\n",
		       device_num);
		spin_unlock(&dev_list_lock);
		return;
	}
	dev = dev_list[device_num];
	spin_unlock(&dev_list_lock);

	spin_lock(&dev->lock);
	printk(KERN_DEBUG "   stored at %p\n", dev);
	printk(KERN_DEBUG "   filename %s\n", dev->filename);
	printk(KERN_DEBUG "   lock %p\n", &dev->lock);
	printk(KERN_DEBUG "   img_fp %p\n", dev->img_fp);
	printk(KERN_DEBUG "   users %d\n", dev->users);
	printk(KERN_DEBUG "   gd %p\n", dev->gd);
	printk(KERN_DEBUG "   gd->slave_dir %p\n", dev->gd->slave_dir);
	printk(KERN_DEBUG "   (kobj) gd->slave_dir %p\n", dev->gd->slave_dir);
	if (dev->gd->slave_dir != NULL)
		printk(KERN_DEBUG "   (kobj) gd->slave_dir->state_initialized %d\n",
		       dev->gd->slave_dir->state_initialized);

	printk(KERN_DEBUG "   queue %p\n", dev->queue);
	printk(KERN_DEBUG "   (kobj) queue.kobj %p\n", &dev->queue->kobj);
	printk(KERN_DEBUG "   (kobj) queue.kobj.state_initialized %d\n",
	       dev->queue->kobj.state_initialized);
	printk(KERN_DEBUG "   size %lld\n", dev->size);
	printk(KERN_DEBUG "   cluster_size %u\n", dev->cluster_size);
	printk(KERN_DEBUG "   block_dev %p\n", dev->block_dev);
	printk(KERN_DEBUG "   ni %p\n", dev->ni);
	printk(KERN_DEBUG "   rl %p\n", dev->rl);
	for (rl = dev->rl; rl->length; rl++) {
		printk(KERN_DEBUG "   file_offset:%lld disk_offset:%lld length:%lld\n",
		       rl->vcn * dev->cluster_size,
		       rl->lcn * dev->cluster_size,
		       rl->length * dev->cluster_size);
	}

	printk(KERN_DEBUG " Queue Limits: %p\n", dev->queue);
	printk(KERN_DEBUG "   max_hw_sectors %u\n",
	       dev->queue->limits.max_hw_sectors);
	printk(KERN_DEBUG "   max_sectors %u\n",
	       dev->queue->limits.max_sectors);
	printk(KERN_DEBUG "   max_segment_size %u\n",
	       dev->queue->limits.max_segment_size);
	printk(KERN_DEBUG "   physical_block_size %u\n",
	       dev->queue->limits.physical_block_size);
	printk(KERN_DEBUG "   alignment_offset %u\n",
	       dev->queue->limits.alignment_offset);
	printk(KERN_DEBUG "   io_min %u\n", dev->queue->limits.io_min);
	printk(KERN_DEBUG "   io_opt %u\n", dev->queue->limits.io_opt);
	printk(KERN_DEBUG "   max_discard_sectors %u\n",
	       dev->queue->limits.max_discard_sectors);
	printk(KERN_DEBUG "   max_write_same_sectors %u\n",
	       dev->queue->limits.max_write_same_sectors);
	printk(KERN_DEBUG "   discard_granularity %u\n",
	       dev->queue->limits.discard_granularity);
	printk(KERN_DEBUG "   discard_alignment %u\n",
	       dev->queue->limits.discard_alignment);
	printk(KERN_DEBUG "   logical_block_size %u\n",
	       dev->queue->limits.logical_block_size);
	printk(KERN_DEBUG "   max_segments %u\n",
	       dev->queue->limits.max_segments);
	printk(KERN_DEBUG "   max_integrity_segments %u\n",
	       dev->queue->limits.max_integrity_segments);
	printk(KERN_DEBUG "   misaligned %u\n", dev->queue->limits.misaligned);
	printk(KERN_DEBUG "   discard_misaligned %u\n",
	       dev->queue->limits.discard_misaligned);
	printk(KERN_DEBUG "   cluster %u\n", dev->queue->limits.cluster);
	printk(KERN_DEBUG "   discard_zeroes_data %u\n",
	       dev->queue->limits.discard_zeroes_data);
	spin_unlock(&dev->lock);
}

