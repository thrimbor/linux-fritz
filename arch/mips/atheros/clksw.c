#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/resource.h>
#include <linux/percpu.h>
#include <linux/smp.h>

#include <linux/console.h>
#include <asm/time.h>
#include <linux/miscdevice.h>
#include <asm/delay.h>
#include <asm/cevt-r4k.h>

#include <atheros.h>
#include <asm/mach_avm.h>

#define ATH_CPU_DDR_CLOCK_CONTROL	(ATH_PLL_BASE + CPU_DDR_CLOCK_CONTROL_OFFSET)
#define ATH_CPU_PLL_CONFIG		(ATH_PLL_BASE + CPU_PLL_CONFIG_OFFSET)

unsigned int ath_get_clock(enum _avm_clock_id id);
unsigned int cpu_get_clock(void);
unsigned int cpu_set_clock(unsigned int freq);
unsigned int ddr_get_clock(void);
unsigned int ddr_set_clock(unsigned int freq);

#define PRINTD(args...) printk(args)
/*--- #define PRINTD(args...) printk("[%d]\n", __LINE__) ---*/

/*
 * Leute:
 *
 * Diese Tabelle ist ein uebler Hack.
 * Ich habe sie nur gebaut, damit man von der cmd-line lesebare Frequenzen eingeben kann.
 * Die eingegebene Wunschfrequenz wird rueckwaerts gerechnet um auf die korrekten Teilerwerte zu kommen.
 * Da waere jedenfalls wuenschenswert. Es kommen aber Teiler raus die immer in anderen 
 * Frequenzen resultieren -> deshalb diese Tabelle!
 */
struct clk_translation_descr {
    unsigned int target_freq;
    unsigned int pll_freq;
} clk_trans[] = {
    { .target_freq = 560, .pll_freq = 560 },
    { .target_freq = 480, .pll_freq = 500 },
    { .target_freq = 400, .pll_freq = 400 },
    { .target_freq = 320, .pll_freq = 350 },
    { .target_freq = 280, .pll_freq = 300 },
    { .target_freq = 200, .pll_freq = 200 },
    { .target_freq = 80, .pll_freq = 100 },
    { .target_freq = 0, .pll_freq = 0 },
};

static ssize_t ath_clksw_read(struct file *file, char __user *buf, size_t count, loff_t *ppos) {
    count = 0;
    count += sprintf(buf + count, "Quick'n'Dirty hack to switch clock on atheros based cpus.\n");
    count += sprintf(buf + count, "Target clock calc ist not perfect, so you have to use the\n");
    count += sprintf(buf + count, "supported values below.\n");
    count += sprintf(buf + count, "\n");
    count += sprintf(buf + count, "Supported clk settings\n");
    count += sprintf(buf + count, "======================\n");
    count += sprintf(buf + count, "Command: 'echo xxx > clksw'\n");
    {
        int i = 0;

        while(clk_trans[i].target_freq != 0) {
            count += sprintf(buf + count, "    %d MHz\n", clk_trans[i].target_freq);
            i++;
        }
    }
    count += sprintf(buf + count, "\n");
    count += sprintf(buf + count, "!!! If you know what you are doing, you can pass a custom value !!!\n");
    count += sprintf(buf + count, "\n");
    count += sprintf(buf + count, "\n");
    count += sprintf(buf + count, "Current clk settings\n");
    count += sprintf(buf + count, "====================\n");
    count += sprintf(buf + count, "cpu:  % 3d MHz\n", ath_get_clock(avm_clock_id_cpu)/1000000);
    count += sprintf(buf + count, "ddr:  % 3d MHz\n", ath_get_clock(avm_clock_id_ddr)/1000000);
    count += sprintf(buf + count, "ahb:  % 3d MHz\n", ath_get_clock(avm_clock_id_ahb)/1000000);
    count += sprintf(buf + count, "ref:  % 3d MHz\n", ath_get_clock(avm_clock_id_ref)/1000000);
    count += sprintf(buf + count, "uart: % 3d MHz\n", ath_get_clock(avm_clock_id_peripheral)/1000000);


    if(*ppos >= count) {
        return 0;
    }

    *ppos += count;
   
	return count;
}

#define DPLL_REG             ((id == avm_clock_id_cpu) ? CPU_DPLL_ADDRESS          : DDR_DPLL_ADDRESS)
#define DPLL2_REG            ((id == avm_clock_id_cpu) ? CPU_DPLL2_ADDRESS         : DDR_DPLL2_ADDRESS)
#define PLL_CONFIG_REG       ((id == avm_clock_id_cpu) ? ATH_CPU_PLL_CONFIG : ATH_DDR_PLL_CONFIG)
#define PLL_NINT_SET(VAL)    ((id == avm_clock_id_cpu) ? CPU_PLL_CONFIG_NINT_SET(VAL)   : DDR_PLL_CONFIG_NINT_SET(VAL))
#define PLL_REFDIV_SET(VAL)  ((id == avm_clock_id_cpu) ? CPU_PLL_CONFIG_REFDIV_SET(VAL) : DDR_PLL_CONFIG_REFDIV_SET(VAL))
#define PLL_RANGE_SET(VAL)   ((id == avm_clock_id_cpu) ? CPU_PLL_CONFIG_RANGE_SET(VAL)  : DDR_PLL_CONFIG_RANGE_SET(VAL))
#define PLL_OUTDIV_SET(VAL)  ((id == avm_clock_id_cpu) ? CPU_PLL_CONFIG_OUTDIV_SET(VAL) : DDR_PLL_CONFIG_OUTDIV_SET(VAL))
#define PLL_PLLPWD_SET(VAL)  ((id == avm_clock_id_cpu) ? CPU_PLL_CONFIG_PLLPWD_SET(VAL) : DDR_PLL_CONFIG_PLLPWD_SET(VAL))
#define PLL_OUTDIV_MASK      ((id == avm_clock_id_cpu) ? CPU_PLL_CONFIG_OUTDIV_MASK     : DDR_PLL_CONFIG_OUTDIV_MASK)
#define PLL_PLLPWD_MASK      ((id == avm_clock_id_cpu) ? CPU_PLL_CONFIG_PLLPWD_MASK     : DDR_PLL_CONFIG_PLLPWD_MASK)    
#define PLL_BYPASS_SET(VAL)  ((id == avm_clock_id_cpu) ? CPU_DDR_CLOCK_CONTROL_CPU_PLL_BYPASS_SET(VAL) : (CPU_DDR_CLOCK_CONTROL_DDR_PLL_BYPASS_SET(VAL) | CPU_DDR_CLOCK_CONTROL_AHB_PLL_BYPASS_SET(VAL)))
#define PLL_BYPASS_MASK      ((id == avm_clock_id_cpu) ? CPU_DDR_CLOCK_CONTROL_CPU_PLL_BYPASS_MASK     : (CPU_DDR_CLOCK_CONTROL_DDR_PLL_BYPASS_MASK     | CPU_DDR_CLOCK_CONTROL_AHB_PLL_BYPASS_MASK))
/*--- #define PLL_BYPASS_SET(VAL)  ((id == avm_clock_id_cpu) ? CPU_DDR_CLOCK_CONTROL_CPU_PLL_BYPASS_SET(VAL) : (CPU_DDR_CLOCK_CONTROL_DDR_PLL_BYPASS_SET(VAL) )) ---*/
/*--- #define PLL_BYPASS_MASK      ((id == avm_clock_id_cpu) ? CPU_DDR_CLOCK_CONTROL_CPU_PLL_BYPASS_MASK     : (CPU_DDR_CLOCK_CONTROL_DDR_PLL_BYPASS_MASK     )) ---*/
#define PLL_REFDIV_MASK      ((id == avm_clock_id_cpu) ? CPU_DPLL_REFDIV_MASK      : DDR_DPLL_REFDIV_MASK)
#define PLL_NINT_MASK        ((id == avm_clock_id_cpu) ? CPU_DPLL_NINT_MASK        : DDR_DPLL_NINT_MASK)

static ssize_t ath_clksw_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos) {
    char my_buf[3];
    int clock = 0;
    unsigned int set_clock = 0;

    if (copy_from_user(my_buf, buf, 3)) {
        return -EFAULT;
    }

    {
        int i = 0;

        while((my_buf[i] >= '0') && (my_buf[i] <= '9')) {
            clock *= 10;
            clock += (my_buf[i] - '0');

            /*--- printk("clock: %d, i: %d, my_buf[i]: %c\n", clock, i, my_buf[i]); ---*/

            i++;
        }

        i = 0;
        while(clk_trans[i].target_freq != 0) {
            if(clk_trans[i].target_freq == clock) {
                set_clock = clk_trans[i].pll_freq;
            }
            i++;
        }

    }
    /*--- printk("clock: %d\n", clock); ---*/

    if(set_clock == 0) {
        /*--- cpu_set_clock(clock * 1000000); ---*/
        ddr_set_clock(clock * 1000000);
    } else {
        /*--- cpu_set_clock(set_clock * 1000000); ---*/
        ddr_set_clock(set_clock * 1000000);
    }
    return count;
}

//static ssize_t
//ath_clksw_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
//{
//	struct clock_event_device *cd;
//	unsigned int cpu = smp_processor_id();
//	uint8_t setting;
//	uint32_t val;
//	struct { uint32_t freq, pre, post; } pll[] = {
//		{ 300,	CPU_PLL_CONFIG_NINT_SET(0x18)	|
//			CPU_PLL_CONFIG_REFDIV_SET(1)	|
//			CPU_PLL_CONFIG_RANGE_SET(1)	|
//			CPU_PLL_CONFIG_OUTDIV_SET(1),
//			CPU_PLL_CONFIG_OUTDIV_SET(1)		},
//		{ 400,	CPU_PLL_CONFIG_NINT_SET(32)	|
//			CPU_PLL_CONFIG_REFDIV_SET(1)	|
//			CPU_PLL_CONFIG_RANGE_SET(0)	|
//			CPU_PLL_CONFIG_OUTDIV_SET(1),
//			CPU_PLL_CONFIG_OUTDIV_SET(1)		},
//		{ 500,	CPU_PLL_CONFIG_NINT_SET(20)	|
//			CPU_PLL_CONFIG_REFDIV_SET(1)	|
//			CPU_PLL_CONFIG_RANGE_SET(3)	|
//			CPU_PLL_CONFIG_OUTDIV_SET(1),
//			CPU_PLL_CONFIG_OUTDIV_SET(0)		},
//		{ 600,	CPU_PLL_CONFIG_NINT_SET(24)	|
//			CPU_PLL_CONFIG_REFDIV_SET(1)	|
//			CPU_PLL_CONFIG_RANGE_SET(0)	|
//			CPU_PLL_CONFIG_OUTDIV_SET(1),
//			CPU_PLL_CONFIG_OUTDIV_SET(0)		},
//	};
//
//	if (copy_from_user(&setting, buf, 1)) {
//		return -EFAULT;
//	}
//
//	setting = setting - '0';
//
//	if (setting < 0 ||
//	    setting >= (sizeof(pll) / sizeof(pll[0]))) {
//		return -EINVAL;
//	}
//
//	printk("%s: setting: %d | freq: %d | pre: 0x%x | post: 0x%x\n", __func__, setting,
//        pll[setting].freq,
//		pll[setting].pre, pll[setting].post);
//
//	// bypass for cpu pll
//	val = ath_reg_rd(ATH_CPU_DDR_CLOCK_CONTROL);
//	val &= ~CPU_DDR_CLOCK_CONTROL_CPU_PLL_BYPASS_MASK;
//	val |= CPU_DDR_CLOCK_CONTROL_CPU_PLL_BYPASS_SET(1);
//	ath_reg_wr(ATH_CPU_DDR_CLOCK_CONTROL, val);
//	udelay(10);
//
//	// pll settings...
//	ath_reg_wr(ATH_CPU_PLL_CONFIG,
//		pll[setting].pre | CPU_PLL_CONFIG_PLLPWD_SET(1));
//	udelay(10);
//
//	// clear pll power
//	val = ath_reg_rd(ATH_CPU_PLL_CONFIG);
//	val &= ~CPU_PLL_CONFIG_PLLPWD_MASK;
//	val |= CPU_PLL_CONFIG_PLLPWD_SET(0);
//	ath_reg_wr(ATH_CPU_PLL_CONFIG, val);
//	udelay(100);
//
//	// reset out div
//	val = ath_reg_rd(ATH_CPU_PLL_CONFIG);
//	val &= ~CPU_PLL_CONFIG_OUTDIV_MASK;
//	val |= pll[setting].post;
//	ath_reg_wr(ATH_CPU_PLL_CONFIG, val);
//	udelay(10);
//
//	// unset bypass for cpu pll
//	val = ath_reg_rd(ATH_CPU_DDR_CLOCK_CONTROL);
//	val &= ~CPU_DDR_CLOCK_CONTROL_CPU_PLL_BYPASS_MASK;
//	val |= CPU_DDR_CLOCK_CONTROL_CPU_PLL_BYPASS_SET(0);
//	ath_reg_wr(ATH_CPU_DDR_CLOCK_CONTROL, val);
//	udelay(10);
//
//	// reconfigure kernel's notion of time
//	mips_hpt_frequency = pll[setting].freq * 1000000 / 2;
//
//	// see r4k_clockevent_init()
//	cd = &per_cpu(mips_clockevent_device, cpu);
//	cd->mult		= div_sc((unsigned long) mips_hpt_frequency, NSEC_PER_SEC, 32);
//	cd->max_delta_ns	= clockevent_delta2ns(0x7fffffff, cd);
//	cd->min_delta_ns	= clockevent_delta2ns(0x300, cd);
//	printk("%s: mult = %lu\n", __func__, cd->mult);
//
//	return 1;
//}

static struct file_operations ath_clksw_fops = {
	.read		= ath_clksw_read,
	.write		= ath_clksw_write,
};

static struct miscdevice ath_clksw_miscdev = {
	ATH_CLKSW_MINOR,
	"clksw",
	&ath_clksw_fops
};


static int __init
ath_clksw_init(void)
{
	u32	tdata;

	printk("%s: Registering Clock Switch Interface ", __func__);

	if ((tdata = misc_register(&ath_clksw_miscdev))) {
		printk("failed %d\n", tdata);
		return tdata;
	} else {
		printk("success\n");
	}

	return 0;
}
late_initcall(ath_clksw_init);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void ath_internal_set_ddr_clk(unsigned int NINT, unsigned int REFDIV, unsigned int RANGE, unsigned int OUTDIV) {
	struct clock_event_device *cd;
	unsigned int cpu = smp_processor_id();
	uint32_t val;
    unsigned int pll_config_reg;
    unsigned int srif_pll_reg,  srif_pll;
    unsigned int srif_pll2_reg, srif_pll2;

#if 0
    pll_config_reg = ATH_PLL_CONFIG;
    srif_pll_reg   = CPU_DPLL_ADDRESS;
    srif_pll2_reg  = CPU_DPLL2_ADDRESS;
#else
    pll_config_reg = ATH_DDR_PLL_CONFIG;
    srif_pll_reg   = DDR_DPLL_ADDRESS;
    srif_pll2_reg  = DDR_DPLL2_ADDRESS;
#endif
	/*--- if ((ath_reg_rd(ATH_BOOTSTRAP_REG) & ATH_REF_CLK_40)) { ---*/
		/*--- ref = (40 * 1000000); ---*/
	/*--- } else { ---*/
		/*--- ref = (25 * 1000000); ---*/
	/*--- } ---*/


	srif_pll2 = ath_reg_rd(srif_pll2_reg);
        printk("[%s] current ddr clock: %u (NINT_new=%u OUTDIV_new=%u RANGE_new=%u REFDIV_new=%u, %s)\n", __FUNCTION__, ddr_get_clock(),
                NINT, OUTDIV, REFDIV, RANGE, CPU_DPLL2_LOCAL_PLL_GET(srif_pll2) ? "SRIF" : "NORMAL");

	if (CPU_DPLL2_LOCAL_PLL_GET(srif_pll2)) {
        /*--- Set PLL clock manually via SRIF Registers ---*/
        unsigned int srif_pll_old, srif_pll2_old, nint_only_switch;
	    srif_pll = ath_reg_rd(srif_pll_reg);

        srif_pll2_old = srif_pll2;
        srif_pll2 &= ~CPU_DPLL2_OUTDIV_MASK;
        srif_pll2 |=  CPU_DPLL2_OUTDIV_SET(OUTDIV);
        srif_pll2 &= ~CPU_DPLL2_RANGE_MASK;
        srif_pll2 |=  CPU_DPLL2_RANGE_SET(RANGE);

        srif_pll_old = srif_pll;
        srif_pll &= ~CPU_DPLL_REFDIV_MASK;
        srif_pll |=  CPU_DPLL_REFDIV_SET(REFDIV);
    
        nint_only_switch = ((srif_pll == srif_pll_old) && (srif_pll2 == srif_pll2_old));

        srif_pll &= ~CPU_DPLL_NINT_MASK;
        srif_pll |=  CPU_DPLL_NINT_SET(NINT);

        if(!nint_only_switch) {
            // bypass for cpu pll
            val = ath_reg_rd(ATH_CPU_DDR_CLOCK_CONTROL);
            val &= ~CPU_DDR_CLOCK_CONTROL_CPU_PLL_BYPASS_MASK;
            val |= CPU_DDR_CLOCK_CONTROL_CPU_PLL_BYPASS_SET(RANGE);
            printk(KERN_ERR "[%s:%d]\n", __FUNCTION__, __LINE__);
            ath_reg_wr(ATH_CPU_DDR_CLOCK_CONTROL, val);
            udelay(10);
        }

            printk(KERN_ERR "[%s:%d] srif_pll=0x%x\n", __FUNCTION__, __LINE__, srif_pll);
	    ath_reg_wr(srif_pll_reg, srif_pll);
            printk(KERN_ERR "[%s:%d] srif_pll2=0x%x\n", __FUNCTION__, __LINE__, srif_pll2);
	    ath_reg_wr(srif_pll2_reg, srif_pll2);
        
        if(!nint_only_switch) {
            // bypass for cpu pll
            val = ath_reg_rd(ATH_CPU_DDR_CLOCK_CONTROL);
            val &= ~CPU_DDR_CLOCK_CONTROL_CPU_PLL_BYPASS_MASK;
            val |= CPU_DDR_CLOCK_CONTROL_CPU_PLL_BYPASS_SET(RANGE);
            printk(KERN_ERR "[%s:%d]\n", __FUNCTION__, __LINE__);
            ath_reg_wr(ATH_CPU_DDR_CLOCK_CONTROL, val);
            udelay(10);
        }

        udelay(10);
	    srif_pll = ath_reg_rd(srif_pll_reg);
	    srif_pll2 = ath_reg_rd(srif_pll2_reg);
        printk("[%s] new srif ddr clock: %u (NINT=%d OUTDIV=%d RANGE=%d REFDIV=%d Bypass=%d\n", __FUNCTION__, ddr_get_clock(),
                DDR_DPLL_NINT_GET(srif_pll),
                DDR_DPLL2_OUTDIV_GET(srif_pll2),
                DDR_DPLL2_RANGE_GET(srif_pll2),
                DDR_DPLL_REFDIV_GET(srif_pll),
                !nint_only_switch
                );

	} else {
        unsigned int nint_only_switch, pll, pll_old;

        pll_old = ath_reg_rd(pll_config_reg);
        pll = DDR_PLL_CONFIG_NINT_SET(NINT)     |
              DDR_PLL_CONFIG_REFDIV_SET(REFDIV) |
              DDR_PLL_CONFIG_RANGE_SET(RANGE)       |
              DDR_PLL_CONFIG_OUTDIV_SET(OUTDIV) |
              DDR_PLL_CONFIG_PLLPWD_SET(1);
        nint_only_switch = (NINT == DDR_PLL_CONFIG_NINT_GET(pll_old));

        if(!nint_only_switch) {
            // bypass for cpu pll
            val = ath_reg_rd(ATH_CPU_DDR_CLOCK_CONTROL);
            val &= ~CPU_DDR_CLOCK_CONTROL_CPU_PLL_BYPASS_MASK;
            val |= CPU_DDR_CLOCK_CONTROL_CPU_PLL_BYPASS_SET(RANGE);
            printk(KERN_ERR "[%s:%d]\n", __FUNCTION__, __LINE__);
            ath_reg_wr(ATH_CPU_DDR_CLOCK_CONTROL, val);
            udelay(10);
        }

        // pll settings...
            printk(KERN_ERR "[%s:%d] NINT=0x%x | pll_old=0x%x => pll=0x%x\n", __FUNCTION__, __LINE__, NINT, pll_old, pll);
        ath_reg_wr(pll_config_reg, pll);
        udelay(10);

        if(!nint_only_switch) {
            // clear pll power
            val = ath_reg_rd(pll_config_reg);
            val &= ~DDR_PLL_CONFIG_PLLPWD_MASK;
            val |= DDR_PLL_CONFIG_PLLPWD_SET(0);
            printk(KERN_ERR "[%s:%d]\n", __FUNCTION__, __LINE__);
            ath_reg_wr(pll_config_reg, val);
            udelay(100);

            // reset out div
            val = ath_reg_rd(pll_config_reg);
            val &= ~DDR_PLL_CONFIG_OUTDIV_MASK;
            val |= DDR_PLL_CONFIG_OUTDIV_SET(OUTDIV);
            printk(KERN_ERR "[%s:%d]\n", __FUNCTION__, __LINE__);
            ath_reg_wr(pll_config_reg, val);
            udelay(10);

            // unset bypass for cpu pll
            val = ath_reg_rd(ATH_CPU_DDR_CLOCK_CONTROL);
            val &= ~CPU_DDR_CLOCK_CONTROL_CPU_PLL_BYPASS_MASK;
            val |= CPU_DDR_CLOCK_CONTROL_CPU_PLL_BYPASS_SET(0);
            printk(KERN_ERR "[%s:%d]\n", __FUNCTION__, __LINE__);
            ath_reg_wr(ATH_CPU_DDR_CLOCK_CONTROL, val);
            udelay(10);
        }

        pll = ath_reg_rd(pll_config_reg);
        printk("[%s] new apb ddr clock: %u (NINT=%d OUTDIV=%d NFRAC=%d REFDIV=%d Bypass=%d\n", __FUNCTION__, ddr_get_clock(),
		        DDR_PLL_CONFIG_NINT_GET(pll),
		        DDR_PLL_CONFIG_OUTDIV_GET(pll),
		        DDR_PLL_CONFIG_NFRAC_GET(pll),
		        DDR_PLL_CONFIG_REFDIV_GET(pll),
                !nint_only_switch
                );
    }

	// reconfigure kernel's notion of time
    {
        unsigned int freq = cpu_get_clock();
        mips_hpt_frequency = freq * 1000000 / 2;

        // see r4k_clockevent_init()
        cd = &per_cpu(mips_clockevent_device, cpu);
        cd->mult		= div_sc((unsigned long) mips_hpt_frequency, NSEC_PER_SEC, 32);
        cd->max_delta_ns	= clockevent_delta2ns(0x7fffffff, cd);
        cd->min_delta_ns	= clockevent_delta2ns(0x300, cd);
    }
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void ath_internal_set_cpu_clk(unsigned int NINT, unsigned int REFDIV, unsigned int RANGE, unsigned int OUTDIV) {
	struct clock_event_device *cd;
	unsigned int cpu = smp_processor_id();
	uint32_t val;

	// bypass for cpu pll
	val = ath_reg_rd(ATH_CPU_DDR_CLOCK_CONTROL);
	val &= ~CPU_DDR_CLOCK_CONTROL_CPU_PLL_BYPASS_MASK;
	val |= CPU_DDR_CLOCK_CONTROL_CPU_PLL_BYPASS_SET(RANGE);
	ath_reg_wr(ATH_CPU_DDR_CLOCK_CONTROL, val);
	udelay(10);

	// pll settings...
	ath_reg_wr(ATH_CPU_PLL_CONFIG,
        CPU_PLL_CONFIG_NINT_SET(NINT)     |
        CPU_PLL_CONFIG_REFDIV_SET(REFDIV) |
        CPU_PLL_CONFIG_RANGE_SET(1)       |
        CPU_PLL_CONFIG_OUTDIV_SET(OUTDIV)
		| CPU_PLL_CONFIG_PLLPWD_SET(1));
	udelay(10);

	// clear pll power
	val = ath_reg_rd(ATH_CPU_PLL_CONFIG);
	val &= ~CPU_PLL_CONFIG_PLLPWD_MASK;
	val |= CPU_PLL_CONFIG_PLLPWD_SET(0);
	ath_reg_wr(ATH_CPU_PLL_CONFIG, val);
	udelay(100);

	// reset out div
	val = ath_reg_rd(ATH_CPU_PLL_CONFIG);
	val &= ~CPU_PLL_CONFIG_OUTDIV_MASK;
	val |= CPU_PLL_CONFIG_OUTDIV_SET(OUTDIV);
	ath_reg_wr(ATH_CPU_PLL_CONFIG, val);
	udelay(10);

	// unset bypass for cpu pll
	val = ath_reg_rd(ATH_CPU_DDR_CLOCK_CONTROL);
	val &= ~CPU_DDR_CLOCK_CONTROL_CPU_PLL_BYPASS_MASK;
	val |= CPU_DDR_CLOCK_CONTROL_CPU_PLL_BYPASS_SET(0);
	ath_reg_wr(ATH_CPU_DDR_CLOCK_CONTROL, val);
	udelay(10);

	// reconfigure kernel's notion of time
    {
        unsigned int freq = cpu_get_clock();
        mips_hpt_frequency = freq * 1000000 / 2;

        // see r4k_clockevent_init()
        cd = &per_cpu(mips_clockevent_device, cpu);
        cd->mult		= div_sc((unsigned long) mips_hpt_frequency, NSEC_PER_SEC, 32);
        cd->max_delta_ns	= clockevent_delta2ns(0x7fffffff, cd);
        cd->min_delta_ns	= clockevent_delta2ns(0x300, cd);
    }
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void ath_internal_set_pll_clk_ctrl(enum _avm_clock_id id, unsigned int NINT, unsigned int REFDIV, unsigned int RANGE, unsigned int OUTDIV) {
	struct clock_event_device *cd;
	unsigned int cpu = smp_processor_id();
	uint32_t val;


            PRINTD("[%s:%d]CPU_PLL=0x%08x DDR_PLL=0x%08x CLK_CTRL=0x%08x\n", __FUNCTION__, __LINE__, 
                    ath_reg_rd(ATH_CPU_PLL_CONFIG),
                    ath_reg_rd(ATH_DDR_PLL_CONFIG),
                    ath_reg_rd(ATH_CPU_DDR_CLOCK_CONTROL));
PRINTD("[%s:%d] NINT=%u, REFDIV=%u, RANGE=%u, OUTDIV=%u\n", __FUNCTION__, __LINE__, NINT, REFDIV, RANGE, OUTDIV);
#if 1
/*--- if(id == avm_clock_id_ddr) ---*/ 
{
	// bypass for cpu pll
	val = ath_reg_rd(ATH_CPU_DDR_CLOCK_CONTROL);
    val &= ~PLL_BYPASS_MASK;
	val |= PLL_BYPASS_SET(RANGE);
	ath_reg_wr(ATH_CPU_DDR_CLOCK_CONTROL, val);
	udelay(10);
}
#endif
            PRINTD("[%s:%d]CPU_PLL=0x%08x DDR_PLL=0x%08x CLK_CTRL=0x%08x\n", __FUNCTION__, __LINE__, 
                    ath_reg_rd(ATH_CPU_PLL_CONFIG),
                    ath_reg_rd(ATH_DDR_PLL_CONFIG),
                    ath_reg_rd(ATH_CPU_DDR_CLOCK_CONTROL));
	// pll settings...
    PRINTD("[%s:%d]CUR_PLL=0x%08x (new)\n", __FUNCTION__, __LINE__,
        PLL_NINT_SET(NINT)     |
        PLL_REFDIV_SET(REFDIV) |
        PLL_RANGE_SET(1)       |
        PLL_OUTDIV_SET(OUTDIV) | 
        PLL_PLLPWD_SET(1)
                    );
	ath_reg_wr(PLL_CONFIG_REG,
        PLL_NINT_SET(NINT)     |
        PLL_REFDIV_SET(REFDIV) |
        PLL_RANGE_SET(1)       |
        PLL_OUTDIV_SET(OUTDIV) | 
        PLL_PLLPWD_SET(1));
	udelay(10);

            PRINTD("[%s:%d]CPU_PLL=0x%08x DDR_PLL=0x%08x CLK_CTRL=0x%08x\n", __FUNCTION__, __LINE__, 
                    ath_reg_rd(ATH_CPU_PLL_CONFIG),
                    ath_reg_rd(ATH_DDR_PLL_CONFIG),
                    ath_reg_rd(ATH_CPU_DDR_CLOCK_CONTROL));
	// clear pll power
	val = ath_reg_rd(PLL_CONFIG_REG);
	val &= ~PLL_PLLPWD_MASK;
	val |= PLL_PLLPWD_SET(0);
	ath_reg_wr(PLL_CONFIG_REG, val);
	udelay(100);

            PRINTD("[%s:%d]CPU_PLL=0x%08x DDR_PLL=0x%08x CLK_CTRL=0x%08x\n", __FUNCTION__, __LINE__, 
                    ath_reg_rd(ATH_CPU_PLL_CONFIG),
                    ath_reg_rd(ATH_DDR_PLL_CONFIG),
                    ath_reg_rd(ATH_CPU_DDR_CLOCK_CONTROL));
	// reset out div
	val = ath_reg_rd(PLL_CONFIG_REG);
	val &= ~PLL_OUTDIV_MASK;
	val |= PLL_OUTDIV_SET(OUTDIV);
	ath_reg_wr(PLL_CONFIG_REG, val);
	udelay(10);

            PRINTD("[%s:%d]CPU_PLL=0x%08x DDR_PLL=0x%08x CLK_CTRL=0x%08x\n", __FUNCTION__, __LINE__, 
                    ath_reg_rd(ATH_CPU_PLL_CONFIG),
                    ath_reg_rd(ATH_DDR_PLL_CONFIG),
                    ath_reg_rd(ATH_CPU_DDR_CLOCK_CONTROL));

#if 1
/*--- if(id == avm_clock_id_ddr) ---*/ 
{
	// unset bypass for cpu pll
	val = ath_reg_rd(ATH_CPU_DDR_CLOCK_CONTROL);
    val &= ~PLL_BYPASS_MASK;
	val |= PLL_BYPASS_SET(0);
	ath_reg_wr(ATH_CPU_DDR_CLOCK_CONTROL, val);
	udelay(10);
            PRINTD("[%s:%d]CPU_PLL=0x%08x DDR_PLL=0x%08x CLK_CTRL=0x%08x\n", __FUNCTION__, __LINE__, 
                    ath_reg_rd(ATH_CPU_PLL_CONFIG),
                    ath_reg_rd(ATH_DDR_PLL_CONFIG),
                    ath_reg_rd(ATH_CPU_DDR_CLOCK_CONTROL));
}
#endif

#if 0
	// reconfigure kernel's notion of time
    {
        unsigned int freq = cpu_get_clock();
        mips_hpt_frequency = freq * 1000000 / 2;

        // see r4k_clockevent_init()
        cd = &per_cpu(mips_clockevent_device, cpu);
        cd->mult		= div_sc((unsigned long) mips_hpt_frequency, NSEC_PER_SEC, 32);
        cd->max_delta_ns	= clockevent_delta2ns(0x7fffffff, cd);
        cd->min_delta_ns	= clockevent_delta2ns(0x300, cd);
    }
#endif
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void ath_internal_set_pll_clk_srif(enum _avm_clock_id id, unsigned int NINT, unsigned int REFDIV, unsigned int RANGE, unsigned int OUTDIV) {
	struct clock_event_device *cd;
	unsigned int cpu = smp_processor_id();
	uint32_t val;

#if 0
#define PLL_NINT_SET(VAL)    ((id == avm_clock_id_cpu) ? CPU_DPLL_NINT_SET(VAL)    : DDR_DPLL_NINT_SET(VAL))
#define PLL_REFDIV_SET(VAL)  ((id == avm_clock_id_cpu) ? CPU_DPLL_REFDIV_SET(VAL)  : DDR_DPLL_REFDIV_SET(VAL))
#define PLL_RANGE_MASK       ((id == avm_clock_id_cpu) ? CPU_DPLL2_RANGE_MASK      : DDR_DPLL2_RANGE_MASK)
#define PLL_RANGE_SET(VAL)   ((id == avm_clock_id_cpu) ? CPU_DPLL2_RANGE_SET(VAL)  : DDR_DPLL2_RANGE_SET(VAL))
#define PLL_OUTDIV_MASK      ((id == avm_clock_id_cpu) ? CPU_DPLL2_OUTDIV_MASK     : DDR_DPLL2_OUTDIV_MASK)
#define PLL_OUTDIV_SET(VAL)  ((id == avm_clock_id_cpu) ? CPU_DPLL2_OUTDIV_SET(VAL) : DDR_DPLL2_OUTDIV_SET(VAL))
#define PLL_PLLPWD_MASK      ((id == avm_clock_id_cpu) ? CPU_DPLL2_PLLPWD_MASK     : DDR_DPLL2_PLLPWD_MASK)    
#define PLL_PLLPWD_SET(VAL)  ((id == avm_clock_id_cpu) ? CPU_DPLL2_PLL_PWD_SET(VAL) : DDR_DPLL2_PLL_PWD_SET(VAL))
#define PLL_BYPASS_MASK      ((id == avm_clock_id_cpu) ? CPU_DDR_CLOCK_CONTROL_CPU_PLL_BYPASS_MASK     : (CPU_DDR_CLOCK_CONTROL_DDR_PLL_BYPASS_MASK     | CPU_DDR_CLOCK_CONTROL_AHB_PLL_BYPASS_MASK))
#define PLL_BYPASS_SET(VAL)  ((id == avm_clock_id_cpu) ? CPU_DDR_CLOCK_CONTROL_CPU_PLL_BYPASS_SET(VAL) : (CPU_DDR_CLOCK_CONTROL_DDR_PLL_BYPASS_SET(VAL) | CPU_DDR_CLOCK_CONTROL_AHB_PLL_BYPASS_SET(VAL)))
/*--- #define PLL_BYPASS_MASK      ((id == avm_clock_id_cpu) ? CPU_DDR_CLOCK_CONTROL_CPU_PLL_BYPASS_MASK     : (CPU_DDR_CLOCK_CONTROL_DDR_PLL_BYPASS_MASK     )) ---*/
/*--- #define PLL_BYPASS_SET(VAL)  ((id == avm_clock_id_cpu) ? CPU_DDR_CLOCK_CONTROL_CPU_PLL_BYPASS_SET(VAL) : (CPU_DDR_CLOCK_CONTROL_DDR_PLL_BYPASS_SET(VAL) )) ---*/
#endif

            PRINTD("[%s:%d]CPU_PLL=0x%08x DDR_PLL=0x%08x CLK_CTRL=0x%08x\n", __FUNCTION__, __LINE__, 
                    ath_reg_rd(CPU_DPLL_ADDRESS),
                    ath_reg_rd(DDR_DPLL2_ADDRESS),
                    ath_reg_rd(ATH_CPU_DDR_CLOCK_CONTROL));
PRINTD("[%s:%d] NINT=%u, REFDIV=%u, RANGE=%u, OUTDIV=%u\n", __FUNCTION__, __LINE__, NINT, REFDIV, RANGE, OUTDIV);
#if 1
/*--- if(id == avm_clock_id_ddr) ---*/ 
{
	// bypass for cpu pll
	val = ath_reg_rd(ATH_CPU_DDR_CLOCK_CONTROL);
    val &= ~PLL_BYPASS_MASK;
	val |= PLL_BYPASS_SET(RANGE);
	ath_reg_wr(ATH_CPU_DDR_CLOCK_CONTROL, val);
	udelay(10);
}
#endif
            PRINTD("[%s:%d]CPU_PLL=0x%08x DDR_PLL=0x%08x CLK_CTRL=0x%08x\n", __FUNCTION__, __LINE__, 
                    ath_reg_rd(CPU_DPLL_ADDRESS),
                    ath_reg_rd(DDR_DPLL2_ADDRESS),
                    ath_reg_rd(ATH_CPU_DDR_CLOCK_CONTROL));
	// pll settings...
	val = ath_reg_rd(DPLL_REG);
	val &= ~PLL_REFDIV_MASK;
    val &= ~PLL_NINT_MASK;
    val |= PLL_NINT_SET(NINT);
    val |= PLL_REFDIV_SET(REFDIV);
	ath_reg_wr(DPLL_REG, val);

	ath_reg_wr(DPLL_REG,
        PLL_NINT_SET(NINT)     |
        PLL_REFDIV_SET(REFDIV) |
        PLL_RANGE_SET(1)       |
        PLL_OUTDIV_SET(OUTDIV) | 
        PLL_PLLPWD_SET(1));
	ath_reg_wr(DPLL2_REG,
        PLL_NINT_SET(NINT)     |
        PLL_REFDIV_SET(REFDIV) |
        PLL_RANGE_SET(1)       |
        PLL_OUTDIV_SET(OUTDIV) | 
        PLL_PLLPWD_SET(1));
	udelay(10);

            PRINTD("[%s:%d]CPU_PLL=0x%08x DDR_PLL=0x%08x CLK_CTRL=0x%08x\n", __FUNCTION__, __LINE__, 
                    ath_reg_rd(CPU_DPLL_ADDRESS),
                    ath_reg_rd(DDR_DPLL2_ADDRESS),
                    ath_reg_rd(ATH_CPU_DDR_CLOCK_CONTROL));
	// clear pll power
	val = ath_reg_rd(PLL_CONFIG_REG);
	val &= ~PLL_PLLPWD_MASK;
	val |= PLL_PLLPWD_SET(0);
	ath_reg_wr(PLL_CONFIG_REG, val);
	udelay(100);

            PRINTD("[%s:%d]CPU_PLL=0x%08x DDR_PLL=0x%08x CLK_CTRL=0x%08x\n", __FUNCTION__, __LINE__, 
                    ath_reg_rd(CPU_DPLL_ADDRESS),
                    ath_reg_rd(DDR_DPLL2_ADDRESS),
                    ath_reg_rd(ATH_CPU_DDR_CLOCK_CONTROL));
	// reset out div
	val = ath_reg_rd(PLL_CONFIG_REG);
	val &= ~PLL_OUTDIV_MASK;
	val |= PLL_OUTDIV_SET(OUTDIV);
	ath_reg_wr(PLL_CONFIG_REG, val);
	udelay(10);

            PRINTD("[%s:%d]CPU_PLL=0x%08x DDR_PLL=0x%08x CLK_CTRL=0x%08x\n", __FUNCTION__, __LINE__, 
                    ath_reg_rd(CPU_DPLL_ADDRESS),
                    ath_reg_rd(DDR_DPLL2_ADDRESS),
                    ath_reg_rd(ATH_CPU_DDR_CLOCK_CONTROL));

#if 1
/*--- if(id == avm_clock_id_ddr) ---*/ 
{
	// unset bypass for cpu pll
	val = ath_reg_rd(ATH_CPU_DDR_CLOCK_CONTROL);
    val &= ~PLL_BYPASS_MASK;
	val |= PLL_BYPASS_SET(0);
	ath_reg_wr(ATH_CPU_DDR_CLOCK_CONTROL, val);
	udelay(10);
            PRINTD("[%s:%d]CPU_PLL=0x%08x DDR_PLL=0x%08x CLK_CTRL=0x%08x\n", __FUNCTION__, __LINE__, 
                    ath_reg_rd(CPU_DPLL_ADDRESS),
                    ath_reg_rd(DDR_DPLL2_ADDRESS),
                    ath_reg_rd(ATH_CPU_DDR_CLOCK_CONTROL));
}
#endif

#if 0
	// reconfigure kernel's notion of time
    {
        unsigned int freq = cpu_get_clock();
        mips_hpt_frequency = freq * 1000000 / 2;

        // see r4k_clockevent_init()
        cd = &per_cpu(mips_clockevent_device, cpu);
        cd->mult		= div_sc((unsigned long) mips_hpt_frequency, NSEC_PER_SEC, 32);
        cd->max_delta_ns	= clockevent_delta2ns(0x7fffffff, cd);
        cd->min_delta_ns	= clockevent_delta2ns(0x300, cd);
    }
#endif
}
void reconfig_kernel_time_notion(void) {
	struct clock_event_device *cd;
	unsigned int cpu = smp_processor_id();
    unsigned int freq = cpu_get_clock();
    mips_hpt_frequency = freq * 1000000 / 2;

    // see r4k_clockevent_init()
    cd = &per_cpu(mips_clockevent_device, cpu);
    cd->mult		= div_sc((unsigned long) mips_hpt_frequency, NSEC_PER_SEC, 32);
    cd->max_delta_ns	= clockevent_delta2ns(0x7fffffff, cd);
    cd->min_delta_ns	= clockevent_delta2ns(0x300, cd);
}

struct _pll_freq_table {
    unsigned int cpu_freq;
    unsigned int ddr_freq;
    enum _avm_clock_id cpu_pll_id;
    enum _avm_clock_id ddr_pll_id;
};

struct _pll_freq_table  pll_freq_table[] = {
    { .cpu_freq=440000000, .ddr_freq=440000000, .cpu_pll_id=avm_clock_id_ddr, .ddr_pll_id=avm_clock_id_ddr },
    { .cpu_freq=400000000, .ddr_freq=400000000, .cpu_pll_id=avm_clock_id_ddr, .ddr_pll_id=avm_clock_id_ddr },
    { .cpu_freq=360000000, .ddr_freq=360000000, .cpu_pll_id=avm_clock_id_ddr, .ddr_pll_id=avm_clock_id_ddr },
    { .cpu_freq=320000000, .ddr_freq=320000000, .cpu_pll_id=avm_clock_id_ddr, .ddr_pll_id=avm_clock_id_ddr },
    { .cpu_freq=280000000, .ddr_freq=280000000, .cpu_pll_id=avm_clock_id_ddr, .ddr_pll_id=avm_clock_id_ddr },
    { .cpu_freq=240000000, .ddr_freq=240000000, .cpu_pll_id=avm_clock_id_ddr, .ddr_pll_id=avm_clock_id_ddr },
    { .cpu_freq=200000000, .ddr_freq=200000000, .cpu_pll_id=avm_clock_id_ddr, .ddr_pll_id=avm_clock_id_ddr },
    { .cpu_freq=160000000, .ddr_freq=160000000, .cpu_pll_id=avm_clock_id_ddr, .ddr_pll_id=avm_clock_id_ddr },
    { .cpu_freq=120000000, .ddr_freq=120000000, .cpu_pll_id=avm_clock_id_ddr, .ddr_pll_id=avm_clock_id_ddr },
    { .cpu_freq=80000000,  .ddr_freq=80000000,  .cpu_pll_id=avm_clock_id_ddr, .ddr_pll_id=avm_clock_id_ddr },
    { .cpu_freq=40000000,  .ddr_freq=40000000,  .cpu_pll_id=avm_clock_id_ddr, .ddr_pll_id=avm_clock_id_ddr },
    { .cpu_freq=0 }
};
#if 0
struct _pll_freq_table  pll_freq_table[] = {
    { .cpu_freq=560000000, .ddr_freq=480000000, .cpu_pll_id=avm_clock_id_cpu, .ddr_pll_id=avm_clock_id_ddr },
    { .cpu_freq=480000000, .ddr_freq=480000000, .cpu_pll_id=avm_clock_id_ddr, .ddr_pll_id=avm_clock_id_ddr },
    { .cpu_freq=440000000, .ddr_freq=440000000, .cpu_pll_id=avm_clock_id_cpu, .ddr_pll_id=avm_clock_id_cpu },
    { .cpu_freq=400000000, .ddr_freq=400000000, .cpu_pll_id=avm_clock_id_ddr, .ddr_pll_id=avm_clock_id_ddr },
    { .cpu_freq=360000000, .ddr_freq=360000000, .cpu_pll_id=avm_clock_id_cpu, .ddr_pll_id=avm_clock_id_cpu },
    { .cpu_freq=320000000, .ddr_freq=320000000, .cpu_pll_id=avm_clock_id_ddr, .ddr_pll_id=avm_clock_id_ddr },
    { .cpu_freq=280000000, .ddr_freq=280000000, .cpu_pll_id=avm_clock_id_cpu, .ddr_pll_id=avm_clock_id_cpu },
    { .cpu_freq=240000000, .ddr_freq=240000000, .cpu_pll_id=avm_clock_id_ddr, .ddr_pll_id=avm_clock_id_ddr },
    { .cpu_freq=200000000, .ddr_freq=200000000, .cpu_pll_id=avm_clock_id_cpu, .ddr_pll_id=avm_clock_id_cpu },
    { .cpu_freq=160000000, .ddr_freq=160000000, .cpu_pll_id=avm_clock_id_ddr, .ddr_pll_id=avm_clock_id_ddr },
    { .cpu_freq=120000000, .ddr_freq=120000000, .cpu_pll_id=avm_clock_id_cpu, .ddr_pll_id=avm_clock_id_cpu },
    { .cpu_freq=80000000,  .ddr_freq=80000000,  .cpu_pll_id=avm_clock_id_ddr, .ddr_pll_id=avm_clock_id_ddr },
    { .cpu_freq=40000000,  .ddr_freq=40000000,  .cpu_pll_id=avm_clock_id_cpu, .ddr_pll_id=avm_clock_id_cpu },
    { .cpu_freq=0 }
};
#endif
unsigned int cpu_set_clock(unsigned int freq_new) {
    unsigned int cpu_pll_config = ath_reg_rd(ATH_CPU_PLL_CONFIG);
    unsigned int ddr_pll_config = ath_reg_rd(ATH_DDR_PLL_CONFIG);
    unsigned int clk_ctrl;
    unsigned int NINT_CPU   = CPU_PLL_CONFIG_NINT_GET(cpu_pll_config);
    unsigned int NINT_DDR   = DDR_PLL_CONFIG_NINT_GET(ddr_pll_config);
    unsigned int REFDIV, DDR_REFDIV;
    unsigned int RANGE, DDR_RANGE;
    unsigned int OUTDIV, DDR_OUTDIV;
    unsigned int REFCLK;
    unsigned int vcoout;
    int step;
    unsigned int cpu_freq_current, ddr_freq_current;
    static unsigned int NINT_DDR_max = 0;
    static unsigned int cpu_freq_max = 0;
    static unsigned int ddr_freq_max = 0;
    enum _avm_clock_id cpu_pll_id, ddr_pll_id;
    struct _pll_freq_table *pll_entry;
    unsigned int pll;
    cpu_freq_current = cpu_get_clock();
    ddr_freq_current = ddr_get_clock();

    printk("[%s] request for cpu clock %u (current CPU: %u, current ddr: %u)\n", __FUNCTION__, freq_new, cpu_freq_current, ddr_freq_current);
            pll = ath_reg_rd(ATH_CPU_PLL_CONFIG);
            PRINTD("[%s] current apb cpu pll: %u (NINT=%d OUTDIV=%d NFRAC=%d REFDIV=%d)\n", __FUNCTION__, cpu_get_clock(),
                    CPU_PLL_CONFIG_NINT_GET(pll),
                    CPU_PLL_CONFIG_OUTDIV_GET(pll),
                    CPU_PLL_CONFIG_NFRAC_GET(pll),
                    CPU_PLL_CONFIG_REFDIV_GET(pll));
            pll = ath_reg_rd(ATH_DDR_PLL_CONFIG);
            PRINTD("[%s] current apb ddr pll: %u (NINT=%d OUTDIV=%d NFRAC=%d REFDIV=%d)\n", __FUNCTION__, ddr_get_clock(),
                    DDR_PLL_CONFIG_NINT_GET(pll),
                    DDR_PLL_CONFIG_OUTDIV_GET(pll),
                    DDR_PLL_CONFIG_NFRAC_GET(pll),
                    DDR_PLL_CONFIG_REFDIV_GET(pll)
                  );
            PRINTD("[%s:%d]\n\ttATH_CPU_PLL_CONFIG=0x%08x\n\tATH_DDR_PLL_CONFIG=0x%08x\n\tATH_CPU_DDR_CLOCK_CONTROL=0x%08x\n", __FUNCTION__, __LINE__, 
                    ath_reg_rd(ATH_CPU_PLL_CONFIG),
                    ath_reg_rd(ATH_DDR_PLL_CONFIG),
                    ath_reg_rd(ATH_CPU_DDR_CLOCK_CONTROL));

	/*--- clk_ctrl = ath_reg_rd(ATH_CPU_DDR_CLOCK_CONTROL); ---*/
    /*--- cpu_pll_id = CPU_DDR_CLOCK_CONTROL_CPUCLK_FROM_CPUPLL_GET(clk_ctrl) ? avm_clock_id_cpu : avm_clock_id_ddr; ---*/
    /*--- ddr_pll_id = CPU_DDR_CLOCK_CONTROL_DDRCLK_FROM_DDRPLL_GET(clk_ctrl) ? avm_clock_id_ddr : avm_clock_id_cpu; ---*/

/*------------------------------------------------------------------------------------------*\
 * XXX TEST POST-DIVIDER
\*------------------------------------------------------------------------------------------*/
#if 0

#endif
/*------------------------------------------------------------------------------------------*\
 * XXX TEST END
\*------------------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------------------*\
 * XXX TEST SRIF
\*------------------------------------------------------------------------------------------*/
#if 1
{
    static unsigned int freq_old = 560*1000*1000;
    static unsigned int NINT_max = 0;
	unsigned int NINT, dpll, dpll2, val;
	unsigned int NINT_new = 0;
        
    PRINTD("[%s:%d]  CPU_DPLL=0x%08x  CPU_DPLL2=0x%08x  CPU_DPLL3=0x%08x  CPU_DPLL4=0x%08x\n", __FUNCTION__, __LINE__, 
            ath_reg_rd(CPU_DPLL_ADDRESS),
            ath_reg_rd(CPU_DPLL2_ADDRESS),
            ath_reg_rd(CPU_DPLL2_ADDRESS+4),
            ath_reg_rd(CPU_DPLL2_ADDRESS+8));

    dpll = ath_reg_rd(CPU_DPLL_ADDRESS);
    NINT = CPU_DPLL_NINT_GET(dpll);
    if(!NINT_max)
        NINT_max = NINT; /*--- Urlader setzt den maximal möglichen Multiplier Wert ---*/
    if((freq_new > freq_old) && (NINT < NINT_max)) {
        NINT_new = NINT + 1;
    }
    if((freq_new < freq_old) && (NINT > 3)) {
        NINT_new = NINT - 1;
    }
    if(!NINT_new) {
        return cpu_get_clock();
    }

    // Switch CPU clock source to DDR-PLL
    clk_ctrl = ath_reg_rd(ATH_CPU_DDR_CLOCK_CONTROL);
    clk_ctrl &= ~CPU_DDR_CLOCK_CONTROL_CPUCLK_FROM_CPUPLL_MASK;
    PRINTD("[%s:%d]\n", __FUNCTION__, __LINE__);
    ath_reg_wr(ATH_CPU_DDR_CLOCK_CONTROL, clk_ctrl);
    udelay(10);

	// bypass for cpu pll
    PRINTD("[%s:%d] set bypass\n", __FUNCTION__, __LINE__);
	val = ath_reg_rd(ATH_CPU_DDR_CLOCK_CONTROL);
	val &= ~CPU_DDR_CLOCK_CONTROL_CPU_PLL_BYPASS_MASK;
	val |= CPU_DDR_CLOCK_CONTROL_CPU_PLL_BYPASS_SET(RANGE);
	ath_reg_wr(ATH_CPU_DDR_CLOCK_CONTROL, val);
	udelay(10);

    PRINTD("[%s:%d] NINT_new=%d\n", __FUNCTION__, __LINE__, NINT_new);

    // pull CPU PLL into power-down 
    dpll2 = ath_reg_rd(CPU_DPLL2_ADDRESS);
    dpll2 |= CPU_DPLL2_PLL_PWD_MASK;
    ath_reg_wr(CPU_DPLL2_ADDRESS, dpll2);
    udelay(10);

    // set new clock settings for CPU PLL
    dpll &= ~CPU_DPLL_NINT_MASK;
    dpll |=  CPU_DPLL_NINT_SET(NINT_new);
    PRINTD("[%s:%d] CPU_DPLL=0x%08x (new)\n", __FUNCTION__, __LINE__, dpll);
    ath_reg_wr(CPU_DPLL_ADDRESS, dpll);
    udelay(10);

    // pull CPU PLL out of power-down 
    dpll2 = ath_reg_rd(CPU_DPLL2_ADDRESS);
    dpll2 &= ~CPU_DPLL2_PLL_PWD_MASK;
    PRINTD("[%s:%d] CPU_DPLL2=0x%08x (new)\n", __FUNCTION__, __LINE__, dpll2);
    ath_reg_wr(CPU_DPLL2_ADDRESS, dpll2);
    udelay(10);

	// unset bypass for cpu pll
    PRINTD("[%s:%d] unset bypass\n", __FUNCTION__, __LINE__);
	val = ath_reg_rd(ATH_CPU_DDR_CLOCK_CONTROL);
	val &= ~CPU_DDR_CLOCK_CONTROL_CPU_PLL_BYPASS_MASK;
	val |= CPU_DDR_CLOCK_CONTROL_CPU_PLL_BYPASS_SET(0);
	ath_reg_wr(ATH_CPU_DDR_CLOCK_CONTROL, val);
	udelay(10);

    // Switch CPU clock source back to CPU-PLL
    clk_ctrl = ath_reg_rd(ATH_CPU_DDR_CLOCK_CONTROL);
    clk_ctrl |= CPU_DDR_CLOCK_CONTROL_CPUCLK_FROM_CPUPLL_MASK;
    PRINTD("[%s:%d]\n", __FUNCTION__, __LINE__);
    ath_reg_wr(ATH_CPU_DDR_CLOCK_CONTROL, clk_ctrl);
    udelay(10);

    reconfig_kernel_time_notion();

    PRINTD("[%s:%d] new cpu freq: %d\n", __FUNCTION__, __LINE__, cpu_get_clock()/1000000);

    freq_old = freq_new;
    return cpu_get_clock();
}
#endif
/*------------------------------------------------------------------------------------------*\
 * XXX TEST END
\*------------------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------------------*\
 * XXX TEST INIT_DDR_PLL
\*------------------------------------------------------------------------------------------*/
#if 0
    if(!NINT_DDR_max) {
        NINT_DDR_max = 1;
        if ((ath_reg_rd(ATH_BOOTSTRAP_REG) & ATH_REF_CLK_40)) {
            REFCLK = (40 * 1000000);
        } else {
            REFCLK = (25 * 1000000);
        }

        // Annnahme: CPU-PLL und DDR-PLL sind bis auf NINT-Wert identisch konfiguriert
        REFDIV = CPU_PLL_CONFIG_REFDIV_GET(cpu_pll_config);
        OUTDIV = CPU_PLL_CONFIG_OUTDIV_GET(cpu_pll_config);
        RANGE  = CPU_PLL_CONFIG_RANGE_GET(cpu_pll_config);
        DDR_REFDIV = DDR_PLL_CONFIG_REFDIV_GET(cpu_pll_config);
        DDR_OUTDIV = DDR_PLL_CONFIG_OUTDIV_GET(cpu_pll_config);
        DDR_RANGE  = DDR_PLL_CONFIG_RANGE_GET(cpu_pll_config);

        // CPU-Clk auf DDR-PLL umhängen
        clk_ctrl = ath_reg_rd(ATH_CPU_DDR_CLOCK_CONTROL);
        clk_ctrl &= ~CPU_DDR_CLOCK_CONTROL_CPUCLK_FROM_CPUPLL_MASK;
        clk_ctrl |=  CPU_DDR_CLOCK_CONTROL_DDRCLK_FROM_DDRPLL_SET(1);
        ath_reg_wr(ATH_CPU_DDR_CLOCK_CONTROL, clk_ctrl);
	    udelay(10);
        reconfig_kernel_time_notion();
	    udelay(10);
printk("[%s:%d]\n", __FUNCTION__, __LINE__);

#if 1
        // CPU PLL auf 240 MHz takten
        ath_internal_set_pll_clk_ctrl(avm_clock_id_cpu, 10, REFDIV, RANGE, OUTDIV);
	    udelay(10);
printk("[%s:%d]\n", __FUNCTION__, __LINE__);
        reconfig_kernel_time_notion();
	    udelay(10);
#endif

        /*--- ath_internal_set_pll_clk_ctrl(avm_clock_id_ddr, 12, DDR_REFDIV, DDR_RANGE, DDR_OUTDIV); ---*/
        /*--- reconfig_kernel_time_notion(); ---*/
        /*--- return cpu_get_clock(); ---*/
            printk("[%s:%d]\n\tATH_CPU_PLL_CONFIG=0x%x\n\tATH_DDR_PLL_CONFIG=0x%x\n\tATH_CPU_DDR_CLOCK_CONTROL=0x%x\n", __FUNCTION__, __LINE__, 
                    ath_reg_rd(ATH_CPU_PLL_CONFIG),
                    ath_reg_rd(ATH_DDR_PLL_CONFIG),
                    ath_reg_rd(ATH_CPU_DDR_CLOCK_CONTROL));
    }
    clk_ctrl = ath_reg_rd(ATH_CPU_DDR_CLOCK_CONTROL);
    if( ! CPU_DDR_CLOCK_CONTROL_CPUCLK_FROM_CPUPLL_GET(clk_ctrl)) {
printk("[%s:%d] umhängen auf CPU-PLL\n", __FUNCTION__, __LINE__);
        // Beide umhängen auf CPU-PLL
        clk_ctrl |=  CPU_DDR_CLOCK_CONTROL_CPUCLK_FROM_CPUPLL_SET(1);
        clk_ctrl &= ~CPU_DDR_CLOCK_CONTROL_DDRCLK_FROM_DDRPLL_MASK;
        ath_reg_wr(ATH_CPU_DDR_CLOCK_CONTROL, clk_ctrl);
	    udelay(10);
printk("[%s:%d]\n", __FUNCTION__, __LINE__);
        reconfig_kernel_time_notion();
            printk("[%s:%d]\n\tATH_CPU_DDR_CLOCK_CONTROL=0x%x => %dMHz\n", __FUNCTION__, __LINE__, 
                    ath_reg_rd(ATH_CPU_DDR_CLOCK_CONTROL), cpu_get_clock() / 1000000);
        return cpu_get_clock();
    } else {
printk("[%s:%d] umhängen auf DDR-PLL\n", __FUNCTION__, __LINE__);
        // Beide umhängen auf DDR-PLL
        clk_ctrl &= ~CPU_DDR_CLOCK_CONTROL_CPUCLK_FROM_CPUPLL_MASK;
        clk_ctrl |=  CPU_DDR_CLOCK_CONTROL_DDRCLK_FROM_DDRPLL_SET(1);
        ath_reg_wr(ATH_CPU_DDR_CLOCK_CONTROL, clk_ctrl);
	    udelay(10);
printk("[%s:%d]\n", __FUNCTION__, __LINE__);
        reconfig_kernel_time_notion();
            printk("[%s:%d]\n\tATH_CPU_DDR_CLOCK_CONTROL=0x%x => %dMHz\n", __FUNCTION__, __LINE__, 
                    ath_reg_rd(ATH_CPU_DDR_CLOCK_CONTROL), ddr_get_clock() / 1000000);
        return ddr_get_clock();
    }
    
#endif
/*------------------------------------------------------------------------------------------*\
 * XXX TEST END
\*------------------------------------------------------------------------------------------*/

    if(!NINT_DDR_max) {
        // Maximalen RAM-Takt vom Urlader übernehmen:
        NINT_DDR_max = NINT_DDR;
        ddr_freq_max = ddr_freq_current;
        cpu_freq_max = cpu_freq_current;
    }

	if ((ath_reg_rd(ATH_BOOTSTRAP_REG) & ATH_REF_CLK_40)) {
		REFCLK = (40 * 1000000);
	} else {
		REFCLK = (25 * 1000000);
	}

    // Annnahme: CPU-PLL und DDR-PLL sind bis auf NINT-Wert identisch konfiguriert
    REFDIV = CPU_PLL_CONFIG_REFDIV_GET(cpu_pll_config);
    OUTDIV = CPU_PLL_CONFIG_OUTDIV_GET(cpu_pll_config);
    RANGE  = CPU_PLL_CONFIG_RANGE_GET(cpu_pll_config);
    DDR_REFDIV = DDR_PLL_CONFIG_REFDIV_GET(ddr_pll_config);
    DDR_OUTDIV = DDR_PLL_CONFIG_OUTDIV_GET(ddr_pll_config);
    DDR_RANGE  = DDR_PLL_CONFIG_RANGE_GET(ddr_pll_config);

/*------------------------------------------------------------------------------------------*\
 * XXX TEST XXX
\*------------------------------------------------------------------------------------------*/
#if 0
/*--- #define PINGPONG_CLOCK_SWITCHING ---*/

    step = freq_new < cpu_freq_current ? 1 : -1;
    pll_entry = pll_freq_table;
    while(pll_entry->cpu_freq > cpu_freq_current) {
        pll_entry++;
    }
    while(((step > 0) && (pll_entry->cpu_freq > freq_new)) || 
          ((step < 0) && (pll_entry->cpu_freq < freq_new))) {
        unsigned int NINT_new, clk_ctrl, pll;
        struct _pll_freq_table *pll_last_entry;
        PRINTD("[%s:%d]\n", __FUNCTION__, __LINE__);
        if((step < 0) && (pll_entry->cpu_freq == cpu_freq_max))
            break;
        pll_last_entry = pll_entry;
        pll_entry += step;
        PRINTD("[%s:%d]\n", __FUNCTION__, __LINE__);
        if(!pll_entry->cpu_freq)
            break;
        PRINTD("[%s:%d]\n", __FUNCTION__, __LINE__);

            printk("f=%u\n", pll_entry->cpu_freq);
        if(pll_entry->cpu_pll_id == avm_clock_id_cpu) {
#ifdef PINGPONG_CLOCK_SWITCHING
            vcoout = (pll_entry->cpu_freq) * (1 << OUTDIV);
            NINT_new = vcoout / (REFCLK / REFDIV);
            PRINTD("[%s:%d] cpu_freq=%u, NINT_new=%u\n", __FUNCTION__, __LINE__, pll_entry->cpu_freq, NINT_new);

            PRINTD("[%s:%d]\n", __FUNCTION__, __LINE__);
            /*--- CPU-PLL umprogrammieren ---*/
            ath_internal_set_pll_clk_ctrl(avm_clock_id_cpu, NINT_new, REFDIV, RANGE, OUTDIV);
            PRINTD("[%s:%d]\n", __FUNCTION__, __LINE__);

/*--- #ifdef PINGPONG_CLOCK_SWITCHING ---*/
            // CPU- & DDR-Clock an die CPU-PLL hängen:
            clk_ctrl = ath_reg_rd(ATH_CPU_DDR_CLOCK_CONTROL);
            clk_ctrl |=  CPU_DDR_CLOCK_CONTROL_CPUCLK_FROM_CPUPLL_SET(1);
            if(pll_entry->ddr_pll_id == avm_clock_id_cpu) {
                // DDR-Clock nur aus CPU-PLL speisen, wenn neues Setting nicht Sonderfall CPU 560 MHz & DDR 480 MHz
                clk_ctrl &= ~CPU_DDR_CLOCK_CONTROL_DDRCLK_FROM_DDRPLL_MASK;
                clk_ctrl &= ~CPU_DDR_CLOCK_CONTROL_AHBCLK_FROM_DDRPLL_MASK;
            }
            PRINTD("[%s:%d]\n\ttATH_CPU_PLL_CONFIG=0x%x\n\tATH_DDR_PLL_CONFIG=0x%x\n\tATH_CPU_DDR_CLOCK_CONTROL=0x%x => 0x%x\n", __FUNCTION__, __LINE__, 
                    ath_reg_rd(ATH_CPU_PLL_CONFIG),
                    ath_reg_rd(ATH_DDR_PLL_CONFIG),
                    ath_reg_rd(ATH_CPU_DDR_CLOCK_CONTROL),
                    clk_ctrl);
            ath_reg_wr(ATH_CPU_DDR_CLOCK_CONTROL, clk_ctrl);
	        udelay(10);
            PRINTD("[%s:%d]\n", __FUNCTION__, __LINE__);
/*--- #endif ---*/

            reconfig_kernel_time_notion();
            PRINTD("[%s:%d]\n", __FUNCTION__, __LINE__);
            pll = ath_reg_rd(ATH_CPU_PLL_CONFIG);
            PRINTD("[%s] new apb cpu pll(%u): %u (NINT=%d OUTDIV=%d NFRAC=%d REFDIV=%d)\n", __FUNCTION__, pll_entry->cpu_freq, cpu_get_clock(),
                    CPU_PLL_CONFIG_NINT_GET(pll),
                    CPU_PLL_CONFIG_OUTDIV_GET(pll),
                    CPU_PLL_CONFIG_NFRAC_GET(pll),
                    CPU_PLL_CONFIG_REFDIV_GET(pll)
                  );
#endif
        } else 
        {
            vcoout = (pll_entry->cpu_freq) * (1 << DDR_OUTDIV);
            NINT_new = vcoout / (REFCLK / DDR_REFDIV);
            PRINTD("[%s:%d] cpu_freq=%u, NINT_new=%u\n", __FUNCTION__, __LINE__, pll_entry->cpu_freq, NINT_new);

            /*--- DDR-PLL umprogrammieren ---*/
            if(pll_last_entry->cpu_freq == pll_last_entry->ddr_freq) {
                PRINTD("[%s:%d]\n", __FUNCTION__, __LINE__);
                // DDR-PLL nur umkonfigurieren, wenn aktuelles Setting nicht Sonderfall CPU 560 MHz & DDR 480 MHz
                ath_internal_set_pll_clk_ctrl(avm_clock_id_ddr, NINT_new, DDR_REFDIV, DDR_RANGE, DDR_OUTDIV);
            }
#ifdef PINGPONG_CLOCK_SWITCHING
            // CPU-, DDR- & AHB-Clock an die DDR-PLL hängen:            
            PRINTD("[%s:%d]\n", __FUNCTION__, __LINE__);
            clk_ctrl = ath_reg_rd(ATH_CPU_DDR_CLOCK_CONTROL);
            clk_ctrl &= ~CPU_DDR_CLOCK_CONTROL_CPUCLK_FROM_CPUPLL_MASK;
            clk_ctrl |=  CPU_DDR_CLOCK_CONTROL_DDRCLK_FROM_DDRPLL_SET(1);
            clk_ctrl |=  CPU_DDR_CLOCK_CONTROL_AHBCLK_FROM_DDRPLL_SET(1);
            PRINTD("[%s:%d]\n", __FUNCTION__, __LINE__);
            ath_reg_wr(ATH_CPU_DDR_CLOCK_CONTROL, clk_ctrl);
	        udelay(10);
#endif
            PRINTD("[%s:%d]\n", __FUNCTION__, __LINE__);
            reconfig_kernel_time_notion();
            PRINTD("[%s:%d]\n", __FUNCTION__, __LINE__);
            pll = ath_reg_rd(ATH_DDR_PLL_CONFIG);
            PRINTD("[%s] new apb ddr pll(%u): %u (NINT=%d OUTDIV=%d NFRAC=%d REFDIV=%d)\n", __FUNCTION__, pll_entry->cpu_freq, ddr_get_clock(),
                    DDR_PLL_CONFIG_NINT_GET(pll),
                    DDR_PLL_CONFIG_OUTDIV_GET(pll),
                    DDR_PLL_CONFIG_NFRAC_GET(pll),
                    DDR_PLL_CONFIG_REFDIV_GET(pll)
                  );
        }
    }
#endif
/*------------------------------------------------------------------------------------------*\
 * XXX TEST END XXX
\*------------------------------------------------------------------------------------------*/

#if 0
    if(cpu_pll_id != ddr_pll_id) {
        // TODO...
    }

    /* ZUSTANDSMASCHINE BAUEN */
    if(cpu_pll_id == avm_clock_id_cpu) {
        REFDIV = CPU_PLL_CONFIG_REFDIV_GET(cpu_pll_config);
        OUTDIV = CPU_PLL_CONFIG_OUTDIV_GET(cpu_pll_config);
    } else {
        REFDIV = CPU_PLL_CONFIG_REFDIV_GET(cpu_pll_config);
        OUTDIV = CPU_PLL_CONFIG_OUTDIV_GET(cpu_pll_config);
    }
    vcoout = freq_new * (1 << OUTDIV);
    NINT_new = vcoout / (REFCLK / REFDIV);

	if (CPU_DDR_CLOCK_CONTROL_DDRCLK_FROM_DDRPLL_GET(clk_ctrl)) {
    }
#endif

    PRINTD("[END]\n");
    return cpu_get_clock();
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if 0
unsigned int cpu_set_clock(unsigned int freq) {
    volatile unsigned int *pcpu_pll_config = (volatile unsigned int *)KSEG1ADDR(ATH_CPU_PLL_CONFIG);
    unsigned int cpu_pll_config = *pcpu_pll_config;
    unsigned int NINT   = CPU_PLL_CONFIG_NINT_GET(cpu_pll_config);
    unsigned int REFDIV = CPU_PLL_CONFIG_REFDIV_GET(cpu_pll_config);
    unsigned int OUTDIV = CPU_PLL_CONFIG_OUTDIV_GET(cpu_pll_config);
    unsigned int RANGE = CPU_PLL_CONFIG_RANGE_GET(cpu_pll_config);
    unsigned int vcoout;
    unsigned int REFCLK;
    unsigned int NINT_new;
    unsigned int NINT_min = 1;
    static unsigned int NINT_max = 0;
    
    if(!NINT_max) {
	    unsigned int pll = ath_reg_rd(CPU_DPLL2_ADDRESS);
        NINT_max = NINT; /*--- Urlader setzt das maximal möglichen Multiplier Wert ---*/
        printk("[%s:%d] current ddr clock: %u (NINT=%u OUTDIV=%u RANGE=%u REFDIV=%u, %s)\n", __FUNCTION__, __LINE__, ddr_get_clock(),
               NINT, OUTDIV, REFDIV, RANGE, CPU_DPLL2_LOCAL_PLL_GET(pll) ? "SRIF" : "NORMAL");
    }

	if ((ath_reg_rd(ATH_BOOTSTRAP_REG) & ATH_REF_CLK_40)) {
		REFCLK = (40 * 1000000);
	} else {
		REFCLK = (25 * 1000000);
	}

    // Annahme: NINT ist variable
    vcoout = freq * (1 << OUTDIV);
    NINT_new = vcoout / (REFCLK / REFDIV);

    while(NINT != NINT_new) {
        if(NINT < NINT_new) {
            if(NINT >= NINT_max)
                break;
            NINT++;
        } else {
            if(NINT <= NINT_min)
                break;
            NINT--;
        }
        /*--- ath_internal_set_ddr_clk(NINT, REFDIV, RANGE, OUTDIV); ---*/
        {
            unsigned int pll = ath_reg_rd(ATH_CPU_PLL_CONFIG);
            // power down the PLL and set NINT
            pll &= ~CPU_PLL_CONFIG_NINT_MASK;
            pll |=  CPU_PLL_CONFIG_NINT_SET(NINT);
    		/*--- pll |=  CPU_PLL_CONFIG_PLLPWD_SET(1); ---*/
	        ath_reg_wr(ATH_CPU_PLL_CONFIG, pll);
	        /*--- udelay(10); ---*/

            // power up the PLL
            /*--- pll = ath_reg_rd(ATH_CPU_PLL_CONFIG); ---*/
            /*--- pll &= ~CPU_PLL_CONFIG_PLLPWD_MASK; ---*/
            /*--- ath_reg_wr(ATH_CPU_PLL_CONFIG, pll); ---*/

            udelay(10);
            pll = ath_reg_rd(ATH_CPU_PLL_CONFIG);
            printk("[%s] new apb ddr clock: %u (NINT=%d OUTDIV=%d NFRAC=%d REFDIV=%d)\n", __FUNCTION__, ddr_get_clock(),
                    CPU_PLL_CONFIG_NINT_GET(pll),
                    CPU_PLL_CONFIG_OUTDIV_GET(pll),
                    CPU_PLL_CONFIG_NFRAC_GET(pll),
                    CPU_PLL_CONFIG_REFDIV_GET(pll)
                  );
            // reconfigure kernel's notion of time
            {
                struct clock_event_device *cd;
                unsigned int cpu = smp_processor_id();
                unsigned int freq = cpu_get_clock();
                mips_hpt_frequency = freq * 1000000 / 2;

                // see r4k_clockevent_init()
                cd = &per_cpu(mips_clockevent_device, cpu);
                cd->mult		= div_sc((unsigned long) mips_hpt_frequency, NSEC_PER_SEC, 32);
                cd->max_delta_ns	= clockevent_delta2ns(0x7fffffff, cd);
                cd->min_delta_ns	= clockevent_delta2ns(0x300, cd);
            }
        }
    }



    /*--- ath_internal_set_cpu_clk(NINT, REFDIV, RANGE, OUTDIV); ---*/

    return cpu_get_clock();
}
#endif

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int ddr_set_clock(unsigned int freq) {
    volatile unsigned int *pddr_pll_config = (volatile unsigned int *)KSEG1ADDR(ATH_DDR_PLL_CONFIG);
    unsigned int ddr_pll_config = *pddr_pll_config;
    unsigned int NINT   = DDR_PLL_CONFIG_NINT_GET(ddr_pll_config);
    unsigned int REFDIV = DDR_PLL_CONFIG_REFDIV_GET(ddr_pll_config);
    unsigned int OUTDIV = DDR_PLL_CONFIG_OUTDIV_GET(ddr_pll_config);
    unsigned int RANGE = DDR_PLL_CONFIG_RANGE_GET(ddr_pll_config);
    unsigned int vcoout;
    unsigned int REFCLK;
    unsigned int NINT_new;
    unsigned int NINT_min = 1;
    static unsigned int NINT_max = 0;
    
    if(!NINT_max) {
	    unsigned int pll = ath_reg_rd(DDR_DPLL2_ADDRESS);
        NINT_max = NINT; /*--- Urlader setzt das maximal möglichen Multiplier Wert ---*/
        printk("[%s:%d] current ddr clock: %u (NINT=%u OUTDIV=%u RANGE=%u REFDIV=%u, %s)\n", __FUNCTION__, __LINE__, ddr_get_clock(),
               NINT, OUTDIV, REFDIV, RANGE, CPU_DPLL2_LOCAL_PLL_GET(pll) ? "SRIF" : "NORMAL");
        /*--- pll &= ~CPU_DPLL2_LOCAL_PLL_MASK; ---*/
	    /*--- pll |=  CPU_DPLL2_LOCAL_PLL_SET(pll); ---*/
	    /*--- ath_reg_wr(DDR_DPLL2_ADDRESS, pll); ---*/
    }

	if ((ath_reg_rd(ATH_BOOTSTRAP_REG) & ATH_REF_CLK_40)) {
		REFCLK = (40 * 1000000);
	} else {
		REFCLK = (25 * 1000000);
	}

    // Annahme: NINT ist variable
    vcoout = freq * (1 << OUTDIV);
    NINT_new = vcoout / (REFCLK / REFDIV);

    {
        unsigned int pll = ath_reg_rd(DDR_DPLL2_ADDRESS);
        printk("[%s] current ddr clock: %u (NINT=%u NINT_new=%u OUTDIV=%u RANGE=%u REFDIV=%u, %s)\n", __FUNCTION__, ddr_get_clock(),
                NINT, NINT_new, OUTDIV, REFDIV, RANGE,
                CPU_DPLL2_LOCAL_PLL_GET(pll) ? "SRIF" : "NORMAL");
    }
    while(NINT != NINT_new) {
        if(NINT < NINT_new) {
            if(NINT >= NINT_max)
                break;
            NINT++;
        } else {
            if(NINT <= NINT_min)
                break;
            NINT--;
        }
        /*--- ath_internal_set_ddr_clk(NINT, REFDIV, RANGE, OUTDIV); ---*/
        {
            unsigned int pll;
            // power down the PLL and set NINT
            pll = ath_reg_rd(ATH_DDR_PLL_CONFIG);
            pll &= ~DDR_PLL_CONFIG_NINT_MASK;
            pll |=  DDR_PLL_CONFIG_NINT_SET(NINT);
    		/*--- pll |=  DDR_PLL_CONFIG_PLLPWD_SET(1); ---*/
	        ath_reg_wr(ATH_DDR_PLL_CONFIG, pll);
	        /*--- udelay(10); ---*/

            // power up the PLL
            /*--- pll = ath_reg_rd(ATH_DDR_PLL_CONFIG); ---*/
            /*--- pll &= ~DDR_PLL_CONFIG_PLLPWD_MASK; ---*/
            /*--- ath_reg_wr(ATH_DDR_PLL_CONFIG, pll); ---*/

            udelay(10);
            pll = ath_reg_rd(ATH_DDR_PLL_CONFIG);
            printk("[%s] new apb ddr clock: %u (NINT=%d OUTDIV=%d NFRAC=%d REFDIV=%d)\n", __FUNCTION__, ddr_get_clock(),
                    DDR_PLL_CONFIG_NINT_GET(pll),
                    DDR_PLL_CONFIG_OUTDIV_GET(pll),
                    DDR_PLL_CONFIG_NFRAC_GET(pll),
                    DDR_PLL_CONFIG_REFDIV_GET(pll)
                  );
            // reconfigure kernel's notion of time
            {
                struct clock_event_device *cd;
                unsigned int cpu = smp_processor_id();
                unsigned int freq = cpu_get_clock();
                mips_hpt_frequency = freq * 1000000 / 2;

                // see r4k_clockevent_init()
                cd = &per_cpu(mips_clockevent_device, cpu);
                cd->mult		= div_sc((unsigned long) mips_hpt_frequency, NSEC_PER_SEC, 32);
                cd->max_delta_ns	= clockevent_delta2ns(0x7fffffff, cd);
                cd->min_delta_ns	= clockevent_delta2ns(0x300, cd);
            }
        }
    }


    return ddr_get_clock();
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int cpu_get_clock(void) {
	uint32_t pll, out_div, ref_div, nint, frac, clk_ctrl, ref, freq;

	if ((ath_reg_rd(ATH_BOOTSTRAP_REG) & ATH_REF_CLK_40)) {
		ref = (40 * 1000000);
	} else {
		ref = (25 * 1000000);
	}

	clk_ctrl = ath_reg_rd(ATH_DDR_CLK_CTRL);
	pll = ath_reg_rd(CPU_DPLL2_ADDRESS);
	if (CPU_DPLL2_LOCAL_PLL_GET(pll)) {
		out_div	= CPU_DPLL2_OUTDIV_GET(pll);
		pll = ath_reg_rd(CPU_DPLL_ADDRESS);
		nint = CPU_DPLL_NINT_GET(pll);
		frac = CPU_DPLL_NFRAC_GET(pll);
		ref_div = CPU_DPLL_REFDIV_GET(pll);
        /*--- printk(KERN_ERR "%s: (CPU_DPLL_ADDRESS)=%x out_div=%x ref_div=%x nint=%x frac=%x ref=%u\n", __func__, pll, out_div, ref_div, nint, frac, ref); ---*/
		pll = ref >> 18;
	} else {
		pll = ath_reg_rd(ATH_PLL_CONFIG);
		out_div	= CPU_PLL_CONFIG_OUTDIV_GET(pll);
		ref_div	= CPU_PLL_CONFIG_REFDIV_GET(pll);
		nint	= CPU_PLL_CONFIG_NINT_GET(pll);
		frac	= CPU_PLL_CONFIG_NFRAC_GET(pll);
        /*--- printk(KERN_ERR "%s: (ATH_PLL_CONFIG)=%x out_div=%x ref_div=%x nint=%x frac=%x ref=%u\n", __func__, pll, out_div, ref_div, nint, frac, ref); ---*/
		pll = ref >> 6;
	}
	frac	= (frac * pll) / ref_div;

    freq = (((nint * (ref / ref_div)) + frac) >> out_div) /
			(CPU_DDR_CLOCK_CONTROL_CPU_POST_DIV_GET(clk_ctrl) + 1);

    /*--- printk(KERN_ERR "freq=%d: clk_ctrl=%x\n", freq, clk_ctrl); ---*/

    clk_ctrl = ath_reg_rd(ATH_CPU_DDR_CLOCK_CONTROL);
    if(CPU_DDR_CLOCK_CONTROL_CPUCLK_FROM_CPUPLL_GET(clk_ctrl)) 
        return freq;
    else
        return ddr_get_clock();
}
EXPORT_SYMBOL(cpu_get_clock);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int ddr_get_clock(void) {
	uint32_t pll, out_div, ref_div, nint, frac, clk_ctrl, ref, freq;

	if ((ath_reg_rd(ATH_BOOTSTRAP_REG) & ATH_REF_CLK_40)) {
		ref = (40 * 1000000);
	} else {
		ref = (25 * 1000000);
	}

	clk_ctrl = ath_reg_rd(ATH_DDR_CLK_CTRL);
	pll = ath_reg_rd(DDR_DPLL2_ADDRESS);
	if (DDR_DPLL2_LOCAL_PLL_GET(pll)) {
		out_div	= DDR_DPLL2_OUTDIV_GET(pll);

		pll = ath_reg_rd(DDR_DPLL_ADDRESS);
		nint = DDR_DPLL_NINT_GET(pll);
		frac = DDR_DPLL_NFRAC_GET(pll);
		ref_div = DDR_DPLL_REFDIV_GET(pll);
		pll = ref >> 18;
		frac	= frac * pll / ref_div;
		/*--- printk("[%s] ddr srif ", __FUNCTION__); ---*/
	} else {
		pll = ath_reg_rd(ATH_DDR_PLL_CONFIG);
		out_div	= DDR_PLL_CONFIG_OUTDIV_GET(pll);
		ref_div	= DDR_PLL_CONFIG_REFDIV_GET(pll);
		nint	= DDR_PLL_CONFIG_NINT_GET(pll);
		frac	= DDR_PLL_CONFIG_NFRAC_GET(pll);
		pll = ref >> 10;
		frac	= frac * pll / ref_div;
		/*--- printk("[%s] ddr apb ", __FUNCTION__); ---*/
	}

    freq = (((nint * (ref / ref_div)) + frac) >> out_div) /
			(CPU_DDR_CLOCK_CONTROL_DDR_POST_DIV_GET(clk_ctrl) + 1);

    /*--- printk(KERN_ERR "freq=%d\n", freq); ---*/

    clk_ctrl = ath_reg_rd(ATH_CPU_DDR_CLOCK_CONTROL);
    if(CPU_DDR_CLOCK_CONTROL_DDRCLK_FROM_DDRPLL_GET(clk_ctrl)) 
        return freq;
    else
        return cpu_get_clock();
}
EXPORT_SYMBOL(ddr_get_clock);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int ahb_get_clock(void) {
	uint32_t clk_ctrl, freq;

	clk_ctrl = ath_reg_rd(ATH_DDR_CLK_CTRL);
	if (CPU_DDR_CLOCK_CONTROL_AHBCLK_FROM_DDRPLL_GET(clk_ctrl)) {
		freq = ddr_get_clock() /
			(CPU_DDR_CLOCK_CONTROL_AHB_POST_DIV_GET(clk_ctrl) + 1);
	} else {
		freq = cpu_get_clock() /
			(CPU_DDR_CLOCK_CONTROL_AHB_POST_DIV_GET(clk_ctrl) + 1);
	}

    /*--- printk(KERN_ERR "freq=%d\n", freq); ---*/

    return freq;
}
EXPORT_SYMBOL(ahb_get_clock);

/*--------------------------------------------------------------------------------*\
 * todo: dynamische Ermittlung
\*--------------------------------------------------------------------------------*/
unsigned int ath_get_clock(enum _avm_clock_id id) {
    switch(id) {
        case avm_clock_id_cpu:
            return cpu_get_clock();
        case avm_clock_id_peripheral:
            return ath_uart_freq;
        case avm_clock_id_ahb:
            /*--- return ath_ahb_freq; ---*/
            return ahb_get_clock();
        case avm_clock_id_ddr:
            /*--- return ath_ddr_freq; ---*/
            return ddr_get_clock();
        case avm_clock_id_ref:
            return ath_ref_freq;
        default:
            break;
    }
    return 0;
}
EXPORT_SYMBOL(ath_get_clock);
