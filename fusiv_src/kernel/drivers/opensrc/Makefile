#
#/* Copyright (C) 2006 Ikanos Communications
# * See IKANOS_PROP_LICENSE.txt for license information. */
#

#for this we used CONFIG_FUSIV_LIBRARY, no need of new one..

OPENSRC_OBJ := 	opensrc.o \
		bme.o \
		voice.o

obj-$(CONFIG_FUSIV_LIBRARY) := opensrc_lkm.o
opensrc_lkm-objs := $(OPENSRC_OBJ)

clean:
	rm -f *.ko *.o *.mod.o */*.o *~ */*~ core .depend .*.cmd *.mod.c */.*.o.cmd

