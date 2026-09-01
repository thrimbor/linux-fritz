#
# Makefile for the kernel mmc device drivers.
#

ifeq ($(CONFIG_MMC_DEBUG),y)
	EXTRA_CFLAGS		+= -DDEBUG
endif

obj-$(CONFIG_MMC)		+= core/
obj-$(CONFIG_MMC)		+= card/
obj-$(CONFIG_MMC)		+= host/

ifeq ($(CONFIG_LANTIQ),y)
-include $(srctree)/drivers/mmc/Makefile.ifx
endif #ifeq ($(CONFIG_LANTIQ),y)
