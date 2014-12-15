#!/bin/bash

set -e

source settings.env

mkdir -p ${NTFS_RO_MOUNT}
mkdir -p ${NTFS_RW_MOUNT}

echo ""
echo "Creating filestsystem..."
${NICE} mkntfs -F ${NTFS_DEV}

echo ""
echo "Mounting R/W for setup..."
mount -t ntfs-3g ${NTFS_DEV} ${NTFS_RW_MOUNT}

echo ""
echo "Filling with random data..."
i=0
while ${NICE} dd if=/dev/urandom of=${NTFS_RW_MOUNT}/random-${i}.data bs=1M count=${FILLER_SIZE}; do
    i=$(($i + 1))
done
if [ ${i} < 4 ] ; then
    echo ""
    echo ""
    echo "ERROR: Unable to create at least 4 random files"
    exit 1
fi

# Remove 3 of them, then generate test files so it's fragmented
rm -f ${NTFS_RW_MOUNT}/random-4.data
rm -f ${NTFS_RW_MOUNT}/random-2.data
rm -f ${NTFS_RW_MOUNT}/random-0.data

echo ""
echo "Generating checkums to detect possible corruption"
(cd ${NTFS_RW_MOUNT} && ${NICE} md5sum random*.data) > ${TEST_HOME}/${RANDOM_CHECKSUMS}
cat ${TEST_HOME}/${RANDOM_CHECKSUMS}

echo ""
echo "Generating Sparse file"
${NICE} dd if=/dev/zero of=${NTFS_RW_MOUNT}/${SPARSE_FILE} bs=1M count=0 seek=${FILLER_SIZE}

echo ""
echo "Generating file with pattern"
python ./make_file.py ${NTFS_RW_MOUNT}/${PATTERN_FILE} ${PATTERN_FILE_SIZE}

echo ""
echo "Generating real file (and filling up all available space)"
${NICE} dd if=/dev/zero of=${NTFS_RW_MOUNT}/${GOOD_FILE} bs=1M || /bin/true

echo ""
echo "Here's what we've got..."
df -h ${NTFS_RW_MOUNT}
ls -lh ${NTFS_RW_MOUNT}

umount ${NTFS_RW_MOUNT}
echo ""
echo "Ready to run tests now"
