/*
 * Copyright (C) 2006  Ikanos Communications. All rights reserved.
 * The information and source code contained herein is the property
 * of Ikanos Communications. 
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
*/
/* usbWrapper.h - USB message queue wrapper header file */

#ifndef _usbwrapperh
#define _usbwrapperh

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/msg.h>
#include <linux/sched.h>
#include <linux/slab.h>

typedef  int MSG_Q_ID;


#define MAXMESGDATA   50
#define  MAXMSGQUEUE  40

typedef struct _Mesg_{
   int mesg_len;
   char mesg_data[MAXMESGDATA];
   struct _Mesg_ *pnext;
} Mesg;


typedef struct _QueueMsgHead_{
  Mesg *pMsgHead;
  Mesg *pMsgTail;
  int	counter;
}QueueMsgHead;


MSG_Q_ID msgQCreate ( key_t key,int maxMsgs, int maxMsgLength, int options);
int msgQDelete (MSG_Q_ID msgQId);
int msgQSend ( MSG_Q_ID msgQId, char *  buffer,  int nBytes, int timeout, int priority);
int msgQReceive ( MSG_Q_ID msgQId, char *   buffer, int  maxNBytes, int timeout);
int msgQNumMsgs( MSG_Q_ID msgQId);
void * AllocAlign(int iSize, int iAlign);
void FreeAlign(void *pBuffer);
void sleeptsk(int timeout);
//void sysWbFlush(void);


void fatalError(char *format, ...);
void sleeptsk(int timeout);

#define MKEY1  1234L
#define PERMS  0666

#define MKEY2  5678L
#define MKEY3  5664L
#define MKEY4  7768L
#define MKEY5  9876L
#define MKEY6  4567L
#define MSG_PRI_NORMAL 0
#define MSG_Q_FIFO     0

#define _LOG_EVENT(fd)  /* Nothing */


#endif

