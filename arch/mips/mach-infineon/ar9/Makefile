#
# Makefile for the platform specific kernel interface routines under Linux.
#

all: ifxmips_ar9.a

obj-y   := ar9.o ifxmips_clk.o avm_hw_config.o
obj-$(CONFIG_AR9_REF_BOARD) += ar9_ref_board.o
obj-$(CONFIG_AR9_AVM_FRITZ_BOX) += ar9_avm_fritz_box.o

EXTRA_AFLAGS := $(CFLAGS) -Wextra -Werror

clean:
	rm -f *.o *.a
