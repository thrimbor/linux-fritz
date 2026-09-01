#
#/* Copyright (C) 2006 Ikanos Communications
# * See IKANOS_PROP_LICENSE.txt for license information. */
#

TIMER_OBJ := timers.o

obj-$(CONFIG_FUSIV_TIMERS) := timers_lkm.o
timers_lkm-objs := $(TIMER_OBJ)

clean:
	rm -f *.ko *.o *.mod.o */*.o *~ */*~ core .depend .*.cmd *.mod.c */.*.o.cmd
	rm -rf .tmp_versions

