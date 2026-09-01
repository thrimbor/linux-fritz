#
#/* Copyright (C) 2006 Ikanos Communications
# * See IKANOS_PROP_LICENSE.txt for license information. */
#

VDSLDRIVER_OBJ :=        vdsldriver.o \
			../../ap_code/vdsl_ptm/driver/vdslptm.o \
			../../ap_code/vdsl_ptm/driver/vdslptmdriver.o \
			../../ap_code/vdsl_ptm/phy/fusiv_decoder.o \
			../../ap_code/vdsl_ptm/phy/fusiv_encoder.o \
			../../ap_code/vdsl_ptm/phy/fusiv_rx_slb.o \
			../../ap_code/vdsl_ptm/phy/fusiv_tx_slb.o \
			../../ap_code/vdsl_ptm/ptmap/apptmload.o \
			../../ap_code/vdsl_ptm/ptmap/vdslptmmain.o \
			../../ap_code/user_config/vdsl_ptm/vdslptmif.o

obj-$(CONFIG_FUSIV_KERNEL_BME_DRIVER_VX180) := vdsldriver_lkm.o
vdsldriver_lkm-objs := $(VDSLDRIVER_OBJ)

clean:
	rm -f *.ko *.o *.mod.o */*.o *~ */*~ core .depend .*.cmd *.mod.c */.*.o.cmd
	rm -rf .tmp_versions

