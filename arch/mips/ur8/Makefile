obj-y := 	prom.o \
			setup.o \
			memory.o \
			irq.o \
			time.o \
			platform.o \
			gpio.o \
			clock.o \
			vbus.o \
			ur8_mtd.o \
			avm_hw_config.o \
			ur8_cppi.o \
			ur8_reset.o

obj-$(CONFIG_PCI) +=  ur8_pci.o \
				ur8_pci_irq.o \
				ur8_pci_generic.o

EXTRA_CFLAGS += -Werror
ur8_cppi.o: EXTRA_CFLAGS += -Wall -Wextra -Werror

