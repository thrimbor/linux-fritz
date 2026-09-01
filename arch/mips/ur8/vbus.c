#include <linux/init.h>
#include <linux/kernel.h>

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#include <asm/mach-ur8/hw_boot.h>
#include <asm/mach-ur8/hw_vbus.h>

struct _hw_vbus {
    union _hw_vbus_prio prio[vbus_last];
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void ur8_vbus_set_prio(enum _ht_vbus_instance instance, unsigned int base_level, 
                       unsigned int escalator_enable, unsigned int escalator_count, unsigned int escalator_floor) {

    struct _hw_vbus *P = (struct _hw_vbus *)0xA8611A38;

#if 0
    printk(KERN_ERR "[ur8_vbus_set_prio] instance %s level %d escalator %s (count %d, floor %d)\n",
        instance == vbus_nwss_dma ?  "vbus_nwss_dma" : 
        instance == vbus_sar_pdsp ?  "vbus_sar_pdsp" : 
        instance == vbus_buffer_manager ?  "vbus_buffer_manager" : 
        instance == vbus_usb ?  "vbus_usb" : 
        instance == vbus_vlynq ?  "vbus_vlynq" : 
        instance == vbus_pci ?  "vbus_pci" : 
        instance == vbus_c55 ?  "vbus_c55" : 
        instance == vbus_tdm ?  "vbus_tdm" : 
        instance == vbus_mips ?  "vbus_mips" : 
        instance == vbus_last ?  "vbus_last" : "unknown",
        base_level, 
        escalator_enable ? "enabled" : "disabled",
        escalator_count, escalator_floor);
#endif

    P->prio[instance].Bits.prio_level                        = base_level;
    P->prio[instance].Bits.lowest_priority_value_achieveable = escalator_floor;
    P->prio[instance].Bits.prio_escalation_count             = escalator_count;
    P->prio[instance].Bits.enable                            = escalator_enable ? 1 : 0;
}



/*------------------------------------------------------------------------------------------*\
 * DSP CACHE-Zugriffe z.Z mit Prio 2
 * DSP Prefetch-Zugriffe mit Prio 1
 * DSP-DMA-Zugriff z.Z. wegen HW-Fehler mit Prio 0 sonst 1?
\*------------------------------------------------------------------------------------------*/
int ur8_vbus_init(void) {
    struct _prio {
        unsigned int level;
        unsigned int enable;
        unsigned int count;
        unsigned int floor;
    } prio[] = {
        { /*--- vbus_nwss_dma, ---*/        /* offset 1A38 */
            /*--- level: 4, ---*/
            level: 2,
            enable: 0,
            count: 255,
            floor: 7
        },
        { /*--- vbus_sar_pdsp, ---*/        /* offset 1A3C */
            /*--- level: 5 ---*/
            level: 2,
            enable: 0,
            count: 255,
            floor: 7
        },
        { /*--- vbus_buffer_manager, ---*/  /* offset 1A40 */
            /*--- level: 6, ---*/
            /*--- enable: 1, ---*/
            /*--- count: 10, ---*/
            /*--- floor: 3 ---*/
            level: 2,
            enable: 0,
            count: 255,
            floor: 7
        },
        { /*--- vbus_usb, ---*/             /* offset 1A44 */
            /*--- level: 2 ---*/
            level: 5,
            enable: 0,
            count: 255,
            floor: 7
        },
        { /*--- vbus_vlynq, ---*/           /* offset 1A48 */
            level: 7,
            enable: 0,
            count: 255,
            floor: 7
        },
        { /*--- vbus_pci, ---*/             /* offset 1A4C */
            /*--- level: 2, ---*/
            /*--- enable: 1, ---*/
            /*--- count: 10, ---*/
            /*--- floor: 1 ---*/
            level: 5,
            enable: 0,
            count: 255,
            floor: 7
        },
        { /*--- vbus_c55, ---*/             /* offset 1A50 */
            /*--- level: 7 ---*/
            level: 3,
            enable: 0,
            count: 255,
            floor: 7
        },
        { /*--- vbus_tdm, ---*/             /* offset 1A54 */
            level: 0,
            enable: 0,
            count: 255,
            floor: 7
        },
        { /*--- vbus_mips, ---*/            /* offset 1A58 */
            /*--- level: 1 ---*/
            level: 3,
            enable: 0,
            count: 255,
            floor: 7
        }
    };

    unsigned int i;

    for(i = 0 ; i < sizeof(prio) / sizeof(prio[0]) ; i++) {
        ur8_vbus_set_prio((enum _ht_vbus_instance)i, prio[i].level, prio[i].enable, prio[i].count, prio[i].floor);
    }
    return 0;
}


/*--- arch_initcall(ur8_vbus_init); ---*/
