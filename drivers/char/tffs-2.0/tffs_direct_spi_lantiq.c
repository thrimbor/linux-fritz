/*------------------------------------------------------------------------------------------*\
 *   
 *   Copyright (C) 2007 AVM GmbH <fritzbox_info@avm.de>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
\*------------------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------------------*\
 * Direkte Ansteuerung des SPI-Flash im Panic Mode (Urlader-Routinen)
\*------------------------------------------------------------------------------------------*/

#include <linux/module.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <asm/fcntl.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/semaphore.h>
#include <ifx_clk.h>

#include <ifx_types.h>
#include <ifx_regs.h>
#include "../../spi/ifxmips_ssc_reg.h"
#include <common_routines.h>

#include "tffs_direct_spi_lantiq.h"
#include <asm/irq.h>

/*--- #define DEBUG_SPI ---*/
#ifdef DEBUG_SPI
#define DBG_SPI(...)        printk(__VA_ARGS__)
#else
#define DBG_SPI(...) 
#endif

#ifndef BIT
    #define BIT(x)              (1 << (x))
#endif /*--- #ifndef BIT ---*/

#if defined(CONFIG_VR9)
#define SPI_CSx     (10 & 0xF)  /* P0.10 *//*---   CS4 ---*/
#define SPI_DIN     (0 & 0xF)  /* P1.0 */
#define SPI_DOUT    (1 & 0xF)  /* P1.1 */
#define SPI_CLK     (2 & 0xF)  /* P1.2 */
#define IFX_SSC_CSx IFX_SSC_CS4
#elif defined(CONFIG_AR10)/*--- #if defined(CONFIG_VR9) ---*/
#define SPI_CSx     (15 & 0xF)  /* P0.15 *//*---   CS1 ---*/
#define SPI_DIN     (0 & 0xF)  /* P1.0 */
#define SPI_DOUT    (1 & 0xF)  /* P1.1 */
#define SPI_CLK     (2 & 0xF)  /* P1.2 */
#define IFX_SSC_CSx IFX_SSC_CS1
#else /*--- #elif defined(CONFIG_AR10) ---*//*--- #if defined(CONFIG_VR9) ---*/
#error: unsupported platform
#endif/*--- #else  ---*//*--- #elif defined(CONFIG_AR10) ---*//*--- #if defined(CONFIG_VR9) ---*/

#define FLASH_BASE              0
#define FLASH_BUFFER_SIZE       256


struct _spi_register   *const SPI = (struct _spi_register *)&(*(volatile unsigned int *)(IFX_SSC_PHY_BASE | KSEG1));

struct tffs_spi_flash_device tffs_spi_device;


extern int tffs_mtd_offset[2];
extern struct semaphore	ifx_sflash_sem;

static DEFINE_SPINLOCK(spi_locking);
#define SPI_BIG_ENDIAN

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static inline void spi_lock(unsigned long *flags, unsigned long *cpuflag __attribute__((unused))) {
    spin_lock_irqsave(&spi_locking, *flags);
#if defined(CONFIG_SMP)
    *cpuflag = dvpe();
#endif/*--- #if defined(CONFIG_SMP) ---*/
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static inline void spi_unlock(unsigned long flags, unsigned long cpuflag __attribute__((unused))) {
#if defined(CONFIG_SMP)
    evpe(cpuflag);
#endif/*--- #if defined(CONFIG_SMP) ---*/
    spin_unlock_irqrestore(&spi_locking, flags);
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static inline int spi_busy_wait(unsigned int usec) {
    while (SPI->STAT.Bits.bsy && --usec) {
        udelay(1);
    }
    return SPI->STAT.Bits.bsy ? 1 : 0;
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static inline int spi_read_wait(unsigned int usec) {
    while ((SPI->FSTAT.Bits.rxffl < 1) && --usec){
        udelay(1);
    }
    return SPI->FSTAT.Bits.rxffl ? 0 : 1;
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void delay(unsigned int units) {
	volatile unsigned int  ii;
	for (ii = 0; ii < units; ii++);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void spi_cs(enum spi_cs cs, enum spi_cs_enum set) {

    switch (set) {
        case deselect:          /*--- set CS high ---*/
            SPI->FGPO = (1 << cs) << 8;
            break;
        case select:            /*--- set CS low ---*/
            SPI->FGPO = (1 << cs);
            break;
    }

}

/*------------------------------------------------------------------------------------------*\
 * es muss RX & TX eingeschaltet sein, damit spi_cmd_simple funktioniert
\*------------------------------------------------------------------------------------------*/
static void spi_cmd_simple(unsigned int cmd) {

    volatile unsigned int tmp __attribute__((unused));

	spi_cs(IFX_SSC_CSx, select);
    SPI->TB.Char.Byte = cmd;
    if(spi_read_wait(1000)) {
        spi_cs(IFX_SSC_CSx, deselect);
        printk(KERN_ERR"%s error on spi_read_wait\n", __func__);
        return;
    }
    tmp = SPI->RB;                          /*--- das gelesene Byte aus der Rx-Fifo löschen ---*/
	spi_cs(IFX_SSC_CSx, deselect);
    delay(5);

}

/*-----------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int spi_read_byte(unsigned int address, unsigned int mtd_id, unsigned char *pdata) {
    unsigned long flags, cpuflags;
    address = address - FLASH_BASE + tffs_mtd_offset[mtd_id];

    spi_lock(&flags, &cpuflags);
    SPI->CON.Bits.rxoff = 1;
    SPI->CON.Bits.txoff = 0;

	spi_cs(IFX_SSC_CSx, select);
    SPI->TB.Register = SPI_READ | address;
    if(spi_busy_wait(1000)) {
        spi_cs(IFX_SSC_CSx, deselect);
        spi_unlock(flags, cpuflags);
        printk(KERN_ERR"%s: error SPI busy %d\n", __func__, __LINE__);
        return 0;
    }
    SPI->CON.Bits.txoff = 1;
    SPI->CON.Bits.rxoff = 0;

    SPI->RXREQ = 1;             /*--- 1 Byte lesen ---*/
    if(spi_read_wait(1000)) {
        spi_cs(IFX_SSC_CSx, deselect);
        SPI->CON.Bits.txoff = 0;
        spi_unlock(flags, cpuflags);
        printk(KERN_ERR"%s error on spi_read_wait\n", __func__);
        return 0;
    }
	spi_cs(IFX_SSC_CSx, deselect);
    SPI->CON.Bits.txoff = 0;

    *pdata = SPI->RB;
    spi_unlock(flags, cpuflags);
    return 1;
}

/*------------------------------------------------------------------------------------------*\
 * 4 Byte aligned, wir kopieren immer 4 Bytes 
\*------------------------------------------------------------------------------------------*/
unsigned int spi_read_block(unsigned int address, unsigned int mtd_id, unsigned char *pdata, unsigned int datalen) {
    unsigned long flags, cpuflags;
    unsigned int len;
   
    len = datalen = datalen >> 2;
    address = address - FLASH_BASE + tffs_mtd_offset[mtd_id];

    spi_lock(&flags, &cpuflags);
    SPI->CON.Bits.rxoff = 1;
    SPI->CON.Bits.txoff = 0;

	spi_cs(IFX_SSC_CSx, select);
    SPI->TB.Register = SPI_READ | address;
    if(spi_busy_wait(1000)) {
        spi_cs(IFX_SSC_CSx, deselect);
        spi_unlock(flags, cpuflags);
        printk(KERN_ERR"%s: error SPI busy %d\n", __func__, __LINE__);
        return 0;
    }
    SPI->CON.Bits.txoff = 1;
    SPI->CON.Bits.rxoff = 0;

    while (len) {
        SPI->RXREQ = 4;                 /*--- 4 Byte lesen ---*/
        if(spi_read_wait(1000)) {
            spi_cs(IFX_SSC_CSx, deselect);
            SPI->CON.Bits.txoff = 0;
            spi_unlock(flags, cpuflags);
            printk("%s error on spi_read_wait\n", __func__);
            return 0;
        }
        delay(5);

#if defined(SPI_BIG_ENDIAN)
        *(unsigned int *)pdata = SPI->RB;
        /*--- DBG_SPI("[%s] 0x%x\n", __func__, *(unsigned int *)pdata); ---*/
        pdata += 4;
#else
        *pdata++ = SPI->data.Byte[3];   /*--- da wir 4 Byte lesen, ist LSB im MSB ---*/
        *pdata++ = SPI->data.Byte[2];
        *pdata++ = SPI->data.Byte[1];
        *pdata++ = SPI->data.Byte[0];
#endif
        len--;
    }
	spi_cs(IFX_SSC_CSx, deselect);
    SPI->CON.Bits.txoff = 0;
    spi_unlock(flags, cpuflags);

    return datalen << 2;
}

/*------------------------------------------------------------------------------------------*\
 * aus Performacegründen kopieren wir zuerst 32bit weise, den Rest 8bit weise
\*------------------------------------------------------------------------------------------*/
int tffs_spi_read(unsigned int address, unsigned int mtd_id, unsigned char *pdata, unsigned int len) {

    unsigned int Bytes = 0;
    unsigned int read, read_len;

    /*--- DBG_SPI("[%s] 0x%x mtd %d len %d 0x%p\n", __func__, address, mtd_id, len, pdata); ---*/

    while (Bytes < len) {
        if ((address & 3) || ((unsigned int)pdata & 3) || ((len - Bytes) < 4)) {     /*--- byteweise lesen ---*/
            read = spi_read_byte(address, mtd_id, pdata);
            /*--- DBG_SPI("[%s] Bytes 0x%x len %d data 0x%x\n", __func__, address, len, *pdata); ---*/
        } else {
            read_len = (len - Bytes) > ((SPI_MAX_FRAME_LEN-1)<<2) ? ((SPI_MAX_FRAME_LEN-1)<<2) : (len - Bytes);
            read = spi_read_block(address, mtd_id, pdata, read_len);
            /*--- DBG_SPI("[%s] Int 0x%x len %d data 0x%x\n", __func__, address, len & ~3, *(unsigned int *)pdata); ---*/
        }
        address += read;
        pdata += read;
        Bytes += read;
        if (read == 0) {
            printk(KERN_ERR"<ERROR: Flash read aborted %x>\n", (int)address);
            break;
        }
    }

    return Bytes;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static unsigned int spi_GetBlockSize(unsigned int address) {

    int i;
    unsigned int regions = 0;

    if((address >= FLASH_BASE) && (address < (FLASH_BASE + tffs_spi_device.Size))) {
        for (i=0;i<tffs_spi_device.Geometry.num_erase_regions;i++) {
            regions += tffs_spi_device.Geometry.erase_regions[i].size; 
            if (address < (FLASH_BASE + regions))
                return tffs_spi_device.Geometry.erase_regions[i].blk_size;
        }
    }
    return(0);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static unsigned char spi_read_status(enum spi_cs cs) {
	spi_cs(cs, select);
    SPI->TB.Short.Bytes = SPI_READ_STATUS << 8;
    if(spi_read_wait(1000)) {
        spi_cs(IFX_SSC_CSx, deselect);
        printk(KERN_ERR"%s error on spi_read_wait\n", __func__);
        return 0xFF;
    }
	spi_cs(cs, deselect);

    return (SPI->RB & 0xFF);
}
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static unsigned int tffs_spi_write_byte(unsigned int address, unsigned int mtd_id, unsigned char *pdata) {
    unsigned long flags, cpuflags;
    unsigned char status;
    unsigned char buffer;
    unsigned int chip_address = address - FLASH_BASE + tffs_mtd_offset[mtd_id];

    if(tffs_mtd_offset[mtd_id] == 0) {
        /*--- falls tffs_mtd_offset noch nicht initialisiert: Urlader nicht ueberschreiben! ---*/
        return 0;
    }
    spi_lock(&flags, &cpuflags);
    spi_cmd_simple(SPI_WRITE_ENABLE);
    DBG_SPI("[spi_write_byte] status 0x%x\n", spi_read_status(IFX_SSC_CSx));

    SPI->CON.Bits.rxoff = 1;

	spi_cs(IFX_SSC_CSx, select);
    SPI->TB.Register = SPI_PAGE_PROGRAM | chip_address;
    if(spi_busy_wait(1000)) {
        spi_cs(IFX_SSC_CSx, deselect);
        spi_unlock(flags, cpuflags);
        printk(KERN_ERR"%s: error SPI busy %d\n", __func__, __LINE__);
        return 0;
    }
    SPI->TB.Char.Byte = *pdata;
    if(spi_busy_wait(1000)) {
        spi_cs(IFX_SSC_CSx, deselect);
        spi_unlock(flags, cpuflags);
        printk(KERN_ERR"%s: error SPI busy %d\n", __func__, __LINE__);
        return 0;
    }
	spi_cs(IFX_SSC_CSx, deselect);

    SPI->CON.Bits.rxoff = 0;

    while (1) {
        status = spi_read_status(IFX_SSC_CSx);
        if (!(status & WIP))
            break;
    }
    spi_unlock(flags, cpuflags);

    tffs_spi_read(address, mtd_id, &buffer, 1);
    if (buffer != *pdata) {
        printk(KERN_ERR"\n<Flash>: B Error: Addr 0x%x should=0x%x read=0x%x\n", address, *pdata, buffer);
        return 0;
    }
    return 1;
    
}
/*------------------------------------------------------------------------------------------*\
 * wir schreiben hier die Daten immer 32 Bit weise
\*------------------------------------------------------------------------------------------*/
unsigned int spi_write_block(unsigned int address, unsigned int mtd_id, unsigned char *data, unsigned int datalen) {
    unsigned long flags, cpuflags;
    unsigned char buffer[FLASH_BUFFER_SIZE];
    unsigned int *pData;
    unsigned int SectorSize = spi_GetBlockSize(address + tffs_mtd_offset[mtd_id]);
    unsigned int  BufferSize, cnt = 10000;
    enum spi_cs   cs = IFX_SSC_CSx;
    int i, status = 0;
    unsigned int chip_address = address - FLASH_BASE + tffs_mtd_offset[mtd_id];

    if(tffs_mtd_offset[mtd_id] == 0) {
        /*--- falls tffs_mtd_offset noch nicht initialisiert: Urlader nicht ueberschreiben! ---*/
        return 0;
    }
    if (tffs_spi_device.Blockmode)
        BufferSize = tffs_spi_device.Geometry.writeBuffer_size;
    else
        BufferSize = sizeof(unsigned short);

    if (!SectorSize) {
        printk(KERN_ERR"[%s] Error: <sector_size == 0 Addr 0x%x>\n", __FUNCTION__, chip_address);
        return 0;
    }

    if ((datalen > BufferSize) || (datalen == 0)) {
        printk(KERN_ERR"Error: <len %d> BufferSize %d>\n", datalen, BufferSize);
        return 0;
    }
        
    if ((unsigned int)data & 3) {
        memset(buffer, 0xff, FLASH_BUFFER_SIZE);
        memcpy(buffer, data, datalen);         /*--- die Daten aligned in den Buffer kopieren ---*/
        pData = (unsigned int *)&buffer;
    } else {
        pData = (unsigned int *)data;
    }

    /*--- wir dürfen nicht über eine Sectorgrenze schreiben ---*/
    if ((chip_address % SectorSize) && (datalen > (chip_address % SectorSize))) {
        datalen -= chip_address % SectorSize;
    }

    /*--- nicht über die Grenze des Writebuffers schreiben ---*/
    if ((chip_address % BufferSize) && (datalen > (BufferSize - (chip_address % BufferSize)))) {
        datalen = (BufferSize - (chip_address % BufferSize));
    }
    spi_lock(&flags, &cpuflags);
    spi_cmd_simple(SPI_WRITE_ENABLE);

    SPI->CON.Bits.rxoff = 1;

	spi_cs(cs, select);
    SPI->TB.Register = SPI_PAGE_PROGRAM | chip_address;
    if(spi_busy_wait(1000)) {
        spi_cs(IFX_SSC_CSx, deselect);
        printk(KERN_ERR"%s: error SPI busy %d\n", __func__, __LINE__);
        return 0;
    }
    for (i=0;i<(datalen>>2);i++) {
        /*--- DBG_SPI("[spi_write_block] 0x%x\n", *(unsigned int *)pData); ---*/
        SPI->TB.Register = *(unsigned int *)pData;
        pData++;
        if(spi_busy_wait(1000)) {
            spi_cs(IFX_SSC_CSx, deselect);
            spi_unlock(flags, cpuflags);
            printk(KERN_ERR"%s: error SPI busy %d\n", __func__, __LINE__);
            return 0;
        }
    }

	spi_cs(cs, deselect);
    SPI->CON.Bits.rxoff = 0;

    for (;;) {
        status = spi_read_status(cs);
        if (!(status & WIP) && !(status & WEL))
            break;
        if(cnt-- == 0) {
            spi_unlock(flags, cpuflags);
            printk(KERN_ERR"%s:%d error on status %x  chip_addr=%x datalen=%u\n", __func__, __LINE__, status, chip_address, datalen);
            return 0;
        }
        udelay(1);
    }
    spi_unlock(flags, cpuflags);

    /*--- zum Schluss noch alles Vergleichen ---*/
    tffs_spi_read(address, mtd_id, buffer, datalen);
    if (memcmp((char *)buffer, (char *)data, datalen)) {
#if 0
        for (i=0;i<datalen;i++)
            printk("0x%x ", buffer[i]);
        printk("\n\n");
        for (i=0;i<datalen;i++)
            printk("0x%x ", data[i]);
        printk("\n\n");
#endif
        printk(KERN_ERR"<FlashBlock>: %d Error: Addr 0x%x should [0] = 0x%x read [0] = 0x%x\n", 
                datalen, address, data[0], buffer[0]);
        return 0;
    }
    DBG_SPI("[spi_write_block] wrote %d\n", datalen);
    return datalen;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int tffs_spi_write(unsigned int address, unsigned int mtd_id, unsigned char *pdata, unsigned int len) {

    unsigned int Bytes = 0;
    unsigned int written, write;
    unsigned int  buffersize = tffs_spi_device.Geometry.writeBuffer_size;

    DBG_SPI("[tffs_spi_write] address 0x%x mtd %d len %d pdata 0x%p buffersize=%d\n", address, mtd_id, len, pdata, buffersize);
    while (Bytes < len) {
        if ((address & 3) || ((len - Bytes) < 4)) {     /*--- byteweise schreiben ---*/
            DBG_SPI("[tffs_spi_write] Bytes addr 0x%x pdata 0x%p\n", address, pdata);
            written = tffs_spi_write_byte(address, mtd_id, pdata);
        } else {
            write = (len - Bytes) > buffersize ? buffersize : (len - Bytes);
            write &= ~3;
            DBG_SPI("[tffs_spi_write] Integer 0x%x len %d\n", address, write);

            written = spi_write_block(address, mtd_id, pdata, write);
        }
        address += written;
        pdata += written;
        Bytes += written;

        if (written == 0) {
            printk(KERN_ERR"[%s] <ERROR: Flash write aborted 0x%x>\n", __FUNCTION__, (int)address);
            break;
        }
    }
    return Bytes;
}

/*------------------------------------------------------------------------------------------*\
 * initialize SSC1 module  
 * Parameter:	baudrate FPI/clock ~ FPI/clock/2^16
\*------------------------------------------------------------------------------------------*/
void spi_init(unsigned int baudrate) {
    unsigned int rmc, ssc_clock, br;
	/* enable SSC1 */
	//*AMAZON_S_PMU_PM_GEN |= AMAZON_S_PMU_PM_GEN_EN11;

    if(down_trylock(&ifx_sflash_sem)) {
        return;
    }


	/* SSC0 Ports */
	/* P0.10/P0.15 as CS4/CS1 for flash or eeprom depends on jumper */
    /* P0.10 ALT0= 0, ALT1=1, DIR=1 */
	*(IFX_GPIO_P0_DIR)     |= BIT(SPI_CSx);
#if defined(CONFIG_VR9)
	*(IFX_GPIO_P0_ALTSEL0) &= ~ BIT(SPI_CSx);
	*(IFX_GPIO_P0_ALTSEL1) |= BIT(SPI_CSx);
#elif defined(CONFIG_AR10)/*--- #if defined(CONFIG_VR9) ---*/
	*(IFX_GPIO_P0_ALTSEL0) |= BIT(SPI_CSx);
	*(IFX_GPIO_P0_ALTSEL1) &= ~BIT(SPI_CSx);
#else 
#error unknown platform
#endif
	*(IFX_GPIO_P0_OD)      |= BIT(SPI_CSx);	

	/* p1.0 SPI_DIN, p1.1 SPI_DOUT, p1.2 SPI_CLK */
	*(IFX_GPIO_P1_DIR)     |=  (BIT(SPI_DOUT) | BIT(SPI_CLK));
    *(IFX_GPIO_P1_DIR)     &= ~(BIT(SPI_DIN));
	*(IFX_GPIO_P1_ALTSEL0) |=  (BIT(SPI_DOUT) | BIT(SPI_CLK) | BIT(SPI_DIN));
	*(IFX_GPIO_P1_ALTSEL1) &= ~(BIT(SPI_DOUT) | BIT(SPI_CLK) | BIT(SPI_DIN));
	*(IFX_GPIO_P1_OD)      |=  (BIT(SPI_DOUT) | BIT(SPI_CLK));	

	/* Clock Control Register */ /* DISS OFF and RMC = 1 */
	/* Disable SSC to get access to the control bits */
	SPI->WHBSTATE = 1;          /*--- Disable SSC ; ---*/
	asm("SYNC");	
    rmc = SPI->CLC >> 8;
    if(rmc == 0) {
        rmc = 1;
        SPI->CLC = (rmc << 8);
    }
	/*CSx */
	SPI->GPOCON = (1<<(IFX_SSC_CSx+8));

	spi_cs(IFX_SSC_CSx, deselect);
	/* disable IRQ */
	SPI->IRNEN = 0;             /*--- *AMAZON_S_SSC1_IRNEN = 0x0; ---*/
   
    /*------------------------------------------------------------------------------------------*\
     * Set the Baudrate 
     *	  BR = (FPI clk / (2 * Baudrate)) - 1 
     *	  Note: Must be set while SSC is disabled!
    \*------------------------------------------------------------------------------------------*/
    ssc_clock =  ifx_get_fpi_hz() / rmc;
    br = (((ssc_clock >> 1) + baudrate / 2) / baudrate) - 1;
    DBG_SPI(KERN_ERR"{%s} baudrate %d ssc_clock=%d br=%d CGU_CLK=0x%x\n", __FUNCTION__, baudrate, ssc_clock, br, *IFX_CGU_IF_CLK);

    SPI->BRT = br;

	/*enable and flush RX/TX FIFO*/
    SPI->RXFCON.Register = (0xF<<8)|(1<<1)|(1<<0);
    SPI->TXFCON.Register = (0xF<<8)|(1<<1)|(1<<0);

	/* set CON, TX off , RX off, ENBV=0, BM=7(8 bit valid) HB=1(MSB first), PO=0,PH=1(SPI Mode 0)*/
	SPI->CON.Register = CON_ENBV | CON_PH | CON_HB | CON_RXOFF | CON_TXOFF;  /*--- 0x00070033 ---*/
	asm("SYNC");

	/*Set Master mode and  Enable SSC */
	SPI->WHBSTATE = (1<<3) | (1<<1);
	asm("SYNC");
	
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int tffs_spi_init(void) {
    unsigned long flags, cpuflags;
    unsigned int flash_data;

    spi_lock(&flags, &cpuflags);

    spi_init(SFLASH_BAUDRATE);

    SPI->CON.Bits.rxoff = 0;
    SPI->CON.Bits.txoff = 0;

	spi_cs(IFX_SSC_CSx, select);
    SPI->TB.Register = SPI_READ_ELECTRONIC_ID_MANUFACTURE << 24;
    while (SPI->STAT.Bits.rxbv < 4);
	spi_cs(IFX_SSC_CSx, deselect);
    
    flash_data = SPI->RB;
    spi_read_status(IFX_SSC_CSx);

    spi_unlock(flags, cpuflags);
    DBG_SPI(KERN_INFO"[%s] FlashID 0x%x\n", __FUNCTION__, flash_data & 0xFFFFFF);

    switch (flash_data & 0x00FFFF00) {
        case ((MX_MANUFACTURE_ID << 16) + (MX_MEMORY_TYPE_SPI << 8)):
            DBG_SPI(KERN_INFO"[%s] Manufacturer MX\n", __FUNCTION__);
            tffs_spi_device.Size = 1<<(flash_data & 0xFF);
            tffs_spi_device.Geometry.writeBuffer_size = FLASH_BUFFER_SIZE;
            tffs_spi_device.Geometry.num_erase_regions = 1;
            tffs_spi_device.Geometry.erase_regions[0].blk_size = 0x10000;
            tffs_spi_device.Geometry.erase_regions[0].size = tffs_spi_device.Size;
            tffs_spi_device.Geometry.erase_regions[0].blk_num = tffs_spi_device.Size / 0x10000;
            tffs_spi_device.Blockmode = 1;
            break;
        case ((SPANSION_MANUFACTURE_ID << 16) + ( SPANSION_MEMORY_TYPE_SPI << 8)):
            DBG_SPI(KERN_INFO"[%s] Manufacturer SPANSION\n", __FUNCTION__);
            tffs_spi_device.Size = 1<<(flash_data & 0xFF);
            tffs_spi_device.Geometry.writeBuffer_size = FLASH_BUFFER_SIZE;
            tffs_spi_device.Geometry.num_erase_regions = 1;
            tffs_spi_device.Geometry.erase_regions[0].blk_size = 0x10000;
            tffs_spi_device.Geometry.erase_regions[0].size = tffs_spi_device.Size;
            tffs_spi_device.Geometry.erase_regions[0].blk_num = tffs_spi_device.Size / 0x10000;
            tffs_spi_device.Blockmode = 1;
            break;
        default:
            DBG_SPI(KERN_ERR"[init_spi] <no valid flash detected>\n");
            return FLASH_ID_FAILED;
    }
    return FLASH_SUCCESS;

}


