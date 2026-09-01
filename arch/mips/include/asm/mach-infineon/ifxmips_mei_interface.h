/******************************************************************************

			       Copyright (c) 2011
			    Lantiq Deutschland GmbH
		     Am Campeon 3; 85579 Neubiberg, Germany

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/

#ifndef IFXMIPS_MEI_H
#define IFXMIPS_MEI_H

#define MEI_VER_MAJOR	5
#define MEI_VER_MINOR	3
#define MEI_VER_REV	0

#if defined(CONFIG_VR9)
#error VR9 platform not supported.
#endif

#if !defined(CONFIG_DANUBE) && !defined(CONFIG_AMAZON_SE) &&\
    !defined(CONFIG_AR9)
#error Platform undefined!!!
#endif

#if defined(CONFIG_BSP_RTT)

#if defined(BSP_PORT_RTEMS)
#  error Under RTEMS real time trace not supported.
#endif /*defined(BSP_PORT_RTEMS)*/

#if defined(DFE_PING_TEST)
#  error RTT support and ping test incompatible.
#endif /*defined(DFE_PING_TEST)*/

#endif /*defined(CONFIG_BSP_RTT)*/

#define CONFIG_BSP_RTT

#ifdef IFX_MEI_BSP

/**
   Defines all possible CMV groups
*/
enum mei_cmv_group {
	/** CNTL group*/
	DSL_CMV_GROUP_CNTL = 1,
	/** STAT group*/
	DSL_CMV_GROUP_STAT = 2,
	/** INFO group*/
	DSL_CMV_GROUP_INFO = 3,
	/** TEST group*/
	DSL_CMV_GROUP_TEST = 4,
	/** OPTN group*/
	DSL_CMV_GROUP_OPTN = 5,
	/** RATE group*/
	DSL_CMV_GROUP_RATE = 6,
	/** PLAM group*/
	DSL_CMV_GROUP_PLAM = 7,
	/** CNFG group*/
	DSL_CMV_GROUP_CNFG = 8,
	/** DSL group*/
	DSL_CMV_GROUP_DSL  = 34
};

/**
   Defines all opcode types
*/
enum mei_cmv_opcode {
	/** CMV Message Code - MEI-to-ARC read */
	H2D_CMV_READ			 = 0x00,
	/** CMV Message Code - MEI-to-ARC write */
	H2D_CMV_WRITE			 = 0x04,
	/** MEI-to-ARC indicate reply */
	H2D_CMV_INDICATE_REPLY		 = 0x10,
	/** MEI-to-ARC opcode unknown */
	H2D_ERROR_OPCODE_UNKNOWN	 = 0x20,
	/** MEI-to-ARC cmv onknown */
	H2D_ERROR_CMV_UNKNOWN		 = 0x30,

	/** CMV Message Codes - ARC-to-MEI ready reply */
	D2H_CMV_READ_REPLY		 = 0x01,
	/** CMV Message Codes - ARC-to-MEI write reply */
	D2H_CMV_WRITE_REPLY		 = 0x05,
	/** CMV Message Codes - ARC-to-MEI indicate */
	D2H_CMV_INDICATE		 = 0x11,
	/** ARC-to-MEI opcode unknown */
	D2H_ERROR_OPCODE_UNKNOWN	 = 0x21,
	/** ARC-to-MEI cmmv unknown */
	D2H_ERROR_CMV_UNKNOWN		 = 0x31,
	/** ARC-to-MEI cmv ready not available */
	D2H_ERROR_CMV_READ_NOT_AVAILABLE = 0x41,
	/** ARC-to-MEI cmv write only */
	D2H_ERROR_CMV_WRITE_ONLY	 = 0x51,
	/** ARC-to-MEI cmv read only */
	D2H_ERROR_CMV_READ_ONLY		 = 0x61,

	/** MEI-to-ARC debug read data memory */
	H2D_DEBUG_READ_DM		 = 0x02,
	/** MEI-to-ARC debug read program memory */
	H2D_DEBUG_READ_PM		 = 0x06,
	/** MEI-to-ARC debug write data memory */
	H2D_DEBUG_WRITE_DM		 = 0x0a,
	/** MEI-to-ARC debug write program memory */
	H2D_DEBUG_WRITE_PM		 = 0x0e,

	/** ARC-to-MEI debug read data memory reply */
	D2H_DEBUG_READ_DM_REPLY		 = 0x03,
	/** ARC-to-MEI debug read program memory reply */
	D2H_DEBUG_READ_PM_REPLY		 = 0x07,
	/** ARC-to-MEI debug write data memory reply */
	D2H_DEBUG_WRITE_DM_REPLY	 = 0x0b,
	/** ARC-to-MEI debug write program memory reply */
	D2H_DEBUG_WRITE_PM_REPLY	 = 0x0f,
	/** ARC-to-MEI address unknown */
	D2H_ERROR_ADDR_UNKNOWN		 = 0x33,

	/** Modem Ready Message Code - ARC-to-MEI */
	D2H_AUTONOMOUS_MODEM_READY_MSG	 = 0xf1
};

/* mutex macros */
#define MEI_MUTEX_INIT(id,flag) sema_init(&id,flag)
#define MEI_MUTEX_LOCK(id)      down_interruptible(&id)
#define MEI_MUTEX_UNLOCK(id)    up(&id)
#define MEI_WAIT(ms) \
	{\
		set_current_state(TASK_INTERRUPTIBLE);\
		schedule_timeout(ms);\
	}
#define MEI_INIT_WAKELIST(name,queue) init_waitqueue_head(&queue)

/* wait for an event, timeout is measured in ms */
#define MEI_WAIT_EVENT_TIMEOUT(ev,timeout)\
        interruptible_sleep_on_timeout(&ev,timeout * HZ / 1000)
#define MEI_WAKEUP_EVENT(ev) wake_up_interruptible(&ev)
#endif /* IFX_MEI_BSP */

/***  Register address offsets, relative to MEI_SPACE_ADDRESS ***/
#define ME_DX_DATA		(0x0000)
#define ME_VERSION		(0x0004)
#define ME_ARC_GP_STAT		(0x0008)
#define ME_DX_STAT		(0x000C)
#define ME_DX_AD		(0x0010)
#define ME_DX_MWS		(0x0014)
#define ME_ME2ARC_INT		(0x0018)
#define ME_ARC2ME_STAT		(0x001C)
#define ME_ARC2ME_MASK		(0x0020)
#define ME_DBG_WR_AD		(0x0024)
#define ME_DBG_RD_AD		(0x0028)
#define ME_DBG_DATA		(0x002C)
#define ME_DBG_DECODE		(0x0030)
#define ME_CONFIG		(0x0034)
#define ME_RST_CTRL		(0x0038)
#define ME_DBG_MASTER		(0x003C)
#define ME_CLK_CTRL		(0x0040)
#define ME_BIST_CTRL		(0x0044)
#define ME_BIST_STAT		(0x0048)
#define ME_XDATA_BASE_SH	(0x004c)
#define ME_XDATA_BASE		(0x0050)
#define ME_XMEM_BAR_BASE	(0x0054)
#define ME_XMEM_BAR0		(0x0054)
#define ME_XMEM_BAR1		(0x0058)
#define ME_XMEM_BAR2		(0x005C)
#define ME_XMEM_BAR3		(0x0060)
#define ME_XMEM_BAR4		(0x0064)
#define ME_XMEM_BAR5		(0x0068)
#define ME_XMEM_BAR6		(0x006C)
#define ME_XMEM_BAR7		(0x0070)
#define ME_XMEM_BAR8		(0x0074)
#define ME_XMEM_BAR9		(0x0078)
#define ME_XMEM_BAR10		(0x007C)
#define ME_XMEM_BAR11		(0x0080)
#define ME_XMEM_BAR12		(0x0084)
#define ME_XMEM_BAR13		(0x0088)
#define ME_XMEM_BAR14		(0x008C)
#define ME_XMEM_BAR15		(0x0090)
#define ME_XMEM_BAR16		(0x0094)

#define WHILE_DELAY		20000

/*
   Define where in ME Processor's memory map the Stratify chip lives
*/

#define MAXSWAPSIZE		(8 * 1024)  /*8k *(32bits)*/

/* Mailboxes*/
#define MSG_LENGTH		16 /* x16 bits*/
#define YES_REPLY		1
#define NO_REPLY		0

#define CMV_TIMEOUT		1000  /*ms*/
#define MBOX_POLL_PERIOD_MS	10    /*ms*/

/* Block size per BAR*/
#define SDRAM_SEGMENT_SIZE 	(64*1024)
/* Number of Bar registers*/
#define MAX_BAR_REGISTERS  	(17)

#define XDATA_REGISTER		(15)
#define RTT_FW_BUFFER_REGISTER	(16)

/* ARC register addresss*/
#define ARC_STATUS		0x0
#define ARC_LP_START		0x2
#define ARC_LP_END		0x3
#define ARC_DEBUG		0x5
#define ARC_INT_MASK		0x10A

#define IRAM0_BASE		(0x00000)
#define IRAM1_BASE		(0x04000)
#if defined(CONFIG_DANUBE)
#define BRAM_BASE		(0x0A000)
#elif defined(CONFIG_AMAZON_SE) || defined(CONFIG_AR9)
#define BRAM_BASE		(0x08000)
#endif
#define XRAM_BASE		(0x18000)
#define YRAM_BASE		(0x1A000)
#define EXT_MEM_BASE		(0x80000)
#define ARC_GPIO_CTRL		(0xC030)
#define ARC_GPIO_DATA		(0xC034)

#define IRAM0_SIZE		(16*1024)
#define IRAM1_SIZE		(16*1024)
#define BRAM_SIZE		(12*1024)
#define XRAM_SIZE		(8*1024)
#define YRAM_SIZE		(8*1024)
#define EXT_MEM_SIZE		(1536*1024)

#define ADSL_BASE		(0x20000)
#define CRI_BASE		(ADSL_BASE + 0x11F00)
#define CRI_CCR0		(CRI_BASE + 0x00)
#define CRI_RST			(CRI_BASE + 0x04*4)
#define ADSL_DILV_BASE		(ADSL_BASE+0x20000)


#define IRAM0_ADDR_BIT_MASK	0xFFF
#define IRAM1_ADDR_BIT_MASK	0xFFF
#define BRAM_ADDR_BIT_MASK	0xFFF
#define RX_DILV_ADDR_BIT_MASK	0x1FFF

/***  Bit definitions ***/
#define ARC_AUX_HALT		(1 << 25)
#define ARC_DEBUG_HALT		(1 << 1)
#define FALSE			0
#define TRUE			1
#define BIT0			(1<<0)
#define BIT1			(1<<1)
#define BIT2			(1<<2)
#define BIT3			(1<<3)
#define BIT4			(1<<4)
#define BIT5			(1<<5)
#define BIT6			(1<<6)
#define BIT7			(1<<7)
#define BIT8			(1<<8)
#define BIT9			(1<<9)
#define BIT10			(1<<10)
#define BIT11			(1<<11)
#define BIT12			(1<<12)
#define BIT13			(1<<13)
#define BIT14			(1<<14)
#define BIT15			(1<<15)
#define BIT16			(1<<16)
#define BIT17			(1<<17)
#define BIT18			(1<<18)
#define BIT19			(1<<19)
#define BIT20			(1<<20)
#define BIT21			(1<<21)
#define BIT22			(1<<22)
#define BIT23			(1<<23)
#define BIT24			(1<<24)
#define BIT25			(1<<25)
#define BIT26			(1<<26)
#define BIT27			(1<<27)
#define BIT28			(1<<28)
#define BIT29			(1<<29)
#define BIT30			(1<<30)
#define BIT31			(1<<31)

/* CRI_CCR0 Register definitions*/
#define CLK_2M_MODE_ENABLE	BIT6
#define ACL_CLK_MODE_ENABLE	BIT4
#define FDF_CLK_MODE_ENABLE	BIT2
#define STM_CLK_MODE_ENABLE	BIT0

/* CRI_RST Register definitions*/
#define FDF_SRST		BIT3
#define MTE_SRST		BIT2
#define FCI_SRST		BIT1
#define AAI_SRST		BIT0

/* MEI_TO_ARC_INTERRUPT Register definitions*/
#define MEI_TO_ARC_INT1		BIT3
#define MEI_TO_ARC_INT0		BIT2
#define MEI_TO_ARC_CS_DONE	BIT1  /* need to check*/
#define MEI_TO_ARC_MSGAV	BIT0

/* ARC_TO_MEI_INTERRUPT Register definitions*/
#define ARC_TO_MEI_INT1		BIT8
#define ARC_TO_MEI_INT0		BIT7
#define ARC_TO_MEI_CS_REQ	BIT6
#define ARC_TO_MEI_DBG_DONE	BIT5
#define ARC_TO_MEI_MSGACK	BIT4
#define ARC_TO_MEI_NO_ACCESS	BIT3
#define ARC_TO_MEI_CHECK_AAITX	BIT2
#define ARC_TO_MEI_CHECK_AAIRX	BIT1
#define ARC_TO_MEI_MSGAV	BIT0

/* ARC_TO_MEI_INTERRUPT_MASK Register definitions*/
#define GP_INT1_EN		BIT8
#define GP_INT0_EN		BIT7
#define CS_REQ_EN		BIT6
#define DBG_DONE_EN		BIT5
#define MSGACK_EN		BIT4
#define NO_ACC_EN		BIT3
#define AAITX_EN		BIT2
#define AAIRX_EN		BIT1
#define MSGAV_EN		BIT0

#define MEI_SOFT_RESET		BIT0

#define HOST_MSTR		BIT0

#define JTAG_MASTER_MODE	0x0
#define MEI_MASTER_MODE		HOST_MSTR

/*  MEI_DEBUG_DECODE Register definitions*/
#define MEI_DEBUG_DEC_MASK	(0x3)
#define MEI_DEBUG_DEC_AUX_MASK	(0x0)
#define ME_DBG_DECODE_DMP1_MASK	(0x1)
#define MEI_DEBUG_DEC_DMP2_MASK	(0x2)
#define MEI_DEBUG_DEC_CORE_MASK	(0x3)

#define AUX_STATUS		(0x0)
#define AUX_ARC_GPIO_CTRL	(0x10C)
#define AUX_ARC_GPIO_DATA	(0x10D)

/*
   ARC_TO_MEI_MAILBOX[11] is a special location used to indicate
   page swap requests.*/
#if defined(CONFIG_DANUBE)
#define OMBOX_BASE		0xDF80
#define ARC_TO_MEI_MAILBOX	0xDFA0
#define IMBOX_BASE		0xDFC0
#define MEI_TO_ARC_MAILBOX	0xDFD0
#elif defined(CONFIG_AMAZON_SE) || defined(CONFIG_AR9)
#define OMBOX_BASE		0xAF80
#define ARC_TO_MEI_MAILBOX	0xAFA0
#define IMBOX_BASE		0xAFC0
#define MEI_TO_ARC_MAILBOX	0xAFD0
#endif

#define MEI_TO_ARC_MAILBOXR	(MEI_TO_ARC_MAILBOX + 0x2C)
#define ARC_MEI_MAILBOXR	(ARC_TO_MEI_MAILBOX + 0x2C)
#define OMBOX1			(OMBOX_BASE+0x4)

/* Codeswap request messages are indicated by setting BIT31*/
#define OMB_CODESWAP_MESSAGE_MSG_TYPE_MASK	(0x80000000)

/* Clear Eoc messages received are indicated by setting BIT17*/
#define OMB_CLEAREOC_INTERRUPT_CODE	(0x00020000)
#define OMB_REBOOT_INTERRUPT_CODE	BIT18

/* RTT data capture interrupt BIT19*/
#define OMB_RTT_DATA_READY		BIT19

/*
Swap page header
*/
/* Page must be loaded at boot time if size field has BIT31 set*/
#define BOOT_FLAG	BIT31
#define BOOT_FLAG_MASK	~BOOT_FLAG

#define FREE_RELOAD	1
#define FREE_SHOWTIME	2
#define FREE_ALL	3
#define FREE_XDATA	4

/* marcos*/
#define MEI_WRITE_REGISTER_L(data,addr)	*((volatile uint32_t*)(addr))\
						= (uint32_t)(data)
#define MEI_READ_REGISTER_L(addr)	(*((volatile uint32_t*)(addr)))

#define SET_BIT(reg, mask)	reg |= (mask)
#define CLEAR_BIT(reg, mask)	reg &= (~mask)
#define CLEAR_BITS(reg, mask)	CLEAR_BIT(reg, mask)
/*#define SET_BITS(reg, mask)	SET_BIT(reg, mask)*/
#define SET_BITFIELD(reg, mask, off, val) {reg &= (~mask); reg |= (val << off);}

/* 1K size align*/
#define ALIGN_SIZE	( 1L<<10 )
#define MEM_ALIGN(addr)	(((addr) + ALIGN_SIZE - 1) & ~(ALIGN_SIZE -1) )

/* swap marco*/
#define MEI_HALF_WORD_SWAP(data) {data = ((data & 0xffff)<<16) +\
					 ((data & 0xffff0000)>>16);}
#define MEI_BYTE_SWAP(data)	 {data = ((data & 0xff)<<24) +\
					 ((data & 0xff00)<<8)+\
					 ((data & 0xff0000)>>8)+\
					 ((data & 0xff000000)>>24);}


#ifdef CONFIG_PROC_FS
struct reg_entry {
	/** pointer to data connected with procfs entry*/
	int32_t *flag;
	/** big enough to hold names */
	char name[30];
	/** big enough to hold description */
	char description[100];
	/** procfs entry inode*/
	uint16_t low_ino;
};
#endif

/**
   Definitions for defining the debug level.
   The smaller the value the less debug output will be printed
*/
enum mei_dbg_lvl {
	/** no output */
	MEI_DBG_NONE = 0x0,
	/** */
	MEI_DBG_PRN = 0x1,
	/** errors will be printed */
	MEI_DBG_ERR = 0x2,
	/** warnings and errors are printed */
	MEI_DBG_WRN = 0x3,
	/** verbose output */
	MEI_DBG_MSG = 0x4,
	/** delimeter only*/
	MEI_DBG_LAST
};

/**
   Definitions for selecting debug modules.
*/
enum mei_dbg_module {
	/** Common MEI driver debug module*/
	MEI_DBG_MOD_COM = 0,
	/** CMV and debug access module*/
	MEI_DBG_MOD_CMV = 1,
	/** MEI driver test and debug module */
	MEI_DBG_MOD_TST = 2,
	/** delimeter only*/
	MEI_DBG_MOD_LAST
};

/**
   \todo add description!
*/
struct mei_dbg_info {
	/** Debug module name*/
	char *dbg_mod_name;
	/** Debug module print level*/
	enum mei_dbg_lvl dbg_lvl;
};

#define MEI_DBG_PRINTF   printk
#define MEI_CRLF        "\n"
#define MEI_DBG_PREFIX  "MEI_DRV: "

#ifdef MEI_DEBUG_DISABLE
#define MEI_DEBUG(module, level, body)   ((void)0)
#else
#define MEI_DEBUG(module, level, body) \
	{\
		if ( (level) <= mei_dbg_lvl[module].dbg_lvl)\
		{\
			MEI_DBG_PRINTF body;\
		}\
	}
extern struct mei_dbg_info mei_dbg_lvl[];

#ifdef CONFIG_PROC_FS
#define MEI_PROCFS_MASK_DBG_MOD_COM   (0x1 << 8)
#define MEI_PROCFS_MASK_DBG_MOD_CMV   (0x2 << 8)
#define MEI_PROCFS_MASK_DBG_MOD_TST   (0x4 << 8)
# endif /* #   ifdef CONFIG_PROC_FS*/
#endif /** #ifdef MEI_DEBUG_DISABLE*/

/**
   Swap page header describes size in 32-bit words, load location,
   and image offset for program and/or data segments
*/
struct arc_swp_page_hdr {
	/** Offset bytes of progseg from beginning of image*/
	uint32_t p_offset;
	/** Destination addr of progseg on processor*/
	uint32_t p_dest;
	/** Size in 32-bitwords of program segment*/
	uint32_t p_size;
	/** Offset bytes of dataseg from beginning of image*/
	uint32_t d_offset;
	/** Destination addr of dataseg on processor*/
	uint32_t d_dest;
	/** Size in 32-bitwords of data segment*/
	uint32_t d_size;
};


/*
** Swap image header
*/
/** Flag used for program mem segment*/
#define GET_PROG  0
/**  Flag used for data mem segment*/
#define GET_DATA  1

/**
   Image header contains size of image, checksum for image, and count of
   page headers. Following that are 'count' page headers followed by
   the code and/or data segments to be loaded
*/
struct arc_img_hdr {
	/** Size of binary image in bytes*/
	uint32_t size;
	/** Checksum for image*/
	uint32_t checksum;
	/** Count of swp pages in image*/
	uint32_t count;
	/** Should be "count" pages - '1' to make compiler happy*/
	struct arc_swp_page_hdr page[1];
};

/** \todo add description!!!
*/
struct mei_smmu_mem_info {
	int32_t type;
	int32_t boot;
	uint32_t nCopy;
	uint32_t size;
	uint8_t *address;
	uint8_t *org_address;
};

#ifdef __KERNEL__
/** \todo add description!!!
*/
struct mei_dev_priv {
	/** */
	int32_t modem_ready;
	/** */
	int32_t arcmsgav;
	/** */
	int32_t cmv_reply;
	/** */
	int32_t cmv_waiting;
	/* Mei to ARC CMV count, reply count, ARC Indicator count*/
	int32_t modem_ready_cnt;
	/** */
	int32_t cmv_count;
	/** */
	int32_t reply_count;
#ifndef MEI_DEBUG_DISABLE
	/* Debug level change*/
	int32_t dbg_level_change;
#endif /* #ifndef MEI_DEBUG_DISABLE*/
	/** */
	uint32_t image_size;
	/** */
	int32_t bar;
	/** */
	uint16_t Recent_indicator[MSG_LENGTH];
	/** */
	uint16_t CMV_RxMsg[MSG_LENGTH] __attribute__ ((aligned (4)));
	/** */
	struct mei_smmu_mem_info adsl_mem_info[MAX_BAR_REGISTERS];
	/** */
	struct arc_img_hdr *img_hdr;
	/* to wait for arc cmv reply, sleep on wait_queue_arcmsgav*/
	wait_queue_head_t wait_queue_arcmsgav;
	/** */
	wait_queue_head_t wait_queue_modemready;
	/** */
	struct semaphore mei_cmv_sema;
};
#endif

struct mei_winhost_msg {
	union {
		uint16_t RxMessage[MSG_LENGTH] __attribute__ ((aligned (4)));
		uint16_t TxMessage[MSG_LENGTH] __attribute__ ((aligned (4)));
	} msg;
};

/******************************************************************************
 * DSL CPE API Driver Stack Interface Definitions
 * ****************************************************************************/
/** IOCTL codes for bsp driver */
#define DSL_IOC_MEI_BSP_MAGIC    's'

#define DSL_FIO_BSP_DSL_START	       	_IO  (DSL_IOC_MEI_BSP_MAGIC, 0)
#define DSL_FIO_BSP_RUN			_IO  (DSL_IOC_MEI_BSP_MAGIC, 1)
#define DSL_FIO_BSP_FREE_RESOURCE	_IO  (DSL_IOC_MEI_BSP_MAGIC, 2)
#define DSL_FIO_BSP_RESET		_IO  (DSL_IOC_MEI_BSP_MAGIC, 3)
#define DSL_FIO_BSP_REBOOT		_IO  (DSL_IOC_MEI_BSP_MAGIC, 4)
#define DSL_FIO_BSP_HALT		_IO  (DSL_IOC_MEI_BSP_MAGIC, 5)
#define DSL_FIO_BSP_BOOTDOWNLOAD	_IO  (DSL_IOC_MEI_BSP_MAGIC, 6)
#define DSL_FIO_BSP_JTAG_ENABLE		_IO  (DSL_IOC_MEI_BSP_MAGIC, 7)
#define DSL_FIO_FREE_RESOURCE		_IO  (DSL_IOC_MEI_BSP_MAGIC, 8)
#define DSL_FIO_ARC_MUX_TEST		_IO  (DSL_IOC_MEI_BSP_MAGIC, 9)
#define DSL_FIO_BSP_REMOTE		_IOW (DSL_IOC_MEI_BSP_MAGIC, 10,\
					      uint32_t)
#define DSL_FIO_BSP_GET_BASE_ADDRESS	_IOR (DSL_IOC_MEI_BSP_MAGIC, 11,\
					      uint32_t)
#define DSL_FIO_BSP_IS_MODEM_READY	_IOR (DSL_IOC_MEI_BSP_MAGIC, 12,\
					      uint32_t)
#define DSL_FIO_BSP_GET_VERSION		_IOR (DSL_IOC_MEI_BSP_MAGIC, 13,\
					      struct mei_version)
#define DSL_FIO_BSP_CMV_WINHOST		_IOWR(DSL_IOC_MEI_BSP_MAGIC, 14,\
					      struct mei_winhost_msg)
#define DSL_FIO_BSP_CMV_READ		_IOWR(DSL_IOC_MEI_BSP_MAGIC, 15,\
					      struct mei_reg)
#define DSL_FIO_BSP_CMV_WRITE		_IOW (DSL_IOC_MEI_BSP_MAGIC, 16,\
					      struct mei_reg)
#define DSL_FIO_BSP_DEBUG_READ		_IOWR(DSL_IOC_MEI_BSP_MAGIC, 17,\
					      struct mei_debug)
#define DSL_FIO_BSP_DEBUG_WRITE		_IOWR(DSL_IOC_MEI_BSP_MAGIC, 18,\
					      struct mei_debug)
#define DSL_FIO_BSP_GET_CHIP_INFO	_IOR (DSL_IOC_MEI_BSP_MAGIC, 19,\
					      struct mei_chip_info)

#define DSL_DEV_MEIDEBUG_BUFFER_SIZES  512

struct mei_debug {
	uint32_t iAddress;
	uint32_t iCount;
	uint32_t buffer[DSL_DEV_MEIDEBUG_BUFFER_SIZES];
};

/**
   Structure is used for debug access only.
   Refer to configure option INCLUDE_ADSL_WINHOST_DEBUG
*/
struct mei_reg {
	/** Specifies that address for debug access */
	uint32_t iAddress;
	/** Specifies the pointer to the data that has to be written or returns
	    a pointer to the data that has been read out*/
	uint32_t iData;
};

/** \todo add description
*/
struct mei_dev {
	/** modem state, update by bsp driver, */
	int nInUse;
	/** \todo add description*/
	void *pPriv;
	/** mei base address */
	uint32_t base_address;
	/** irq number */
	int nIrq[2];
#define IFX_DFEIR    0
#define IFX_DYING_GASP  1
	/** \todo add description*/
	struct mei_debug lop_debugwr;
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0))
	/** \todo add description */
	struct module *owner;
#endif
};

#define MEI_PRIV(dev)  ((struct mei_dev_priv*)(dev->pPriv))

/**
   \todo add description!!!
*/
struct mei_version {
	/** Major number*/
	uint32_t major;
	/** Minor number*/
	uint32_t minor;
	/** Revision number*/
	uint32_t revision;
};

/** \todo add description
*/
struct mei_chip_info {
	/** Major number*/
	uint32_t major;
	/** Minor number*/
	uint32_t minor;
};

/**
   error code definitions
*/
enum mei_error {
	/** */
	DSL_DEV_MEI_ERR_SUCCESS		=  0,
	/** */
	DSL_DEV_MEI_ERR_FAILURE		= -1,
	/** */
	DSL_DEV_MEI_ERR_MAILBOX_FULL	= -2,
	/** */
	DSL_DEV_MEI_ERR_MAILBOX_EMPTY	= -3,
	/** */
	DSL_DEV_MEI_ERR_MAILBOX_TIMEOUT	= -4
};

/**
*/
enum mei_mem_access_type {
	/** */
	DSL_BSP_MEMORY_READ = 0,
	/** */
	DSL_BSP_MEMORY_WRITE,
};

/**
   LED funtinalities are DEPRECATED!
*/
enum mei_led_id {
	/** */
	DSL_LED_LINK_ID = 0,
	/** */
	DSL_LED_DATA_ID
};

/**
   LED funtinalities are DEPRECATED!
*/
enum mei_led_type {
	/** */
	DSL_LED_LINK_TYPE = 0,
	/** */
	DSL_LED_DATA_TYPE
};

/**
   LED funtinalities are DEPRECATED!
*/
enum mei_led_handler {
	/** */
	DSL_LED_HD_CPU = 0,
	/** */
	DSL_LED_HD_FW
};

/**
    ARC CPU mode
*/
enum mei_cpu_mode {
	/** ARC CPU halt */
	DSL_CPU_HALT=0,
	/** ARC CPU run */
	DSL_CPU_RUN,
	/** ARC CPU reset */
	DSL_CPU_RESET,
};


/*
   external functions (from the MEI Driver)
*/

/**
   Return driver context associated with given major and minor numbers.

   \param maj Major driver number.
   \param num Minor driver number.

   \return Pointer to driver context or NULL if nothing is found.

   \ingroup Internal
*/
extern struct mei_dev* mei_drv_handle_get(int32_t maj, int32_t num);

/**
   Delete driver context.

   \param nHandle Pointer to driver context.

   \return Error code.
   \ingroup Internal
*/
extern int mei_drv_handle_delete(struct mei_dev *nHandle);

/**
   This copies the firmware from secondary storage to 64k memory segment
   in SDRAM

   \param dev		 Pointer to driver context.
   \param pBuf		 Pointer to firmware image.
   \param size		 Firmware image size.
   \param pLoff		 Offset of the firmware from pBuf pointer.
   \param pCurrentOffset Pointer to variable where copied firmware size
			 to be stored.

   \note There is no pointer validation implemented, be careful!

   \return Error code.
   \ingroup Internal
*/
extern enum mei_error mei_fw_download(struct mei_dev *dev, const char *pBuf,
				      uint32_t size, int32_t *pLoff,
				      int32_t *pCurrentOffset);

/**
   IOCTL call.

   \param dev 		Pointer to driver context.
   \param command 	IOCTL number.
   \param lon 		Call parameter.

   \return Error code.
   \ingroup Internal
*/
extern int32_t mei_kernel_ioctl(struct mei_dev *dev, uint32_t command,
				uint32_t lon);

/**
   Send a message to ARC and read the response
   This function sends a message to arc, waits the response, and reads
   the responses.

   \param dev      	device pointer
   \param request      	Pointer to the request
   \param reply      	Wait reply or not.
   \param response	Pointer to the response

   \return  DSL_DEV_MEI_ERR_SUCCESS or DSL_DEV_MEI_ERR_FAILURE
   \ingroup Internal
*/
extern enum mei_error mei_cmv_send(struct mei_dev *dev, uint16_t *request,
				   int reply, uint16_t *response);

/**
   Call deprecated.

   \return  DSL_DEV_MEI_ERR_SUCCESS, always.
   \ingroup Internal
*/
extern enum mei_error mei_adsl_led_init(struct mei_dev *dev,
					enum mei_led_id nLedNumber,
					enum mei_led_type nType,
					enum mei_led_handler handler);

/**
   Update data rates and enter showtime.

   \param dev 		Pointer to driver context.
   \param rate_fast 	Fast data rate.
   \param nRateIntl 	Interleaved data rate.

   \return DSL_DEV_MEI_ERR_SUCCESS
   \ingroup Internal
*/
extern enum mei_error mei_showtime_entry(struct mei_dev *dev,
					 uint32_t rate_fast,
					 uint32_t rate_intl);

/**
   Exit showtime.

   \param dev Pointer to driver context.

   \return DSL_DEV_MEI_ERR_SUCCESS
   \ingroup Internal
*/
extern enum mei_error mei_showtime_exit(struct mei_dev *dev);

/**
   Set up LED callback.

   \param ifx_adsl_ledcallback Pointer to callback function.

   \return 0
   \ingroup Internal
*/
extern int32_t mei_atm_led_cb_register(int (*ifx_adsl_ledcallback)(void));

/**
   Accress DFE memory.
   This function provide a way to access DFE memory;

   \param dev      	the device pointer
   \param type      	read or write
   \param destaddr   	destination address
   \param databuff   	pointer to hold data
   \param databuffsize	size want to read/write

   \return  DSL_DEV_MEI_ERR_SUCCESS or DSL_DEV_MEI_ERR_FAILURE
   \ingroup Internal
*/
extern enum mei_error mei_dbg_mem_access(struct mei_dev *dev,
					 enum mei_mem_access_type type,
					 uint32_t dst_addr, uint32_t *buf,
					 uint32_t sz);

/* Function aliases*/
/** \todo remove after refactoring the upper layer parts (CPE API ,etc)
*/
#define DSL_BSP_DriverHandleGet		mei_drv_handle_get
#define DSL_BSP_DriverHandleDelete	mei_drv_handle_delete
#define DSL_BSP_FWDownload		mei_fw_download
#define DSL_BSP_KernelIoctls		mei_kernel_ioctl
#define DSL_BSP_SendCMV			mei_cmv_send
#define DSL_BSP_AdslLedInit		mei_adsl_led_init
#define DSL_BSP_Showtime		mei_showtime_entry
#define DSL_BSP_ShowtimeExit		mei_showtime_exit
#define DSL_BSP_ATMLedCBRegister	mei_atm_led_cb_register
#define DSL_BSP_MemoryDebugAccess	mei_dbg_mem_access
#define DSL_BSP_EventCBRegister		mei_event_cb_register
#define DSL_BSP_EventCBUnregister	mei_event_cb_unregister

extern volatile struct mei_dev *adsl_dev;

/**
   Dummy structure by now to show mechanism of extended data that will be
   provided within event callback itself.
*/
typedef struct
{
	/** Dummy value */
	uint32_t nDummy1;
} DSL_BSP_CB_Event1DataDummy_t;

/**
   Dummy structure by now to show mechanism of extended data that will be
   provided within event callback itself.*/
typedef struct
{
	/** Dummy value */
	uint32_t nDummy2;
} DSL_BSP_CB_Event2DataDummy_t;

/**
   encapsulate all data structures that are necessary for status event
   callbacks.*/
union mei_cb_data {
	DSL_BSP_CB_Event1DataDummy_t dataEvent1;
	DSL_BSP_CB_Event2DataDummy_t dataEvent2;
};


enum mei_cb_type {
	/**
	Informs the upper layer driver (DSL CPE API) about a reboot request
	from the firmware.
	\note This event does NOT include any additional data.
	More detailed information upon reboot reason has to be requested from
	upper layer software via CMV (INFO 109) if necessary. */
	DSL_BSP_CB_FIRST = 0,
	DSL_BSP_CB_DYING_GASP,
	DSL_BSP_CB_CEOC_IRQ,
	DSL_BSP_CB_FIRMWARE_REBOOT,
	/**
	Delimiter only */
	DSL_BSP_CB_LAST
};

/**
   Specifies the common event type that has to be used for registering and
   signalling of interrupts/autonomous status events from MEI BSP Driver.

   \param dev Context pointer from MEI BSP Driver.
   \param IFX_ADSL_BSP_CallbackType_t Specifies the event callback type (reason
				      of callback). Regrading to the
				      setting of this value the data which is
				      included in the following union might have
				      different meanings.
				      Please refer to the description of the
				      union to get information about
				      the meaning of the included data.
   \param data Data according to \ref mei_cb_data. If this pointer
                is NULL there is no additional data available.

   \return depending on event
*/
typedef int32_t (*mei_event_cb_funct_t) (struct mei_dev *dev,
					 enum mei_cb_type nCallbackType,
					 union mei_cb_data *data);

/**
	Structure descibed callback function.
*/
struct mei_event_cb {
	/** Callback function address.*/
	mei_event_cb_funct_t function;
	/** Event to be processed.*/
	enum mei_cb_type event;
	/** Pointer to function context.*/
	union mei_cb_data *data;
};

/**
	Register callback function.

	\param pCallback
	Pointer to mei_event_cb variable describes callback to
	register.

	\return -EINVAL parameter invalid
	  -1      if the event already has a callback function registered.
	   0      success
*/
extern int32_t mei_event_cb_register(struct mei_event_cb *cb);

/**
	Remove callback function.

	\param pCallback
	Pointer to mei_event_cb variable describes callback to remove.

	\return -EINVAL parameter invalid
	  -1      if the event has no a callback function registered.
	   0      success
*/
extern int32_t mei_event_cb_unregister(struct mei_event_cb *cb);

/** Modem states */
#define DSL_DEV_STAT_InitState		0x0000
#define DSL_DEV_STAT_ReadyState		0x0001
#define DSL_DEV_STAT_FailState		0x0002
#define DSL_DEV_STAT_IdleState		0x0003
#define DSL_DEV_STAT_QuietState		0x0004
#define DSL_DEV_STAT_GhsState		0x0005
#define DSL_DEV_STAT_FullInitState	0x0006
#define DSL_DEV_STAT_ShowTimeState	0x0007
#define DSL_DEV_STAT_FastRetrainState	0x0008
#define DSL_DEV_STAT_LoopDiagMode	0x0009
#define DSL_DEV_STAT_ShortInit		0x000A/* Bis short initialization */

#define DSL_DEV_STAT_CODESWAP_COMPLETE	0x0002

#ifdef CONFIG_BSP_RTT

#define DSL_DEV_IP_ADDRESS_OCTETS	4
#define MAXIMUM_PACKET_SIZE		544 /*in bytes*/

/**
	Real time trace packet header structure.
*/
struct rtt_header {
	/** Packet status indicator. Should be set to 0xDEADBEEF in valid
	    packet. */
	uint32_t id;
	/** Copy FSM Rx state.*/
	uint32_t state;
	/** Copy symbol count (training & show time). */
	uint32_t frame;
	/** Copy super flame count during show time. */
	uint32_t sframe;
	/** Can be used for additional information, ex. Compression etc. */
	uint32_t skip_criteria;
	/** Indicates the "number" of Trace Dump (as sequence ID). */
	uint32_t trace_seq_id;
	/** Size of the Trace dump (i.e. packet size (header+payload)
	    in bytes) */
	uint32_t trace_size;
	/** Information of the trace, like FFT data, last packe info,
	    error etc. */
	uint32_t trace_info;
};

/**
   Real time trace packet structure.
*/
union rtt_packet {
	/** Packet header. */
	struct rtt_header header;
	/** Real Time Trace data. */
	uint8_t rawData[MAXIMUM_PACKET_SIZE];
};

/**
   Structure for initialization of the MEI Driver related RTT functionality.
   This structure has to be used for ioctl \ref DSL_FIO_BSP_RTT_INIT
*/
struct mei_rtt_init {
	/** Ip address of PC host to connect with */
	uint8_t ip_addr[DSL_DEV_IP_ADDRESS_OCTETS];
	/** Port number of PC host to connect with */
	uint16_t port;
};

/**
   Structure for configuration of the MEI Driver related RTT functionality.
   This structure has to be used for ioctl \ref DSL_FIO_BSP_RTT_CONFIG

   \note This configuration parameters are basically important for partitioning
         of the first level shared circular buffer. The configuration that is
         necessary on CMV level will be already done via the API within context
         of the link configuration which is done after firmware download and
         link activation.
*/
struct mei_rtt_config {
	/** Configures the first tone index to capture FFT data. */
	uint16_t start_idx;
	/** Configures the number of FFT values to be captured. */
	uint16_t size;
	/** Configures the criteria to stop the FFT data trace dump. */
	uint16_t stop_criteria;
};

/**
   Structure for getting statistics counter that are related to RTT packet
   processing.
   This structure has to be used for ioctl \ref DSL_FIO_BSP_RTT_STATISTICS_GET
*/
struct mei_rtt_statistics {
	/** Includes information about how many RTT-packets has been
	    successfully processed (send via UDP/TCP) within MEI Driver.
	    \note This is a running 32-bit wrap around counter. */
	uint32_t sw_pkt_send;
	/** Includes information about how many RTT-packets has been dropped by
	    the software. The software will for example drop a packet in case of
	    - an overflow of the second level FIFO buffer
	    \note This is a running 32-bit wrap around counter. */
	uint32_t sw_pkt_dropped;
};

/**
   This ioctl initializes the Real Time Trace (RTT) functionality of the MEI
   Driver.
   This ioctl shall be only used in case the higher layer software (DSL CPE API)
   has already checked that the firmware supports RTT functionality in general.

   \param struct mei_rtt_init* The parameter points to a \ref mei_rtt_init
			       structure

   \return
      0 if successful.
      Any other value indicates an error. */
#define DSL_FIO_BSP_RTT_INIT \
   _IOW (DSL_IOC_MEI_BSP_MAGIC, 20, struct mei_rtt_init)

/**
   This ioctl configures the Real Time Trace (RTT) functionality of the MEI
   Driver.

   \param struct mei_rtt_config*
      The parameter points to a \ref mei_rtt_config structure

   \return
      0 if successful.
      Any other value indicates an error. */
#define DSL_FIO_BSP_RTT_CONFIG \
   _IOW (DSL_IOC_MEI_BSP_MAGIC, 21, struct mei_rtt_config)

/**
   This ioctl return statistics counters of the Real Time Trace (RTT)
   functionality.

   \param mei_rtt_statistics*
      The parameter points to a \ref mei_rtt_statistics structure

   \return
      0 if successful.
      Any other value indicates an error. */
#define DSL_FIO_BSP_RTT_STATISTICS_GET \
   _IOW (DSL_IOC_MEI_BSP_MAGIC, 22, struct mei_rtt_statistics)

#endif /*#ifdef CONFIG_BSP_RTT*/

#endif /** IFXMIPS_MEI_H*/

