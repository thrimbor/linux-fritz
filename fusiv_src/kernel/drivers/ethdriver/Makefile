#
#/* Copyright (C) 2006 Ikanos Communications
# * See IKANOS_PROP_LICENSE.txt for license information. */
#

SYSUTIL_OBJ := sysutil_kernel_code.o
ETHDRIVER_OBJ :=        driver.o userif.o oswrapper.o \
                        ../../ap_code/ethdriver/fusiv/osif.o \
                        ../../ap_code/ethdriver/phy/ieeephy.o \
                        ../../ap_code/ethdriver/std_ap/sraap.o \
                        ../../ap_code/ethdriver/std_ap/switchvlan.o \
                        ../../ap_code/ethdriver/std_ap/apethload.o \
                        ../../ap_code/ethdriver/std_ap/eth_syms.o \
                        ../../ap_code/user_config/eth/ethapif.o
ifeq ($(CONFIG_FUSIV_VX180),y)
ETHDRIVER_OBJ +=        ../../ap_code/ethdriver/std_ap/ethap_vx180.o
else
ETHDRIVER_OBJ +=        ../../ap_code/ethdriver/std_ap/ethap.o
endif

obj-$(CONFIG_FUSIV_KERNEL_ETHERNET) := ethdriver_lkm.o sysKCode_lkm.o
ethdriver_lkm-objs := $(ETHDRIVER_OBJ)
sysKCode_lkm-objs := $(SYSUTIL_OBJ)

clean:
	rm -f *.ko *.o *.mod.o */*.o *~ */*~ core .depend .*.cmd *.mod.c */.*.o.cmd
	rm -rf .tmp_versions
	rm -f ../../ap_code/ethdriver/fusiv/osif.o
	rm -f ../../ap_code/ethdriver/phy/ieeephy.o
	rm -f ../../ap_code/ethdriver/std_ap/sraap.o
	rm -f ../../ap_code/ethdriver/std_ap/apethload.o
	rm -f ../../ap_code/ethdriver/std_ap/ethap.o
	rm -f ../../ap_code/ethdriver/std_ap/eth_syms.o
	rm -f ../../ap_code/user_config/eth/ethapif.o

