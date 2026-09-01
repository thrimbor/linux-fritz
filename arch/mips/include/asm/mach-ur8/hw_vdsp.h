/*--------------------------------------------------------------------------------*\
 * C55-VDSP
\*--------------------------------------------------------------------------------*/
#ifndef __hw_vdsp_h__
#define __hw_vdsp_h__

#define LITTLE_ENDIAN

#define UR8_VDSP_MEMBASE           KERNEL_ADDR(0x4000000)

#define UR8_VDSP_REGISTER_BASE    	KERNEL_ADDR(0x05000000)
#define UR8_VDSP_EXTERNAL_MEM_SIZE	0x00100000

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
struct _vdsp_registers {
    union _local_bootconf {      /*--- Base + 0 ---*/
        volatile unsigned int reg;
        struct {
#ifdef LITTLE_ENDIAN
            volatile unsigned int local_boot_vec:24;
            volatile unsigned int reserved:7;
            volatile unsigned int local_boot:1; /*--- = DSPSS boots from external memory, 1 address translation to boot from local memory according to local boot vector ---*/
#endif/*--- #ifdef LITTLE_ENDIAN ---*/
#ifdef BIG_ENDIAN
            volatile unsigned int local_boot:1; /*--- = DSPSS boots from external memory, 1 address translation to boot from local memory according to local boot vector ---*/
            volatile unsigned int reserved:7;
            volatile unsigned int local_boot_vec:24;
#endif/*--- #ifdef BIG_ENDIAN ---*/
        } Bits;
    } local_bootconf;
    volatile unsigned int external_address_conf;    /*--- only bit 31:24 ---*/
    volatile unsigned int xport_base_address_conf;  /*--- only bit 14:0 ---*/
    union _memory_protection_conf {  /*--- Base + 0xC ---*/
        volatile unsigned int reg;
        struct {
#ifdef LITTLE_ENDIAN
            volatile unsigned int aerr_intr_en:1;
            volatile unsigned int reserved:31;
#endif/*--- #ifdef LITTLE_ENDIAN ---*/
#ifdef BIG_ENDIAN
            volatile unsigned int reserved:31;
            volatile unsigned int aerr_intr_en:1;
#endif/*--- #ifdef BIG_ENDIAN ---*/
        } Bits;
    } memory_protection_conf;
    volatile unsigned int memory_protection_startaddr;    /*--- only bit 23:5 ---*/
    volatile unsigned int memory_protection_endaddr;      /*--- only bit 23:5 ---*/
    union _address_error_cause {  /*--- Base + 0x18 ---*/
        volatile unsigned int reg;
        struct {
#ifdef LITTLE_ENDIAN
            volatile unsigned int mem_err_addr:24;
            volatile unsigned int reserved:7;
            volatile unsigned int err_addr_en:1;    /*--- R/W/C ---*/
#endif/*--- #ifdef LITTLE_ENDIAN ---*/
#ifdef BIG_ENDIAN
            volatile unsigned int err_addr_en:1;    /*--- R/W/C ---*/
            volatile unsigned int reserved:7;
            volatile unsigned int mem_err_addr:24;
#endif/*--- #ifdef BIG_ENDIAN ---*/
        } Bits;
    } address_error_cause;
    unsigned int reserved1;
    union _uma_boundary_in {  /*--- Base + 0x20 ---*/
        volatile unsigned int reg;
        struct {
#ifdef LITTLE_ENDIAN
            volatile unsigned int gpio_tr:4; 
            volatile unsigned int uma_boot_mode_tr:4; 
            volatile unsigned int reserved:24; 
#endif/*--- #ifdef LITTLE_ENDIAN ---*/
#ifdef BIG_ENDIAN
            volatile unsigned int reserved:24; 
            volatile unsigned int uma_boot_mode_tr:4; 
            volatile unsigned int gpio_tr:4; 
#endif/*--- #ifdef BIG_ENDIAN ---*/
        } Bits;
    } uma_boundary_in;
    union _uma_boundary_status {  /*--- Base + 0x24 ---*/
        volatile unsigned int reg;
        struct {
#ifdef LITTLE_ENDIAN
            volatile unsigned int gl_uma_gpio_tr:8;
            volatile unsigned int mabort:1;
            volatile unsigned int suspend:1; 
            volatile unsigned int uma_sleep1:1; 
            volatile unsigned int reserved:20; 
#endif/*--- #ifdef LITTLE_ENDIAN ---*/
#ifdef BIG_ENDIAN

            volatile unsigned int reserved:20; 
            volatile unsigned int uma_sleep1:1; 
            volatile unsigned int suspend:1; 
            volatile unsigned int mabort:1;
            volatile unsigned int gl_uma_gpio_tr:8;
#endif/*--- #ifdef BIG_ENDIAN ---*/
        } Bits;
    } uma_boundary_status ;
    unsigned int reserved2[2];
    union _dsp_ack_ivec {        /*--- Base + 0x30 ---*/
        volatile unsigned int reg;
        struct {
#ifdef LITTLE_ENDIAN
            volatile unsigned int gl_int_na:23;
            volatile unsigned int gl_nmi:1;
            volatile unsigned int gl_uma_iack_nr:1; 
            volatile unsigned int reserved:7; 
#endif/*--- #ifdef LITTLE_ENDIAN ---*/
#ifdef BIG_ENDIAN
            volatile unsigned int reserved:7; 
            volatile unsigned int gl_uma_iack_nr:1; 
            volatile unsigned int gl_nmi:1;
            volatile unsigned int gl_int_na:23;
#endif/*--- #ifdef BIG_ENDIAN ---*/
        } Bits;
    } dsp_ack_ivec;
    union _dsp_interrupt_polarity {  /*--- Base + 0x34 ---*/
        volatile unsigned int reg;
        struct {
#ifdef LITTLE_ENDIAN
            volatile unsigned int polarity:22;
            volatile unsigned int reserved:10; 
#endif/*--- #ifdef LITTLE_ENDIAN ---*/
#ifdef BIG_ENDIAN
            volatile unsigned int reserved:10; 
            volatile unsigned int polarity:22;
#endif/*--- #ifdef BIG_ENDIAN ---*/
        } Bits;
    } dsp_interrupt_polarity;
    unsigned int reserved4[2];
    union _dspss_host_idle_handshake {   /*--- Base + 0x40 ---*/
        volatile unsigned int reg;
        struct {
#ifdef LITTLE_ENDIAN
            volatile unsigned int full_idle_en_tr:1;
            volatile unsigned int gl_wait_tr:1;
            volatile unsigned int gl_idleack_periph_tr:1; 
            volatile unsigned int gl_swakeup_ta:1; 
            volatile unsigned int gl_idleack_initiator_tr:1; 
            volatile unsigned int reserved:27; 
#endif/*--- #ifdef LITTLE_ENDIAN ---*/
#ifdef BIG_ENDIAN
            volatile unsigned int reserved:27; 
            volatile unsigned int gl_idleack_initiator_tr:1; 
            volatile unsigned int gl_swakeup_ta:1; 
            volatile unsigned int gl_idleack_periph_tr:1; 
            volatile unsigned int gl_wait_tr:1;
            volatile unsigned int full_idle_en_tr:1;
#endif/*--- #ifdef BIG_ENDIAN ---*/
        } Bits;
    } dspss_host_idle_handshake;
    union _dspss_idle_status{   /*--- Base + 0x44 ---*/
        volatile unsigned int reg;
        struct {
#ifdef LITTLE_ENDIAN
            volatile unsigned int uma_mwakeup_tr:1;
            volatile unsigned int uma_mstandby_tr:1;
            volatile unsigned int uma_idlereq_periph_tr:1; 
            volatile unsigned int uma_idlereq_initiator_tr:1; 
            volatile unsigned int reserved:28; 
#endif/*--- #ifdef LITTLE_ENDIAN ---*/
#ifdef BIG_ENDIAN
            volatile unsigned int reserved:28; 
            volatile unsigned int uma_idlereq_initiator_tr:1; 
            volatile unsigned int uma_idlereq_periph_tr:1; 
            volatile unsigned int uma_mstandby_tr:1;
            volatile unsigned int uma_mwakeup_tr:1;
#endif/*--- #ifdef BIG_ENDIAN ---*/
        } Bits;
    } dspss_idle_status;
    union _dspss_idle_interrupt { /*--- Base + 0x48 ---*/
        volatile unsigned int reg;
        struct {
#ifdef LITTLE_ENDIAN
            volatile unsigned int idl_irq:1;
            volatile unsigned int reserved:31; 
#endif/*--- #ifdef LITTLE_ENDIAN ---*/
#ifdef BIG_ENDIAN
            volatile unsigned int reserved:31; 
            volatile unsigned int idl_irq:1;
#endif/*--- #ifdef BIG_ENDIAN ---*/
        } Bits;
    } dspss_idle_interrupt;
    unsigned int reserved5;
    volatile unsigned int dspss_priority0_timeperiod; /*--- only 15:0 ---*/
    volatile unsigned int dspss_priority1_timeperiod; /*--- only 15:0 ---*/
    volatile unsigned int dspss_priority2_timeperiod; /*--- only 15:0 ---*/
    union _dspss_priority_level_conf { /*--- Base + 0x5C ---*/
        volatile unsigned int reg;
        struct {
#ifdef LITTLE_ENDIAN
            volatile unsigned int M0_PriorityLevel:3;
            volatile unsigned int Priority0_release:1; 
            volatile unsigned int reserved1:4; 
            volatile unsigned int M1_PriorityLevel:3;
            volatile unsigned int Priority1_release:1; 
            volatile unsigned int reserved2:4; 
            volatile unsigned int M2_PriorityLevel:3;
            volatile unsigned int Priority2_release:1; 
            volatile unsigned int reserved3:12; 
#endif/*--- #ifdef LITTLE_ENDIAN ---*/
#ifdef BIG_ENDIAN
            volatile unsigned int reserved3:12; 
            volatile unsigned int Priority2_release:1; 
            volatile unsigned int M2_PriorityLevel:3;
            volatile unsigned int reserved2:4; 
            volatile unsigned int Priority1_release:1; 
            volatile unsigned int M1_PriorityLevel:3;
            volatile unsigned int reserved1:4; 
            volatile unsigned int Priority0_release:1; 
            volatile unsigned int M0_PriorityLevel:3;
#endif/*--- #ifdef BIG_ENDIAN ---*/
        } Bits;
    } dspss_priority_level_conf;
    union _dspss_new_priority { /*--- Base + 0x60 ---*/
        volatile unsigned int reg;
        struct {
#ifdef LITTLE_ENDIAN
            volatile unsigned int new_priority:1;
            volatile unsigned int reserved:31;
#endif/*--- #ifdef LITTLE_ENDIAN ---*/
#ifdef BIG_ENDIAN
            volatile unsigned int reserved:31;
            volatile unsigned int new_priority:1;
#endif/*--- #ifdef BIG_ENDIAN ---*/
        } Bits;
    } dspss_new_priority;
};
#endif/*--- #ifndef __hw_vdsp_h__ ---*/
