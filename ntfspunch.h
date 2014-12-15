/*
 * ntfspunch.h - Common datastructures and function definitions for
 *		 the NTFS Punch Driver
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

#ifndef _NTFSPUNCH_H_
#define _NTFSPUNCH_H_

#include <linux/types.h>
#include <linux/genhd.h>
#include "ntfs/inode.h"
#include "ntfs/runlist.h"

/*
 * Set to non-zero for some serious log spewage for troubleshooting
 */
#define NP_DEBUG_SETUP	1
#define NP_DEBUG_IO	0

int proc_init(void);
void proc_exit(void);
int proc_remove_node(int index);
int proc_add_node(int index);
void dump_unlocked_device(int device_num);

int add_device(char *filename);

struct mapping_dev {
	char filename[PATH_MAX+1];
	struct file *img_fp;
	short users;
	struct gendisk *gd;
	struct request_queue *queue;
	s64 size;  /* in bytes */
	u32 cluster_size;  /* in bytes */
	struct block_device *block_dev;
	ntfs_inode *ni;
	spinlock_t lock;
	runlist_element *rl;
};

extern struct mapping_dev **dev_list;
extern spinlock_t dev_list_lock;
extern int num_devices;
extern int ntfspunch_major;

#endif
