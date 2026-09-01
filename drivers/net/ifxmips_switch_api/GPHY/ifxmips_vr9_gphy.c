/****************************************************************************
                               Copyright  2009
                            Infineon Technologies AG
                     Am Campeon 1-12; 81726 Munich, Germany
  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

 *****************************************************************************

   \file ifxmips_vr9_gphy.c
   \remarks implement GPHY driver on ltq platform
 *****************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h> /* printk() */
#include <linux/moduleparam.h>
#include <linux/types.h>  /* size_t */
#include <linux/netdevice.h>   /* struct device, and other headers */
#include <linux/ethtool.h>
#include <linux/mii.h>
#include <linux/if_ether.h>
#include <linux/etherdevice.h> /* eth_type_trans */
#include <asm/delay.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <linux/proc_fs.h>
#include <asm/io.h>
#include <asm/irq.h>

#include <asm/ifx/ifx_types.h>
#include <asm/ifx/ifx_regs.h>
#include <asm/ifx/common_routines.h>
#include <asm/ifx/ifx_pmu.h>
#include <asm/ifx/ifx_rcu.h>
#include <asm/ifx/ifx_pmu.h>
#include <asm/ifx/ifx_gpio.h>
#include <ifx_ethsw_core_platform.h>
//#if ( defined(CONFIG_VR9) || defined(CONFIG_AR10) || defined(CONFIG_HN1) )
#if ( defined(CONFIG_AR10) || defined(CONFIG_HN1) )
#if defined(CONFIG_GE_MODE)
	#include "gphy_fw_ge.h"
#endif 
#if defined(CONFIG_FE_MODE)
	#include "gphy_fw_fe.h"
#endif
#endif /* defined(CONFIG_VR9) || defined(CONFIG_AR10) || defined(CONFIG_HN1) */
#if  defined(CONFIG_AR10) 
#include <asm/ifx/ar10/ar10.h>
#endif

#if defined(CONFIG_GE_MODE)
	#define IFX_GPHY_MODE                   "GE Mode"
#endif /*CONFIG_GE_MODE*/

#if defined(CONFIG_FE_MODE)
	#define IFX_GPHY_MODE                   "FE Mode"
#endif /*CONFIG_FE_MODE*/
#define	FW_GE_MODE					0
#define	FW_FE_MODE					1
#define IFX_DRV_MODULE_NAME             "ifxmips_vr9_gphy"
#define IFX_DRV_MODULE_VERSION          "1.0.1"
#define PROC_DATA_LENGTH                12

static char version[]   __devinitdata = IFX_DRV_MODULE_NAME ": V" IFX_DRV_MODULE_VERSION "";
static char ver[]       __devinitdata = ":v" IFX_DRV_MODULE_VERSION "";
#define GPHY_FW_LEN		(64 * 1024)
#define GPHY_FW_LEN_D	(128 * 1024)
static unsigned char gphy_fw_dma[GPHY_FW_LEN_D];

//GPHY register GFS_ADD0 requires 16Kb starting address alignment boundary
//     It was set to 64Kb, but 16Kb works fine
#define GPHY_FW_START_ADDR_ALIGN        0x4000 
#define GPHY_FW_START_ADDR_ALIGN_MASK   (GPHY_FW_START_ADDR_ALIGN-1)
#if defined(CONFIG_VR9)
static unsigned char proc_gphy_fw_dma[GPHY_FW_LEN_D];
#define	IH_MAGIC_NO	0x27051956	/* Image Magic Number		*/
#define IH_NMLEN		32	/* Image Name Length		*/
/*
 * Legacy format image header,
 * all data in network byte order (aka natural aka bigendian).
 */
typedef struct gphy_image_header {
	uint32_t	ih_magic;	/* Image Header Magic Number	*/
	uint32_t	ih_hcrc;	/* Image Header CRC Checksum	*/
	uint32_t	ih_time;	/* Image Creation Timestamp	*/
	uint32_t	ih_size;	/* Image Data Size		*/
	uint32_t	ih_load;	/* Data	 Load  Address		*/
	uint32_t	ih_ep;		/* Entry Point Address		*/
	uint32_t	ih_dcrc;	/* Image Data CRC Checksum	*/
	uint8_t		ih_os;		/* Operating System		*/
	uint8_t		ih_arch;	/* CPU architecture		*/
	uint8_t		ih_type;	/* Image Type			*/
	uint8_t		ih_comp;	/* Compression Type		*/
	uint8_t		ih_name[IH_NMLEN];	/* Image Name		*/
} gphy_image_header_t;
#endif /* CONFIG_VR9 */
struct proc_data_t {
   char name[PROC_DATA_LENGTH + 1];
   char gphyNum[PROC_DATA_LENGTH + 1];
   char value[PROC_DATA_LENGTH + 1];
};
int ltq_gphy_firmware_config(int total_gphys, int mode, int dev_id, int ge_fe_mode );
static struct proc_dir_entry*   ifx_gphy_dir=NULL;
static int dev_id, total_gphys,ge_fe_mode ; 

void inline ifxmips_mdelay( int delay){
	int i;
	for ( i=delay; i > 0; i--)
		udelay(1000);
}

int vr9_gphy_pmu_set(void)
{
	/* Config GPIO3 clock */
	SWITCH_PMU_SETUP(IFX_PMU_ENABLE);
	GPHY_PMU_SETUP(IFX_PMU_ENABLE);
	return 0;
}

void ltq_write_mdio(unsigned int phyAddr, unsigned int regAddr,unsigned int data )
{
    unsigned int reg;
    reg = SW_READ_REG32(MDIO_CTRL_REG);
    while (reg & MDIO_CTRL_MBUSY ) {
        reg = SW_READ_REG32(MDIO_CTRL_REG);
    }
    reg = MDIO_READ_WDATA(data);
    SW_WRITE_REG32( reg, MDIO_WRITE_REG);
    reg = ( MDIO_CTRL_OP_WR | MDIO_CTRL_PHYAD_SET(phyAddr) | MDIO_CTRL_REGAD(regAddr) );
    reg |= MDIO_CTRL_MBUSY;
    SW_WRITE_REG32( reg, MDIO_CTRL_REG);
    udelay(100);
    reg = SW_READ_REG32(MDIO_CTRL_REG);
    while (reg & MDIO_CTRL_MBUSY ) {
        reg = SW_READ_REG32(MDIO_CTRL_REG);
        udelay(10);
    }
    ifxmips_mdelay(10);
    return ;
}

unsigned short ltq_read_mdio(unsigned int phyAddr, unsigned int regAddr )
{
	unsigned int reg;
    reg = SW_READ_REG32(MDIO_CTRL_REG);
    while (reg & MDIO_CTRL_MBUSY ) {
        reg = SW_READ_REG32(MDIO_CTRL_REG);
    }
	reg = ( MDIO_CTRL_OP_RD | MDIO_CTRL_PHYAD_SET(phyAddr) | MDIO_CTRL_REGAD(regAddr) );
    reg |= MDIO_CTRL_MBUSY;
    SW_WRITE_REG32( reg, MDIO_CTRL_REG);
    reg = SW_READ_REG32(MDIO_CTRL_REG);
    while (reg & MDIO_CTRL_MBUSY ) {
        reg = SW_READ_REG32(MDIO_CTRL_REG);
    }
    ifxmips_mdelay(10);
    reg = SW_READ_REG32(MDIO_READ_REG);
    return (MDIO_READ_RDATA(reg));
}

int ltq_gphy_reset(int phy_num)
{
	unsigned int reg;
	reg = ifx_rcu_rst_req_read();
	if ( phy_num == 3 ) 
		reg |= ( ( 1 << 31 ) | (1 << 29) | (1 << 28) );
	else if ( phy_num == 2 )
		reg |= ( 1 << 31 ) | (1 << 29);
	else if ( phy_num == 0 )
		reg |= (1 << 31);
	else if ( phy_num == 1 )
		reg |= (1 << 29);
	else
		return 1;
	ifx_rcu_rst_req_write(reg, reg);
	return 0;
}
 
int ltq_gphy_reset_released(int phy_num)
{
	unsigned int mask = 0;
	if (phy_num == 3)
		mask = (( 1 << 31 ) | (1 << 29) | ( 1<<28) );
	else if (phy_num == 2)
		mask = ( 1 << 31 ) | (1 << 29);
	else if ( phy_num == 0 )
		mask = ( 1 << 31 );
	else if ( phy_num == 1 )
		mask = ( 1 << 29 );
	else
		return 1;
	ifx_rcu_rst_req_write(0, mask);
	return 0;
}

/** Driver version info */
static inline int gphy_drv_ver(char *buf)
{
#if  (defined(CONFIG_VR9) || defined(CONFIG_HN1) )
	return sprintf(buf, "IFX GPHY driver %s, version %s - Firmware: %x\n", IFX_GPHY_MODE, version, ltq_read_mdio(0x11, 0x1e));
#endif
#if  defined(CONFIG_AR10)
	return sprintf(buf, "IFX GPHY driver %s, version %s - Firmware: %x\n", IFX_GPHY_MODE, version, ltq_read_mdio(0x2, 0x1e));
#endif
}

/** Driver version info for procfs */
static inline int gphy_ver(char *buf)
{
#if	 (defined(CONFIG_VR9) || defined(CONFIG_HN1) )
   return sprintf(buf, "IFX GPHY driver, version %s Firmware: %x\n", ver,  ltq_read_mdio(0x11, 0x1e));
#endif
#if  defined(CONFIG_AR10)
   return sprintf(buf, "IFX GPHY driver, version %s Firmware: %x\n", ver,  ltq_read_mdio(0x2, 0x1e));
#endif
}

/** Displays the version of GPHY module via proc file */
static int gphy_proc_version(IFX_char_t *buf, IFX_char_t **start, off_t offset,
                         int count, int *eof, void *data)
{
	int len = 0;
	/* No sanity check cos length is smaller than one page */
	len += gphy_ver(buf + len);
	*eof = 1;
	return len;
}
#if defined(CONFIG_VR9)
static int proc_read_gphy(char *page, char **start, off_t off, int count, int *eof, void *data)
{
    int len = 0;
    len += sprintf(page + len, "Not Implemented\n");
    *eof = 1;
    return len;
}

unsigned int found_magic = 0, found_img = 0, first_block =0, fw_len=0, rcv_size = 0, second_block = 0 ;
static int proc_write_gphy(struct file *file, const char *buf, unsigned long count, void *data)
{
	gphy_image_header_t header;
    int len = 0;
    char local_buf[4096];
    len = sizeof(local_buf) < count ? sizeof(local_buf) - 1 : count;
    len = len - copy_from_user((local_buf), buf, len);
   
	if (!found_img) {
		memcpy(&header, (local_buf + (found_magic * sizeof(gphy_image_header_t))), sizeof(gphy_image_header_t) );
		if (header.ih_magic == IH_MAGIC_NO) {
			found_magic++;
			first_block = 0;
			second_block = 0;
		}
	}
	if (!found_img) {
#if defined(CONFIG_GE_MODE)
		if( ( strncmp(header.ih_name, "VR9 V1.1 GPHY GE", sizeof(header.ih_name)) == 0) && (dev_id == 0)  )  {
			first_block = 1;
			fw_len = header.ih_size;
			found_img = 1;
//			printk("GPHY FIRMWARE LOAD SUCCESSFULLY AT ADDR (VR9)(GE MODE V11) \n" );
		} else if( ( strncmp(header.ih_name, "VR9 V1.2 GPHY GE", sizeof(header.ih_name)) == 0) && (dev_id == 1) ) {
			first_block = 1;
			fw_len = header.ih_size;
			found_img = 1;
//			printk("GPHY FIRMWARE LOAD SUCCESSFULLY AT ADDR (VR9)(GE MODE V12) \n" );
		} 
#endif
#if defined(CONFIG_FE_MODE)
		if( ( strncmp(header.ih_name, "VR9 V1.1 GPHY FE", sizeof(header.ih_name)) == 0) && (dev_id == 0) ) {
			first_block = 1;
			fw_len = header.ih_size;
			found_img = 1;
//			printk("GPHY FIRMWARE LOAD SUCCESSFULLY AT ADDR (VR9)(FE MODE V11) \n" );
		} else if(strncmp(header.ih_name, "VR9 V1.2 GPHY FE", sizeof(header.ih_name)) == 0 && (dev_id == 1) ) {
			first_block = 1;
			fw_len = header.ih_size;
			found_img = 1;
//			printk("GPHY FIRMWARE LOAD SUCCESSFULLY AT ADDR (VR9)(FE MODE V12) \n" );
		}
		
#endif
	}	
	if ( (first_block == 1) && (!second_block) && found_img ) {
		memset(proc_gphy_fw_dma, 0, sizeof(proc_gphy_fw_dma));
		rcv_size = (len - (found_magic * sizeof(gphy_image_header_t)));
		memcpy(proc_gphy_fw_dma, local_buf+(found_magic * sizeof(gphy_image_header_t)), rcv_size);
		second_block = 1;
	} else if ((second_block == 1 ) && found_img) {
		if (rcv_size < (fw_len) ) {
			memcpy(proc_gphy_fw_dma+rcv_size, local_buf, (len ));
			rcv_size += len;
		} else {
			first_block = 0;
			found_img = 0;
			second_block = 0;
			found_magic = 0;
			ltq_gphy_firmware_config(2, 1, dev_id, ge_fe_mode );
		}	
	}
	return len;
}
#endif /* CONFIG_VR9 */
/** create proc for debug  info, \used ifx_eth_module_init */
static int gphy_proc_create(void)
{
#if defined(CONFIG_VR9)
	struct proc_dir_entry *res;
#endif
	/* procfs */
	ifx_gphy_dir = proc_mkdir ("driver/ifx_gphy", NULL);
	if (ifx_gphy_dir == NULL) {
		printk(KERN_ERR "%s: Create proc directory (/driver/ifx_gphy) failed!!!\n", __func__);
		return IFX_ERROR;
	}
#if defined(CONFIG_VR9)
	res = create_proc_entry("phyfirmware", 0, ifx_gphy_dir);
	if ( res ) {
		res->read_proc  = proc_read_gphy;
        res->write_proc = proc_write_gphy;
    }
#endif /*CONFIG_VR9*/
	create_proc_read_entry("version", 0, ifx_gphy_dir, gphy_proc_version,  NULL);
	return IFX_SUCCESS;
}

/** remove of the proc entries, \used ifx_eth_module_exit */
static void gphy_proc_delete(void)
{
#if 0
	remove_proc_entry("version", gphy_drv_ver);
#endif
}

#if defined(CONFIG_HN1_USE_TANTOS)
unsigned short tantos_read_mdio(unsigned int tRegAddr )
{
	unsigned int phyAddr, regAddr;
	if (tRegAddr >= 0x200) 
		return (0xff);
	regAddr = tRegAddr & 0x1f;          //5-bits regAddr
	phyAddr = (tRegAddr>>5) & 0x1f;     //5-bits phyAddr
    return (ltq_read_mdio(phyAddr, regAddr));
}

void tantos_write_mdio(unsigned int tRegAddr, unsigned int data)
{
    unsigned int phyAddr, regAddr;
    if (tRegAddr >= 0x200)
    	return;
    regAddr = tRegAddr & 0x1f;          //5-bits regAddr
    phyAddr = (tRegAddr>>5) & 0x1f;     //5-bits phyAddr
    return (ltq_write_mdio(phyAddr, regAddr, data));    
}
#define TANTOS_MIIAC    0x120
#define TANTOS_MIIWD    0x121
#define TANTOS_MIIRD    0x122
unsigned short tantos_read_mdio_phy(unsigned int phyAddr, unsigned int tPhyRegAddr )
{
    unsigned int cmd=0;
    if (phyAddr > 4 || tPhyRegAddr > 0x1D ) {
    	return;
    }
    cmd = (1<<15) | (1<<11) | (phyAddr << 5) | tPhyRegAddr; 
    tantos_write_mdio(TANTOS_MIIAC, cmd);
    return (tantos_read_mdio(TANTOS_MIIRD));
}

//phyAddr : 0 to 4 corresponds to Tantos internal PHY ports
unsigned short tantos_write_mdio_phy(unsigned int phyAddr, unsigned int tPhyRegAddr, unsigned int data)
{
    unsigned int cmd=0;
    if (phyAddr > 4 || tPhyRegAddr > 0x1D ) {
    	return;
    }
    cmd = (1<<15) | (1<<10) | (phyAddr << 5) | tPhyRegAddr; 
    tantos_write_mdio(TANTOS_MIIWD, data);
    tantos_write_mdio(TANTOS_MIIAC, cmd);

}
EXPORT_SYMBOL(tantos_read_mdio);
EXPORT_SYMBOL(tantos_write_mdio);
EXPORT_SYMBOL(tantos_read_mdio_phy);
EXPORT_SYMBOL(tantos_write_mdio_phy);
#endif

/* Workaround for Clean-on-Read to external ports*/
void gphy_COR_configure(int phy_addr)
{
	unsigned short mdio_read_value;
	
	ltq_write_mdio(phy_addr, 0xd, 0x1F);
	ltq_write_mdio(phy_addr, 0xe, 0x1FF);
	ltq_write_mdio(phy_addr, 0xd, 0x401F);
	mdio_read_value =  ltq_read_mdio(phy_addr, 0xe);
	mdio_read_value |= 1;
	ltq_write_mdio(phy_addr, 0xd, 0x1F);
	ltq_write_mdio(phy_addr, 0xe, 0x1FF);
	ltq_write_mdio(phy_addr, 0xd, 0x401F);
	ltq_write_mdio(phy_addr, 0xe, mdio_read_value); 
}

 /* MDIO auto-polling for T2.12 version */
void gphy_MDIO_AutoPoll_configure(int phy_addr) 
{
	/* MDIO auto-polling for T2.12 version */
	ltq_write_mdio(phy_addr, 0xd, 0x1F);
	ltq_write_mdio(phy_addr, 0xe, 0x1FF);
	ifxmips_mdelay(10);
	ltq_write_mdio(phy_addr, 0xd, 0x401F);
	ifxmips_mdelay(10);
	ltq_write_mdio(phy_addr, 0xe, 0x1);
	ifxmips_mdelay(10);
	/* Enable STICKY functionality for internal GPHY */
	ltq_write_mdio(phy_addr, 0x14, 0x8106);
	/* Issue GPHY reset */
	ltq_write_mdio(phy_addr, 0x0, 0x9000);
	ifxmips_mdelay(30);
}

 /* Disable the MDIO interrupt for external GPHY */
void gphy_disable_MDIO_interrupt(int phy_addr) 
{
	ltq_write_mdio(0x0, 0xd, 0x1F);
	ifxmips_mdelay(10);
	ltq_write_mdio(0x0, 0xe, 0x0703);
	ifxmips_mdelay(10);
	ltq_write_mdio(0x0, 0xd, 0x401F);
	ifxmips_mdelay(10);
	ltq_write_mdio(0x0, 0xe, 0x0002);
	ifxmips_mdelay(10);
}

 /* preferred MASTER device */
void configure_gphy_master_mode(int phy_addr)
{
   ltq_write_mdio(0x0, 0x9, 0x700);
   ifxmips_mdelay(20);
}

/* Disabling of the Power-Consumption Scaling for external GPHY (Bit:2)*/
void disable_power_scaling_mode(int phy_addr)
{
	unsigned short value;
	value =  ltq_read_mdio(phy_addr, 0x14);
	value &= ~(1<<2);
	ltq_write_mdio(phy_addr, 0x14, value);
}

void ltq_phy_mmd_write(unsigned char phyaddr, unsigned int reg_addr, unsigned short data )
{
	unsigned int temp = SW_READ_REG32(MDC_CFG_0_REG);
	/* Disable Switch Auto Polling for all ports */
	SW_WRITE_REG32(0x0, MDC_CFG_0_REG);
	ltq_write_mdio(phyaddr, 0xd, 0x1F);
	ltq_write_mdio(phyaddr, 0xe, reg_addr);
	ltq_write_mdio(phyaddr, 0xd, 0x401F);
	ltq_write_mdio(phyaddr, 0xe, data);
	/* Enable Switch Auto Polling for all ports */
	SW_WRITE_REG32(temp, MDC_CFG_0_REG);
}
unsigned short ltq_phy_mmd_read(unsigned char phyaddr, unsigned int reg_addr )
{
	unsigned short data;
	unsigned int temp = SW_READ_REG32(MDC_CFG_0_REG);
	/* Disable Switch Auto Polling for all ports */
	SW_WRITE_REG32(0x0, MDC_CFG_0_REG);
	ltq_write_mdio(0x0, 0xd, 0x1F);
	ltq_write_mdio(phyaddr, 0xd, 0x1F);
	ltq_write_mdio(phyaddr, 0xe, reg_addr);
	ltq_write_mdio(phyaddr, 0xd, 0x401F);
	data = ltq_read_mdio(phyaddr, 0xe);
	/* Enable Switch Auto Polling for all ports */
	SW_WRITE_REG32(temp, MDC_CFG_0_REG);
	return data;
}

void ltq_gphy_led0_config (unsigned char phyaddr)
{
	/* for GE modes, For LED0    (SPEED/LINK INDICATION ONLY) */
	ltq_phy_mmd_write(phyaddr, 0x1e2, 0x42);
	ltq_phy_mmd_write(phyaddr, 0x1e3, 0x10);
}

void ltq_gphy_led1_config (unsigned char phyaddr)
{
	/* for GE modes, For LED1, DATA TRAFFIC INDICATION ONLY */
	ltq_phy_mmd_write(phyaddr, 0x1e4, 0x70);
	ltq_phy_mmd_write(phyaddr, 0x1e5, 0x03);
}

/**
Mode			Link-LED			Data-LED
Link-Down		OFF					OFF
10baseT			BLINK-SLOW			PULSE on TRAFFIC
100baseTX		BLINK-FAST			PULSE on TRAFFIC
1000baseT		ON					PULSE on TRAFFIC
**/
void ltq_ext_gphy_led_config (void)
{
#if defined(CONFIG_GE_MODE)	
	ltq_gphy_led0_config(0);
	ltq_gphy_led1_config(0);
	ltq_gphy_led0_config(1);
	ltq_gphy_led1_config(1);
	ltq_gphy_led0_config(5);
	ltq_gphy_led1_config(5);
#endif
}
void ltq_int_gphy_led_config (void)
{
#if defined(CONFIG_GE_MODE)
	unsigned int reg;
#if  defined(CONFIG_AR10)
	reg = SW_READ_REG32(PHY_ADDR_1);
	reg &= (0x1F);
	ltq_gphy_led0_config(reg);
	ltq_gphy_led1_config(reg);
#endif
	reg = SW_READ_REG32(PHY_ADDR_2);
	reg &= (0x1F);
	ltq_gphy_led0_config(reg);
	ltq_gphy_led1_config(reg);
	reg = SW_READ_REG32(PHY_ADDR_4);
	reg &= (0x1F);
	ltq_gphy_led0_config(reg);
	ltq_gphy_led1_config(reg);
#if 0
	ltq_gphy_led0_config(0x11);
	ltq_gphy_led1_config(0x11);
	ltq_gphy_led0_config(0x13);
	ltq_gphy_led1_config(0x13);
#endif
#endif
}
int ltq_ext_Gphy_hw_Specify_init(void)
{
	/* Workaround for Clean-on-Read to external ports*/
	gphy_COR_configure(0);
	gphy_COR_configure(1);
	gphy_COR_configure(5);
	 /* Disable the MDIO interrupt for external GPHY */
	gphy_disable_MDIO_interrupt(0);
	gphy_disable_MDIO_interrupt(1);
	gphy_disable_MDIO_interrupt(5);
	/* preferred MASTER device */
	configure_gphy_master_mode(0);
	configure_gphy_master_mode(1);
	configure_gphy_master_mode(5);
	return 0;
}

int ltq_Int_Gphy_hw_Specify_init(void)
{
	gphy_MDIO_AutoPoll_configure(0x11);
	configure_gphy_master_mode(0x11);
#if ( defined(CONFIG_VR9) )
	/* MDIO auto-polling for T2.12 version */
	gphy_MDIO_AutoPoll_configure(0x13);
	configure_gphy_master_mode(0x13);
#endif	
	return 0;
}
#if 0
int ltq_Gphy_hw_Specify_init(void)
{
#if ( defined(CONFIG_VR9) )
/* Workaround for Clean-on-Read to external ports*/
	gphy_COR_configure(0);
	gphy_COR_configure(1);
	gphy_COR_configure(5);
	/* MDIO auto-polling for T2.12 version */
	gphy_MDIO_AutoPoll_configure(0x13);
	 /* Disable the MDIO interrupt for external GPHY */
	gphy_disable_MDIO_interrupt(0);
	gphy_disable_MDIO_interrupt(1);
	gphy_disable_MDIO_interrupt(5);
	/* preferred MASTER device */
	configure_gphy_master_mode(0);
	configure_gphy_master_mode(1);
	configure_gphy_master_mode(5);
	
	configure_gphy_master_mode(0x13);
    /* Disabling of the Power-Consumption Scaling for external GPHY */
	disable_power_scaling_mode(0x0);
	disable_power_scaling_mode(0x1);
	disable_power_scaling_mode(0x5);
#endif	
	gphy_MDIO_AutoPoll_configure(0x11);
	configure_gphy_master_mode(0x11);
	return 0;
}
#endif
int ltq_gphy_firmware_config(int total_gphys, int mode, int dev_id, int ge_fe_mode )
{
	int i, fwSize = GPHY_FW_LEN;
	unsigned int alignedPtr;
	unsigned char *pFw;

	/* Disable Switch Auto Polling for all ports */
	SW_WRITE_REG32(0x0, MDC_CFG_0_REG);
	ifxmips_mdelay(100);
	/* RESET GPHY */
	if(ltq_gphy_reset(total_gphys) == 1)
		printk(KERN_ERR "GPHY driver init RESET FAILED !!\n");

	// find the 64kb aligned boundary in the 128kb buffer
	alignedPtr = (u32)gphy_fw_dma;
	alignedPtr &= ~GPHY_FW_START_ADDR_ALIGN_MASK; 
	alignedPtr += GPHY_FW_START_ADDR_ALIGN;
	if ( mode == 1 ) {
#if defined(CONFIG_VR9)
		memset(gphy_fw_dma, 0, sizeof(gphy_fw_dma));
		pFw = (unsigned char *)proc_gphy_fw_dma;
		for(i=0; i<fwSize; i++)
			*((unsigned char *)(alignedPtr+i)) = pFw[i];
#endif
	} else {
#if ( defined(CONFIG_AR10) || defined(CONFIG_HN1) )	
#if defined(CONFIG_FE_MODE)
		if (dev_id == 0) {
			pFw = (unsigned char *)gphy_fe_fw_data;
		} else {
			pFw = (unsigned char *)gphy_fe_fw_data_a12;
		}
//		printk("GPHY FIRMWARE LOAD SUCCESSFULLY AT ADDR (VR9)(FE MODE) : %x\n", alignedPtr);
#endif
#if defined(CONFIG_GE_MODE)
		if (dev_id == 0 ) {
			pFw = (unsigned char *)gphy_ge_fw_data;
		} else {
			pFw = (unsigned char *)gphy_ge_fw_data_a12;
		}
//		printk("GPHY FIRMWARE LOAD SUCCESSFULLY AT ADDR (VR9)(GE MODE) : %x\n", alignedPtr);
#endif
	for(i=0; i<fwSize; i++)
		*((char *)(alignedPtr+i)) = pFw[i];
#endif
	} 
	// remove the kseg prefix before giving this address to GPHY's DMA
	alignedPtr &= 0x0FFFFFFF;
#if  defined(CONFIG_VR9) 
	/* Load GPHY0 firmware module  */
	SW_WRITE_REG32( alignedPtr, GFS_ADD0);
	/* Load GPHY1 firmware module  */
	SW_WRITE_REG32( alignedPtr, GFS_ADD1);
	printk("GPHY FIRMWARE LOAD SUCCESSFULLY AT ADDR (VR9) : %x\n", alignedPtr);
#endif  /*CONFIG_VR9*/

#if  defined(CONFIG_AR10) 
	/* Load GPHY0 firmware module  */
	SW_WRITE_REG32( alignedPtr, GFS_ADD0);
	/* Load GPHY1 firmware module  */
	SW_WRITE_REG32( alignedPtr, GFS_ADD1);
	/* Load GPHY2 firmware module  */
	SW_WRITE_REG32( alignedPtr, GFS_ADD2);
	printk("GPHY FIRMWARE LOAD SUCCESSFULLY AT ADDR (AR10) : %x\n", alignedPtr);
#endif /*CONFIG_AR10 */
#if  defined(CONFIG_HN1) 
	/* Load GPHY0 firmware module  */
	SW_WRITE_REG32( alignedPtr, GFS_ADD0);
	printk("GPHY FIRMWARE LOAD SUCCESSFULLY AT ADDR (GHN) : %x\n", alignedPtr);
#endif  /*CONFIG_HN1*/
//	ifxmips_mdelay(1000);
	if(ltq_gphy_reset_released(total_gphys) == 1)
		printk(KERN_ERR "GPHY driver init RELEASE FAILED !!\n");
	ifxmips_mdelay(500);
	if (dev_id == 0) {
		if(ltq_Int_Gphy_hw_Specify_init() == 1)
			printk(KERN_ERR "GPHY driver Specify init FAILED !!\n");
	}
#if  defined(CONFIG_HN1)
	SW_WRITE_REG32(0x3, MDC_CFG_0_REG);
#else
	SW_WRITE_REG32(0x3f, MDC_CFG_0_REG);
#endif
	ifxmips_mdelay(500);
	ltq_int_gphy_led_config();
	return 0;
}

/** Initilization GPHY module */
int ifx_phy_module_init (void)
{
	char ver_str[128] = {0};

	dev_id = 0;
	total_gphys = 0;
	ge_fe_mode = 0;
	memset(gphy_fw_dma, 0, sizeof(gphy_fw_dma));
#if  defined(CONFIG_AR10) 
	{
		unsigned int temp = SW_READ_REG32(IFX_CGU_IF_CLK);
		temp &= ~(0x7<<2);
		temp |= (0x2 << 2);  /*Set 25MHz clock*/
		SW_WRITE_REG32(temp, IFX_CGU_IF_CLK);
	}
	total_gphys = 3;
	dev_id = 1;
	printk(KERN_ERR "GPHY FW load for ARX300 !!\n");
#endif  /*CONFIG_AR10 */
#if  defined(CONFIG_HN1) 
	total_gphys = 2;
	dev_id = 0;
	printk(KERN_ERR "GPHY FW load for G.Hn !!\n");
#endif
#if ( defined(CONFIG_VR9) )
	{
	ifx_chipid_t chipID;
	memset(proc_gphy_fw_dma, 0, sizeof(proc_gphy_fw_dma));
	total_gphys = 2;
	ifx_get_chipid(&chipID);
	if (chipID.family_id == IFX_FAMILY_xRX200) {
		if ( chipID.family_ver == IFX_FAMILY_xRX200_A1x ) {
			printk(KERN_ERR "GPHY FW load for A11 !!\n");
			dev_id = 0;
		} else {
			dev_id = 1;
			printk(KERN_ERR "GPHY FW load for A12 !!\n");
		}
	}
#if 0
	{
		unsigned int temp = SW_READ_REG32(IFX_CGU_IF_CLK);
		temp &= ~(0x7<<2);
		temp |= (0x2 << 2);  /*Set 25MHz clock*/
		SW_WRITE_REG32(temp, IFX_CGU_IF_CLK);
	}
#endif
	}
#endif /* CONFIG_VR9*/
#if defined(CONFIG_GE_MODE)
	ge_fe_mode = FW_GE_MODE;
#else
	ge_fe_mode = FW_FE_MODE;
#endif
#if ( defined(CONFIG_AR10) || defined(CONFIG_HN1) )
	ltq_gphy_firmware_config(total_gphys, 0, dev_id, ge_fe_mode );
#endif
	if (dev_id == 0) {
		if(ltq_ext_Gphy_hw_Specify_init() == 1)
			printk(KERN_ERR "GPHY driver Specify init FAILED !!\n");
	}
	ltq_ext_gphy_led_config();
	/* Create proc entry */
	gphy_proc_create();
	/* Print the driver version info */
	gphy_drv_ver(ver_str);
	printk(KERN_INFO "%s", ver_str);
	return  0;
}

void  ifx_phy_module_exit (void)
{
	gphy_proc_delete();
}

module_init(ifx_phy_module_init);
module_exit(ifx_phy_module_exit);

MODULE_AUTHOR("Sammy Wu");
MODULE_DESCRIPTION("IFX GPHY driver (Supported LTQ devices)");
MODULE_LICENSE("GPL");
MODULE_VERSION(IFX_DRV_MODULE_VERSION);
