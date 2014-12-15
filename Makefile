# If you want to point to a specific Netkine Nile build
# use something like
# make KERNELDIR=/path/to/tree/nk_build/kernel/debian/build/build-generic
#
KERNELDIR ?= /lib/modules/$(shell uname -r)/build


DEBUG = y

ifeq ($(DEBUG),y)
  DEBFLAGS = -O -g -DNTFSPUNCH_DEBUG
else
  DEBFLAGS = -O2
endif

EXTRA_CFLAGS += $(DEBFLAGS)
EXTRA_CFLAGS += -I$(shell pwd)

ifneq ($(KERNELRELEASE),)

ntfspunch-objs := proc.o main.o debug.o

obj-m   := ntfspunch.o

else

PWD       := $(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR)  M=$(PWD) modules

endif



clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions

depend .depend dep:
	$(CC) $(EXTRA_CFLAGS) -M *.c > .depend


ifeq (.depend,$(wildcard .depend))
include .depend
endif

load:
	sudo insmod -f $(PWD)/ntfspunch.ko
	grep ntfspunch /proc/devices | awk '{print $$1}'
	MAJOR=`grep ntfspunch /proc/devices | awk '{print $$1}'`; \
	    echo "MAJOR=$${MAJOR}"
unload:
	sudo rmmod ntfspunch
