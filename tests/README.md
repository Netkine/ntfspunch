NTFS Punch Tests
================

This directory contains a number of test utilities helpful for
puttin the NTFS Punch driver through its paces.


Preconditions
-------------

1. All of these commands need to run as root
2. You'll need at least 10G of space available on your disk (you can tune
   some variables to adjust this, but other scripts might blow up)
3. Edit settings.env to your liking
4. Run bootstrap.sh to get things ready


Scenarios
---------

1. reset.sh - If something goes wrong, run this to unmount/reset the rig
2. read_test.sh - Very simple read-only test of a single device
3. write_test.sh - very simple test to do some writing to a single device
4. badblocks.sh - Use the badblocks command to do more aggressive reading
   and writing to the device, and verify no corruption occurs in the process.
