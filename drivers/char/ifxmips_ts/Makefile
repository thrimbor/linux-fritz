#
# Makefile for the platform specific kernel interface routines under Linux.
#

obj-$(CONFIG_IFX_TS) += ifxmips_ts.o

ifxmips_ts-objs =
ifneq ($(CONFIG_VR9),)
   ifxmips_ts-objs += ifxmips_ts_vr9.o
endif
ifneq ($(CONFIG_AR10),)
   ifxmips_ts-objs += ifxmips_ts_ar10.o
endif

