#!/usr/bin/env python

# Super simple script to make a large file with
# a common pattern in it for low-level examination
# of block devices and filesystems

import sys
import struct

BLOCK_SIZE = 4 * 1024  # 4K default block size


def marker():
    return struct.pack('Q', 0xdeadbeefdeadbeef)


def page(page_num):
    buf = marker()
    i = 8
    while i < BLOCK_SIZE:
        buf += struct.pack('Q', page_num)
        i += 8
    return buf


def main():
    if len(sys.argv) < 3:
        print 'Usage: make_file <name> <length in MB>'
        sys.exit(1)
    length = int(float(sys.argv[2]) * 1024 * 1024)
    print 'Writing %d bytes to file %r' % (length, sys.argv[1])
    with open(sys.argv[1], 'w') as output:
        i = 0
        while i < length / BLOCK_SIZE:
            output.write(page(i))
            i += 1
    print 'Done'


if __name__ == "__main__":
    main()
