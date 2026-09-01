/******************************************************************************
**
** FILE NAME    : ifxmips_nand.c
** PROJECT      : UEIP
** MODULES      : NAND Flash
**
** DATE         : 23 Apr 2005
** AUTHOR       : Wu Qi Ming
** DESCRIPTION  : NAND Flash MTD Driver
** COPYRIGHT    :       Copyright (c) 2006
**                      Infineon Technologies AG
**                      Am Campeon 1-12, 85579 Neubiberg, Germany
**
**    This program is free software; you can redistribute it and/or modify
**    it under the terms of the GNU General Public License as published by
**    the Free Software Foundation; either version 2 of the License, or
**    (at your option) any later version.
**
** HISTORY
** $Date        $Author      $Version   $Comment
** 23 Apr 2008  Wu Qi Ming   1.0        initial version
** 7  Aug 2009  Yin Elaine   1.1        Modification for UEIP project
*******************************************************************************/

#ifndef AUTOCONF_INCLUDED
#include <linux/config.h>
#endif /* AUTOCONF_INCLUDED */

/*!
  \defgroup IFX_NAND_DRV UEIP Project - nand flash driver
  \brief UEIP Project - Nand flash driver, supports LANTIQ CPE platforms(Danube/ASE/ARx/VRx).
 */

/*!
  \defgroup IFX_NAND_DRV_API External APIs
  \ingroup IFX_NAND_DRV
  \brief External APIs definitions for other modules.
 */

/*!
  \defgroup IFX_NAND_DRV_STRUCTURE Driver Structures
  \ingroup IFX_NAND_DRV
  \brief Definitions/Structures of nand module.
 */

/*!
  \file ifxmips_mtd_nand.h
  \ingroup IFX_NAND_DRV
  \brief Header file for LANTIQ nand driver
 */

/*!
  \file ifxmips_mtd_nand.c
  \ingroup IFX_NAND_DRV
  \brief nand driver main source file.
*/
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <asm/io.h>


/* Project header */
#include <ifx_types.h>
#include <ifx_regs.h>
#include <ifx_gpio.h>
#include <ifx_pmu.h>
#include <common_routines.h>


#include "ifxmips_mtd_nand.h"

/*--- #define DBG_NAND(args...)            printk(args) ---*/
#define DBG_NAND(args...)

#define IFX_MTD_NAND_BANK_NAME      "ifx_nand"   /* cmd line bank name should be the same */


#define WRITE_NAND_COMMAND(d)   NAND_WRITE(NAND_WRITE_CMD,  (d));
#define WRITE_NAND_ADDRESS(d)   NAND_WRITE(NAND_WRITE_ADDR, (d));
#define WRITE_NAND(d)           NAND_WRITE(NAND_WRITE_DATA, (d))
#define READ_NAND               *((volatile u8*)(NAND_BASE_ADDRESS | (NAND_READ_DATA)))

#define RESET_CHIP()    { WRITE_NAND_COMMAND(NAND_WRITE_CMD_RESET); NAND_WAIT_READY(this); mb();} 

/* the following are NOP's in our implementation */
#define NAND_CTL_CLRALE(nandptr)
#define NAND_CTL_SETALE(nandptr)
#define NAND_CTL_CLRCLE(nandptr)
#define NAND_CTL_SETCLE(nandptr)

/*
 * MTD structure for NAND controller
 */
static struct mtd_info *ifx_nand_mtd = NULL;
static u32 latchcmd=0;

#ifdef CONFIG_MTD_PARTITIONS
#ifdef CONFIG_MTD_CMDLINE_PARTS
static const char *part_probes[] = { "cmdlinepart", NULL };
#endif //ifdef CONFIG_MTD_CMDLINE_PARTS
#endif //ifdef CONFIG_MTD_PARTITIONS

/* Partition table, defined in platform_board.c */
extern struct mtd_partition ifx_nand_partitions[IFX_MTD_NAND_PARTS];


static inline void NAND_DISABLE_CE(struct nand_chip *nand) { IFX_REG_W32_MASK(NAND_CON_CE,0,IFX_EBU_NAND_CON); }
static inline void NAND_ENABLE_CE(struct nand_chip *nand)  { IFX_REG_W32_MASK(0,NAND_CON_CE,IFX_EBU_NAND_CON); }

static void __exit ifx_nand_exit(void);


#if !defined(RDBY_NOT_USED)
static void NAND_WAIT_READY(struct nand_chip *nand) 
{
    while(!NAND_READY){} 
}

static int ifx_nand_ready(struct mtd_info *mtd)
{
      struct nand_chip *nand = mtd->priv;

      NAND_WAIT_READY(nand);
      return 1;
}
#endif

/*!
  \fn u_char ifx_nand_read_byte(struct mtd_info *mtd)
  \ingroup  IFX_NAND_DRV
  \brief read one byte from the chip, read function for 8bit buswith
  \param  mtd  MTD device structure
  \return value of the byte
*/ 
static u_char ifx_nand_read_byte(struct mtd_info *mtd)
{
      //struct nand_chip *nand = mtd->priv;
      u_char ret;
      
      asm("sync");
      NAND_READ(NAND_READ_DATA, ret);
      asm("sync");
      return ret;
}


/*!
  \fn void ifx_nand_select_chip(struct mtd_info *mtd, int chip)
  \ingroup  IFX_NAND_DRV
  \brief control CE line 
  \param  mtd MTD device structure
  \param  chipnumber to select, -1 for deselect
  \return none
*/ 
static void ifx_nand_select_chip(struct mtd_info *mtd, int chip)
{
      struct nand_chip *nand = mtd->priv;

        switch (chip) {
        case -1:
             NAND_DISABLE_CE(nand);
    		 IFX_REG_W32_MASK(IFX_EBU_NAND_CON_NANDM, 0, IFX_EBU_NAND_CON);
             break;
        case 0:
             IFX_REG_W32_MASK(0, IFX_EBU_NAND_CON_NANDM, IFX_EBU_NAND_CON);
             NAND_ENABLE_CE(nand);
             /*--- Schrott (macht bei jedem CS einen Reset was bei manchen NANDs halt laenger dauert.) =>: NAND_WRITE(NAND_WRITE_CMD, NAND_WRITE_CMD_RESET); // Reset nand chip ---*/
             break;

        default:
             BUG();
        }


}


/*!
  \fn void ifx_nand_read_buf(struct mtd_info *mtd, uint8_t *buf, int len)
  \ingroup  IFX_NAND_DRV
  \brief  read chip data into buffer
  \param  mtd   MTD device structure
  \param  buf   buffer to store date
  \param  len   number of bytes to read
  \return none
*/  
static void ifx_nand_read_buf(struct mtd_info *mtd, uint8_t *buf, int len)
{
        int i;
        //struct nand_chip *chip = mtd->priv;

        for (i = 0; i < len; i++)
                buf[i]=READ_NAND;
}


/*!
  \fn void ifx_nand_write_buf(struct mtd_info *mtd, const uint8_t *buf, int len)
  \ingroup  IFX_NAND_DRV
  \brief  write buffer to chip
  \param  mtd   MTD device structure
  \param  buf   data buffer
  \param  len   number of bytes to write
  \return none
*/   
static void ifx_nand_write_buf(struct mtd_info *mtd, const uint8_t *buf, int len)
{
        int i;
        //struct nand_chip *chip = mtd->priv;

        for (i = 0; i < len; i++)
               WRITE_NAND(buf[i]);
             // writeb(buf[i], chip->IO_ADDR_W);
}


 /*!
  \fn int ifx_nand_verify_buf(struct mtd_info *mtd, const uint8_t *buf, int len)
  \ingroup  IFX_NAND_DRV
  \brief  Verify chip data against buffer
  \param  mtd   MTD device structure
  \param  buf   buffer containing the data to compare
  \param  len   number of bytes to compare
  \return none
*/   
static int ifx_nand_verify_buf(struct mtd_info *mtd, const uint8_t *buf, int len)
{
        int i;
        //struct nand_chip *chip = mtd->priv;

        for (i = 0; i < len; i++)
                if (buf[i] != READ_NAND)
                        return -EFAULT;
        return 0;
}



/*!
  \fn void ifx_nand_cmd_ctrl(struct mtd_info *mtd, int data, unsigned int ctrl)
  \ingroup  IFX_NAND_DRV
  \brief  Hardware specific access to control-lines
  \param  mtd   MTD device structure
  \param  data  data to write to nand if necessary
  \param  ctrl  control value, refer to nand_base.c
  \return none
*/    
 
static void ifx_nand_cmd_ctrl(struct mtd_info *mtd, int data, unsigned int ctrl)
{
    struct nand_chip *this = mtd->priv;

    if (ctrl & NAND_CTRL_CHANGE){
         if(ctrl & NAND_CLE) latchcmd=NAND_WRITE_CMD;
	 else if(ctrl & NAND_ALE) latchcmd=NAND_WRITE_ADDR;
       }

    if(data != NAND_CMD_NONE){
	    *(volatile u8*)((u32)this->IO_ADDR_W | latchcmd)=data;
        while((IFX_REG_R32(IFX_EBU_NAND_WAIT) & IFX_EBU_NAND_WAIT_WR_C) == 0);
    }
    return;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static struct nand_ecclayout nand_oob_64_NANDwithHWEcc_FAILURE = {
    .eccbytes = 32,
    .eccpos = {
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
        0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
        0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
        0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f},
    .oobfree = {
        {.offset = 0x04, .length = 4},
        {.offset = 0x14, .length = 4},
        {.offset = 0x24, .length = 4},
        {.offset = 0x34, .length = 4}
    }
};
static struct nand_ecclayout nand_oob_64_NANDwithHWEcc = {
    .oobfree = {
        {.offset = 0x04, .length = 4},
        {.offset = 0x14, .length = 4},
        {.offset = 0x24, .length = 4},
        {.offset = 0x34, .length = 4}
    }
};

struct _nand_probe_register_status_save {
    unsigned int ifx_ebu_addsel0;
    unsigned int ifx_ebu_addsel1;
    unsigned int ifx_ebu_nand_con;
    unsigned int ifx_ebu_buscon0;
} nand_probe_register_status_save;

/*!
  \fn void ifx_nand_chip_init(void)
  \ingroup  IFX_NAND_DRV
  \brief  platform specific initialization routine
  \param  none
  \return none
*/    
static void ifx_nand_chip_init(void)
{
    u32 reg;

    if(ifx_gpio_register(IFX_GPIO_MODULE_NAND) != IFX_SUCCESS) {
        panic("[NAND]%s: ifx_gpio_register(IFX_GPIO_MODULE_NAND) failed\n", __func__);
    }
    EBU_PMU_SETUP(IFX_EBU_ENABLE);

#if !defined(CONFIG_AR10)
    /*P1.7 FL_CS1 used as output*/
    ifx_gpio_pin_reserve(IFX_NAND_CS1, IFX_GPIO_MODULE_NAND);
    ifx_gpio_dir_out_set(IFX_NAND_CS1, IFX_GPIO_MODULE_NAND);
    ifx_gpio_altsel0_set(IFX_NAND_CS1, IFX_GPIO_MODULE_NAND);
    ifx_gpio_altsel1_clear(IFX_NAND_CS1, IFX_GPIO_MODULE_NAND);
    ifx_gpio_open_drain_set(IFX_NAND_CS1, IFX_GPIO_MODULE_NAND);
    
	/*P1.8 FL_A23 NAND_CLE used as output*/
    ifx_gpio_pin_reserve(IFX_NAND_CLE, IFX_GPIO_MODULE_NAND);
    ifx_gpio_dir_out_set(IFX_NAND_CLE, IFX_GPIO_MODULE_NAND);
    ifx_gpio_altsel0_set(IFX_NAND_CLE, IFX_GPIO_MODULE_NAND);
    ifx_gpio_altsel1_clear(IFX_NAND_CLE, IFX_GPIO_MODULE_NAND);
    ifx_gpio_open_drain_set(IFX_NAND_CLE, IFX_GPIO_MODULE_NAND);

    /*P0.13 FL_A24 used as output, set GPIO 13 to NAND_ALE*/
    ifx_gpio_pin_reserve(IFX_NAND_ALE, IFX_GPIO_MODULE_NAND);
    ifx_gpio_dir_out_set(IFX_NAND_ALE, IFX_GPIO_MODULE_NAND);
    ifx_gpio_altsel0_set(IFX_NAND_ALE, IFX_GPIO_MODULE_NAND);
    ifx_gpio_altsel1_clear(IFX_NAND_ALE, IFX_GPIO_MODULE_NAND);
    ifx_gpio_open_drain_set(IFX_NAND_ALE, IFX_GPIO_MODULE_NAND);
    
#if defined(CONFIG_VR9) || defined(CONFIG_AR9)												
    /*P3.0 set as NAND Read Busy*/
    ifx_gpio_pin_reserve(IFX_NAND_RDY, IFX_GPIO_MODULE_NAND);
    ifx_gpio_dir_in_set(IFX_NAND_RDY, IFX_GPIO_MODULE_NAND);
    ifx_gpio_altsel0_set(IFX_NAND_RDY, IFX_GPIO_MODULE_NAND);
    ifx_gpio_altsel1_clear(IFX_NAND_RDY, IFX_GPIO_MODULE_NAND);

	/*P3.1 set as NAND Read*/
    ifx_gpio_pin_reserve(IFX_NAND_RD, IFX_GPIO_MODULE_NAND);
    ifx_gpio_dir_out_set(IFX_NAND_RD, IFX_GPIO_MODULE_NAND);
    ifx_gpio_altsel0_set(IFX_NAND_RD, IFX_GPIO_MODULE_NAND);
    ifx_gpio_altsel1_clear(IFX_NAND_RD, IFX_GPIO_MODULE_NAND);
    ifx_gpio_open_drain_set(IFX_NAND_RD, IFX_GPIO_MODULE_NAND);
#endif
#endif /*--- #if !defined(CONFIG_AR10) ---*/

    nand_probe_register_status_save.ifx_ebu_addsel0 = IFX_REG_R32(IFX_EBU_ADDSEL0);
    nand_probe_register_status_save.ifx_ebu_addsel1 = IFX_REG_R32(IFX_EBU_ADDSEL1);

#if defined(CONFIG_NAND_CS1)
    reg = (NAND_BASE_ADDRESS & 0x1fffff00)| IFX_EBU_ADDSEL1_MASK(2)| IFX_EBU_ADDSEL1_REGEN;
    IFX_REG_W32(~IFX_EBU_ADDSEL1_REGEN, IFX_EBU_ADDSEL0);
    IFX_REG_W32(reg, IFX_EBU_ADDSEL1);

    reg = SM(IFX_EBU_NAND_CON_NANDM_ENABLE, IFX_EBU_NAND_CON_NANDM) |
          SM(IFX_EBU_NAND_CON_CSMUX_E_ENABLE,IFX_EBU_NAND_CON_CSMUX_E) |
          SM(IFX_EBU_NAND_CON_CS_P_LOW,IFX_EBU_NAND_CON_CS_P) |
          SM(IFX_EBU_NAND_CON_SE_P_LOW,IFX_EBU_NAND_CON_SE_P) |
          SM(IFX_EBU_NAND_CON_WP_P_LOW,IFX_EBU_NAND_CON_WP_P) |
          SM(IFX_EBU_NAND_CON_PRE_P_LOW,IFX_EBU_NAND_CON_PRE_P) |
          SM(IFX_EBU_NAND_CON_IN_CS1,IFX_EBU_NAND_CON_IN_CS) |
          SM(IFX_EBU_NAND_CON_OUT_CS1,IFX_EBU_NAND_CON_OUT_CS);
    IFX_REG_W32(reg,IFX_EBU_NAND_CON);         
     /* byte swap;minimum delay*/
    reg = IFX_EBU_BUSCON1_SETUP | 
          SM(IFX_EBU_BUSCON1_ALEC3,IFX_EBU_BUSCON1_ALEC) |
          SM(IFX_EBU_BUSCON1_BCGEN_RES,IFX_EBU_BUSCON1_BCGEN) |
          SM(IFX_EBU_BUSCON1_WAITWRC2,IFX_EBU_BUSCON1_WAITWRC) |
          SM(IFX_EBU_BUSCON1_WAITRDC2,IFX_EBU_BUSCON1_WAITRDC) |
          SM(IFX_EBU_BUSCON1_HOLDC1,IFX_EBU_BUSCON1_HOLDC) |
          SM(IFX_EBU_BUSCON1_RECOVC1,IFX_EBU_BUSCON1_RECOVC) |
          SM(IFX_EBU_BUSCON1_CMULT4,IFX_EBU_BUSCON1_CMULT);
    IFX_REG_W32(reg, IFX_EBU_BUSCON1);
#else
    reg = (NAND_BASE_ADDRESS & 0x1fffff00)| IFX_EBU_ADDSEL0_MASK(3)| IFX_EBU_ADDSEL0_REGEN;
    IFX_REG_W32(reg, IFX_EBU_ADDSEL0);
    reg = IFX_EBU_BUSCON1_SETUP | 
          SM(IFX_EBU_BUSCON1_ALEC3,IFX_EBU_BUSCON1_ALEC) |
          SM(IFX_EBU_BUSCON1_BCGEN_RES,IFX_EBU_BUSCON1_BCGEN) |
          SM(IFX_EBU_BUSCON1_WAITWRC2,IFX_EBU_BUSCON1_WAITWRC) |
          SM(IFX_EBU_BUSCON1_WAITRDC2,IFX_EBU_BUSCON1_WAITRDC) |
          SM(IFX_EBU_BUSCON1_HOLDC1,IFX_EBU_BUSCON1_HOLDC) |
          SM(IFX_EBU_BUSCON1_RECOVC1,IFX_EBU_BUSCON1_RECOVC) |
          SM(IFX_EBU_BUSCON1_CMULT4,IFX_EBU_BUSCON1_CMULT);
    IFX_REG_W32(reg, IFX_EBU_BUSCON0);

    reg = SM(IFX_EBU_NAND_CON_NANDM_ENABLE, IFX_EBU_NAND_CON_NANDM) |
          SM(IFX_EBU_NAND_CON_CSMUX_E_ENABLE,IFX_EBU_NAND_CON_CSMUX_E) |
          SM(IFX_EBU_NAND_CON_CS_P_LOW,IFX_EBU_NAND_CON_CS_P) |
          SM(IFX_EBU_NAND_CON_SE_P_LOW,IFX_EBU_NAND_CON_SE_P) |
          SM(IFX_EBU_NAND_CON_WP_P_LOW,IFX_EBU_NAND_CON_WP_P) |
          SM(IFX_EBU_NAND_CON_PRE_P_LOW,IFX_EBU_NAND_CON_PRE_P) |
          SM(IFX_EBU_NAND_CON_IN_CS0,IFX_EBU_NAND_CON_IN_CS) |
          SM(IFX_EBU_NAND_CON_OUT_CS0,IFX_EBU_NAND_CON_OUT_CS);
    IFX_REG_W32(reg,IFX_EBU_NAND_CON);         
#endif

    mb();

    /* Set bus signals to inactive */
    NAND_WRITE(NAND_WRITE_CMD, NAND_WRITE_CMD_RESET); // Reset nand chip
}

void ifx_nand_avm_check_for_hweccnand(struct nand_chip *this) {
#define PRINT_TO_SCREEN(...)    printk(KERN_ERR "[NAND] "); printk(__VA_ARGS__); printk("\n")
#define MAX_HWECC_TRIES         10

    // NAND-chip mehrmals resetten.
    //  -speziell der MT29F hat Probleme, nach einem Kaltstart den Dienst aufzunehmen
    {
        char i;
        
        for(i=0; i<50; i++) {
            RESET_CHIP();
        }
    }

    // NAND-Chip erkennen.
    // Wenn MT29F1
    //    HW-ECC des Chips nutzen, wenn das nicht geht -> software ECC mit fallback layout um die Daten noch lesen zu können
    //    (dann natürlich nicht mehr ECC geschützt)
    // sonst
    //    SW-ECC vom NAND-Treiber
    {
        unsigned char result = 0;
        unsigned char chip_maf_id;
        unsigned char chip_dev_id;

        WRITE_NAND_COMMAND(0x90); // Read id
        WRITE_NAND_ADDRESS(0x00);

        chip_maf_id = READ_NAND;
        chip_dev_id = READ_NAND;

        if(   (chip_maf_id == 0x2c)
           && (   chip_dev_id == 0xf1       // MT29F1G08ABADA
               || chip_dev_id == 0xdc)) {   // MT29F4G08ABADA
            int no_tries = 0;
try_again:
            // use setfeature option to turn on internal ecc
            WRITE_NAND_COMMAND(0xEF); // setFeature
            WRITE_NAND_ADDRESS(0x90); // array operation mode
            WRITE_NAND(0x08);
            WRITE_NAND(0x00);
            WRITE_NAND(0x00);
            WRITE_NAND(0x00);
            NAND_WAIT_READY(this);

            // use getfeature to read result
            WRITE_NAND_COMMAND(0xEE); // getFeature
            WRITE_NAND_ADDRESS(0x90); // array operation mode
            NAND_WAIT_READY(this);
            // read 4 bytes...
            result = READ_NAND;

            PRINT_TO_SCREEN("read feature bytes (after setting hw ecc): P1=0x%02x|P2=0x%02x|P3=0x%02x|P4=0x%02x",
                    result,
                    READ_NAND,
                    READ_NAND,
                    READ_NAND);
            result &= 0x08;

            if(!result) {
                no_tries++;
                if(no_tries < MAX_HWECC_TRIES) {
                    PRINT_TO_SCREEN("Could not activate HW ECC -> try: %u/%u",
                            no_tries, MAX_HWECC_TRIES);
                    RESET_CHIP();
                    goto try_again;
                }
            }

            if(!result) {
                this->ecc.layout = &nand_oob_64_NANDwithHWEcc_FAILURE;

                PRINT_TO_SCREEN("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
                PRINT_TO_SCREEN("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
                PRINT_TO_SCREEN("!! MT29F1/MT29F4 erkannt, konnte aber den Hardware-ECC nicht aktivieren -> nutze SW-ECC !!");
                PRINT_TO_SCREEN("!!   -Fallback-Layout wird genutzt, welches ein Lesen der Daten erlaubt                 !!");
                PRINT_TO_SCREEN("!!   -YAFFS wird trotzdem massenhaft Warnungen auswerfen -> ECC stimmt halt nicht       !!");
                PRINT_TO_SCREEN("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
                PRINT_TO_SCREEN("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
            } else {
                PRINT_TO_SCREEN("Hardware-ECC activated");
                this->ecc.layout = &nand_oob_64_NANDwithHWEcc;
                this->ecc.mode = NAND_ECC_NONE;

                // Option, damit der generische nand-Part weiß, dass er nach einer Leseoperation noch
                // den Status auf ECC-Fehler abfragen muss
                this->options |= NAND_FLASH_HWECC;
            }
        }
    }
}

/*------------------------------------------------------------------------------------------*\
 * hbl: start
 * Section mismatch, weil __init eine __exit funktion aufgerufen hat
\*------------------------------------------------------------------------------------------*/
static inline void ifx_nand_cleanup(void)
{
    /* Release resources, unregister device */
    nand_release (ifx_nand_mtd);
    ifx_gpio_deregister(IFX_GPIO_MODULE_NAND);
    /* Free the MTD device structure */
    kfree (ifx_nand_mtd);
}
/*------------------------------------------------------------------------------------------*\
 * hbl: end
\*------------------------------------------------------------------------------------------*/

/*
 * Main initialization routine
 */
 
/*!
  \fn int ifx_nand_init(void)
  \ingroup  IFX_NAND_DRV
  \brief  Main initialization routine
  \param  none
  \return error message
*/     
static int __init ifx_nand_init(void)
{
        struct nand_chip *this;
#if defined(CONFIG_MTD_PARTITIONS) && defined(CONFIG_MTD_CMDLINE_PARTS)
        struct mtd_partition *mtd_parts = 0;
#endif 

        int retval = 0;

        extern unsigned long long ifxmips_flashsize_nand;

        printk(KERN_ERR "[NAND] nand_size = 0x%llx" , ifxmips_flashsize_nand);
        if(ifxmips_flashsize_nand == 0ULL) {
            printk(KERN_ERR "ifx_nand_init: no NAND\n");
            return -ENXIO;
        }
        DBG_NAND(KERN_ERR "[NAND]%s: scan for NAND size=0x%llx\n", __func__, ifxmips_flashsize_nand);

        ifx_nand_chip_init();

        /* Allocate memory for MTD device structure and private data */
        ifx_nand_mtd = kmalloc(sizeof(struct mtd_info) + sizeof(struct nand_chip), GFP_KERNEL);
        if (!ifx_nand_mtd) {
                printk("Unable to allocate NAND MTD dev structure.\n");
                return -ENOMEM;
        }

        /* Get pointer to private data */
        this = (struct nand_chip *)(&ifx_nand_mtd[1]);

        /* Initialize structures */
        memset(ifx_nand_mtd, 0, sizeof(struct mtd_info));
        memset(this, 0, sizeof(struct nand_chip));

        ifx_nand_mtd->name = (char *) kmalloc (16, GFP_KERNEL);
        if (ifx_nand_mtd->name == NULL) {
            retval = -ENOMEM;
            goto out;
        }
        memset ((void *) ifx_nand_mtd->name, 0, 16);

        sprintf ((char *)ifx_nand_mtd->name, IFX_MTD_NAND_BANK_NAME);
        
        
        /* Link the private data with the MTD structure */
        ifx_nand_mtd->priv = this;
        ifx_nand_mtd->owner = THIS_MODULE;

         /* insert callbacks */
        this->IO_ADDR_R = (void *)NAND_BASE_ADDRESS;
        this->IO_ADDR_W = (void *)NAND_BASE_ADDRESS;
        this->cmd_ctrl = ifx_nand_cmd_ctrl;

#if defined(CONFIG_AR9)
        this->options|=NAND_USE_FLASH_BBT;
#endif
        /* 30 us command delay time */
        this->chip_delay = 30;
        this->ecc.mode = NAND_ECC_SOFT;

        this->read_byte=ifx_nand_read_byte;
        this->read_buf=ifx_nand_read_buf;
        this->write_buf=ifx_nand_write_buf;
        this->verify_buf=ifx_nand_verify_buf;
#if !defined(RDBY_NOT_USED)	    
        this->dev_ready=ifx_nand_ready;
#endif  
        this->select_chip=ifx_nand_select_chip;

        ifx_nand_avm_check_for_hweccnand(this);
        
        /* Scan to find existence of the device */
        printk("Probe for NAND flash...\n");
        if (nand_scan(ifx_nand_mtd, 1)) {
                retval = -ENXIO;
                goto out;
        }

#ifdef CONFIG_MTD_PARTITIONS
#ifdef CONFIG_MTD_CMDLINE_PARTS 
    int n=0;
    struct mtd_partition *mtd_parts=NULL;
    /*
     * Select dynamic from cmdline partition definitions
     */
     n= parse_mtd_partitions(ifx_nand_mtd, part_probes, &mtd_parts, 0);

     if (n<= 0) {
			kfree(ifx_nand_mtd);
            return retval;
		 }
     add_mtd_partitions(ifx_nand_mtd, mtd_parts,n);
		 
#else
     retval=add_mtd_partitions(ifx_nand_mtd, ifx_nand_partitions, IFX_MTD_NAND_PARTS);
#endif //  CONFIG_MTD_CMDLINE_PARTS  	 
#else
       retval=add_mtd_device(ifx_nand_mtd);
#endif //CONFIG_MTD_PARTITIONS

out:
    if(retval < 0) {
        ifx_nand_cleanup();
    }
    return retval;
}

static void __exit ifx_nand_exit(void)
{
    ifx_nand_cleanup();
}


module_init(ifx_nand_init);
module_exit(ifx_nand_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Wu Qi Ming");
MODULE_DESCRIPTION("NAND driver for IFX");
