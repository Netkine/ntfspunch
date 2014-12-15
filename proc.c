/*
 * proc.c - Proc support for the NTFS Punch Driver
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
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/string.h>
#include <asm/uaccess.h>

MODULE_LICENSE("GPL v2");

static struct proc_dir_entry *proc_dir = NULL;
static struct proc_dir_entry *proc_add = NULL;

static ssize_t
add_write(struct file *fp, const char *userBuf, size_t len, loff_t *off)
{
	char *filename;
	int ret;

	if (len > PATH_MAX) {
		printk(KERN_WARNING "ntfspunch: added path too long\n");
		return -EFAULT;
	}
	filename = kmalloc(len + 1, GFP_KERNEL);
	if (filename == NULL) {
		printk(KERN_WARNING "ntfspunch: failed alloc string\n");
		return -ENOMEM;
	}
	if (copy_from_user(filename, userBuf, len)) {
		printk(KERN_WARNING "ntfspunch: failed copy_from_user\n");
		kfree(filename);
		return -EFAULT;
	}
	filename[len] = '\0';
	if ((ret = add_device(filename))) {
		printk(KERN_WARNING "ntfspunch: Failed setup \"%s\"\n",
		       filename);
		kfree(filename);
		return -EFAULT;
	}
	kfree(filename);
	return len;
}

/*
 * Opertions for the dump nodes - per device
 */

static int
dump_node(struct seq_file *m, void *v, int index)
{
	struct mapping_dev *dev;
	runlist_element *rl;
	spin_lock(&dev_list_lock);
	if (index < 0 || index >= num_devices) {
		printk(KERN_WARNING "ntfspunch: index out of bounds\n");
		spin_unlock(&dev_list_lock);
		return -EFAULT;
	}

	dev = dev_list[index];
	spin_unlock(&dev_list_lock);

#if NP_DEBUG_SETUP
	dump_unlocked_device(index);
#endif

	spin_lock(&dev->lock);

	seq_printf(m, "filename: %s\n", dev->filename);
	seq_printf(m, "minor_number: %d\n", dev->gd->first_minor);
	seq_printf(m, "use_count: %d\n", dev->users);
	seq_printf(m, "size: %lld\n", dev->size);
	seq_printf(m, "cluster_size: %u\n", dev->cluster_size);
	seq_printf(m, "\nfile_offset:disk_offset:length\n");

	/* XXX This could pop if the runlist is long... */
	for (rl = dev->rl; rl->length; rl++) {
		seq_printf(m, "%lld:%lld:%lld\n",
			   rl->vcn * dev->cluster_size,
			   rl->lcn * dev->cluster_size,
			   rl->length * dev->cluster_size);
	}
	spin_unlock(&dev->lock);
	return 0;
}

static int
dump_show(struct seq_file *m, void *v)
{
	int i;
	i = (int)(unsigned long long)m->private;
	return dump_node(m, v, i);
}

static int
dump_open(struct inode *inode, struct file *file)
{
	int ret;
	char *pathname;
	struct path *path = &file->f_path;
	char *tmp = kmalloc(PATH_MAX, GFP_KERNEL);
	int i;

	if (tmp == NULL)
		return -ENOMEM;

	path_get(path);
	pathname = d_path(path, tmp, PATH_MAX);
	path_put(path);

	if (IS_ERR(pathname)) {
		kfree(tmp);
		return PTR_ERR(pathname);
	}
	for (i=0; pathname[i+1] != '\0'; i++);
	i = pathname[i] - 'a';
	kfree(tmp);
	ret = single_open(file, dump_show, (void*)(unsigned long long)i);
	return ret;
}

static struct file_operations dump_fops = {
	.owner = THIS_MODULE,
	.open = dump_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

/*
 * Operations for the add node (global)
 */

static int
add_show(struct seq_file *m, void *v)
{
	seq_printf(m, "major_num: %d\n", ntfspunch_major);
	seq_printf(m, "num_devices: %d\n", num_devices);
	return 0;
}

static int
add_open(struct inode *inode, struct file *file)
{
	return single_open(file, add_show, NULL);
}

static struct file_operations add_fops = {
	.owner = THIS_MODULE,
	.open = add_open,
	.read = seq_read,
	.write = add_write,
	.llseek = seq_lseek,
	.release = single_release,
};

int
proc_add_node(int index)
{
	char name[2];
	name[0] = index + 'a';
	name[1] = '\0';
	return PTR_ERR(proc_create(name, 0, proc_dir, &dump_fops));
}

int
proc_remove_node(int index)
{
	char name[2];
	name[0] = index + 'a';
	name[1] = '\0';
	remove_proc_entry(name, proc_dir);
	return 0;
}

int
proc_init(void)
{
	proc_dir = proc_mkdir("ntfspunch", NULL);
	if (proc_dir == NULL) {
		printk(KERN_WARNING "ntfspunch: failed alloc ntfspunch proc dir\n");
		return -ENOMEM;
	}
	proc_add = proc_create("add", 0, proc_dir, &add_fops);
	if (proc_add == NULL) {
		printk(KERN_WARNING "ntfspunch: failed alloc ntfspunch add node\n");
		return -ENOMEM;
	}
	return 0;
}

void
proc_exit(void)
{
	if (proc_add != NULL)
		remove_proc_entry("add", proc_dir);
	if (proc_dir != NULL)
		remove_proc_entry("ntfspunch", NULL);
}
