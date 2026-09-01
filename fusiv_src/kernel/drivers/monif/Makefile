#
#/* Copyright (C) 2006 Ikanos Communications
# * See IKANOS_PROP_LICENSE.txt for license information. */
#

MONIF_OBJ := monif.o

obj-$(CONFIG_FUSIV_KERNEL_MONIF) := monif_lkm.o
monif_lkm-objs := $(MONIF_OBJ)

clean:
	rm -f *.ko *.o *.mod.o */*.o *~ */*~ core .depend .*.cmd *.mod.c */.*.o.cmd
	rm -rf .tmp_versions

