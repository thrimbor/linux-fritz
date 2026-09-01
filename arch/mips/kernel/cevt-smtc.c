/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2007 MIPS Technologies, Inc.
 * Copyright (C) 2007 Ralf Baechle <ralf@linux-mips.org>
 * Copyright (C) 2008 Kevin D. Kissell, Paralogos sarl
 */
#include <linux/clockchips.h>
#include <linux/interrupt.h>
#include <linux/percpu.h>
#include <linux/smp.h>

#include <asm/smtc_ipi.h>
#include <asm/time.h>
#include <asm/cevt-r4k.h>

/*
 * Variant clock event timer support for SMTC on MIPS 34K, 1004K
 * or other MIPS MT cores.
 *
 * Notes on SMTC Support:
 *
 * SMTC has multiple microthread TCs pretending to be Linux CPUs.
 * But there's only one Count/Compare pair per VPE, and Compare
 * interrupts are taken opportunisitically by available TCs
 * bound to the VPE with the Count register.  The new timer
 * framework provides for global broadcasts, but we really
 * want VPE-level multicasts for best behavior. So instead
 * of invoking the high-level clock-event broadcast code,
 * this version of SMTC support uses the historical SMTC
 * multicast mechanisms "under the hood", appearing to the
 * generic clock layer as if the interrupts are per-CPU.
 *
 * The approach taken here is to maintain a set of NR_CPUS
 * virtual timers, and track which "CPU" needs to be alerted
 * at each event.
 *
 * It's unlikely that we'll see a MIPS MT core with more than
 * 2 VPEs, but we *know* that we won't need to handle more
 * VPEs than we have "CPUs".  So NCPUs arrays of NCPUs elements
 * is always going to be overkill, but always going to be enough.
 */

int cp0_timer_irq_installed;

/*
 * Timestamps stored are absolute values to be programmed
 * into Count register.  Valid timestamps will never be zero.
 * If a Zero Count value is actually calculated, it is converted
 * to be a 1, which will introduce 1 or two CPU cycles of error
 * roughly once every four billion events, which at 1000 HZ means
 * about once every 50 days.  If that's actually a problem, one
 * could alternate squashing 0 to 1 and to -1.
 */

#define MAKEVALID(x) (((x) == 0L) ? 1L : (x))
#define ISVALID(x) ((x) != 0L)

/*
 * Time comparison is subtle, as it's really truncated
 * modular arithmetic.
 */

#define IS_SOONER(a, b, reference) \
    (((a) - (unsigned long)(reference)) < ((b) - (unsigned long)(reference)))

/*
 * CATCHUP_INCREMENT, used when the function falls behind the counter.
 * Could be an increasing function instead of a constant;
 */

#define CATCHUP_INCREMENT 64
static volatile unsigned long smtc_trigger[NR_CPUS][NR_CPUS];
#if NR_CPUS > 2
/*--------------------------------------------------------------------------------*\
 * get cpu that expire next time
 * < 0 kein Eintrag
\*--------------------------------------------------------------------------------*/
static int get_next_expire_cpu(unsigned long reference, unsigned int vpe) {
    unsigned int i;
    int cpu = -1;
    unsigned long mindiff = (unsigned long)LONG_MAX;

    for_each_online_cpu(i) {
        if(ISVALID(smtc_trigger[vpe][i])) {
            unsigned long diff = smtc_trigger[vpe][i] - reference;
            if(diff < mindiff) {
                mindiff = diff;
                cpu     = i;
            }
        }
    }
    return cpu;
}
#endif
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static int mips_next_event(unsigned long delta, struct clock_event_device *evt) {
	unsigned long flags;
	unsigned int mtflags;
	unsigned long timestamp, actual_timestamp;
    int next_expire_cpu __attribute__((unused));
	int vpe = current_cpu_data.vpe_id;
	int cpu = smp_processor_id();
	local_irq_save(flags);
	mtflags = dmt();

	actual_timestamp = (unsigned long)read_c0_count();
	timestamp        = MAKEVALID(actual_timestamp + delta);
#if NR_CPUS == 2
    /*--- only one TC per VPE: strictly programm this expire ---*/
    smtc_trigger[vpe][cpu] = timestamp;
#else
	/*
	 * Maintain the per-TC virtual timer
	 * and program the per-VPE shared Count register
	 * as appropriate here...
	 */
    next_expire_cpu = get_next_expire_cpu(actual_timestamp, vpe);
    if(unlikely(next_expire_cpu < 0)) {
        /*--- no actual expire on this vpe -> set actual timestamp at actual cpu (per-vpe) ---*/
        smtc_trigger[vpe][cpu] = timestamp;
    } else if(IS_SOONER(timestamp, actual_timestamp, smtc_trigger[vpe][next_expire_cpu])) {
        /*--- entry before actual expire -> program new expire at actual cpu (per-vpe) ---*/
        smtc_trigger[vpe][cpu] = timestamp;
    } else {
        if(next_expire_cpu != cpu) {
            /*--- check if actual timestamp sooner than stored for this vpe ---*/
            if(IS_SOONER(timestamp, actual_timestamp, smtc_trigger[vpe][cpu])) {
                smtc_trigger[vpe][cpu] = timestamp;
            }
        }
        /*--- timestamp behind next expire ---*/
        timestamp = smtc_trigger[vpe][cpu];
    }
#endif
	if (ISVALID(timestamp)) {
		write_c0_compare(timestamp);
		ehb();
		/*
		 * We never return an error, we just make sure
		 * that we trigger the handlers as quickly as
		 * we can if we fell behind.
		 */
		if ((timestamp - (unsigned long)read_c0_count()) > (unsigned long)LONG_MAX) {
			write_c0_compare((unsigned long)read_c0_count() + CATCHUP_INCREMENT);
			ehb();
		}
	}
	emt(mtflags);
	local_irq_restore(flags);
	return 0;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
void smtc_distribute_timer(int vpe) {
	unsigned long flags;
	unsigned int mtflags;
	unsigned long nextstamp;
	int cpu;
	struct clock_event_device *cd;
	unsigned long actual_timestamp;

    for(;;) {
        nextstamp = 0;
        for_each_online_cpu(cpu) {
            if (cpu_data[cpu].vpe_id != vpe) {
                continue;
            }
            local_irq_save(flags);
            mtflags = dmt();
            if(!ISVALID(smtc_trigger[vpe][cpu])) {
                local_irq_save(flags);
                mtflags = dmt();
                continue;
            }
            actual_timestamp = (unsigned long)read_c0_count();
            if((smtc_trigger[vpe][cpu] - actual_timestamp) > (unsigned long)LONG_MAX) {
    /*--- if(vpe== 1) ---*/
    /*--- printk("[%x]%s: act=%lu (%lu %lu) trigger cpu=%d", vpe, __func__, actual_timestamp, smtc_trigger[0], smtc_trigger[1], vpe); ---*/
                smtc_trigger[vpe][cpu] = 0L;
                emt(mtflags);
                local_irq_restore(flags);
                /* We don't send IPIs to ourself.  */
                if (cpu != smp_processor_id()) {
                    smtc_send_ipi(cpu, SMTC_CLOCK_TICK, 0);
                } else {
                    cd = &per_cpu(mips_clockevent_device, cpu);
                    cd->event_handler(cd);
                }
            } else {
                if(!ISVALID(nextstamp) || IS_SOONER(smtc_trigger[vpe][cpu], nextstamp, actual_timestamp)) {
                    nextstamp = smtc_trigger[vpe][cpu];
                }
                emt(mtflags);
                local_irq_restore(flags);
            }
        }
        /* Reprogram for interrupt at next soonest timestamp for VPE */
        if (ISVALID(nextstamp)) {
            write_c0_compare(nextstamp);
            ehb();
            if ((nextstamp - (unsigned long)read_c0_count()) > (unsigned long)LONG_MAX)  {
                continue;
            }
        }
        break;
    }
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
irqreturn_t c0_compare_interrupt(int irq, void *dev_id) {
	int cpu = smp_processor_id();

	/* If we're running SMTC, we've got MIPS MT and therefore MIPS32R2 */
	handle_perf_irq(1);

	if (read_c0_cause() & (1 << 30)) {
		/* Clear Count/Compare Interrupt */
		write_c0_compare(read_c0_compare());
		smtc_distribute_timer(cpu_data[cpu].vpe_id);
	}
	return IRQ_HANDLED;
}


int __cpuinit smtc_clockevent_init(void)
{
	uint64_t mips_freq = mips_hpt_frequency;
	unsigned int cpu = smp_processor_id();
	struct clock_event_device *cd;
	unsigned int irq;
	int i;
	int j;

	if (!cpu_has_counter || !mips_hpt_frequency)
		return -ENXIO;
	if (cpu == 0) {
		for (i = 0; i < num_possible_cpus(); i++) {
			for (j = 0; j < num_possible_cpus(); j++)
				smtc_trigger[i][j] = 0L;
		}
		/*
		 * SMTC also can't have the usablility test
		 * run by secondary TCs once Compare is in use.
		 */
		if (!c0_compare_int_usable())
			return -ENXIO;
	}

	/*
	 * With vectored interrupts things are getting platform specific.
	 * get_c0_compare_int is a hook to allow a platform to return the
	 * interrupt number of it's liking.
	 */
	irq = MIPS_CPU_IRQ_BASE + cp0_compare_irq;
	if (get_c0_compare_int)
		irq = get_c0_compare_int();

	cd = &per_cpu(mips_clockevent_device, cpu);

	cd->name		= "MIPS";
	cd->features		= CLOCK_EVT_FEAT_ONESHOT;

	/* Calculate the min / max delta */
	cd->mult	= div_sc((unsigned long) mips_freq, NSEC_PER_SEC, 32);
	cd->shift		= 32;
	cd->max_delta_ns	= clockevent_delta2ns(0x7fffffff, cd);
	cd->min_delta_ns	= clockevent_delta2ns(0x300, cd);

	cd->rating		= 300;
	cd->irq			= irq;
	cd->cpumask		= cpumask_of(cpu);
	cd->set_next_event	= mips_next_event;
	cd->set_mode		= mips_set_clock_mode;
	cd->event_handler	= mips_event_handler;

	clockevents_register_device(cd);

	/*
	 * On SMTC we only want to do the data structure
	 * initialization and IRQ setup once.
	 */
	if (cpu)
		return 0;
	/*
	 * And we need the hwmask associated with the c0_compare
	 * vector to be initialized.
	 */
	irq_hwmask[irq] = (0x100 << cp0_compare_irq);
	if (cp0_timer_irq_installed)
		return 0;

	cp0_timer_irq_installed = 1;

	setup_irq(irq, &c0_compare_irqaction);

	return 0;
}
