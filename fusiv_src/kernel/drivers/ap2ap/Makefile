#
#/* Copyright (C) 2006 Ikanos Communications
# * See IKANOS_PROP_LICENSE.txt for license information. */
#

AP2AP_OBJ :=    ap2aproute.o ap2apbridge.o ap2apmulticast.o ap2apmulticastrt.o

obj-$(CONFIG_FUSIV_KERNEL_AP_2_AP) := ap2ap_lkm.o
ap2ap_lkm-objs := $(AP2AP_OBJ)

clean:
	rm -f *.ko *.o *.mod.o */*.o *~ */*~ core .depend .*.cmd *.mod.c */.*.o.cmd
	rm -rf .tmp_versions

