#
# Makefile for the platform specific kernel interface routines under Linux.
#

all: ifxmips_vr9.a

obj-y   := vr9.o ifxmips_clk.o ifxmips_dcdc.o avm_hw_config.o
obj-$(CONFIG_VR9_REF_BOARD) += vr9_ref_board.o
obj-$(CONFIG_VR9_AVM_FRITZ_BOX) += vr9_avm_fritz_box.o 

EXTRA_AFLAGS := $(CFLAGS) -Wextra -Werror

clean:
	rm -f *.o *.a

