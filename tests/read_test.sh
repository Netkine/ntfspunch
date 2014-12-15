#!/bin/bash

source settings.env

load_driver
mount_ro

od -x ${NTFS_RO_MOUNT}/${PATTERN_FILE} > ${TEST_HOME}/expected

punch_good ${NTFS_RO_MOUNT}/${PATTERN_FILE}

od -x /dev/ntfspuncha > ${TEST_HOME}/punched

if ! diff ${TEST_HOME}/expected ${TEST_HOME}/punched ; then
    echo "ERROR: mismatched results"
    exit 1
fi
unload_driver
umount_ro

mount_ro
check_for_corruption  # Unlikely, but just in case
umount_ro

echo "PASS"
exit 0

