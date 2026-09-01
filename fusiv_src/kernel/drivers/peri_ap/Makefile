
 # =============================================================================
 #
 # Copyright (C) 2005 Analog Devices Inc. All rights reserved
 #
 # The information and source code contained herein is the exclusive property
 # of Analog Devices and may not be disclosed, examined or reproduced in whole
 # or in part without the explicit written authorization from Analog Devices.
 #
 # ===========================================================================

EXTRA_CFLAGS += -DPER
PERIAPDRIVER_OBJ :=             peri_ap_driver.o  \
				../../ap_code/peri_ap/apperiload.o \
				../../ap_code/user_config/peri/periapif.o

ifeq ($(CONFIG_FUSIV_VX180),y)
PERIAPDRIVER_OBJ += ../../ap_code/peri_ap/periapmain_vx180.o
else
PERIAPDRIVER_OBJ += ../../ap_code/peri_ap/periapmain.o
endif

obj-$(CONFIG_FUSIV_KERNEL_PERI_AP) := periap_driver_lkm.o
periap_driver_lkm-objs := $(PERIAPDRIVER_OBJ)

clean:
	rm -f *.ko *.o *.mod.o */*.o *~ */*~ core .depend .*.cmd *.mod.c */.*.o.cmd
	rm -rf .tmp_versions \
	rm ../../ap_code/peri_ap/apperiload.o \
	rm ../../ap_code/peri_ap/periapmain.o
