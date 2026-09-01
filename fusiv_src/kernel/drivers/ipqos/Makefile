#!/bin/sh
temp1 = $(PWD)
pace :=
remove := fusiv_src/kernel/drivers/ipqos
IPQOS_LINUX_KERNEL_PATH := $(subst $(remove),$(comma),$(temp1))linux-2.6.18
RCS_FIND_IGNORE := \( -name SCCS -o -name BitKeeper -o -name .svn -o -name CVS -o -name .pc \) -prune -o

ipqos_lkm:
	make -C $(IPQOS_LINUX_KERNEL_PATH)  M=$(PWD) LOCAL_IPQOS_FLAGS="-DCONFIG_FUSIV_KERNEL_IPQOS_TRAFFIC_SCHEDULER -DCONFIG_FUSIV_KERNEL_IPQOS_STATISTICS -DCONFIG_FUSIV_KERNEL_AP_2_AP_MODULE -DCONFIG_FUSIV_KERNEL_IPQOS_APQOS -DCONFIG_FUSIV_KERNEL_IPQOS_TC_MARKING -DCONFIG_FUSIV_KERNEL_IPQOS_TC_PRIORITY_CONVERSION -DCONFIG_FUSIV_KERNEL_IPQOS_TC_MAPPING -DCONFIG_FUSIV_KERNEL_IPQOS_DEBUG_LOG -DATM_UPSTREAM_THRU_HOST" 

clean:
	@find ./ \
	        \( -name '*.[oas]' -o -name '*.ko' -o -name '.*.cmd' \
					  -o -name '.*.d' -o -name '.*.tmp' -o -name '*.mod.c' \) \
						                -type f -print | xargs rm -f

