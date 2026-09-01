#ifndef _ASM_MACH_HW_VBUS_H_
#define _ASM_MACH_HW_VBUS_H_

enum _ht_vbus_instance {
    vbus_nwss_dma,        /* offset 1A38 */
    vbus_sar_pdsp,        /* offset 1A3C */
    vbus_buffer_manager,  /* offset 1A40 */
    vbus_usb,             /* offset 1A44 */
    vbus_vlynq,           /* offset 1A48 */
    vbus_pci,             /* offset 1A4C */
    vbus_c55,             /* offset 1A50 */
    vbus_tdm,             /* offset 1A54 */
    vbus_mips,            /* offset 1A58 */
    vbus_last
};

void ur8_vbus_set_prio(enum _ht_vbus_instance instance, unsigned int base_level, unsigned int escalator_enable, unsigned int escalator_count, unsigned int escalator_floor);
int ur8_vbus_init(void);


#endif /*--- #ifndef _ASM_MACH_HW_VBUS_H_ ---*/
