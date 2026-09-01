# CPUfreq core
obj-$(CONFIG_CPU_FREQ)			+= cpufreq.o
# CPUfreq stats
obj-$(CONFIG_CPU_FREQ_STAT)             += cpufreq_stats.o

# CPUfreq governors 
obj-$(CONFIG_CPU_FREQ_GOV_PERFORMANCE)	+= cpufreq_performance.o
obj-$(CONFIG_CPU_FREQ_GOV_POWERSAVE)	+= cpufreq_powersave.o
obj-$(CONFIG_CPU_FREQ_GOV_USERSPACE)	+= cpufreq_userspace.o
obj-$(CONFIG_CPU_FREQ_GOV_ONDEMAND)	+= cpufreq_ondemand.o
obj-$(CONFIG_CPU_FREQ_GOV_TEMPERATURE)	+= cpufreq_temperature.o
obj-$(CONFIG_CPU_FREQ_GOV_CONSERVATIVE)	+= cpufreq_conservative.o

# CPUfreq cross-arch helpers
obj-$(CONFIG_CPU_FREQ_TABLE)		+= freq_table.o

ifeq ($(CONFIG_AR10),y)
extra-$(CONFIG_CPU_FREQ_GOV_TEMPERATURE)    += S89-overheat_guard

$(src)/S89-overheat_guard: FORCE
	rm -rf $(src)/S89-overheat_guard
	echo "#! /bin/sh" >$(src)/S89-overheat_guard
	echo "echo performance > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor" >>$(src)/S89-overheat_guard
	mkdir -p $(INSTALL_MOD_PATH)/etc/init.d
	cp $(src)/S89-overheat_guard $(INSTALL_MOD_PATH)/etc/init.d/S89-overheat_guard
endif
