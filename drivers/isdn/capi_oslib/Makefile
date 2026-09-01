
TARGET		 := "isdn_oslib"

EXTRA_CFLAGS += -D__CAPI_OSLIB__ -Wall -Wextra

ifneq ($(CONFIG_CAPI_OSLIB_DEBUG),y)
EXTRA_CFLAGS		+= -DNDEBUG 
else
EXTRA_CFLAGS 		+= -g -ggdb 
EXTRA_CFLAGS		+= -DDEBUG 
endif

capi_oslib-objs		+= \
						appl.o \
						ca.o \
						ca_mem.o \
						ca_sched.o \
						ca_timer.o \
						local_capi.o \
						capi.o \
						capi_events.o \
						capi_pipe.o \
						host.o \
						main.o \
						transparent.o \
						file.o \
						memmap.o \
						debug.o \
						socket_if.o

obj-$(CONFIG_CAPI_OSLIB) += capi_oslib.o
