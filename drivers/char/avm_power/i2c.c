/*--------------------------------------------------------------------------------------*\

\*-------------------------------------------------------------------------------------*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28)
#include <asm/mach-ikan_mips/vx180.h>
#else /*--- #if LINUX_VERSION_CODE < KERNEL_VERSION(2.6.28) ---*/
#include <vx180.h>
#endif /*--- #else ---*/ /*--- #if LINUX_VERSION_CODE < KERNEL_VERSION(2.6.28) ---*/
#include "i2c.h"

/*--- #define DBG_TRC(arg...)  __printk(arg) ---*/
#define DBG_TRC(arg...)

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
struct _i2cmgr {
    unsigned int sdata;
    unsigned int sclk;
    unsigned int isoutput;
};
static struct _i2cmgr gI2Cmgr;

/*-------------------------------------------------------------------------------------*\
  Lokale Funktionen:
\*-------------------------------------------------------------------------------------*/
void I2C_WritePort(struct _i2cmgr *i2cmgr, unsigned int Signal, unsigned int Value);
void I2C_Start(void);
void I2C_Stop(void);
void I2C_PutByte(unsigned char ch);
unsigned char I2C_GetByte(void);
void I2C_SetAck(void); 
void I2C_GetAck(void); 

/*-------------------------------------------------------------------------------------*\
    I2C_Delay
    250 KHz -Clock
\*-------------------------------------------------------------------------------------*/
inline void I2C_Delay(void) {
    udelay(10);
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
inline void I2C_SetClktoOutput(struct _i2cmgr *i2cmgr) {
    VX180_GPIO_SETDIR(i2cmgr->sclk, VX180_GPIO_AS_OUTPUT);
}
/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
inline void I2C_SetDataToOutput(struct _i2cmgr *i2cmgr) {
    if(i2cmgr->isoutput == 0) {
        VX180_GPIO_SETDIR(i2cmgr->sdata, VX180_GPIO_AS_OUTPUT);
        i2cmgr->isoutput = 1;
    }
}
/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
inline void I2C_SetDataToInput(struct _i2cmgr *i2cmgr) {
    i2cmgr->isoutput = 0;
    VX180_GPIO_SETDIR(i2cmgr->sdata, VX180_GPIO_AS_INPUT);
}
/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
inline unsigned int I2C_ReadPort(struct _i2cmgr *i2cmgr) {
    return VX180_GPIO_INPUT(i2cmgr->sdata);
}

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
void I2C_WritePort(struct _i2cmgr *i2cmgr, unsigned int Signal, unsigned int Value) {

    switch(Signal) {
        case I2C_SDA:  
            I2C_SetDataToOutput(i2cmgr);
            VX180_GPIO_OUTPUT(i2cmgr->sdata, Value);
            break;
        case I2C_SCK:  
            VX180_GPIO_OUTPUT(i2cmgr->sclk, Value);
            break;
    }
    I2C_Delay();
}
/*-------------------------------------------------------------------------------------*\
The I2C FAQ 2.0 A Driver in PseudoCode 
    It is written in PseudoCode which is an imaginary programming language that 
    any programmer should be capable of porting to his/her favorite language.

    First we will define a set of basic interface routines. All text between / / is considered as remark.

    Following variables are used :

    n,x = a general purpose BYTE 
    SIZE = a byte holding the maximum number of transferred data at a time 
    DATA(SIZE) = an array holding up to SIZE number of bytes. This will contain the data we 
    want to transmit and will store the received data. 
    BUFFER = a byte value holding immediate received or transmit data. 
    Note. This is the improved Version 1.0. It features startup code which is of use 
    immediately after boot of your system.
    
    A number of adaptations have been made to kill some nasty beasties.
    
    A Bug in the Start routine was fixed thanks to Tony Ayling. Thanks Tony
    
    / $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ / 
    / **** I2C Driver V1.1 Written by V.Himpe. Released as Public Domain **** / 
    / $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ /
    
    DECLARE N,SIZE,BUFFER,X Byte 
    
    DECLARE DATA() Array of SIZE elements
    
\*-------------------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------------------*\
    SUBroutine I2C_INIT / call this immediately after power-on / 
        SDA=1 
        SCK=0 
        FOR n = 0 to 3 
            CALL STOP 
        NEXT n 
    ENDsub
\*-------------------------------------------------------------------------------------*/
void I2C_Init(unsigned int sdata, unsigned int sclk){
    struct _i2cmgr *i2cmgr = &gI2Cmgr;

    i2cmgr->sdata    = sdata;
    i2cmgr->sclk     = sclk;
    i2cmgr->isoutput = 0;
    I2C_SetClktoOutput(i2cmgr);

    I2C_WritePort(i2cmgr, I2C_SDA, 1);
    I2C_WritePort(i2cmgr, I2C_SCK, 0);
    I2C_Stop();
    I2C_Stop();
    I2C_Stop();
    I2C_Stop();
}
/*-------------------------------------------------------------------------------------*\
    SUBroutine START 
        SCK=1 / BUGFIX !/ 
        SDA=1 / Improvement / 
        SDA=0 
        SCK=0 
        SDA=1 
    ENDsub
\*-------------------------------------------------------------------------------------*/
void I2C_Start(void) {
    register struct _i2cmgr *i2cmgr = &gI2Cmgr;
    I2C_WritePort(i2cmgr, I2C_SCK, 1);
    I2C_WritePort(i2cmgr, I2C_SDA, 1);
    I2C_WritePort(i2cmgr, I2C_SDA, 0);
    I2C_WritePort(i2cmgr, I2C_SCK, 0);
    I2C_WritePort(i2cmgr, I2C_SDA, 1);
}

/*-------------------------------------------------------------------------------------*\
    SUBroutine STOP 
        SDA=0 
        SCK=1 
        SDA=1 
    ENDsub
\*-------------------------------------------------------------------------------------*/
void I2C_Stop(void) {
    register struct _i2cmgr *i2cmgr = &gI2Cmgr;
    I2C_WritePort(i2cmgr, I2C_SDA, 0);
    I2C_WritePort(i2cmgr, I2C_SCK, 1);
    I2C_WritePort(i2cmgr, I2C_SDA, 1);
    DBG_TRC("P");
}

/*-------------------------------------------------------------------------------------*\
    SUBroutine PUTBYTE(BUFFER) 
        FOR n = 7 TO 0 
            SDA= BIT(n) of BUFFER 
            SCK=1 
            SCK=0 
        NEXT n 
        SDA=1 
    ENDsub
\*-------------------------------------------------------------------------------------*/
void I2C_PutByte(unsigned char ch) {
    register struct _i2cmgr *i2cmgr = &gI2Cmgr;
    int Count;
    for(Count = 7 ; Count >= 0 ; Count--) {
        I2C_WritePort(i2cmgr, I2C_SDA, (ch >> Count) & 0x01);
        I2C_WritePort(i2cmgr, I2C_SCK, 1);
        I2C_WritePort(i2cmgr, I2C_SCK, 0);
    }
    I2C_WritePort(i2cmgr, I2C_SDA, 1);
    DBG_TRC("B(%x)", ch);
}

/*-------------------------------------------------------------------------------------*\
    SUBroutine GETBYTE 
        FOR n = 7 to 0 
            SCK=1 
            BIT(n) OF BUFFER = SDA 
            SCK=0 
        NEXT n 
        SDA=1 
    ENDsub
\*-------------------------------------------------------------------------------------*/
unsigned char I2C_GetByte(void) {
    register struct _i2cmgr *i2cmgr = &gI2Cmgr;
    int Count;
    unsigned char Value = 0;

    DBG_TRC("G");
    I2C_SetDataToInput(i2cmgr);

    for(Count = 7 ; Count >= 0 ; Count--) {
        I2C_WritePort(i2cmgr, I2C_SCK, 1);
        Value |= I2C_ReadPort(i2cmgr) ? (1 << Count) : 0;
        I2C_WritePort(i2cmgr, I2C_SCK, 0);
    }
    I2C_WritePort(i2cmgr, I2C_SDA, 1);
    return Value;
}

/*-------------------------------------------------------------------------------------*\
    SUBroutine GIVEACK 
        SDA=0 
        SCK=1 
        SCK=0 
        SDA=1 
    ENDsub
\*-------------------------------------------------------------------------------------*/
void I2C_SetAck(void) {
    register struct _i2cmgr *i2cmgr = &gI2Cmgr;
    I2C_WritePort(i2cmgr, I2C_SDA, 0);
    I2C_WritePort(i2cmgr, I2C_SCK, 1);
    I2C_WritePort(i2cmgr, I2C_SCK, 0);
    I2C_WritePort(i2cmgr, I2C_SDA, 1);
    DBG_TRC("!");
}

/*-------------------------------------------------------------------------------------*\
    SUBroutine GETACK 
        SDA=1 
        SCK=1 
        WAITFOR 
            SDA=0 
        SCK=0 
    ENDSUB
    vorzeitiger Abbruch : nach mind. 300 usec 
\*-------------------------------------------------------------------------------------*/
void I2C_GetAck(void) {
    register struct _i2cmgr *i2cmgr = &gI2Cmgr;
    unsigned int i;

    I2C_WritePort(i2cmgr, I2C_SDA, 1);
    I2C_WritePort(i2cmgr, I2C_SCK, 1);

    I2C_SetDataToInput(i2cmgr);

    DBG_TRC("A");
    for( i = 0; i < 300; i++) {
        if(I2C_ReadPort(i2cmgr) == 0) {
            break;
        }
        udelay(1);
    }
    I2C_WritePort(i2cmgr, I2C_SCK, 0);
    DBG_TRC("a(%d)", i);
}

/*--------------------------------------------------------------------------------*\
 Read Programming Sequence
    Wr = 0 (Write to Device), Rd = 1 (Read frm Device)
    A  = Get Ack
    SA = Set Ack
    Sr repeated Start-Condition
    P   Stop-Condition
    START  Slave_Address Wr   A   CommandCode  A  SA   Slave_Address  Rd   A   DataByte  SA   P
      (1)       (7)      (1) (1)       (8)    (1) (1)     (7)        (1)  (1)     (8)    (1) (1) 
\*--------------------------------------------------------------------------------*/
unsigned char I2C_ReadByte(unsigned char DeviceAddr, unsigned char addr) {
    unsigned char CommandCode = addr | 0x80; /*--- Byte-Read-Operation ---*/
    unsigned char Byte;
    I2C_Start();
    I2C_PutByte(DeviceAddr & ~0x01);
    I2C_GetAck();
    I2C_PutByte(CommandCode);
    I2C_GetAck();
    I2C_Start();
    I2C_PutByte(DeviceAddr | 0x01);
    I2C_GetAck();
    Byte = I2C_GetByte();
    I2C_SetAck();
    I2C_Stop();
    return Byte;
}
/*-------------------------------------------------------------------------------------*\
 Block Read Programming Sequence
    Wr = 0 (Write to Device), Rd = 1 (Read frm Device)
    A  = Get Ack
    SA = Set Ack
    Sr repeated Start-Condition
    P   Stop-Condition
    START  Slave_Address Wr   A   CommandCode  A   Sr   Slave_Address   Rd   A   Get Byte Count N  SA  DataByte 0 SA   ... P
      (1)       (7)      (1) (1)       (8)    (1)  (1)      (7)         (1) (1)        (8)         (1)   (8)      (1)     (1) 

    ret: Anzahl der gelesenen Bytes (ohne Begrenzung durch maxbuffersize)
\*-------------------------------------------------------------------------------------*/
unsigned int I2C_ReadBuffer(unsigned char DeviceAddr, unsigned char addr, unsigned char *Buffer, unsigned char maxbuffersize) {
    unsigned char CommandCode = addr &  ~0x80; /*--- Block-Read-Operation ---*/
    unsigned int Count;
    unsigned char ReadBytes, MinReadBytes, val;

    I2C_Start();
    I2C_PutByte(DeviceAddr & ~0x01);
    I2C_GetAck();
    I2C_PutByte(CommandCode);
    I2C_GetAck();
    I2C_Start();
    I2C_PutByte(DeviceAddr | 0x01);
    I2C_GetAck();
    ReadBytes = I2C_GetByte();
    I2C_SetAck();
    /*--- printk("bytes to read: %d\n", ReadBytes); ---*/
    for(Count = 0 ; Count < ReadBytes; Count++) {
        val = I2C_GetByte();
        if(maxbuffersize) {
            Buffer[Count] = val;
            maxbuffersize--;
        }
        I2C_SetAck();
    }
    I2C_Stop();
    return ReadBytes;
}
/*-------------------------------------------------------------------------------------*\
 Write Programming Sequence
    Wr = 0 (Write to Device), Rd = 1 (Read from Device)
    A  = Get Ack
    P   Stop-Condition
    START  Slave_Address Wr   A   addr  A   DataByte  A   P
      (1)       (7)      (1) (1)       (8)    (1)     (8)   (1) (1) 
\*-------------------------------------------------------------------------------------*/
void I2C_WriteByte(unsigned char DeviceAddr, unsigned char addr, unsigned char Byte) {
    unsigned char CommandCode = addr | 0x80; /*--- Byte-Write-Operation ---*/

    I2C_Start();
    I2C_PutByte(DeviceAddr & ~0x01);
    I2C_GetAck();
    I2C_PutByte(CommandCode);
    I2C_GetAck();
    I2C_PutByte(Byte);
    I2C_GetAck();
    I2C_Stop();
}
/*-------------------------------------------------------------------------------------*\
 Block Write Programming Sequence
    Wr = 0 (Write to Device), Rd = 1 (Read frm Device)
    A  = Get Ack
    P   Stop-Condition
    START  Slave_Address Wr   A   CommandCode  A   Byte Count N  A  DataByte 0  A   ... P
      (1)       (7)      (1) (1)       (8)    (1)     (8)       (1)   (8)      (1)     (1) 
\*-------------------------------------------------------------------------------------*/
void I2C_WriteBuffer(unsigned char DeviceAddr, unsigned char addr, unsigned char Write_Bytes, unsigned char *Buffer) {
    unsigned char CommandCode = addr &  ~0x80; /*--- Block-Write-Operation ---*/
    unsigned int Count;

    /*--- printk("byte count: %x\n", Write_Bytes); ---*/
    I2C_Start();
    I2C_PutByte(DeviceAddr & ~0x01);
    I2C_GetAck();
    I2C_PutByte(CommandCode);
    I2C_GetAck();
    I2C_PutByte(Write_Bytes);
    I2C_GetAck();
    for(Count = 0 ; Count < Write_Bytes ; Count++) {
        /*--- printk("byte : %x\n", Buffer[Count]); ---*/
        I2C_PutByte(Buffer[Count]);
        I2C_GetAck();
    }
    I2C_Stop();
}
