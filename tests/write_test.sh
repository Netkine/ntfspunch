#!/bin/bash

# Test case using dd to write some data

# Size in MB
TEST_SIZE=10

source settings.env

load_driver
mount_ro

punch_good ${NTFS_RO_MOUNT}/${GOOD_FILE}

${NICE} dd of=${TEST_HOME}/scratch if=/dev/urandom bs=1M count=10

# Now copy it to the device
${NICE} dd if=${TEST_HOME}/scratch of=/dev/ntfspuncha bs=1M

unload_driver

# Need to unmount to flush out any stale cache content
umount_ro

mount_ro
# See if the same data was written
${NICE} dd of=${TEST_HOME}/scratch.after if=${NTFS_RO_MOUNT}/${GOOD_FILE} bs=1M count=10

if ! diff ${TEST_HOME}/scratch ${TEST_HOME}/scratch.after ; then
    echo "FAIL: Data was not correctly written out"
    exit 1
fi

check_for_corruption
umount_ro


echo "PASS"
exit 0

