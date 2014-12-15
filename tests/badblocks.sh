#!/bin/bash

# Test case using badblocks to write lots of data to the system

source settings.env

load_driver
mount_ro

punch_good ${NTFS_RO_MOUNT}/${PATTERN_FILE}
punch_good ${NTFS_RO_MOUNT}/${GOOD_FILE}

echo "Starting with simple parallel read"
badblocks -v /dev/ntfspuncha &
badblocks -v /dev/ntfspunchb


echo "Moving on to writes..."
badblocks -v /dev/ntfspuncha &
badblocks -vw /dev/ntfspunchb

unload_driver
# Remount to flush any stale buffers
umount_ro
mount_ro

check_for_corruption

umount_ro

echo "PASS"
exit 0
