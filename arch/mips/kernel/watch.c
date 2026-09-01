/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2008 David Daney
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>

#include <asm/processor.h>
#include <asm/watch.h>
#include <asm/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>


/*
 * Install the watch registers for the current thread.  A maximum of
 * four registers are installed although the machine may have more.
 */
void mips_install_watch_registers(void)
{
	struct mips3264_watch_reg_state *watches =
		&current->thread.watch.mips3264;
	switch (current_cpu_data.watch_reg_use_cnt) {
	default:
		BUG();
	case 4:
		write_c0_watchlo3(watches->watchlo[3]);
		/* Write 1 to the I, R, and W bits to clear them, and
		   1 to G so all ASIDs are trapped. */
		write_c0_watchhi3(0x40000007 | watches->watchhi[3]);
	case 3:
		write_c0_watchlo2(watches->watchlo[2]);
		write_c0_watchhi2(0x40000007 | watches->watchhi[2]);
	case 2:
		write_c0_watchlo1(watches->watchlo[1]);
		write_c0_watchhi1(0x40000007 | watches->watchhi[1]);
	case 1:
		write_c0_watchlo0(watches->watchlo[0]);
		write_c0_watchhi0(0x40000007 | watches->watchhi[0]);
	}
}

/*
 * Read back the watchhi registers so the user space debugger has
 * access to the I, R, and W bits.  A maximum of four registers are
 * read although the machine may have more.
 */
void mips_read_watch_registers(void)
{
	struct mips3264_watch_reg_state *watches =
		&current->thread.watch.mips3264;
	switch (current_cpu_data.watch_reg_use_cnt) {
	default:
		BUG();
	case 4:
		watches->watchhi[3] = (read_c0_watchhi3() & 0x0fff);
	case 3:
		watches->watchhi[2] = (read_c0_watchhi2() & 0x0fff);
	case 2:
		watches->watchhi[1] = (read_c0_watchhi1() & 0x0fff);
	case 1:
		watches->watchhi[0] = (read_c0_watchhi0() & 0x0fff);
	}
	if (current_cpu_data.watch_reg_use_cnt == 1 &&
	    (watches->watchhi[0] & 7) == 0) {
		/* Pathological case of release 1 architecture that
		 * doesn't set the condition bits.  We assume that
		 * since we got here, the watch condition was met and
		 * signal that the conditions requested in watchlo
		 * were met.  */
		watches->watchhi[0] |= (watches->watchlo[0] & 7);
	}
 }

/*
 * Disable all watch registers.  Although only four registers are
 * installed, all are cleared to eliminate the possibility of endless
 * looping in the watch handler.
 */
void mips_clear_watch_registers(void)
{
	switch (current_cpu_data.watch_reg_count) {
	default:
		BUG();
	case 8:
		write_c0_watchlo7(0);
	case 7:
		write_c0_watchlo6(0);
	case 6:
		write_c0_watchlo5(0);
	case 5:
		write_c0_watchlo4(0);
	case 4:
		write_c0_watchlo3(0);
	case 3:
		write_c0_watchlo2(0);
	case 2:
		write_c0_watchlo1(0);
	case 1:
		write_c0_watchlo0(0);
	}
}

__cpuinit void mips_probe_watch_registers(struct cpuinfo_mips *c)
{
	unsigned int t;


	if ((c->options & MIPS_CPU_WATCH) == 0)
		return;
	/*
	 * Check which of the I,R and W bits are supported, then
	 * disable the register.
	 */
	write_c0_watchlo0(7);
	back_to_back_c0_hazard();
	t = read_c0_watchlo0();
	write_c0_watchlo0(0);
	c->watch_reg_masks[0] = t & 7;

	/* Write the mask bits and read them back to determine which
	 * can be used. */
	c->watch_reg_count = 1;
	c->watch_reg_use_cnt = 1;
	t = read_c0_watchhi0();
	write_c0_watchhi0(t | 0xff8);
	back_to_back_c0_hazard();
	t = read_c0_watchhi0();
	c->watch_reg_masks[0] |= (t & 0xff8);
	if ((t & 0x80000000) == 0)
		return;

	write_c0_watchlo1(7);
	back_to_back_c0_hazard();
	t = read_c0_watchlo1();
	write_c0_watchlo1(0);
	c->watch_reg_masks[1] = t & 7;

	c->watch_reg_count = 2;
	c->watch_reg_use_cnt = 2;
	t = read_c0_watchhi1();
	write_c0_watchhi1(t | 0xff8);
	back_to_back_c0_hazard();
	t = read_c0_watchhi1();
	c->watch_reg_masks[1] |= (t & 0xff8);
	if ((t & 0x80000000) == 0)
		return;

	write_c0_watchlo2(7);
	back_to_back_c0_hazard();
	t = read_c0_watchlo2();
	write_c0_watchlo2(0);
	c->watch_reg_masks[2] = t & 7;

	c->watch_reg_count = 3;
	c->watch_reg_use_cnt = 3;
	t = read_c0_watchhi2();
	write_c0_watchhi2(t | 0xff8);
	back_to_back_c0_hazard();
	t = read_c0_watchhi2();
	c->watch_reg_masks[2] |= (t & 0xff8);
	if ((t & 0x80000000) == 0)
		return;

	write_c0_watchlo3(7);
	back_to_back_c0_hazard();
	t = read_c0_watchlo3();
	write_c0_watchlo3(0);
	c->watch_reg_masks[3] = t & 7;

	c->watch_reg_count = 4;
	c->watch_reg_use_cnt = 4;
	t = read_c0_watchhi3();
	write_c0_watchhi3(t | 0xff8);
	back_to_back_c0_hazard();
	t = read_c0_watchhi3();
	c->watch_reg_masks[3] |= (t & 0xff8);
	if ((t & 0x80000000) == 0)
		return;

	/* We use at most 4, but probe and report up to 8. */
	c->watch_reg_count = 5;
	t = read_c0_watchhi4();
	if ((t & 0x80000000) == 0)
		return;

	c->watch_reg_count = 6;
	t = read_c0_watchhi5();
	if ((t & 0x80000000) == 0)
		return;

	c->watch_reg_count = 7;
	t = read_c0_watchhi6();
	if ((t & 0x80000000) == 0)
		return;

	c->watch_reg_count = 8;

}



#if defined(CONFIG_AVM_WP)
struct avm_wp watchpoints[NR_WATCHPOINTS];
int global_watch_count = 0;

int wp_enable_count = 0;
int wp_disable_count = 0 ;

int set_watchpoint(int watch_nr, int addr, int mask, int type){
	return set_watchpoint_handler(watch_nr, addr, mask, type, default_wp_handler);
}

int set_watchpoint_handler(int watch_nr, int addr, int mask, int type, void (*wp_handler)(int,int,int)){
	if ( ( watch_nr <= current_cpu_data.watch_reg_count ) && ( watch_nr < NR_WATCHPOINTS ) ){
		int lo_reg;
		switch( watch_nr ){
			case 0:
				write_c0_watchlo0(7);
				mb();
				lo_reg = read_c0_watchlo0();
				mb();
				if ( (lo_reg & type) == 0 ){
					printk("lo %#x\n", lo_reg);
					return SET_WATCHPOINT_FAIL_WRONG_TYPE;
				}

				write_c0_watchlo0( (addr & 0xFFFFFFF8) | (type & 0x7) );
				write_c0_watchhi0( (1 << 30) | (( mask & 0x1FF ) << 3 ) | 7 );
				break;

			case 1:
				write_c0_watchlo1(7);
				mb();
				lo_reg = read_c0_watchlo1();
				mb();
				if ( (lo_reg & type) == 0 ){
					printk("lo %#x\n", lo_reg);
					return SET_WATCHPOINT_FAIL_WRONG_TYPE;
				}

				write_c0_watchlo1( (addr & 0xFFFFFFF8 ) | (type & 0x7) );
				write_c0_watchhi1( (1 << 30) | (( mask & 0x1FF ) << 3 ) | 7 );
				break;

			case 2:
				write_c0_watchlo2(7);
				mb();
				lo_reg = read_c0_watchlo2();
				mb();
				if ( (lo_reg & type) == 0 ){
					printk("lo %#x\n", lo_reg);
					return SET_WATCHPOINT_FAIL_WRONG_TYPE;
				}

				write_c0_watchlo2( (addr & 0xFFFFFFF8) | (type & 0x7) );
				write_c0_watchhi2( (1 << 30) | (( mask & 0x1FF ) << 3 ) | 7);
				break;

			case 3:
				write_c0_watchlo3(7);
				mb();
				lo_reg = read_c0_watchlo3();
				mb();
				if ( (lo_reg & type) == 0 ){
					printk("lo %#x\n", lo_reg);
					return SET_WATCHPOINT_FAIL_WRONG_TYPE;
				}

				write_c0_watchlo3( ( addr & 0xFFFFFFF8 ) | (type & 0x7) );
				write_c0_watchhi3( (1 << 30) | (( mask & 0x1FF ) << 3 ) | 7);
				break;
			default:
				return SET_WATCHPOINT_FAIL_NO_REG;
		}

		watchpoints[watch_nr].addr = addr;
		watchpoints[watch_nr].mask = mask;
		watchpoints[watch_nr].type = type;
		watchpoints[watch_nr].handler = wp_handler;

		wp_enable_count++;

		return SET_WATCHPOINT_SUCCESS;

	} else {
		return SET_WATCHPOINT_FAIL_NO_REG;
	}

}

void del_watchpoint(int watch_nr ){

		switch( watch_nr ){
			case 0:
				write_c0_watchlo0( 0 );
				write_c0_watchhi0( 7 );
				break;
			case 1:
				write_c0_watchlo1( 0 );
				write_c0_watchhi1( 7 );
				break;
			case 2:
				write_c0_watchlo2( 0 );
				write_c0_watchhi2( 7 );
				break;
			case 3:
				write_c0_watchlo3( 0 );
				write_c0_watchhi4( 7 );
				break;
			default:
				return;
		}
		wp_disable_count++;
}

int watchpoint_busy( int watch_nr ){

	volatile int lo_reg = -1;

	switch( watch_nr ) {
	case 0:
		lo_reg = read_c0_watchlo0();
		break;
	case 1:
		lo_reg = read_c0_watchlo1();
		break;
	case 2:
		lo_reg = read_c0_watchlo2();
		break;
	case 3:
		lo_reg = read_c0_watchlo3();
		break;
	}

	return lo_reg;
}


void default_wp_handler( int status, int deferred, int epc ) {

	printk("[%s] status=%#x, deferred=%d, epc=%#x \n", __FUNCTION__, status, deferred, epc);
}

int avm_wp_dispatcher(void) {
	int handled = 0;
	int watch_nr;
	int status;
	int deferred = 0;
	volatile int hi_reg = 0;


	printk( "[%s] %#x\n", __FUNCTION__, global_watch_count );
	global_watch_count++;

	for (watch_nr = 0; watch_nr < NR_WATCHPOINTS; watch_nr++  ){
		switch(watch_nr){
			case 0:
				hi_reg = read_c0_watchhi0();
				break;
			case 1:
				hi_reg = read_c0_watchhi1();
				break;
			case 2:
				hi_reg = read_c0_watchhi2();
				break;
			case 3:
				hi_reg = read_c0_watchhi3();
				break;
		}
		mb();
		status = hi_reg & 7;

		if ( status && watchpoints[watch_nr].handler ){
			int epc;

			deferred = (read_c0_cause() & ( 1 < 22 )) ;
			epc = read_c0_epc();
			mb();
			watchpoints[watch_nr].handler(status, deferred, epc);
			handled = 1;
			break;
		}
	}

	// clear legs
	del_watchpoint( watch_nr );

	// clear cause -> wp
	if ( deferred ){
		volatile int cause = read_c0_cause();
		cause &= ~(1 << 22);
		write_c0_cause( cause );
	}
#if 0
	// neusetzen scheint im exception handler nicht möglich zu sein - das fuehrt zum Dauer-Interrupt
	msleep(2);
	// reset hi_reg
	switch( watch_nr ){
		case 0:
			write_c0_watchhi0( (1 << 30) | (( watchpoints[watch_nr].mask & 0x1FF ) << 3 ));
			break;
		case 1:
			write_c0_watchhi1( (1 << 30) | (( watchpoints[watch_nr].mask & 0x1FF ) << 3 ));
			break;
		case 2:
			write_c0_watchhi2( (1 << 30) | (( watchpoints[watch_nr].mask & 0x1FF ) << 3 ));
			break;
		case 3:
			write_c0_watchhi3( (1 << 30) | (( watchpoints[watch_nr].mask & 0x1FF ) << 3 ));
			break;
	}
#endif

	return handled;
}

#if defined(CONFIG_AVM_WP_PROC)

#define WP_READ_BUF_SIZE 1024
char wp_read_buff[WP_READ_BUF_SIZE];


static int proc_write_mem(struct file *file __attribute__ ((unused)), const char *buf, unsigned long count, void *data __attribute__ ((unused))) {
	int len;
	int addr;
	int ival;
	char cval;

	len = count < sizeof(wp_read_buff) ? count : sizeof(wp_read_buff) - 1;
	copy_from_user(wp_read_buff, buf, len);
	wp_read_buff[len] = 0;

	if ( sscanf( wp_read_buff, "readchar %x",  &addr ) == 1){
		char res;
		res = *((char *)addr);
		printk ("%#x: %c\n", addr, res) ;
	}

	if (sscanf( wp_read_buff, "readint %x",  &addr ) == 1){
		int res;
		res = *((char *)addr);
		printk ("%#x: %#x\n", addr, res) ;
	}

	if ( sscanf( wp_read_buff, "writechar %x %c",  &addr, &cval ) == 2){
		*((char *)addr) = cval;
		printk ("%#x: %c\n", addr, cval) ;
	}

	if (sscanf( wp_read_buff, "writeint %x %x",  &addr, &ival ) == 2){
		*((char *)addr) = ival;
		printk ("%#x: %#x\n", addr, ival) ;
	}

	return count;
}

static int proc_read_mem(char *page, char **start __attribute__ ((unused)),
		off_t off __attribute__((unused)), int count __attribute__ ((unused)), int *eof,
		void *data __attribute__ ((unused))) {
	int len = 0;

	len += sprintf(page + len, "Syntax: readchar|readint|writechar|writeint\n" );

	*eof = 1;
	return len;
}


static int proc_write_wp(struct file *file __attribute__ ((unused)), const char *buf, unsigned long count, void *data __attribute__ ((unused))) {
	int len;
	int rlen;

	int watch_nr;
	int addr;
	int mask;
	int type;

	len = count < sizeof(wp_read_buff) ? count : sizeof(wp_read_buff) - 1;
	rlen = copy_from_user(wp_read_buff, buf, len);
	wp_read_buff[len] = 0;
//	printk("parse: '%s'\n", wp_read_buff);

	if ((rlen = sscanf( wp_read_buff, "%d , %x , %x , %x", &watch_nr, &addr, &mask, &type )) == 4){
		int res;
		res = set_watchpoint(watch_nr, addr, mask, type);
		printk("set watchpoint: nr=%d, addr=%#x, mask=%#x, type=%#x, res = %d\n", watch_nr, addr, mask, type, res);
	} else {
		printk("(%d != 4): set watchpoint syntax: 'watch_nr(dec) addr(hex) mask(hex) type(hex)'\n", rlen);
	}

	return count;
}

static int proc_read_wp(char *page, char **start __attribute__ ((unused)),
		off_t off __attribute__((unused)), int count __attribute__ ((unused)), int *eof,
		void *data __attribute__ ((unused))) {
	int len = 0;

	len += sprintf(page + len, "-------- global wp count = %d ---------------\n", global_watch_count );
	len += sprintf(page + len, "-------- global wp_enable_count = %d ---------------\n", wp_enable_count );
	len += sprintf(page + len, "-------- global wp_disable_count = %d ---------------\n", wp_disable_count );
	len += sprintf(page + len, "wp0(lo): %#x \t-  wp0(hi): %#x\n", (unsigned int)read_c0_watchlo0(), (unsigned int)read_c0_watchhi0());
	len += sprintf(page + len, "wp1(lo): %#x \t-  wp1(hi): %#x\n", (unsigned int)read_c0_watchlo1(), (unsigned int)read_c0_watchhi1());
	len += sprintf(page + len, "wp2(lo): %#x \t-  wp2(hi): %#x\n", (unsigned int)read_c0_watchlo2(), (unsigned int)read_c0_watchhi2());
	len += sprintf(page + len, "wp3(lo): %#x \t-  wp3(hi): %#x\n", (unsigned int)read_c0_watchlo3(), (unsigned int)read_c0_watchhi3());

	*eof = 1;
	return len;
}
#endif

#if defined(CONFIG_DUMP_CP0)
#define NR_CP0_REGS 80
int cp0_dump_array[NR_CP0_REGS];

int read_cp0_array(int i) {
    if (i >= NR_CP0_REGS)
        return -1;
    else
        return cp0_dump_array[i];
}

void dump_cp0(void){
	 cp0_dump_array[0] = __read_32bit_c0_register($0, 0);
	 cp0_dump_array[1] = __read_32bit_c0_register($0, 1);
	 cp0_dump_array[2] = __read_32bit_c0_register($0, 2);
	 cp0_dump_array[3] = __read_32bit_c0_register($0, 3);
	 cp0_dump_array[4] = __read_32bit_c0_register($1, 0);
	 cp0_dump_array[5] = __read_32bit_c0_register($1, 1);
	 cp0_dump_array[6] = __read_32bit_c0_register($1, 2);
	 cp0_dump_array[7] = __read_32bit_c0_register($1, 3);
	 cp0_dump_array[8] = __read_32bit_c0_register($1, 4);
	 cp0_dump_array[9] = __read_32bit_c0_register($1, 5);
	 cp0_dump_array[10] = __read_32bit_c0_register($1, 6);
	 cp0_dump_array[11] = __read_32bit_c0_register($1, 7);
	 cp0_dump_array[12] = __read_32bit_c0_register($2, 0);
	 cp0_dump_array[13] = __read_32bit_c0_register($2, 1);
	 cp0_dump_array[14] = __read_32bit_c0_register($2, 2);
	 cp0_dump_array[15] = __read_32bit_c0_register($2, 3);
	 cp0_dump_array[16] = __read_32bit_c0_register($2, 4);
	 cp0_dump_array[17] = __read_32bit_c0_register($2, 5);
	 cp0_dump_array[18] = __read_32bit_c0_register($2, 6);
	 cp0_dump_array[19] = __read_32bit_c0_register($2, 7);
	 cp0_dump_array[20] = __read_32bit_c0_register($4, 0);
	 cp0_dump_array[21] = __read_32bit_c0_register($4, 2);
	 cp0_dump_array[22] = __read_32bit_c0_register($5, 0);
	 cp0_dump_array[23] = __read_32bit_c0_register($6, 0);
	 cp0_dump_array[24] = __read_32bit_c0_register($6, 1);
	 cp0_dump_array[25] = __read_32bit_c0_register($6, 2);
	 cp0_dump_array[26] = __read_32bit_c0_register($6, 3);
	 cp0_dump_array[27] = __read_32bit_c0_register($6, 4);
	 cp0_dump_array[28] = __read_32bit_c0_register($6, 5);
	 cp0_dump_array[29] = __read_32bit_c0_register($7, 0);
	 cp0_dump_array[30] = __read_32bit_c0_register($8, 0);
	 cp0_dump_array[31] = __read_32bit_c0_register($9, 0);
	 cp0_dump_array[32] = __read_32bit_c0_register($10, 0);
	 cp0_dump_array[33] = __read_32bit_c0_register($11, 0);
	 cp0_dump_array[34] = __read_32bit_c0_register($12, 0);
	 cp0_dump_array[35] = __read_32bit_c0_register($12, 1);
	 cp0_dump_array[36] = __read_32bit_c0_register($12, 2);
	 cp0_dump_array[37] = __read_32bit_c0_register($12, 3);
	 cp0_dump_array[38] = __read_32bit_c0_register($13, 0);
	 cp0_dump_array[39] = __read_32bit_c0_register($14, 0);
	 cp0_dump_array[40] = __read_32bit_c0_register($15, 0);
	 cp0_dump_array[41] = __read_32bit_c0_register($15, 1);
	 cp0_dump_array[42] = __read_32bit_c0_register($16, 0);
	 cp0_dump_array[43] = __read_32bit_c0_register($16, 1);
	 cp0_dump_array[44] = __read_32bit_c0_register($16, 2);
	 cp0_dump_array[45] = __read_32bit_c0_register($16, 3);
	 cp0_dump_array[46] = __read_32bit_c0_register($16, 7);
	 cp0_dump_array[47] = __read_32bit_c0_register($17, 0);
	 cp0_dump_array[48] = __read_32bit_c0_register($18, 0);
	 cp0_dump_array[49] = __read_32bit_c0_register($18, 1);
	 cp0_dump_array[50] = __read_32bit_c0_register($18, 2);
	 cp0_dump_array[51] = __read_32bit_c0_register($18, 3);
	 cp0_dump_array[52] = __read_32bit_c0_register($19, 0);
	 cp0_dump_array[53] = __read_32bit_c0_register($19, 1);
	 cp0_dump_array[54] = __read_32bit_c0_register($19, 2);
	 cp0_dump_array[55] = __read_32bit_c0_register($19, 3);
	 cp0_dump_array[56] = __read_32bit_c0_register($23, 0);
	 cp0_dump_array[57] = __read_32bit_c0_register($23, 1);
	 cp0_dump_array[58] = __read_32bit_c0_register($23, 2);
	 cp0_dump_array[59] = __read_32bit_c0_register($23, 3);
	 cp0_dump_array[60] = __read_32bit_c0_register($23, 4);
	 cp0_dump_array[61] = __read_32bit_c0_register($23, 5);
	 cp0_dump_array[62] = __read_32bit_c0_register($24, 0);
	 cp0_dump_array[63] = __read_32bit_c0_register($25, 0);
	 cp0_dump_array[64] = __read_32bit_c0_register($25, 1);
	 cp0_dump_array[65] = __read_32bit_c0_register($25, 2);
	 cp0_dump_array[66] = __read_32bit_c0_register($25, 3);
	 cp0_dump_array[67] = __read_32bit_c0_register($26, 0);
	 cp0_dump_array[68] = __read_32bit_c0_register($27, 0);
	 cp0_dump_array[69] = __read_32bit_c0_register($28, 0);
	 cp0_dump_array[70] = __read_32bit_c0_register($28, 1);
	 cp0_dump_array[71] = __read_32bit_c0_register($28, 2);
	 cp0_dump_array[72] = __read_32bit_c0_register($28, 3);
	 cp0_dump_array[73] = __read_32bit_c0_register($28, 4);
	 cp0_dump_array[74] = __read_32bit_c0_register($28, 5);
	 cp0_dump_array[75] = __read_32bit_c0_register($29, 1);
	 cp0_dump_array[76] = __read_32bit_c0_register($29, 5);
	 cp0_dump_array[77] = __read_32bit_c0_register($30, 0);
	 cp0_dump_array[78] = __read_32bit_c0_register($31, 0);
}

static int proc_read_cp0(char *page, char **start __attribute__ ((unused)),
		off_t off __attribute__((unused)), int count __attribute__ ((unused)), int *eof,
		void *data __attribute__ ((unused))) {
	int len = 0;
	int i;


	dump_cp0();
	for ( i = 0; i < 78; i++)
		len += sprintf(page + len, "cp0_dump_array[%d] = %#x\n", i, read_cp0_array(i));

	*eof = 1;
	return len;
}
#endif

#if defined(CONFIG_AVM_WP_PROC)
static int __init setup_proc_wp(void){

	struct proc_dir_entry *res = NULL;
	res = create_proc_entry("watchpoint", 0, NULL);

	if (res) {
		res->read_proc = proc_read_wp;
		res->write_proc = proc_write_wp;
	}

#if defined(CONFIG_DUMP_CP0)
	res = create_proc_entry("dump_cp0", 0, NULL);

	if (res) {
		res->read_proc = proc_read_cp0;
		res->write_proc = NULL;
	}
#endif

	res = create_proc_entry("directmem", 0, NULL);

	if (res) {
		res->read_proc = proc_read_mem;
		res->write_proc = proc_write_mem;
	}
	return 0;
}

late_initcall(setup_proc_wp);
#endif

EXPORT_SYMBOL(watchpoint_busy);
EXPORT_SYMBOL(set_watchpoint);
EXPORT_SYMBOL(del_watchpoint);

#endif


