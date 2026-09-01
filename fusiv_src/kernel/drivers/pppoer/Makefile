#
#/* Copyright (C) 2006 Ikanos Communications
# * See IKANOS_PROP_LICENSE.txt for license information. */
#

PPPRELAY_OBJ := pppoerdrv.o pppoerelay.o ppprelaylib.o ppprelayconf.o ppprelayflashconf.o

obj-$(CONFIG_FUSIV_KERNEL_PPPOE_RELAY) := pppRelay_lkm.o
pppRelay_lkm-objs := $(PPPRELAY_OBJ)

clean:
	rm -f *.ko *.o *.mod.o */*.o *~ */*~ core .depend .*.cmd *.mod.c */.*.o.cmd
	rm -rf .tmp_versions
