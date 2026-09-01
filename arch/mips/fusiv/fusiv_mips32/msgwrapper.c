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

/* usbwrapper.c - USB Message queue wrapper functions for UCLINUX  */

/*
DESCRIPTION
This library is a USB Message queue wrapper library.

SEE ALSO: usb

INCLUDES: usbwrapper.h
*/

#define wchar_t char 
#include "defines.h"
#include <msgwrapper.h>

#ifndef EXPORT_SYMTAB
#  define EXPORT_SYMTAB
#endif


wait_queue_head_t queue;
QueueMsgHead  QH4[MAXMSGQUEUE];
int nextfree_queue = 0;

/* Message Q - din't work */
/*
int mesg_send (MSG_Q_ID id, Mesg *mesgptr)
{
     if(sys_msgsnd(id, (void *) &(mesgptr->mesg_type), mesgptr->mesg_len, 0) != 0)
     {
      // printk("\n msgsnd error\n");
       return FAILURE;
     }
     return SUCCESS;
}


int mesg_recv(MSG_Q_ID id, Mesg *mesgptr, int maxNBytes)
{
    int n = 0;
    
    n = sys_msgrcv(id, (void *) &(mesgptr->mesg_type), maxNBytes, mesgptr->mesg_type, IPC_NOWAIT);
    if((mesgptr->mesg_len = n) < 0)
    {
        //printk("\n msgrcv error\n");
        return n;
    } 
   
    return n;
}
*/


MSG_Q_ID msgQCreate (  key_t  key, int maxMsgs, int maxMsgLength, int options)
{
   static int first_queue = 0;
   if ( nextfree_queue < MAXMSGQUEUE)
   {
     QH4[nextfree_queue].pMsgHead = NULL;
     QH4[nextfree_queue].pMsgTail = NULL;
	 QH4[nextfree_queue].counter = 0;
     nextfree_queue ++;
     if(first_queue == 0)
     {
        init_waitqueue_head(&queue);
        first_queue++;
     }
     return (nextfree_queue-1);
   }
   return -1;
}
EXPORT_SYMBOL(msgQCreate);

int msgQDelete (MSG_Q_ID msgQId)
{
   Mesg *temp; 
   while(QH4[msgQId].pMsgHead != NULL)
   {
     temp = QH4[msgQId].pMsgHead;
     QH4[msgQId].pMsgHead = QH4[msgQId].pMsgHead->pnext;
     kfree(temp);
   }  
   QH4[msgQId].pMsgTail = NULL;
  
   return SUCCESS;
}
EXPORT_SYMBOL(msgQDelete);


int msgQSend ( MSG_Q_ID msgQId, char *  buffer,  int nBytes, int timeout, int priority)
{

     Mesg *msg; 
     
     if(nBytes > MAXMESGDATA)
      printk("\n error\n");
    
     msg = kmalloc (sizeof(Mesg), GFP_ATOMIC); 
     if(msg == NULL)
     {
      return -1;
     }
     
    memcpy(msg->mesg_data ,buffer, nBytes);
    msg->mesg_len = nBytes;
    msg->pnext = NULL;
  
    if(QH4[msgQId].pMsgTail == NULL)
    {
       QH4[msgQId].pMsgTail = msg;
       QH4[msgQId].pMsgHead = msg;
	   QH4[msgQId].counter++;
    }     
    else
    {
       QH4[msgQId].pMsgTail->pnext = msg;
       QH4[msgQId].pMsgTail = msg;
    }

    wake_up_interruptible(&queue);
    return SUCCESS;

}

EXPORT_SYMBOL(msgQSend);
int msgQReceive ( MSG_Q_ID msgQId, char *   buffer, int  maxNBytes, int timeout)
{
     Mesg *msg = NULL; 
     int n = 0;
     
     if (QH4[msgQId].pMsgHead != NULL)
     {
        msg = QH4[msgQId].pMsgHead;
        QH4[msgQId].pMsgHead = QH4[msgQId].pMsgHead->pnext;
        if(QH4 [msgQId].pMsgHead == NULL)
          QH4[msgQId].pMsgTail = QH4 [msgQId].pMsgHead;

     }
     else
     {
       if( timeout != IPC_NOWAIT)
                 interruptible_sleep_on_timeout(&queue, timeout);
       if (QH4[msgQId].pMsgHead != NULL)
       {
          msg = QH4[msgQId].pMsgHead;
          QH4[msgQId].pMsgHead = QH4[msgQId].pMsgHead->pnext;
          if(QH4 [msgQId].pMsgHead == NULL)
             QH4[msgQId].pMsgTail = QH4 [msgQId].pMsgHead;
       }
       else
         return -1; 
    }

    if(msg !=NULL)
    {
	  QH4[msgQId].counter--;
      memcpy(buffer, msg->mesg_data, msg->mesg_len);
      n = msg->mesg_len;
      kfree(msg);
    }
    return (n);
         
}

int msgQNumMsgs(MSG_Q_ID msgQId)
{
	if(msgQId >= 0 && msgQId < MAXMSGQUEUE)
		return QH4[msgQId].counter;
	else
		printk("msgQNumMsgs: invalid msgQId = %d\n\r", msgQId);
	return 0;
}

EXPORT_SYMBOL(msgQReceive);
#if 0
/*
 * Function Name     : fatalError()
 * Description       : Handler for fatal program errors. Should only
 *                     be used when a fatal, unrecoverable logical error
 *                     has been detected.
 * Input Parameters  : format           A printf-like format string
 *                     ...              Arguments to be used by printf
 * Output Parameters : none
 * Result            : none (never returns)
 */
void
fatalError(char *format, ...)
{
    // Impelementation is system dependent, but the following
    // handling is recommended:
    //
    // 1. Log the supplied error information. During development this could
    //    be to the console, but later it should be to some sort of persistent
    //    error log
    //
    // 2. Cause program execution to terminate. During development a breakpoint
    //    exception should be raised. After this the system should reset itself

    va_list     args;
    static char buff[2048]; 

    // Set up the variable argument list
    va_start(args, format);
   
    // Print the error message to stdout
    vsprintf(buff, format, args);
    printk("\r\n FATAL: %s", buff);

    // Generate a breakpoint exception
    asm("\tbreak");

    // If the 'break' instruction returns then we have a problem. We really
    // should reset, but just hang this thread for the moment
/**********BAMA */
//    tskSuspend(0);
/**********/
}
#endif
void sleeptsk ( int timeout)
{
      interruptible_sleep_on_timeout(&queue, 1);
      return;
}
//EXPORT_SYMBOL(sleeptsk);


void * AllocAlign(int iSize, int iAlign)
{
   char * pTmp;
   void * pRet;
   int iTmp;

   if (iAlign < sizeof(void*))
       iAlign = sizeof(void*);

   if ((pTmp = kmalloc(iSize + (iAlign * 2), GFP_KERNEL)) != NULL)
   {
       iTmp = ((unsigned int)pTmp) % iAlign;
       pRet = (void*)(pTmp + iAlign + iTmp);
       *(char **)(((char*)pRet) - sizeof(char *)) = pTmp;
   }
   else
      pRet = NULL;

#if 0 /* Debug */
   printk("ma:%x\n",pTmp);
   printk("m:%x\n",pRet);
#endif
   return pRet;
}

//EXPORT_SYMBOL(AllocAlign);

void FreeAlign(void *pBuffer)
{
#if 0 /* Debug */
     printk("f:%x\n",pBuffer);
     printk("fa:%x\n",(*(char **)(((char *)pBuffer) - sizeof(char *))));
#endif
     if ( pBuffer == NULL )
	printk("Illegal\n");
     else 
        kfree(*(char **)(((char *)pBuffer) - sizeof(char *)));
}
//EXPORT_SYMBOL(FreeAlign);







     

         


     

         



