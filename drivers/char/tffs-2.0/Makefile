#
# Makefile for the memory technology device drivers.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now inherited from the
# parent makes..
#
# $Id: Makefile 1.4 2003/12/16 08:28:16Z mpommerenke Exp $

EXTRA_CFLAGS += -DFLASH_FS_INDEX

# Each configuration option enables a list of files.

obj-$(CONFIG_TFFS)	+= tffs_init.o tffs_open.o \
                       tffs_read.o tffs_write.o \
                       tffs_mtd.o tffs_intern.o tffs_proc.o \
					   tffs_ioctl.o

obj-$(CONFIG_TFFS_PANIC_LOG) += tffs_panic.o
ifdef CONFIG_VR9
obj-$(CONFIG_TFFS_PANIC_LOG) += tffs_direct_spi_lantiq.o
endif
ifdef CONFIG_AR10
obj-$(CONFIG_TFFS_PANIC_LOG) += tffs_direct_spi_lantiq.o
endif
