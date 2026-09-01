#
#/* Copyright (C) 2006 Ikanos Communications
# * See IKANOS_PROP_LICENSE.txt for license information. */
#

EXTRA_CFLAGS += -DBUFFMANAGER
BMDRV_OBJ :=    bmdriver.o \
                ../../ap_code/user_config/bm/bmapif.o \
                ../../ap_code/buffmanager/apbmload.o \
                ../../ap_code/buffmanager/apbm_syms.o 
                #../../ap_code/buffmanager/bmmain.o #This file is included in ap_code/lib/Makefile

obj-$(CONFIG_FUSIV_BMDRIVER) := bmdriver_lkm.o
bmdriver_lkm-objs := $(BMDRV_OBJ)

clean:
	rm -f *.ko *.o *.mod.o */*.o *~ */*~ core .depend .*.cmd *.mod.c */.*.o.cmd
	rm -rf .tmp_versions
	rm -f ../../ap_code/buffmanager/*.o ../../ap_code/buffmanager/*.mod.o \
                ../../ap_code/buffmanager/*.o.cmd
	rm -f ../../ap_code/user_config/bm/*.o

