/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2007 by Ralf Baechle
 */
#include <linux/clocksource.h>
#include <linux/init.h>

#include <asm/time.h>

static cycle_t c0_hpt_read(struct clocksource *cs)
{
	return read_c0_count();
}

#define CLOCKSOURCE_DEFAULT         0
#define CLOCKSOURCE_AR9_111MHZ      1
#define CLOCKSOURCE_AR9_166MHZ      2
#define CLOCKSOURCE_AR9_333MHZ      3
#define CLOCKSOURCE_AR9_393MHZ      4
#define CLOCKSOURCE_VR9_125MHZ      5
#define CLOCKSOURCE_VR9_333MHZ      CLOCKSOURCE_AR9_333MHZ
#define CLOCKSOURCE_VR9_393MHZ      CLOCKSOURCE_AR9_393MHZ
#define CLOCKSOURCE_VR9_500MHZ      6
#define CLOCKSOURCE_UR8_240MHZ      7
#define CLOCKSOURCE_UR8_300MHZ      8
#define CLOCKSOURCE_UR8_360MHZ      9
#define CLOCKSOURCE_AR10_125MHZ     CLOCKSOURCE_VR9_125MHZ      
#define CLOCKSOURCE_AR10_250MHZ     10
#define CLOCKSOURCE_AR10_500MHZ     CLOCKSOURCE_VR9_500MHZ
#define CLOCKSOURCE_AR10_600MHZ     11

struct clocksource clocksource_mips[] = {
    [CLOCKSOURCE_DEFAULT] = {
	.name		= "MIPS",
	.read		= c0_hpt_read,
	.mask		= CLOCKSOURCE_MASK(32),
	.flags		= CLOCK_SOURCE_IS_CONTINUOUS,
    },
    // AR9-Takte
    [CLOCKSOURCE_AR9_111MHZ] = {
        .name  = "MIPS-111",
        .read  = c0_hpt_read,
        .mask  = CLOCKSOURCE_MASK(32),
        .flags = CLOCK_SOURCE_IS_CONTINUOUS,
    },
    [CLOCKSOURCE_AR9_166MHZ] = {
        .name  = "MIPS-166",
        .read  = c0_hpt_read,
        .mask  = CLOCKSOURCE_MASK(32),
        .flags = CLOCK_SOURCE_IS_CONTINUOUS,
    },
    [CLOCKSOURCE_AR9_333MHZ] = {
        .name  = "MIPS-333",
        .read  = c0_hpt_read,
        .mask  = CLOCKSOURCE_MASK(32),
        .flags = CLOCK_SOURCE_IS_CONTINUOUS,
    },
    [CLOCKSOURCE_AR9_393MHZ] = {
        .name  = "MIPS-393",
        .read  = c0_hpt_read,
        .mask  = CLOCKSOURCE_MASK(32),
        .flags = CLOCK_SOURCE_IS_CONTINUOUS,
    },
    [CLOCKSOURCE_AR10_250MHZ] = {
        .name  = "MIPS-250",
        .read  = c0_hpt_read,
        .mask  = CLOCKSOURCE_MASK(32),
        .flags = CLOCK_SOURCE_IS_CONTINUOUS,
    },
    // VR9-Takte
    [CLOCKSOURCE_VR9_125MHZ] = {
        .name  = "MIPS-125",
        .read  = c0_hpt_read,
        .mask  = CLOCKSOURCE_MASK(32),
        .flags = CLOCK_SOURCE_IS_CONTINUOUS,
    },
    [CLOCKSOURCE_VR9_500MHZ] = {
        .name  = "MIPS-500",
        .read  = c0_hpt_read,
        .mask  = CLOCKSOURCE_MASK(32),
        .flags = CLOCK_SOURCE_IS_CONTINUOUS,
    },
    // UR8
    [CLOCKSOURCE_UR8_240MHZ] = {
        .name  = "MIPS-240",
        .read  = c0_hpt_read,
        .mask  = CLOCKSOURCE_MASK(32),
        .flags = CLOCK_SOURCE_IS_CONTINUOUS,
    },
    [CLOCKSOURCE_UR8_300MHZ] = {
        .name  = "MIPS-300",
        .read  = c0_hpt_read,
        .mask  = CLOCKSOURCE_MASK(32),
        .flags = CLOCK_SOURCE_IS_CONTINUOUS,
    },
    [CLOCKSOURCE_UR8_360MHZ] = {
        .name  = "MIPS-360",
        .read  = c0_hpt_read,
        .mask  = CLOCKSOURCE_MASK(32),
        .flags = CLOCK_SOURCE_IS_CONTINUOUS,
    }
};

int __init init_r4k_clocksource(void)
{
    int i = 0;
	if (!cpu_has_counter || !mips_hpt_frequency)
		return -ENXIO;

	/* Calculate a somewhat reasonable rating value */
	clocksource_mips[i].rating = 200 + mips_hpt_frequency / 10000000;

	clocksource_set_clock(&clocksource_mips[i], mips_hpt_frequency);

	clocksource_register(&clocksource_mips[i]);

    for(i = 1 ; i < sizeof(clocksource_mips) / sizeof(clocksource_mips[0]) ; i++) {
        unsigned long int freq;
        switch(i) {
#ifdef CONFIG_AR9
            case CLOCKSOURCE_AR9_111MHZ: 
                freq = 111111111UL / 2;
                break;
            case CLOCKSOURCE_AR9_166MHZ: 
                freq = 166000000UL / 2;
                break;
            case CLOCKSOURCE_AR9_333MHZ: 
                freq = 333333333UL / 2;
                break;
            case CLOCKSOURCE_AR9_393MHZ: 
                freq = 394715332UL / 2;
                break;
#endif /*--- #ifdef CONFIG_AR9 ---*/

#ifdef CONFIG_AR10
            case CLOCKSOURCE_AR10_125MHZ: 
                freq = 125000000UL / 2;
                break;
            case CLOCKSOURCE_AR10_250MHZ: 
                freq = 250000000UL / 2;
                break;
            case CLOCKSOURCE_AR10_500MHZ: 
                freq = 500000000UL / 2;
                break;
            case CLOCKSOURCE_AR10_600MHZ: 
                freq = 600000000UL / 2;
                break;
#endif /*--- #ifdef CONFIG_VR9 ---*/

#ifdef CONFIG_VR9
            case CLOCKSOURCE_VR9_125MHZ: 
                freq = 125000000UL / 2;
                break;
            case CLOCKSOURCE_VR9_333MHZ: 
                freq = 333333333UL / 2;
                break;
            case CLOCKSOURCE_VR9_393MHZ: 
                freq = 394715332UL / 2;
                break;
            case CLOCKSOURCE_VR9_500MHZ: 
                freq = 500000000UL / 2;
                break;
#endif /*--- #ifdef CONFIG_VR9 ---*/
#ifdef CONFIG_MIPS_UR8
            case CLOCKSOURCE_UR8_240MHZ: 
                freq = 240000000UL / 2;
                break;
            case CLOCKSOURCE_UR8_300MHZ: 
                freq = 300000000UL / 2;
                break;
            case CLOCKSOURCE_UR8_360MHZ: 
                freq = 360000000UL / 2;
                break;
#endif /*--- #ifdef CONFIG_MIPS_UR8 ---*/
            default:
                freq = 0;
                break;
        }
        if (freq) {
            clocksource_mips[i].rating = 200 + freq / 1000000;
            clocksource_set_clock(&clocksource_mips[i], freq );
            clocksource_register(&clocksource_mips[i]);
            /*--- printk("%s add clock[%d] %lu\n", __func__, i, freq); ---*/
        }
    }

	return 0;
}

void mips_change_clocksource(u32 cpu_clk) {
}
