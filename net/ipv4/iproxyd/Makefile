#
# Makefile for the Linux TCP/IP (INET) layer.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definition is now in the main makefile...

#include ${KERNEL_SOURCE_DIR}/.config
obj-m    := iproxyd.o

CONFIG_FULL_PACKAGE=y
ifeq ($(CONFIG_FULL_PACKAGE),y)
iproxyd-objs	:= mMain.o mUtil.o

# include $(TOPDIR)/Rules.make

# ifeq ($(CONFIG_ADM6996_SUPPORT),m)
# DUMMY	:= $(shell cp iproxyd.o iproxyd.o.T2)
# else
# DUMMY	:= $(shell cp iproxyd.o iproxyd.o.T1)
# endif


else

# include $(TOPDIR)/Rules.make

ifeq ($(CONFIG_ADM6996_SUPPORT),m)

iproxyd.o: iproxyd.o.T2
	cp iproxyd.o.T2 iproxyd.o

else

iproxyd.o: iproxyd.o.T1
	cp iproxyd.o.T1 iproxyd.o

endif

endif
