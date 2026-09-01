XVM              = sub-projects/allocators/xvmalloc-kmod
EXTRA_CFLAGS	:=	-DCONFIG_RAMZSWAP_STATS	-Wall
MODUL			:= ramzswap

obj-$(CONFIG_COMPCACHE)		    +=	ramzswap.o
ramzswap-objs	:=	ramzswap_drv.o xvmalloc.o

ifneq ($(CONFIG_COMPCACHE),n)
extra-y			+= rzscontrol ramzswap.sh

rzscontrol-objs	:= rzscontrol.o

#CFLAGS_rzscontrol.o  +=  -v
CFLAGS_rzscontrol.o  += $(shell xx=$(CROSS_COMPILE); echo "-isystem$${xx%/*}/../include")


$(src)/rzscontrol: $(src)/rzscontrol.o FORCE
	$(CROSS_COMPILE)gcc $(CFLAGS) $(CFLAGS_rzscontrol.o) -o $(src)/rzscontrol $(src)/rzscontrol.o
	$(CROSS_COMPILE)strip -s $(src)/rzscontrol 
	if [ ! -d $(INSTALL_MOD_PATH)/sbin ]; then \
		echo "  MKDIR $(INSTALL_MOD_PATH)/sbin" ; \
		mkdir -p "$(INSTALL_MOD_PATH)/sbin" ; \
	fi
	cp $(src)/rzscontrol $(INSTALL_MOD_PATH)/sbin

$(src)/rzscontrol.o: $(src)/rzscontrol.c
	$(CROSS_COMPILE)gcc $(CFLAGS) $(CFLAGS_rzscontrol.o) -c -o $(src)/rzscontrol.o $(src)/rzscontrol.c


$(src)/ramzswap.sh: FORCE
	rm -f $(src)/ramzswap.sh
	echo '#! /bin/sh' >>$(src)/ramzswap.sh
	echo 'start_swap() {' >>$(src)/ramzswap.sh
ifeq ($(CONFIG_COMPCACHE),m)
	echo 'modprobe ramzswap num_devices=1 disksize_kb=8192 memlimit_kb=2048' >>$(src)/ramzswap.sh
endif
	echo 'temp=`grep ramzswap /proc/devices`' >>$(src)/ramzswap.sh
	echo 'b_major=$${temp%%ramzswap}' >>$(src)/ramzswap.sh
	echo 'mknod /dev/ramzswap0 b $$b_major 0' >>$(src)/ramzswap.sh
ifneq ($(CONFIG_COMPCACHE),m)
	echo '/sbin/rzscontrol /dev/ramzswap0 disksize_kb=8192 memlimit_kb=2048 --init' >>$(src)/ramzswap.sh
endif
	echo 'swapon /dev/ramzswap0' >>$(src)/ramzswap.sh
	echo '}' >>$(src)/ramzswap.sh
	echo 'start_swap & ' >>$(src)/ramzswap.sh
	if [ ! -d $(INSTALL_MOD_PATH)/etc/init.d ]; then \
		echo "  MKDIR $(INSTALL_MOD_PATH)/etc/init.d" ; \
		mkdir -p "$(INSTALL_MOD_PATH)/etc/init.d" ; \
	fi
	cp $(src)/ramzswap.sh $(INSTALL_MOD_PATH)/etc/init.d/ramzswap.sh

endif
