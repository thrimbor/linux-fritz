/*--- #define CONFIG_PCI_DEBUG ---*/

#ifdef CONFIG_PCI_DEBUG
	#define DbgPrint  printk 
#else
	#define DbgPrint(x...) 
#endif
/*
*	This is a file specific for yamuna soc. other soc's MAY work with this or may need modification
*	what this means is that CONFIG_AVALANCHE_MIPS_PCI is assumed defined, and hence the EVM_III related
*	code has been removed
*/
/* 
*	the kernel include files for definitions 
*/

#include <linux/types.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <asm/mips-boards/generic.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <linux/ioport.h>
#include <linux/kernel_stat.h>
#include <asm/io.h>
#include <linux/mm.h>
#include <linux/proc_fs.h>
#include <hw_pci.h>
#include <hw_reset.h>
#include <ur8_vlynq.h>
#include <ur8.h>
#include <ur8int.h>
#include <asm/mach_avm.h>
#include <hw_gpio.h>
#include <linux/reboot.h>

#include <asm/pci.h>
#include <asm/cacheflush.h>
/* get the cpu endian related files */
#if defined( CONFIG_CPU_LITTLE_ENDIAN )
	#include <linux/byteorder/little_endian.h>
#else
	#include <linux/byteorder/big_endian.h>
#endif

/*	this has the names of the pci config space addresses, so debug data reading is easier */
static char *pci_reg_names[] = 
{
	"Device_ID, Vendor_ID", "Status, Command", "ClassCode (3), RevID", "BIST, Header, Latency, CachelineSize",  
	"BAR 0", "BAR 1", "BAR 2", "BAR 3", "BAR 4", "BAR 5", 
	"CardBus ptr", "Subsys_ID, SubVendor_ID","ExpROM_addr","3 0, CapPtr","should_be_0", "max_lat, min-gnt, int_pin, int_line", 
	"powerCap, next, capID", "pow Data", 
	"reserved", "reserved", "reserved", "reserved", "reserved", "reserved", "reserved", "reserved", "reserved", "reserved", 
	"reserved", "reserved", "reserved", "reserved", "reserved", "reserved", "reserved", "reserved", "reserved", "reserved", 
	"reserved", "reserved", "reserved", "reserved", "reserved", "reserved", "reserved", "reserved", "reserved", "reserved", 
	"reserved", "reserved", "reserved", "reserved", "reserved", "reserved", "reserved", "reserved", "reserved", "reserved", 
	"reserved", "reserved", "reserved", "reserved", "reserved", "reserved", "reserved", "reserved", "reserved", "reserved", 
	"reserved", "reserved", "reserved", "reserved", "reserved", "reserved", "reserved", "reserved", "reserved", "reserved", 
	"reserved", "reserved", "reserved", "reserved", "reserved", "reserved", "reserved", "reserved", "reserved", "reserved"
};


/*----------------------------------
* the function prototypes,at the moment this seems
* general enough. it may even be possible to shift part 
* of this file into ti_generic directory as basic pci support
*/
static int pcibios_read_config( struct pci_bus *bus, unsigned int devfn, int where, int size, u32 * val);
static int pcibios_write_config(struct pci_bus *bus, unsigned int devfn, int where, int size, u32 val);
static void setup_pci_master( void );
static void setup_pci_slave( void );
static int wait_pciif_ready( void );
static inline int cfg_read( u32 addr, u32 *val, u32 size, u32 actual_addr );
static inline int cfg_write( u32 addr, u32 val, u32 size, u32 actual_addr );


/* default timeour for wait_pciif_ready() */
static unsigned int ur8_pci_max_wait_time = 0x100; /* the real value is calculated later in pci_hw_init() below */

/*local functions declaration */
//int setup_unified_pci_interrupt(void);
/*
*--- PCI Ops structure for registering our controller with the linux pci subsystem
*/ 
static struct pci_ops ur8_pci_ops = {
	.read = pcibios_read_config,
	.write = pcibios_write_config,
};

/*--- Resource allocation structures for registering with resource controller of linux */
 
static struct resource mem_resource = {
	.name   = "PCI Memory",
	.start  = UR8_PCI_MEM_START,
	.end    = UR8_PCI_MEM_END,
	.flags  = IORESOURCE_MEM,
};
 
static struct resource io_resource = {
	.name   = "PCI I/O",
	.start  = UR8_PCI_IO_START,
	.end    = UR8_PCI_IO_END, 
	.flags  = IORESOURCE_IO,
};
/*----- The structure for registering controller with linux pci system */
static struct pci_controller  ur8_pci_controller = {
	.pci_ops        = &ur8_pci_ops,
	.io_resource    = &io_resource,
	.mem_resource   = &mem_resource,
	.mem_offset     = 0x00000000UL,
	.io_offset      = 0x00000000UL,
};

/* Reboot notify handling */
int ur8_pci_reboot_notify_handler(struct notifier_block *nb, unsigned long event, void *buf)
{
    if ((event != SYS_RESTART) && (event != SYS_HALT) && (event != SYS_POWER_OFF))
        return (NOTIFY_DONE);

    printk("ur8_pci reboot notify handler\n");
    /* PCI core reset */
    ur8_put_device_into_reset(PCI_RESET_BIT);
    /* WLAN and PCI reset to 0/active */	
    ur8_put_device_into_reset(PCI_RESET_OUT_BIT);

    return (NOTIFY_OK);
}

static struct notifier_block ur8_pci_reboot_notifier = {
    ur8_pci_reboot_notify_handler,
    NULL,
    1
};

/*------------------------------------------
*
* the decoding of device_fn and where to the value to put on the pci address bus
* while configuring IDSEL has to be made high. idsel is created from address bus lines only
* a0-a2 MUST be 0, a0-a7 contain the offset in configspace 256 byte block
* a8-a10 contain 3 byte function index of possible 8 device functions
* a11 is tied to idsel of first possible dev
* a12 to next and so on for possible 15 devices.
* the shifts just put the values in proper place
* note : actually even the bus number is available, but we have no bus except 0
* so we never do anything with it, as we are NOT a pci-pci bridge
*/
#ifdef CONFIG_UR8_EVM
#define CFG_COMMAND( device_fn, where) ( (1<<( PCI_SLOT(device_fn)+11)) | (where & ~3) | (PCI_FUNC(device_fn)<< 8) )
#else
//#define CFG_COMMAND( device_fn, where)   ( 0x40000000 | (where & ~3) | (PCI_FUNC(device_fn)<< 8) ) 
#define CFG_COMMAND( device_fn, where) ( (1<<( PCI_SLOT(device_fn)+16)) | (where & ~3) | (PCI_FUNC(device_fn)<< 8) )
#endif
/*---APIs to access configuration space------
* given a 32 bit data read, extract the correct byte. 
*/
static inline u32	get_byte_enables( u32 size, u32 addr )
{
	u32 mask = 0xf;

	addr = addr & 3;					/* we are interested only in the lower 2 bits */
	if ( ( size == 2 ) && (addr == 3 ) )
		BUG();					/* a word access cant span dwords, so panic */
	if ( size == 1 )
		mask = 1;
	else if ( size == 2 )
		mask = 3;
	else
		mask = 0xf;
	if ( addr )
		mask = mask << addr;				/* rotate the mask into the right place */
	return mask << 4;					/* return as byte enable value, put in place for including in the pci command */
}
/*
* get the correct placement of the data for the byte enables
*/
static inline u32	setup_bus_data( u32 data, u32 size, u32 addr )
{
	addr = addr & 3;
	if ( (size == 2) && (addr == 3 ) )
		BUG( );
	if ( size == 4 )
		return data;
	if ( size == 1 )
		data = data & 0xff;
	else
		data = data & 0xffff;
	if ( addr )
		data = data << (addr*8);
	return data;
}
/* get the correct result of the data if it was a read operation */
static inline u32	setup_bus_result( u32 data, u32 size, u32 addr )
{
	addr = addr & 3;
	if ( size == 4 )
		return data;
	addr *= 8;
	if ( size == 1 )
		data = ( data >> addr ) & 0xff;
	else
		data = ( data >> addr ) & 0xffff;
	return data;
}
/* Clear the aborts in preparation for sending a pci command */
static inline void clear_aborts(void)
{//       DbgPrint( "[clear_aborts]\n", UR8_PCI_DATA );
	UR8_PCI_INTR_CLEAR = UR8_PCI_MASTER_ABORT;
	UR8_PCI_INTR_CLEAR = UR8_PCI_TARGET_ABORT;
}


/*
* wait until the pciif is ready
* signalled by the busy/command bit turning off
* if there was no device, master abort will be turned on
* as of early silicon, the master abort is not completely reliable
*/ 
static int wait_pciif_ready( )
{
	unsigned int i;
    static int last_jiffies;
	
	DbgPrint( "%s: state %lx\n" , __func__, UR8_PCI_INTR_STATE);
	//for ( i = 0; i < ur8_pci_max_wait_time; i++ )
	for ( i = 0; i < 16000; i++ ) {

		if ( UR8_PCI_INTR_STATE & UR8_PCI_MASTER_ABORT ) {
            if((last_jiffies == 0) || (jiffies - last_jiffies) > HZ) {
                printk(KERN_ERR "[wait_pciif_ready] UR8_PCI_MASTER_ABORT! %08lx. retry=%d\n", UR8_PCI_DATA, i );
                last_jiffies = jiffies;
            }
			UR8_PCI_INTR_CLEAR = UR8_PCI_MASTER_ABORT;
			//return 0; /* 1;  */
		}
			/* if a target aborted things, indicate error */
		if ( UR8_PCI_INTR_STATE & UR8_PCI_TARGET_ABORT ) {
			DbgPrint( " %lx ", UR8_PCI_INTR_STATE );
			DbgPrint( "Target Aborted but transaction complete ? \n" );
			UR8_PCI_INTR_CLEAR = UR8_PCI_TARGET_ABORT;
			return -1;
		}
		udelay(1);
		
		if ( (UR8_PCI_COMMAND & UR8_PCI_BUSY_MASK ) == UR8_PCI_BUSY )
			continue;

		/* everything is ok */
		return 0;
	}
	/* the pciif ready never got asserted, so indicate error */
	printk(KERN_ERR " PCI Interface Not Ready (i=%i), state = 0x%lu\n" , i, UR8_PCI_INTR_STATE);
	return -1;
}
/*
* the routine to actually write to the config space
*/
static inline int cfg_write( u32 addr, u32 val, u32 size, u32 actual_addr )
{
	/* wait for the pciif to finish any earlier transaction */
	wait_pciif_ready();

	/* set up the config space address in pciif */
	UR8_PCI_ADDRESS = UR8_PCIIF_MASTER_DEVICE;		/* address the master's vendor-id reg */
	UR8_PCI_DATA = 0xdeaddead;				/* setup a bad value on data */
	UR8_PCI_COMMAND = UR8_PCI_WRITE_CFG | 0xf0;		/* this is a dummy dword write to the pciif slave, the reg is readonly */
	wait_pciif_ready( );					/* this seems required to get the fifos into a known state */

	UR8_PCI_ADDRESS = addr;
	UR8_PCI_DATA = setup_bus_data( val, size, actual_addr );	/* now do the actual write */
	UR8_PCI_COMMAND = UR8_PCI_WRITE_CFG | get_byte_enables( size,  actual_addr );
	return wait_pciif_ready();
}
/*
* the routine to actually read the config space
*/
static inline int cfg_read( u32 addr, u32 *val, u32 size, u32 actual_addr )
{
	int retval;
	/* wait for the pciif to become ready */
	wait_pciif_ready();

	UR8_PCI_ADDRESS = UR8_PCIIF_MASTER_DEVICE;
	UR8_PCI_COMMAND = UR8_PCI_READ_CFG | 0xf0;		// dummy dword read of master vendor-id, must always succeed 
	wait_pciif_ready( );

	UR8_PCI_ADDRESS = addr;
	UR8_PCI_COMMAND = UR8_PCI_READ_CFG | get_byte_enables( size, actual_addr );
	retval = wait_pciif_ready( );

	*val = setup_bus_result( UR8_PCI_DATA, size, actual_addr );
	return retval;
}

/*
* read config data from the pci bus devices.
* this routine is called from arch/mips/pci routines to scan devices and other stuff
* the routines get to know of this routine via the register_pci_controller which
* passes the address of this routine to them.
*/
static int pcibios_read_config(struct pci_bus *bus, unsigned int devfn, int where, int size, u32 * val)
{
	u32 data;
	int state;
	/* blank out the pciif master and slave, which are ALWAYS at adress 0x10000 on pcibus
	* this is because we do the master and slave programming directly and we 
	* DO NOT want the linux pci core to configure them. it would cause havoc in
	* the pci address space !!
	* 
	* we dont let the kernel detect our master and slave
	* so we configure them ourseleves when the last scan comes in at slot 0x1f
	*/
	//DbgPrint( "Slot# %i\n", PCI_SLOT( devfn ) );
	DbgPrint( "%s: devfn: %x, where %x, size %x, slot: %i\n",__func__,devfn,where,size,PCI_SLOT( devfn ) );
	if ( PCI_SLOT( devfn) == 0x1f )
	{
		/* bus scan is complete, so enable the slave and master, the base address has been programmed
			in the setup_pci_slave, so all we need to do is to enable the slave. it should be done
			here, and not in setup_pci_slave via mirror register, because we do not want bus scans to be 
			responded to by the slave at pci scan time */
		clear_aborts( );
		wait_pciif_ready( );
		UR8_PCI_ADDRESS = UR8_PCIIF_MASTER_DEVICE + 0x4;	/* read the slave command and status */
		UR8_PCI_COMMAND = UR8_PCI_ALL_BYTES | UR8_PCI_READ_CFG;
		udelay( 20 );								/* wait for the device to respond */
		UR8_PCI_DATA |= UR8_PCI_SLAVE_START;			/* enable busmaster and mem, io response */
		UR8_PCI_COMMAND = 0xf0 | UR8_PCI_WRITE_CFG;		/* give the command to the slave */
		udelay( 20 );
		printk(KERN_ERR "Master and Slave Enabled\n" );
	} 
	
	
	/* this is a hack because the master abort is not reliable due to req2
	* the pci slot is either 6 or 7 or 8, so 3 slots. others we just ignore
	*/

#ifndef CONFIG_UR8_EVM
#	if CONFIG_AVM_PCI_DEVICE_COUNT > 0
	if (   (PCI_SLOT(devfn) != CONFIG_AVM_PCI_DEV1_SLOT)
#	if CONFIG_AVM_PCI_DEVICE_COUNT > 1
		&& (PCI_SLOT(devfn) != CONFIG_AVM_PCI_DEV2_SLOT)
#	if CONFIG_AVM_PCI_DEVICE_COUNT > 2
		&& (PCI_SLOT(devfn) != CONFIG_AVM_PCI_DEV3_SLOT)
#	endif /* > 2 */
#	endif /* > 1 */
		)
#	endif /* > 0 */
	{
		return PCIBIOS_DEVICE_NOT_FOUND;
	}
	if (where == 0)	{
		printk(KERN_ERR "%s: accessing present PCI slot : %i, where %x, size %x\n",__func__,PCI_SLOT( devfn ),where,size);
	}
#else
	if ( PCI_SLOT(devfn) < MIN_PCI_SLOT_NO )
		return PCIBIOS_DEVICE_NOT_FOUND;
	if ( PCI_SLOT( devfn) > MAX_PCI_SLOT_NO )
		return PCIBIOS_DEVICE_NOT_FOUND;		
#endif
	/* now do the actual read */
	data = 0;
	state = cfg_read( CFG_COMMAND( devfn, where), &data, size, (u32) where );
	DbgPrint( "[PCI CfgRead] state is: %d,%x\n",state,data);
	
	/* if -1 is returned, there was either a target abort, or the pciif has not gone back
	* to ready state. if 1 is returned, there was a master abort, which means device_not_found
	*/
	if ( state < 0 ) {
		printk(KERN_ERR "%s, state is %x, returning FUNC_NOT_SUPPORTED\n",__func__,state);
		return PCIBIOS_FUNC_NOT_SUPPORTED;
	}
	if ( state ) {
		printk(KERN_ERR "%s, state is %x, returning DEVICE_NOT_FOUND\n",__func__,state);
		return PCIBIOS_DEVICE_NOT_FOUND;
	}
	/* let the kernel have the data	*/
	*val = data;
	/* on tests in EVMIII it was found necessary to blank the expansion ROM
	* this is commented out, but uncomment if u get into problems where
	* the rom is claiming a HUGE PCI space */
	/*
	if ( where == 0x30 )
	{
		*val = 0;
		return PCIBIOS_SUCCESSFUL;	
	}
	*/
	DbgPrint( "[PCI CfgRead]  Addr=%08lx Dev=%04x(at=%04x,size=%d) => Read=%08x Name=%s\n", UR8_PCI_ADDRESS, PCI_SLOT( devfn ), where, size, data, pci_reg_names[(where>>2) & 0x3f] );
	return PCIBIOS_SUCCESSFUL;
}








/* write config data to the pci bus devices.
* this routine is called from arch/mips/pci routines to setup devices and other stuff
* the routines get to know of this routine via the register_pci_controller which
* passes the address of this routine to them.
*/
static int pcibios_write_config(struct pci_bus *bus, unsigned int devfn, int where, int size, u32 data)
{
	int state;
	/* do the write to the config space */
	state = cfg_write( CFG_COMMAND( devfn, where ), data, size, where );
	if ( !state )
	{
		DbgPrint( "[PCI CfgWrite] Addr=%08lx Dev=%04x(At=%04x,Size=%d) <= Value=%08x Name=%s [OK]\n", UR8_PCI_ADDRESS, PCI_SLOT( devfn ), where, size, data, pci_reg_names[(where>>2) & 0x3f] );
		return PCIBIOS_SUCCESSFUL;
	}
	printk(KERN_ERR "[PCI CfgWrite] Addr=%08lx Dev=%04x(At=%04x,Size=%d) <= Value=%08x Name=%s [ERROR]\n", UR8_PCI_ADDRESS, PCI_SLOT( devfn ), where, size, data, pci_reg_names[(where>>2) & 0x3f] );
	return PCIBIOS_FUNC_NOT_SUPPORTED;
}










static void setup_pci_master()
{
	/* the master will respond to vbus access at vbus-pci-space- begin to end
	* and translate that onto the pci bus accesss at addresses
	* ( (addr) & 0xfff ffff ) | master_bar_1
	* since the pci routines will set the device bars to respond to the same address
	* we do a one to one translation, generically that is not required
	* only the first 4 bits actually get transferred to the register, rest are ignored
	* only in EVMII, there are master address translation registers
	* in yamuna, from the spec it appears as if it is done automatically, 
	* because there are no master translation registers.
	* for yamuna, there are no master translation registers, so it looks like
	* the translation from the specified vbus map to corresponding pci address is 
	* automatically done one to one
	*/
	/* enable the arbiter */
	//DbgPrint("Before Arbiter Enable UR8_PCI_ARBITER_CONTROL = %lx\n", UR8_PCI_ARBITER_CONTROL );
	printk(KERN_ERR "[setup_pci_master]\n");

	UR8_PCI_ARBITER_CONTROL |= UR8_PCI_ARBITER_ON;

	//DbgPrint("After Arbiter Enable UR8_PCI_ARBITER_CONTROL = %lx\n", UR8_PCI_ARBITER_CONTROL );
	/* enable the master */

	UR8_PCI_MASTER_CONTROL |= UR8_PCI_ENABLE_BUSMASTER;
}









//extern char *prom_getenv(const char *);
extern char *prom_getenv(char *name);

static void setup_pci_slave( )
{
	/* the slave will respond to the pci access by busmaster device doing dma.
	* the pci slave is supposed to be able to access the entire vbus address space
	* (ref avalanche.pdf. the internal sram to cs4(async ram) is marked as accessible
	* to PCIS. )
	* so the slave address space will be from 0 to beginning of pci space
	* so there must be NO slave translation, and the slave mask should be
	* 0xE000 0000 to exclude the pci space. ( this is necessary, otherwise it
	* becomes a deadlock ). it is possible to create spaces with holes if required
	* by programming the other slave registers.
	*/
	u32 slave_mask = 0;
	u32 mask = 0x80000000;
	u32 memsize = 0;
	int i;

	/*--- printk(KERN_ERR "[setup_pci_slave]\n"); ---*/
	/* the slave area size is supposed to be less than 2 GB because it is supposed to 
	* conver the SDRAM only, this is checked in compile time only in yamuna_pci.h , 
	* we check for 30 bits, because SDRAM better not be less that 256 bytes ! 
	*/
	if(prom_getenv("memsize") == (char *)NULL )
		memsize = 32*1024*1024;					/* default memory size is 32 MB */
	else
		memsize = simple_strtol(prom_getenv("memsize"), NULL, 0);

	/*--- printk(KERN_ERR "Exposing %d memory over PCI\n",memsize); ---*/

	for  ( i = 0; i != 30; i++ )
	{
		if ( memsize & mask )
			break;
		slave_mask |= mask;
		mask = mask >> 1;
	}
	if ( slave_mask > 0xffff0000 )
		WARN_ON( 1 );

	/* commented out. see note below. */
	/* UR8_PCI_SLAVE_MASK_0 = slave_mask; */	/* set the size of the slave response area */
	/*enable the prefecch abliity of memory*/
	/* UR8_PCI_SLAVE_MASK_0|=UR8_PCI_PREFETCH_ENABLE; */
	/*set prefecth value to 32*/
	UR8_PCI_SLAVE_CACHE |= PREFETCH_SIZE_32; 

	/* 
	set the mask to enable the pci bus to access vbus space. This should actually be the
	slave_mask calculated above. however, if that is used, the pci busmaster card is unable to
	access the sdram off the vbus. hence this is kept to the value 0xe000 0000 as in UR8_PCI_SLAVE_MASK_VALUE
	*/
	UR8_PCI_SLAVE_MASK_0 = UR8_PCI_SLAVE_MASK_VALUE | UR8_PCI_PREFETCH_ENABLE;		
	UR8_PCI_SLAVE_TRANS_0 = 0;					/* set the translation to 0 */

	/* 
	* enable slave base 0, disable slave base 1 and base 2, 3, 4, 5
	* the other bases could be used later to do specific area accesses 
	*/
	UR8_PCI_SLAVE_CONTROL |= UR8_PCI_SLAVE_0_ENABLE;
	UR8_PCI_SLAVE_CONTROL &= ~(UR8_PCI_SLAVE_1_ENABLE );
	UR8_PCI_SLAVE_CONTROL &= ~(UR8_PCI_SLAVE_2_ENABLE );
	UR8_PCI_SLAVE_CONTROL &= ~(UR8_PCI_SLAVE_3_ENABLE );
	UR8_PCI_SLAVE_CONTROL &= ~(UR8_PCI_SLAVE_4_ENABLE );
	UR8_PCI_SLAVE_CONTROL &= ~(UR8_PCI_SLAVE_5_ENABLE );
	UR8_PCI_SLAVE_0_MIRROR = UR8_SDRAM_BASE;  /* set the base address of slave to 0 */

	/* enable the slave */
	UR8_PCI_SLAVE_CMD_STATUS = 0xffffffff;			/* write 1's to initially clear everything that is 1 to clear */
	UR8_PCI_SLAVE_CMD_STATUS = 0;					/* then set all bits to 0 */
	UR8_PCI_SLAVE_CMD_STATUS = UR8_PCI_SLAVE_RUN_CMD;		/* turn on only the bits wanted */
}


/*
* The hardware initialization routine. 
*/

/* >= 1ms, < 23 s, (if clock speed 360 MHz) */
static void udelay_save(unsigned int usec) {

#define time_before_32bit(a,b)		\
	 (((int)(a) - (int)(b)) < 0)

	unsigned int mips_clock  = ur8_get_clock(avm_clock_id_cpu);
	volatile unsigned int end_tick, tick = read_c0_count();

	end_tick = ((mips_clock / 2 / 1000 / 1000) * (usec)) + tick;

	do { 
		tick = read_c0_count();
	} while (time_before_32bit(tick, end_tick));
}

//Ehemals: static void pci_hw_init ( void )
int __init ur8_pci_init ( void )
{
	unsigned int ur8_mips_speed;
	unsigned long deviceID;
	char *p;
	volatile struct _hw_gpio *G = (struct _hw_gpio *)UR8_GPIO_BASE;

	/*-- initialize timeout for wait_pciif_ready()-----*/

	/*--- printk(KERN_ERR "[ur8_pci_init]\n"); ---*/


	p = prom_getenv("wlan_cal");
	if (p && !strncmp(p,"disabled",8))
		return 0;

	ur8_mips_speed = ur8_get_clock(avm_clock_id_cpu);
        ur8_pci_max_wait_time = ((ur8_mips_speed /( UR8_PCI_SPEED * 1000000 ))* UR8_PCI_SLOTS * UR8_PCI_MAX_GRANT );

        if( (ur8_pci_max_wait_time < 0x200) || (ur8_pci_max_wait_time > 0x3000) ) /* to prevent short & very long delays */
        {
            ur8_pci_max_wait_time = 0x1000;
        }

    /*--- WLAN-Reset-GPIO registrieren ---*/
    ur8_register_reset_gpio(_GPIO_BIT_PREQ1, LOW_ACTIVE);

	do {
		/* Set PCI clock to 33 MHz */
		ur8_set_clock(avm_clock_id_pci, UR8_PCI_SPEED * 1000 * 1000);
        /*--- printk(KERN_ERR "[pci] default: %d MHz\n", UR8_PCI_SPEED); ---*/
		udelay_save(1000);

		/* PCI core reset */
		ur8_put_device_into_reset(PCI_RESET_BIT);
		udelay_save(50000);
		ur8_take_device_out_of_reset(PCI_RESET_BIT);
		udelay_save(2000);

		/* PCI reset pin to 1/inactive */
		ur8_take_device_out_of_reset(PCI_RESET_OUT_BIT);
		/* PCI reset buffer enable/drive PCI reset */
		ur8_take_device_out_of_reset(PCI_OUTPUT_EN_BIT);
		
#if (CONFIG_AVM_PCI_DEVICE_COUNT == 1) && ! defined (CONFIG_AVM_PCI_REQ_AND_GNT_1)
		/* use preq1 as GPIO for WLAN reset */
		G->OutputData2.Bits.preq1=1;
#endif

		/* wait for 150 ms */
		udelay_save(150000);
	
		/* WLAN and PCI reset to 0/active */	
		ur8_put_device_into_reset(PCI_RESET_OUT_BIT);
#if (CONFIG_AVM_PCI_DEVICE_COUNT == 1) && ! defined (CONFIG_AVM_PCI_REQ_AND_GNT_1)
		/* use preq1 as GPIO for WLAN reset */
		G->OutputData2.Bits.preq1=0;
#endif

		/* wait for 150 ms */
		udelay_save(150000);

		/* WLAN and PCI reset to 1/inactive */
#if (CONFIG_AVM_PCI_DEVICE_COUNT == 1) && ! defined (CONFIG_AVM_PCI_REQ_AND_GNT_1)
		/* use preq1 as GPIO for WLAN reset */
		G->OutputData2.Bits.preq1=1;
#endif
		udelay_save(150000);
		ur8_take_device_out_of_reset(PCI_RESET_OUT_BIT);
		udelay_save(2000);
	
		/*-----Set up PCIIF config registers-------------
		* set the pciif to enable arbiter, cfg_done  and other defaults
		* out of reset, the cfg done is off, so first set the PCI master and slave properly
		*/
		setup_pci_master();
		setup_pci_slave();
		clear_aborts();

		/* wait for 1s due to PCI spec */
		udelay_save(1000000);

		/* Sometimes the first bus access does not succeed. Test this here and do reset again
		 * in case of failure. 
		 */
		UR8_PCI_ADDRESS = UR8_PCIIF_MASTER_DEVICE;
		UR8_PCI_COMMAND = UR8_PCI_READ_CFG | 0xf0;

		wait_pciif_ready( );
		
		deviceID = setup_bus_result(UR8_PCI_DATA, 4, UR8_PCIIF_MASTER_DEVICE);

		if (deviceID == 0) {
			printk("PCI core not ready, resetting again ...\n");
		}

	} while (deviceID == 0);

	/*--- printk(KERN_ERR "Setup Complete, Enabling PCI Interface\n"); ---*/

	/* now turn on the cfg_done, to enable pci access, delay to let everything settle down */
	UR8_PCI_CFG_DONE_REG |= UR8_PCI_CFG_DONE;

	/*--- printk(KERN_ERR "PCI Interface Running\n"); ---*/

	/* setup resources, iomem is already defined with 0-~0
	* ioport is with 0-0ffff in arch/mips, but we need 0x7fff 0000 space
	* so increase the ioport_resource.end to the end, because at this stage
	* of kernel booting we cant split up resources */
	ioport_resource.start = 0;
	ioport_resource.end = ~(0UL);
	/* Registering the pci controller structure so that when the pci system gets
	* initialized it will know we exist and will use our routines that we
	* are passing to scan our hardware
	*/
	register_pci_controller( &ur8_pci_controller);

    register_reboot_notifier(&ur8_pci_reboot_notifier);

	/*--- printk(KERN_ERR "PCI controller initialized\n"); ---*/
	return 0;
}

/* 
* register for an early initialization call, before the kernel proper is initialized
* this will be called from the initialization control routines
*/
//late_initcall( ur8_pci_init );








/* this will be called when resources need to be updated */
void __init pcibios_update_resource(struct pci_dev *dev, struct resource *root, struct resource *res, int resource)
{
	unsigned long where, size;
	u32 reg;

	DbgPrint( "[pcibios update resource]\n" );
	where = PCI_BASE_ADDRESS_0 + (resource * 4);
	size = res->end - res->start;
	pci_read_config_dword(dev, where, &reg);
	reg = (reg & size) | (((u32)(res->start - root->start)) & ~size);
	pci_write_config_dword(dev, where, reg);
}




/*
* now the IO section. this is to be brought in only if there is a 
* IO possible on PCI. in short, __TI_AVALANCHE_PCI_FIX should be same as CONFIG_YAMUNA 
* right now, but could be different in later versions
* the __TI_AVALANCHE_PCI_FIX changes the <linux>/include/asm-mips/io.h to 
* create calls to ti_ioreadx/ti_iowritex to enable PCI io space
*/

//#define __TI_AVALANCHE_PCI_FIX

#if defined( __TI_AVALANCHE_PCI_FIX )






/*
* the following functions are implemented for the avalanche PCIIF io base
*/
/* get the correct placement of the data for the byte enables */
/* there is no endianness treatment here, because that is done in the read/write ifdef( CONFIG_SWAP_IO_SPACE ) */
static u32	setup_io_data( u32 data, u32 size, u32 addr )
{

	DbgPrint( "[setup_io_data]\n" );
	/* this is the same as setup_bus_data */
	addr = addr & 3;
	if ( (size == 2) && (addr == 3 ) )
		BUG( );
	if ( size == 4 )
		return data;
	if ( size == 1 )
		data = data & 0xff;
	else
		data = data & 0xffff;
	data = data << (addr*8);
	return data;
}
/* get the correct result of the data if it was a read operation */
/* there is no endianness treatment here, because that is done in the read/write ifdef( CONFIG_SWAP_IO_SPACE ) */
static u32	setup_io_result( u32 data, u32 size, u32 addr  )
{	DbgPrint( "[setup_io_result]\n" );
	/* a copy of setup_bus_result */
	addr = addr & 3;
	if ( size == 4 )
		return data;
	addr *= 8;
	if ( size == 1 )
		data = ( data >> addr ) & 0xff;
	else
		data = ( data >> addr ) & 0xffff;
	return data;
}

/*
* the routines NEED to be not static, so the rest of the kernel and modules can link to them
* the routines to do the ioreadb, writeb, readw, writew etc.
* these routines come in when the io.h that has been modified is used
* it contains generated calls to these routines for io when __TI_AVALANCHE_PCI_FIX is defined
*/
void ti_iowriteb( u8 val, volatile u8 *addr )
{
	DbgPrint( "[ti_iowriteb]\n" );
	if ( ( (u32)addr >= UR8_PCI_IO_START ) && ( (u32)addr <= UR8_PCI_IO_END ) )
	{
		wait_pciif_ready( );
		UR8_PCI_ADDRESS = (u32) addr & ( ~0x3 );
		UR8_PCI_DATA = setup_io_data( val, 1, (u32) addr );
		UR8_PCI_COMMAND = UR8_PCI_WRITE_IO | get_byte_enables( 1, (u32) addr );	
	}
	else
		*addr = val;	/* if we are not in pci space, do normal memory mapped io */
	return;
}
u8 ti_ioreadb( volatile u8 *addr)
{	
	u32 data;
	DbgPrint( "[ti_ioreadb]\n" );
	if ( ( (u32)addr >= UR8_PCI_IO_START ) && ( (u32)addr <= UR8_PCI_IO_END ) )
	{
#if defined( CONFIG_PCI_DEBUG )
//		ti_bread++;
#endif
		wait_pciif_ready( );
		UR8_PCI_ADDRESS = (u32) addr & ( ~0x3 );
		UR8_PCI_DATA = 0xffffffff; 		
		UR8_PCI_COMMAND = UR8_PCI_READ_IO | get_byte_enables( 1, (u32) addr );
		wait_pciif_ready( );
		data = UR8_PCI_DATA;
		data = setup_io_result( data, 1, (u32) addr );
	}
	else
		return *addr;
	return data;
}

void ti_iowritew( u16 val, volatile u16 *addr )
{
	DbgPrint( "[ti_iowritew]\n" );
	if ( ( (u32)addr >= UR8_PCI_IO_START ) && ( (u32)addr <= UR8_PCI_IO_END ) )
	{
#if defined( CONFIG_PCI_DEBUG )
//		ti_wwrite++;
#endif
		wait_pciif_ready( );
		UR8_PCI_ADDRESS = (u32) addr & ( ~0x3 );
#ifdef CONFIG_SWAP_IO_SPACE        
		UR8_PCI_DATA = setup_io_data( cpu_to_le16(val), 2, (u32) addr );
#else
        UR8_PCI_DATA = setup_io_data( val, 2, (u32) addr );
#endif        
		UR8_PCI_COMMAND = UR8_PCI_WRITE_IO | get_byte_enables( 2, (u32) addr );	
	}
	else
		*addr = val;
	return;
}

u16 ti_ioreadw( volatile u16 *addr)
{
	u32 data;
	DbgPrint( "[ti_ioreadw]\n" );
	if ( ( (u32)addr >= UR8_PCI_IO_START ) && ( (u32)addr <= UR8_PCI_IO_END ) )
	{
#if defined( CONFIG_PCI_DEBUG )
//		ti_wread++;
#endif
		wait_pciif_ready( );
		UR8_PCI_ADDRESS = (u32) addr & ( ~0x3 );
		UR8_PCI_DATA = 0xffffffff; 	
		UR8_PCI_COMMAND = UR8_PCI_READ_IO | get_byte_enables( 2, (u32) addr );	
		wait_pciif_ready( );
		data = UR8_PCI_DATA;
#ifdef CONFIG_SWAP_IO_SPACE        
		data = le16_to_cpu(setup_io_result( data, 2, (u32) addr ));
#else        
        data = setup_io_result( data, 2, (u32) addr );
#endif        
	}
	else
		return *addr;
	return data;
}

void ti_iowritel(  u32 val, volatile u32 *addr)
{	DbgPrint( "[ti_iowritel]\n" );
	if ( ( (u32)addr >= UR8_PCI_IO_START ) && ( (u32)addr <= UR8_PCI_IO_END ) )
	{
#if defined( CONFIG_PCI_DEBUG )
//		ti_lwrite++;
#endif
		wait_pciif_ready( );
#ifdef CONFIG_SWAP_IO_SPACE        
		UR8_PCI_DATA = cpu_to_le32(val);
#else
        UR8_PCI_DATA = val;
#endif        
		UR8_PCI_ADDRESS = (u32) addr & ( ~0x3 );
		UR8_PCI_COMMAND = UR8_PCI_WRITE_IO | UR8_PCI_ALL_BYTES;
	}
	else
		*addr = val;
	return;
}

u32 ti_ioreadl( volatile u32 *addr)
{
	u32 data;
	DbgPrint( "[ti_ioreadl]\n" );
	if ( ( (u32)addr >= UR8_PCI_IO_START ) && ( (u32)addr <= UR8_PCI_IO_END ) )
	{
		wait_pciif_ready( );
#if defined( CONFIG_PCI_DEBUG )
//		ti_lread++;
#endif
		UR8_PCI_ADDRESS = (u32) addr & ( ~0x3 );
		UR8_PCI_COMMAND = UR8_PCI_READ_IO | UR8_PCI_ALL_BYTES;
		wait_pciif_ready( );
        
#ifdef CONFIG_SWAP_IO_SPACE        
		data = le32_to_cpu(UR8_PCI_DATA);
#else   
        data = UR8_PCI_DATA;
#endif        
	}
	else
		return *addr;
	return data;
}

void ti_iowriteq( u64 val, volatile u64 *addr )
{	DbgPrint( "[ti_iowriteq]\n" );
	if ( ( (u32)addr >= UR8_PCI_IO_START ) && ( (u32)addr <= UR8_PCI_IO_END ) )
	{
		/* 64 bit pci io write on yamuna ?  the pciif is not capable of it */
		BUG( );
	}
	else
		*addr = val;
	return;
}

u64 ti_ioreadq( volatile u64 *addr)
{
	DbgPrint( "[ti_ioreadq]\n" );
	if ( ( (u32)addr >= UR8_PCI_IO_START ) && ( (u32)addr <= UR8_PCI_IO_END ) )
	{	
		/* 64 bit pci io read on avalanche ?  the pciif does not support it */
		BUG( );
	}
	else
		return *addr;
	return 0;
}
/*
* end of the IO routines section
*/

#endif		/* __TI_AVALANCHE_PCI_FIX */




#ifdef __TI_AVALANCHE_PCI_FIX
	EXPORT_SYMBOL( ti_ioreadq );
	EXPORT_SYMBOL( ti_ioreadl );
	EXPORT_SYMBOL( ti_ioreadw );
	EXPORT_SYMBOL( ti_ioreadb );
	EXPORT_SYMBOL( ti_iowriteq );
	EXPORT_SYMBOL( ti_iowritel );
	EXPORT_SYMBOL( ti_iowritew );
	EXPORT_SYMBOL( ti_iowriteb );
#endif

//#endif /* CONFIG_PCI */
