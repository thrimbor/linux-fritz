
 # =============================================================================
 #
 # Copyright (C) 2005 Analog Devices Inc. All rights reserved
 #
 # The information and source code contained herein is the exclusive property
 # of Analog Devices and may not be disclosed, examined or reproduced in whole
 # or in part without the explicit written authorization from Analog Devices.
 #
 # ===========================================================================

ACLAPDRIVER_OBJ :=             acl_ap_driver.o aclapi.o aclRFTcheck.o  aclBFTcheck.o \
				../../ap_code/acl_ap/apaclload.o
ifeq ($(CONFIG_FUSIV_VX180),y)
ACLAPDRIVER_OBJ += ../../ap_code/acl_ap/aclapmain_vx180.o
else
ACLAPDRIVER_OBJ += ../../ap_code/acl_ap/aclapmain.o
endif

obj-$(CONFIG_FUSIV_LIBRARY) := aclap_driver_lkm.o
aclap_driver_lkm-objs := $(ACLAPDRIVER_OBJ)

clean:
	rm -f *.ko *.o *.mod.o */*.o *~ */*~ core .depend .*.cmd *.mod.c */.*.o.cmd
	rm -rf .tmp_versions \
	rm ../../ap_code/acl_ap/apaclload.o \
	rm ../../ap_code/acl_ap/aclapmain.o
	rm ../../ap_code/acl_ap/aclapmain_vx180.o
