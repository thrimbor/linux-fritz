
# Makefile for Ar7240 gigabit MAC and Phys
#

ENETDIR := $(src)

ifneq ($(CONFIG_AG7240_GE0_IS_CONNECTED),n)
ifneq ($(CONFIG_AG7240_GE1_IS_CONNECTED),n)
#EXTRA_CFLAGS += -DCONFIG_ATHR_SUPPORT_DUAL_PHY
endif
endif

#EXTRA_CFLAGS += -DCONFIG_ATHRS_S27_HW_ACL
#EXTRA_CFLAGS += -DCONFIG_ATHRS_HW_ACL

ifneq ($(CONFIG_ATH_GMAC_HW_QOS),n)
#EXTRA_CFLAGS += -DCONFIG_ATHRS_QOS
endif

EXTRA_CFLAGS += -DCONFIG_GMAC_RX_TASKLET
EXTRA_CFLAGS += -DCONFIG_GMAC0_RXFCTL
EXTRA_CFLAGS += -DCONFIG_GMAC0_TXFCTL
EXTRA_CFLAGS += -DCONFIG_GMAC1_RXFCTL
EXTRA_CFLAGS += -DCONFIG_GMAC1_TXFCTL

obj-$(CONFIG_AG7240)			        += athrs_gmac.o
obj-phy-$(CONFIG_MACH_AR7240)		    += ag7240.o
obj-phy-$(CONFIG_MACH_AR724x)		    += ag7240.o
obj-phy-$(CONFIG_AR7240_S26_PHY)	    += phys/ar7240_s26_phy.o
obj-phy-$(CONFIG_ATHRF1_PHY)            += phys/athrf1_phy.o
obj-phy-$(CONFIG_ATHRF2_PHY)            += phys/athrf2_phy.o
obj-phy-$(CONFIG_ATHRS27_PHY)           += phys/athrs27_phy.o
obj-phy-$(CONFIG_ATHRS16_PHY)		    += phys/athrs16_phy.o
obj-phy-$(CONFIG_ATHRS17_PHY)		    += phys/athrs17_phy.o
obj-phy-$(CONFIG_AR7242_S16_PHY)	    += phys/athrs16_phy.o
obj-phy-$(CONFIG_LANTIQ_11G_PHY)	    += phys/lantiq_11g_phy.o
obj-phy-$(CONFIG_AG7240_QOS)		    += athrs_qos.o vlan_igmp.o
obj-phy-$(CONFIG_AR7240_S26_VLAN_IGMP)	+= vlan_igmp.o
obj-phy-$(CONFIG_AR7240_S27_VLAN_IGMP)	+= vlan_igmp.o
obj-hw-nat-$(CONFIG_ATHRS_HW_NAT)	    += athrs_nf_nat.o athrs_hw_nat.o
obj-hw-acl-$(CONFIG_ATHRS_HW_ACL)	    += athrs_nf_acl.o athrs_hw_acl.o

obj-phy-$(CONFIG_MACH_AR934x) 	        += ag934x.o
obj-phy-$(CONFIG_MACH_HORNET)   	    += ag7240.o
obj-phy-$(CONFIG_AR8021_PHY)        	+= phys/ar8021_phy.o 
obj-phy-$(CONFIG_ATHRS_VIR_PHY)        	+= phys/athrs_vir_phy.o 

ifdef ATH_GMAC_TXQUEUELEN
EXTRA_CFLAGS += -DATH_GMAC_TXQUEUELEN=$$ATH_GMAC_TXQUEUELEN
else
# refer to ether_setup
EXTRA_CFLAGS += -DATH_GMAC_TXQUEUELEN=1000
endif

athrs_gmac-objs				:= $(obj-phy-y) athrs_mac.o athrs_phy.o athrs_mac_timer.o  athrs_phy_ctrl.o athrs_gmac_ctrl.o athrs_flowmac.o $(obj-hw-nat-y) $(obj-hw-acl-y)


EXTRA_CFLAGS += -I ${ENETDIR}/include -I ${ENETDIR}/include/phys -I$(KERNELPATH)/arch/mips/include
EXTRA_CFLAGS += -Wall -Wextra

ifdef FLOWMACDIR
EXTRA_CFLAGS+= -I ${FLOWMACDIR}
endif
EXTRA_CFLAGS += -I ${KERNELPATH}/../../../drivers/hybrid_bridge/os/linux
EXTRA_CFLAGS += -I ${KERNELPATH}/../../../drivers/wlan/include/sys

ifndef NO_PUSH_BUTTON
export NO_PUSH_BUTTON=1
endif

ifeq ($(strip ${NO_PUSH_BUTTON}), 1)
EXTRA_CFLAGS+= -DNO_PUSH_BUTTON=1
else
EXTRA_CFLAGS+= -DNO_PUSH_BUTTON=0
endif

ifeq ($(strip ${HYBRID_VLAN_COMMUNICATE}), 1)
EXTRA_CFLAGS+= -DHYBRID_VLAN_COMMUNICATE=1
else
EXTRA_CFLAGS+= -DHYBRID_VLAN_COMMUNICATE=0
endif

ifeq ($(strip ${HYBRID_PATH_SWITCH}), 1)
EXTRA_CFLAGS+= -DATH_HY_PATH_SWITCH
endif

ifeq ($(strip ${HYBRID_LINK_CHANGE_EVENT}), 1)
EXTRA_CFLAGS+= -DHYBRID_LINK_CHANGE_EVENT=1
else
EXTRA_CFLAGS+= -DHYBRID_LINK_CHANGE_EVENT=0
endif

ifeq ($(strip ${HYBRID_SWITCH_PORT6_USED}), 1)
EXTRA_CFLAGS+= -DHYBRID_SWITCH_PORT6_USED=1
else
EXTRA_CFLAGS+= -DHYBRID_SWITCH_PORT6_USED=0
endif

clean:
	rm -f *.o *.ko 
	rm -f phys/*.o *.ko
ifneq ($(ACCEL-OBJS),)
	rm -f hwaccels/*.o
endif



