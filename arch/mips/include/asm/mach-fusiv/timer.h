/*
 * Copyright (C) 2006  Ikanos Communications. All rights reserved.
 * The information and source code contained herein is the property
 * of Ikanos Communications.
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
*/
#ifndef __AD6843_TIMER_H__
#define __AD6843_TIMER_H__

/* 16 bit wide registers spaced 32-bits apart */
struct timer_regs
{
/* timer1 */
    u16 status;
    u16 _res1;
    u16 config;
    u16 _res2;
    u16 counter_lo;
    u16 _res3;
    u16 counter_hi;
    u16 _res4;
    u16 period_lo;
    u16 _res5;
    u16 period_hi;
    u16 _res6;
    u16 width_lo;
    u16 _res7;
    u16 width_hi;
    u16 _res8;
/* timer2 */
    u16 status1;
    u16 _res11;
    u16 config1;
    u16 _res12;
    u16 counter_lo1;
    u16 _res13;
    u16 counter_hi1;
    u16 _res14;
    u16 period_lo1;
    u16 _res15;
    u16 period_hi1;
    u16 _res16;
    u16 width_lo1;
    u16 _res17;
    u16 width_hi1;
    u16 _res18;
/* timer3 */
    u16 status2;
    u16 _res21;
    u16 config2;
    u16 _res22;
    u16 counter_lo2;
    u16 _res23;
    u16 counter_hi2;
    u16 _res24;
    u16 period_lo2;
    u16 _res25;
    u16 period_hi2;
    u16 _res26;
    u16 width_lo2;
    u16 _res27;
    u16 width_hi2;
    u16 _res28;

};

#endif
