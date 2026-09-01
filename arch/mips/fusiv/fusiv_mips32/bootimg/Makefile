#/*
# * Copyright (C) 2006  Ikanos Communications. All rights reserved.
# * The information and source code contained herein is the property
# * of Ikanos Communications.
# *
# *  This program is free software; you can redistribute  it and/or modify it
# *  under  the terms of  the GNU General  Public License as published by the
# *  Free Software Foundation;  either version 2 of the  License, or (at your
# *  option) any later version.
# */

ifndef IMG_NAME
 IMG_NAME = Ikanos_kernel_2.6.28.8
endif


MKIMG = $(TOPDIR)/scripts/mkbootimg
MKIMG_ARCH = mips
BOOTIMG = kernel.img

KERNEL_IMAGE = $(TOPDIR)/$(KBUILD_IMAGE)
KERNEL_START = $(shell $(NM) $(KERNEL_IMAGE) | grep " _text" | cut -f1 -d\ )
KERNEL_ENTRY = $(shell $(NM) $(KERNEL_IMAGE) | grep kernel_entry | cut -f1 -d\ )
#KERNEL_START = $(shell $(NM) $(KERNEL_IMAGE) | grep " _text" | cut -f1 -d\ | cut -c9- )
#KERNEL_ENTRY = $(shell $(NM) $(KERNEL_IMAGE) | grep kernel_entry | cut -f1 -d\ | cut -c9- )


adiimage:	$(KERNEL_IMAGE)
	@ echo 
	@ echo Creating Ikanos Boot Image...
	$(Q) $(OBJCOPY) -S -O binary $< kernel.tmp.bin
ifdef CONFIG_SQUASHFS
	if [ -f /usr/local/bin/lzma ]; then \
		lzma e kernel.tmp.bin kernel.tmp.bin.lzma; \
		$(MKIMG) -A mips -O linux -T kernel -a $(KERNEL_START) -e $(KERNEL_ENTRY) -n $(IMG_NAME) -C lzma -d kernel.tmp.bin.lzma $(TOPDIR)/$(BOOTIMG); \
	fi
else
	$(Q) gzip -f kernel.tmp.bin
	$(Q) $(MKIMG) -A mips -O linux -T kernel -a $(KERNEL_START) -e $(KERNEL_ENTRY) -n $(IMG_NAME) -C gzip -d kernel.tmp.bin.gz $(TOPDIR)/$(BOOTIMG)
endif
	$(Q) rm -f kernel.tmp.bin*
	@ echo
	@ echo Ikanos Boot Image [ $(BOOTIMG) ] created!
	@ echo
	
clean:
	$(Q) rm -f $(TOPDIR)/$(BOOTIMG)
