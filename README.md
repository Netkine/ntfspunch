NTFS Punch Driver
=================

The goal of this driver is to provide block-device read-write access
to pre-allocated file(s) residing within an NTFS filesystem mounted
read-only.  You might ask, why not just use a normal loopback device?
If your NTFS is cleanly unmounted and does not contain a hibernate
file that works fine.  Unfortunately Windows really likes to hibernate
(even during shutdown with their "fast boot" feature), and thus the NTFS
isn't unmounted cleanly, so you'd only be able to read-only mount the
loopback file.  However if we pre-allocate the file(s) we care about
before hibernating and flush them within Windows, then we can safely
map the disk blocks for those files even though the broader NTFS isn't
safe for read-write access.  This can be useful when trying to setup a
Linux environment on an existing Windows system without re-partitioning
the whole drive.


Limitations
-----------

1. The files must be fully allocated within the NTFS - growing is not supported.
2. Partitions within the files are not currently supported
3. This code relies on the kernel mode driver, not the ntfs-3g user-space
   driver.  Some modern distro's make it difficult to use the kernel
   driver.  If /sbin/mount.ntfs is a sym-link to ntfs-3g, you can
   remove it so you can explicitly specify "ntfs" as the type to get
   the kernel driver.
4. The NTFS must be mounted read-only to prevent possible changes to the
   block mappings while the system is running.  If you've got a hibernate
   file on the NTFS, this is the only option anyways.
5. You'll have to read the files before attempting to punch them through
   so that the NTFS driver loads the runlist (block mappings.)


TODO Items
----------

1. Automatically read the files to trigger the ntfs driver to read in the
   runlist.
2. Switch from using proc to sysfs for configuration and debug dumping
3. Support individual device removal
4. Block device removal/unload if refcount says the device is not in-use
5. Performance evaluation, and optimization
