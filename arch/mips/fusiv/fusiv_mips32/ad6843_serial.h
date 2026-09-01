/*
 * Copyright (C) 2006  Ikanos Communications. All rights reserved.
 * The information and source code contained herein is the property
 * of Ikanos Communications.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#ifndef __AD6843_SERIAL_H__
#define __AD6843_SERIAL_H__

#define UART1_BASE_ADDRESS 0xb9020000

#define THR  0
#define RBR  0
#define DLL  0

#define IER  4
#define IER_EDSSI 0x08
#define IER_ELSI  0x04
#define IER_ETBEI 0x02
#define IER_ERBFI 0x01

#define DLH  4

#define IIR  8
#define IIR_STATUS 0x06
#define IIR_NINT   0x01

#define LCR 12
#define LCR_DLAB 0x80
#define LCR_BRK  0x40
#define LCR_STKP 0x20
#define LCR_EPS  0x10
#define LCR_PEN  0x08
#define LCR_STOP 0x04
#define LCR_WLS  0x03

#define LCR_WLS_5BIT 0x00
#define LCR_WLS_6BIT 0x01
#define LCR_WLS_7BIT 0x02
#define LCR_WLS_8BIT 0x03

#define MCR 16 
#define MCR_LOOP 0x10
#define MCR_OUT1 0x08
#define MCR_OUT2 0x04
#define MCR_RTS  0x02
#define MCR_DTR  0x01

#define LSR 20
#define LSR_TEMT 0x40
#define LSR_THRE 0x20
#define LSR_BI   0x10
#define LSR_FE   0x08
#define LSR_PE   0x04
#define LSR_OE   0x02
#define LSR_DR   0x01

#define MSR 24
#define MSR_DCD  0x80
#define MSR_RI   0x40
#define MSR_DSR  0x20
#define MSR_CTS  0x10
#define MSR_DDCD 0x08
#define MSR_TERI 0x04
#define MSR_DDSR 0x02
#define MSR_DCTS 0x01

#define SCR 28

#endif
