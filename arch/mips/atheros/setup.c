//#include <linux/config.h>
#include <linux/init.h>
#include <linux/tty.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/serial.h>
#include <linux/serial_core.h>
#include <linux/serial_reg.h>
#include <linux/serial_8250.h>
#include <linux/console.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/pci.h>
#include <linux/mtd/mtd.h>

#include <asm/reboot.h>
#include <asm/io.h>
#include <asm/time.h>
#include <asm/pgtable.h>
#include <asm/processor.h>
#include <asm/reboot.h>
#include <asm/system.h>
#include <asm/serial.h>
#include <asm/traps.h>

#include <atheros.h>
#include <asm/bootinfo.h>
#include <asm/bugs.h>
#include <asm/prom.h>

uint32_t ath_cpu_freq, ath_ahb_freq, ath_ddr_freq, ath_ref_freq, ath_uart_freq;
uint32_t serial_inited;

static int __init ath_init_ioc(void);
void serial_print(const char *fmt, ...);
void writeserial(char *str, int count);
void ath_sys_frequency(void);

void UartInit(void);

/*
 * Export AHB freq value to be used by Ethernet MDIO.
 */
EXPORT_SYMBOL(ath_ahb_freq);

/*
 * Export Ref freq value to be used by I2S module.
 */
EXPORT_SYMBOL(ath_ref_freq);


static char *reboot_status[] = {
#define RS_FIRMWARE_UPDATE         0
    "Firmware-Update",
#define RS_SOFT_REBOOT             1
    "Software-Reboot",
#define RS_SOFT_WATCHDOG           2
    "Software-NMI-Watchdog",
#define RS_NMI_WA                  3
    "Software-NMI-Workaround",
#define RS_BUS_ERROR               4
    "Bus Error",
#define RS_NMI_PON                 5
    "Power-On-Reboot"   /*--- last entry ---*/
};

#if defined(CONFIG_MACH_AR934x)
/*--- nutze internen SRAM ---*/
#define ATH_SRAM_PRINTKLEN      ((ATH_SRAM_SIZE / 2) + (ATH_SRAM_SIZE / 4))
#define ATH_REBOOT_LOGBUFFER    (ATH_SRAM_BASE_UNCACHED + ATH_SRAM_SIZE - ATH_SRAM_PRINTKLEN)  	
#else/*--- #if defined(CONFIG_MACH_AR934x) ---*/
#undef ATH_SRAM_PRINTKLEN 
#define ATH_REBOOT_LOGBUFFER    (0xA1000000 - 512)
#endif/*--- #else ---*//*--- #if defined(CONFIG_MACH_AR934x) ---*/

#define MAGIC_PRINK_WORD          0x5352414D
#define MAGIC_REBOOT_STATUS_WORD  0x53544130

#define MAGIC_REBOOT_STATUS_WORD_SET(a)   (((a) & 0xF) | MAGIC_REBOOT_STATUS_WORD)
#define MAGIC_REBOOT_STATUS_WORD_CHECK(a) (((a) & ~0xF)  == MAGIC_REBOOT_STATUS_WORD)

struct _ram_print_data {
    volatile unsigned int rebootstatus;
    volatile unsigned int magic_word;
    volatile unsigned int len;
    unsigned char data[1];
};
/*--------------------------------------------------------------------------------*\
 * im internen SRAM ablegen
\*--------------------------------------------------------------------------------*/
static void ath_savelog_to_ram(void) {
#if defined(CONFIG_TFFS_PANIC_LOG) && defined(ATH_SRAM_PRINTKLEN)
    extern unsigned long printk_get_buffer(char **p_log_buf, unsigned long *p_log_end, unsigned long *p_anzahl);
    struct _ram_print_data *psram_print = (struct _ram_print_data *)ATH_REBOOT_LOGBUFFER;
    char *buf;
    unsigned long end, len;
    unsigned long log_bufsize, limit;
    
    restore_printk();
    log_bufsize = printk_get_buffer(&buf, &end, &len);
    psram_print->len        = 0;
    psram_print->magic_word = MAGIC_PRINK_WORD;
    limit =  ATH_SRAM_PRINTKLEN - sizeof(struct _ram_print_data);
    
    /*--- printk("%s %d: len=%lu end=%lu log_bufsize =%lu limit=%lu\n",__func__, __LINE__, len, end, log_bufsize, limit); ---*/
    if(log_bufsize >= len) {
        /*--- alles im Buffer ---*/
        unsigned int start;
        if(limit > len) {
            start = 0;
            limit = len;
        } else {
            start = len - limit;
        }
        memcpy(psram_print->data, buf + start, limit);
    } else {
        if(limit > end) {
            unsigned int start_rest   = limit - end;
            unsigned int start_offset = log_bufsize - start_rest;
            memcpy(psram_print->data, buf + start_offset, start_rest);
            memcpy(psram_print->data + start_rest, buf, limit - start_rest);
        } else {
            unsigned int start = end - limit;
            memcpy(psram_print->data, buf + start, limit);
        }
    }
    psram_print->len        = limit;
    psram_print->magic_word = MAGIC_PRINK_WORD;
#endif /*--- #ifdef CONFIG_TFFS_PANIC_LOG ---*/
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void ath_restore_log_from_ram(void) {
#if defined(CONFIG_TFFS_PANIC_LOG) && defined(ATH_SRAM_PRINTKLEN)
    struct _ram_print_data *psram_print = (struct _ram_print_data *)ATH_REBOOT_LOGBUFFER;
    char buf[128];

    if((psram_print->len < ATH_SRAM_PRINTKLEN) && (psram_print->magic_word == MAGIC_PRINK_WORD)) {
        char *p          = psram_print->data;
        unsigned int idx = 0;
        unsigned int len = psram_print->len;
        psram_print->magic_word = 0;
        psram_print->len        = 0;
        printk("--------------- LOG found in SRAM (len = %u) ------------------------\n", len);
        while(len) {
            if(idx < (sizeof(buf) - 2)) {  
                if((*p != '\r') && (*p != '\n')) {
                    if((*p >= 0x80) || (*p < 0x20) && (*p != '\t')) {
                        buf[idx] = '?';
                    } else {
                        buf[idx] = *p;
                    }
                    p++, idx++, len--;
                    continue;
                }
                p++, len--;
            }
            if(idx) {
                buf[idx] = 0;
                printk("LOG:%s\n", buf);
                idx = 0;
            }
        }
        if(idx) printk("LOG: %s\n", buf);
        printk("---------------------------------------------------------------------\n");
    }
#endif/*--- #if defined(CONFIG_TFFS_PANIC_LOG) && defined(ATH_SRAM_PRINTKLEN) ---*/
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void ath_save_rebootstatus_to_ram(unsigned int idx) {
    struct _ram_print_data *pram_print = (struct _ram_print_data *)ATH_REBOOT_LOGBUFFER;
    
    if(!MAGIC_REBOOT_STATUS_WORD_CHECK(pram_print->rebootstatus)) {
        pram_print->rebootstatus = MAGIC_REBOOT_STATUS_WORD_SET(idx);
    }
}
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
static void ath_reboot_status_from_ram(void) {
    struct _ram_print_data *pram_print = (struct _ram_print_data *)ATH_REBOOT_LOGBUFFER;
    unsigned int status = RS_NMI_PON;

    if(MAGIC_REBOOT_STATUS_WORD_CHECK(pram_print->rebootstatus)) {
        status =  pram_print->rebootstatus & 0xF;
        pram_print->rebootstatus = 0x0;
    }
    if((status >= sizeof(reboot_status) / sizeof(reboot_status[0]))) {
        status = sizeof(reboot_status) / sizeof(reboot_status[0]) - 1;
    }
    printk(KERN_ERR"(c) AVM 2013, Reboot Status is: %s\n", reboot_status[status]);
    ath_restore_log_from_ram();
}
arch_initcall(ath_reboot_status_from_ram);
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void set_reboot_status_to_NMI(void) {
    ath_save_rebootstatus_to_ram(RS_SOFT_WATCHDOG);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void set_reboot_status_to_Update(void) {
    ath_save_rebootstatus_to_ram(RS_FIRMWARE_UPDATE);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ath_restart(char *command)
{
    unsigned int val = (1<<0);

    if (command) {
        if ( ! strncmp(command, "watchdog", sizeof("watchdog") - 1)) {
            val = (1<<1);
            ath_save_rebootstatus_to_ram(RS_SOFT_WATCHDOG);
            ath_savelog_to_ram();
        } else if ( ! strncmp(command, "nmi_workaround", sizeof("nmi_workaround") - 1)) {
            val = (1<<2);
            ath_save_rebootstatus_to_ram(RS_NMI_WA);
            ath_savelog_to_ram();
        } else if ( ! strncmp(command, "bus_error", sizeof("bus_error") - 1)) {
            ath_save_rebootstatus_to_ram(RS_BUS_ERROR);
            ath_savelog_to_ram();
        }
    } else {
        ath_save_rebootstatus_to_ram(RS_SOFT_REBOOT);
        if(oops_in_progress) {
            /*--- wir kommen aus panic() ---*/
            ath_savelog_to_ram();
        }
    }
#if !defined(CONFIG_MACH_AR724x)
    ath_reg_wr(ATH_SPARE_STICKY, val);   /*--- save-Rebootstatus ---*/
#endif/*--- #if !defined(CONFIG_MACH_AR724x) ---*/

#if defined(CONFIG_NMI_ARBITER_WORKAROUND)
    ath_reg_wr(ATH_WATCHDOG_TMR_CONTROL, ATH_WD_ACT_NONE);
    /*--- SPI-Mode ausschalten ---*/
    while(ath_reg_rd(ATH_SPI_FS) == 1) {
        printk("%s:spi-gpio-mode %x %x\n", __func__, ath_reg_rd(ATH_SPI_FS), ath_reg_rd(ATH_SPI_CLOCK));
        ath_reg_wr(ATH_SPI_FS, 0);
    }
    printk("%s:reset %x %x\n", __func__, ath_reg_rd(ATH_SPI_FS), ath_reg_rd(ATH_SPI_CLOCK));
    mdelay(500);
#endif/*--- #if defined(CONFIG_NMI_ARBITER_WORKAROUND) ---*/
    do {
		ath_reg_wr(ATH_RESET, RST_RESET_FULL_CHIP_RESET_MASK);
    } while(1);
}
EXPORT_SYMBOL(ath_restart);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ath_halt(void)
{
	printk(KERN_NOTICE "\n** You can safely turn off the power\n");
	while (1) ;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ath_power_off(void)
{
	ath_halt();
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
const char *get_system_type(void) 
{
    extern uint32_t ath_otp_read(uint32_t addr);
#ifdef CONFIG_ATH_EMULATION
#	define	ath_sys_type(x)	x " emu"
#else
#	define	ath_sys_type(x)	x
#	define	ath_sys_type_otp(x)	x " wmac not present"
#endif

    /*
     * Make sure WMAC is enabled.
     * Read Memory Address 0 of OTP and Check if WMAC is Disabled
     * If Disabled, do not initialize WMAC
     */
    /*--- #ifdef ATH_OTP_MEM_0FFSET_ZERO ---*/
#if 0
    if (ath_otp_read(ATH_OTP_MEM_0FFSET_ZERO) & ATH_OTP_WMAC_DISABLED) {
        return ath_sys_type_otp(CONFIG_ATH_SYS_TYPE);
    } else
#endif
    {
        return ath_sys_type(CONFIG_ATH_SYS_TYPE);
    }
}
EXPORT_SYMBOL(get_system_type);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#ifndef CONFIG_MACH_AR724x
int valid_wmac_num(u_int16_t wmac_num)
{
	return (wmac_num == 0);
}

/*------------------------------------------------------------------------------------------*\
 * HOWL has only one wmac device, hence the following routines
 * ignore the wmac_num parameter
\*------------------------------------------------------------------------------------------*/
int get_wmac_irq(u_int16_t wmac_num)
{
	return ATH_CPU_IRQ_WLAN;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned long get_wmac_base(u_int16_t wmac_num)
{
	return KSEG1ADDR(ATH_WMAC_BASE);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned long get_wmac_mem_len(u_int16_t wmac_num)
{
	return ATH_WMAC_LEN;
}

EXPORT_SYMBOL(valid_wmac_num);
EXPORT_SYMBOL(get_wmac_irq);
EXPORT_SYMBOL(get_wmac_base);
EXPORT_SYMBOL(get_wmac_mem_len);
#endif /*--- #ifndef CONFIG_MACH_AR724x ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if !defined(CONFIG_ATH_HS_UART) && defined(CONFIG_SERIAL_8250)
void __init ath_serial_setup(void)
{
	struct uart_port p;

	memset(&p, 0, sizeof(p));

	p.flags = (UPF_BOOT_AUTOCONF | UPF_SKIP_TEST);
	p.iotype = UPIO_MEM32;
	p.uartclk = ath_ahb_freq;
	p.irq = ATH_MISC_IRQ_UART;
	p.regshift = 2;
	p.mapbase = (u32) KSEG1ADDR(ATH_UART_BASE);
	p.membase = (void __iomem *)p.mapbase;

	if (early_serial_setup(&p) != 0)
		printk(KERN_ERR "early_serial_setup failed\n");

	serial_print("%s: early_serial_setup done..\n", __func__);

}
#endif /*--- #if !defined(CONFIG_ATH_HS_UART) && defined(CONFIG_SERIAL_8250) ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int __cpuinit get_c0_compare_int(void)
{
	//printk("%s: returning timer irq : %d\n",__func__, ATH_CPU_IRQ_TIMER);
	return ATH_CPU_IRQ_TIMER;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void __init plat_time_init(void)
{
	mips_hpt_frequency = ath_cpu_freq / 2;
	printk("%s: plat time init done\n", __func__);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int ath_be_handler(struct pt_regs *regs, int is_fixup)
{
#ifdef CONFIG_MACH_AR934x
	printk("ath data bus error: cause %#x epc %#lx\nrebooting...", read_c0_cause(), read_c0_epc());
	ath_restart("bus_error");
#else
	printk("ath data bus error: cause %#x\n", read_c0_cause());
#endif
	return (is_fixup ? MIPS_BE_FIXUP : MIPS_BE_FATAL);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void __init plat_mem_setup(void)
{
    char *s, *p;
    unsigned int memsize = 0;

    /*--- serial_print("[plat_mem_setup]:\n"); ---*/

#if 1
	board_be_handler = ath_be_handler;
#endif
	_machine_restart = ath_restart;
	_machine_halt = ath_halt;
	pm_power_off = ath_power_off;

    s = prom_getenv("memsize");
    /*--- serial_print("[plat_mem_setup]: \"memsize\" \"%s\"\n", s); ---*/
    if (s) {
		memsize = simple_strtoul(s, &p, 16);
	} else {
	  serial_print("[%s] memsize konnte nicht aus env ermittelt werden\n",__FUNCTION__);
	  BUG_ON(1);
	}

    printk("[%s] memsize 0x%x\n", __FUNCTION__, memsize);
    /*--- serial_print("[plat_mem_setup]: memsize 0x%x\n", memsize); ---*/

    add_memory_region(PHYS_OFFSET, memsize, BOOT_MEM_RAM);

	/*
	 ** early_serial_setup seems to conflict with serial8250_register_port()
	 ** In order for console to work, we need to call register_console().
	 ** We can call serial8250_register_port() directly or use
	 ** platform_add_devices() function which eventually calls the
	 ** register_console(). AP71 takes this approach too. Only drawback
	 ** is if system screws up before we register console, we won't see
	 ** any msgs on the console.  System being stable now this should be
	 ** a special case anyways. Just initialize Uart here.
	 */

	UartInit();

#ifdef CONFIG_MACH_AR933x
	/* clear wmac reset */
	ath_reg_wr(ATH_RESET, (ath_reg_rd(ATH_RESET) & (~ATH_RESET_WMAC)));
#endif
	serial_print("\n\rBooting %s\n", get_system_type());
#ifdef CONFIG_SERIAL_8250
    ath_serial_setup();
#endif /*--- #ifdef CONFIG_SERIAL_8250 ---*/
}

/*------------------------------------------------------------------------------------------*\
 * Early printk hack
\*------------------------------------------------------------------------------------------*/
u8 UartGetPoll(void) __attribute__ ((weak));
void UartPut(u8 byte) __attribute__ ((weak));

u8 UartGetPoll(void)
{
#if defined(CONFIG_ATH_HS_UART)   /*--- nicht als Modul und nicht als Console ---*/
	char ch;
	u_int32_t rx_data;

	do {
		rx_data = ath_reg_rd(0xB8500000);	// UART DATA Reg
	} while ((rx_data & 0x100) != 0x100);
	ch = rx_data & 0xff;
	ath_reg_wr(0xB8500000, 0x100);

	return ch;
#else
	while ((UART_READ(OFS_LINE_STATUS) & 0x1) == 0) ;
	return UART_READ(OFS_RCV_BUFFER);
#endif
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void UartPut(u8 byte)
{
#ifdef CONFIG_ATH_HS_UART
	u_int32_t tx_data;
#endif
	if (!serial_inited) {
		serial_inited = 1;
		UartInit();
	}

#ifdef CONFIG_ATH_HS_UART
	do {
		tx_data = ath_reg_rd(0xB8500000);	// UART DATA Reg
	} while ((tx_data & 0x200) != 0x200);

	tx_data = byte | 0x200;
	ath_reg_wr(0xB8500000, tx_data);
	//tx_data = ath_reg_rd(0xB8500000);

#else
	while (((UART_READ(OFS_LINE_STATUS)) & 0x20) == 0x0) ;
	UART_WRITE(OFS_SEND_BUFFER, byte);
#endif
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
extern int vsprintf(char *buf, const char *fmt, va_list args);
static char sprint_buf[1024];

void serial_print(const char *fmt, ...)
{
	va_list args;
	int n;

	va_start(args, fmt);
	n = vsprintf(sprint_buf, fmt, args);
	va_end(args);
	writeserial(sprint_buf, n);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void writeserial(char *str, int count)
{
	int i;
	for (i = 0; i < count; i++)
		UartPut(str[i]);

	UartPut('\r');
	memset(str, '\0', 1024);
	return;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int ath_serial_in(int offset)
{
	return UART_READ(offset);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ath_serial_out(int offset, int value)
{
	UART_WRITE(offset, (u8) value);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#include <asm/uaccess.h>
#define M_PERFCTL_EVENT(event)          ((event) << 5)
unsigned int clocks_at_start;

void start_cntrs(unsigned int event0, unsigned int event1)
{
	write_c0_perfcntr0(0x00000000);
	write_c0_perfcntr1(0x00000000);
	/*
	 * go...
	 */
	write_c0_perfctrl0(0x80000000 | M_PERFCTL_EVENT(event0) | 0xf);
	write_c0_perfctrl1(0x00000000 | M_PERFCTL_EVENT(event1) | 0xf);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void stop_cntrs(void)
{
	write_c0_perfctrl0(0);
	write_c0_perfctrl1(0);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void read_cntrs(unsigned int *c0, unsigned int *c1)
{
	*c0 = read_c0_perfcntr0();
	*c1 = read_c0_perfcntr1();
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int ath_ioc_open(struct inode *inode, struct file *file)
{
	return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static ssize_t ath_ioc_read(struct file *file, char *buf, size_t count, loff_t * ppos)
{
#ifdef CONFIG_ATH_HS_UART
	extern void ath_hs_uart_init(void);
#endif /* CONFIG_ATH_HS_UART */

	unsigned int c0, c1, ticks = (read_c0_count() - clocks_at_start);
	char str[256];
	unsigned int secs = ticks / mips_hpt_frequency;

	read_cntrs(&c0, &c1);
	stop_cntrs();
	sprintf(str, "%d secs (%#x) event0:%#x event1:%#x", secs, ticks, c0,
		c1);
	copy_to_user(buf, str, strlen(str));

	return (strlen(str));
}

#if 0
static void ath_dcache_test(void)
{
	int i, j;
	unsigned char p;

	for (i = 0; i < 4; i++) {
		for (j = 0; j < (10 * 1024); j++) {
			p = *((unsigned char *)0x81000000 + j);
		}
	}
}
#endif

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static ssize_t ath_ioc_write(struct file *file, const char *buf, size_t count,
		 loff_t * ppos)
{
	int event0, event1;

	sscanf(buf, "%d:%d", &event0, &event1);
	printk("\nevent0 %d event1 %d\n", event0, event1);

	clocks_at_start = read_c0_count();
	start_cntrs(event0, event1);

	return (count);
}

struct file_operations ath_ioc_fops = {
      open:ath_ioc_open,
      read:ath_ioc_read,
      write:ath_ioc_write,
};

/*------------------------------------------------------------------------------------------*\
 * General purpose ioctl i/f
\*------------------------------------------------------------------------------------------*/
static int __init ath_init_ioc()
{
	static int _mymajor;

	_mymajor = register_chrdev(77, "ATH_GPIOC", &ath_ioc_fops);

	if (_mymajor < 0) {
		printk("Failed to register GPIOC\n");
		return _mymajor;
	}

	printk("ATH GPIOC major %d\n", _mymajor);
	return 0;
}

device_initcall(ath_init_ioc);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned int ath_reboot_status;
unsigned int avm_reset_status(void) {
#if 0
    unsigned int val = ath_reg_rd(ATH_WATCHDOG_TMR_CONTROL);
    if(val & (0x1 << 31)) {
        return 2; /*--- Watchdog Reset ---*/
    }

#if !defined(CONFIG_MACH_AR724x)
    val = (ath_reg_rd(ATH_SPARE_STICKY) & 0x3);
    switch (val) {
        case 0:
        case 1:
        case 2:
        case 4:
            return val;
    }
#endif/*--- #if !defined(CONFIG_MACH_AR724x) ---*/
    
    return 0xff;
#else
    return ath_reboot_status;
#endif
}
EXPORT_SYMBOL(avm_reset_status);



/*--- Kernel-Schnittstelle für das LED-Modul ---*/
enum _led_event { /* DUMMY DEFINITION */ LastEvent = 0 };
int (*led_event_action)(int, enum _led_event , unsigned int ) = NULL;
EXPORT_SYMBOL(led_event_action);


