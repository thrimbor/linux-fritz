#if defined(CONFIG_MACH_AR724x)
#include "ag7240.h"
#elif defined(CONFIG_MACH_AR934x)
#include "ag934x.h"
#endif
#include "athrs_mac.h"

#define MODULE_NAME "ATH_MAC_TIMER"

static int enable_timer = 0;

#if 0
static int mii0_if = ATHR_GMAC_MII0_INTERFACE;
static int mii1_if = ATHR_GMAC_MII1_INTERFACE;

static char *mii_str[2][4] = {
    {"GMii", "Mii", "RGMii", "RMii"},
    {"GMii","Mii","RGMii", "RMii"}
};
#endif

static athr_gmac_t *mac_timer[ATHR_GMAC_NMACS];

static void timer_work_handler_mac0(struct work_struct *work);
struct workqueue_struct *timer_workqueue_mac0;
DECLARE_WORK(timer_work_mac0, timer_work_handler_mac0);

#if (ATHR_GMAC_NMACS == 1)
static void timer_work_handler_mac1(struct work_struct *work);
struct workqueue_struct *timer_workqueue_mac1;
DECLARE_WORK(timer_work_mac1, timer_work_handler_mac1);
#endif

#if HYBRID_LINK_CHANGE_EVENT

#define LAN_INTERFACE "eth0.1"
#define WAN_INTERFACE "eth0.5"
#define PLC_INTERFACE "eth0.1024"
#if CONFIG_ATHRS17_PHY
#if HYBRID_SWITCH_PORT6_USED
/* here is the map for aph128 */
#define LAN_INTERFACE_MASK 0x1e  /* port 1,2,3,4 */
#define WAN_INTERFACE_MASK 0x1  /* port 4 */
#define PLC_INTERFACE_MASK 0x0  /* no more */
#else
/* here is the map for db12x */
#define LAN_INTERFACE_MASK 0xe  /* port 1,2,3 */
#define WAN_INTERFACE_MASK 0x1  /* port 0 */
#define PLC_INTERFACE_MASK 0x10  /* port 4 */
#endif
#else
/* 
 * !!CAUTION 
 * on pb92 board for hybrid network, 
 * here is the map for each interfce.
 *
 * phyUnit = 0 is eth0.1024 for PLC,
 * phyUnit = 1 is eth0.5 for WAN,
 * phyUnit = 2,3,4 is eth0.1 for LAN
 *
 * Maybe the map needs to change for other board!
 */ 

#define LAN_INTERFACE_MASK 0x1c /* port 2,3,4 */
#define WAN_INTERFACE_MASK 0x2  /* port 1 */
#define PLC_INTERFACE_MASK 0x1  /* port 0 */
#endif
static int eth01flag = 0;
void hybrid_netif_off_all()
{
    struct net_device *dev;
    dev = dev_get_by_name(&init_net, LAN_INTERFACE);
    if(dev != NULL) {
        netif_carrier_off(dev);
        dev_put(dev);
    }
    dev = dev_get_by_name(&init_net, WAN_INTERFACE);
    if(dev != NULL) {
        netif_carrier_off(dev);
        dev_put(dev);
    }
#if !HYBRID_SWITCH_PORT6_USED
    dev = dev_get_by_name(&init_net, PLC_INTERFACE);
    if(dev != NULL) {
        netif_carrier_off(dev);
        dev_put(dev);
    }
#endif
}

int hybrid_netif_on(int rc, int mask, char *name)
{
    struct net_device *dev;
    if((rc&mask) != 0){
        dev = dev_get_by_name(&init_net, name);
        if(dev != NULL){
            netif_carrier_on(dev);
            dev_put(dev);
            return 1;
        }
    }
    return 0;
}
int hybrid_netif_off(int rc, int mask, char *name)
{
    struct net_device *dev;
    if((rc& (mask<<8)) != 0){
        dev = dev_get_by_name(&init_net, name);
        if(dev != NULL){
            netif_carrier_off(dev);
            dev_put(dev);
            return 1;
        }
    }
    return 0;
}
void hybrid_netif_carrier(int rc, int mask, char *name)
{
    struct net_device *dev;
    if((rc&mask) != 0){
        dev = dev_get_by_name(&init_net, name);
        if(dev != NULL){
            netif_carrier_on(dev);
            dev_put(dev);
        }
    }else if((rc& (mask<<8)) != 0){
        dev = dev_get_by_name(&init_net, name);
        if(dev != NULL){
            netif_carrier_off(dev);
            dev_put(dev);
        }
    }else
        ;
}
#endif
/*
 * phy link state management
 */
#if 0
extern int athr_gmac_get_link_st(athr_gmac_t *mac, int *link, int *fdx, 
                                 athr_phy_speed_t *speed,int phyUnit);

extern void athr_gmac_set_mac_from_link(athr_gmac_t *mac, 
                                        athr_phy_speed_t speed, int fdx);

static int
athr_gmac_poll_link(athr_gmac_t *mac)
{
    struct net_device  *dev     = mac->mac_dev;
    int                 carrier = netif_carrier_ok(dev), fdx, phy_up;
    athr_phy_speed_t  speed;
    int  rc,phyUnit = 0;

    printk("{%s} mac %d\n", __FUNCTION__, mac->mac_unit);
    assert(mac->mac_ifup);
#if !defined(CONFIG_ATHR_PHY_SWAP)
    if (mac->mac_unit == 0)
        phyUnit = 4;
#endif

    rc = athr_gmac_get_link_st(mac, &phy_up, &fdx, &speed, phyUnit);

    if (rc < 0)
        goto done;
#if HYBRID_LINK_CHANGE_EVENT
    if(unlikely(mac->link_up == 0)
#if HYBRID_SWITCH_PORT6_USED
            || ((mac->link_up == 1)&&(phy_up == 1))
#endif
     ){
        hybrid_netif_off_all(); 
    }
    if((rc != 0) && phy_up && (mac->link_up != 0)){
        hybrid_netif_carrier(rc, PLC_INTERFACE_MASK, PLC_INTERFACE);
        hybrid_netif_carrier(rc, WAN_INTERFACE_MASK, WAN_INTERFACE);
        /* deal with the first cable plug in, or the last cable plug out for eth0.1*/
        if((((rc>>16)&0xf) < 2) && (eth01flag < 2)){
            hybrid_netif_carrier(rc, LAN_INTERFACE_MASK, LAN_INTERFACE);
        }
        eth01flag = (rc>>16)&0xf;
        goto done;
    }
    eth01flag = (rc>>16)&0xf;
#endif
    if (!phy_up)
    {
        if (carrier)
        {
            printk(MODULE_NAME ": unit %d: phy %0d not up carrier %d\n", mac->mac_unit, phyUnit, carrier);

            /* A race condition is hit when the queue is switched on while tx interrupts are enabled.
             * To avoid that disable tx interrupts when phy is down.
             */
            mac->link_up = 0;
            athr_gmac_intr_disable_tx(mac);
            netif_carrier_off(dev);
            netif_stop_queue(dev);

       }
       goto done;
    }

    if(!mac->mac_ifup) {
        goto done;
    }

    if ((fdx < 0) || (speed < 0))
    {
        printk(MODULE_NAME ": phy not connected?\n");
        return 0;
    }

    if (athr_chk_phy_in_rst(mac))
        goto done;

    if (carrier && (speed == mac->mac_speed ) && (fdx == mac->mac_fdx)) {
        goto done;
    }

    printk(MODULE_NAME ": enet unit:%d is up...\n", mac->mac_unit);
    printk("%s %s %s\n", mii_str[mac->mac_unit][mii_if(mac)], spd_str[speed], dup_str[fdx]);

    athr_gmac_set_mac_from_link(mac, speed, fdx);

    printk(MODULE_NAME ": done cfg2 %#x ifctl %#x miictrl  \n",
       athr_gmac_reg_rd(mac, ATHR_GMAC_CFG2),
       athr_gmac_reg_rd(mac, ATHR_GMAC_IFCTL));
    /*
     * in business
     */
#if HYBRID_LINK_CHANGE_EVENT
    if(hybrid_netif_on(rc, LAN_INTERFACE_MASK, LAN_INTERFACE)){
        eth01flag = 1;
    }
#endif
    netif_carrier_on(dev);
    netif_start_queue(dev);
    mac->link_up = 1;

done:
    
    return 0;
}
#endif

extern int athr_gmac_check_link(athr_gmac_t *mac, int phyUnit);
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static void timer_work_handler_mac0(struct work_struct *work __attribute__ ((unused))) {

    athr_gmac_check_link(mac_timer[0], 0);
    /*--- athr_gmac_poll_link(mac_timer[0]); ---*/
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if (ATHR_GMAC_NMACS == 1)
static void timer_work_handler_mac1(struct work_struct *work __attribute__ ((unused))) {

    athr_gmac_check_link(mac_timer[1], 0);
    /*--- athr_gmac_poll_link(mac_timer[1]); ---*/
}
#endif

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static int
athr_gmac_timer_func(athr_gmac_t *mac)
{
    athr_gmac_ops_t *ops = mac->ops;

    if (mac_has_flag(mac,ETH_SOFT_LED) && ops->soft_led) {
        if (athr_gmac_get_diff(mac->led_blink_ts,jiffies) >= (ATH_GMAC_LED_BLINK_FREQ)) {
            ops->soft_led(mac);
            mac->led_blink_ts = jiffies;
        }
    }
    if (mac_has_flag(mac,CHECK_DMA_STATUS)&& ops->check_dma_st) { 
        if(athr_gmac_get_diff(mac->dma_chk_ts,jiffies) >= (ATH_GMAC_CHECK_DMA_FREQ)) {
            if (mac->mac_ifup) ops->check_dma_st(mac,0);
            mac->dma_chk_ts = jiffies;
        }
    }
    if (mac_has_flag(mac, ETH_LINK_POLL)) {
        if (athr_gmac_get_diff(mac->link_ts,jiffies) >= (ATH_GMAC_POLL_LINK_FREQ)) {
            switch(mac->mac_unit) {
                case 0:
                    queue_work(timer_workqueue_mac0, &timer_work_mac0);
                    break;
#if (ATHR_GMAC_NMACS == 1)
                case 1:
                    queue_work(timer_workqueue_mac1, &timer_work_mac1);
                    break;
#endif
                default:
                    printk(KERN_ERR "ERROR: {%s} no unit %d\n", __FUNCTION__, mac->mac_unit);
            }
            mac->link_ts = jiffies;
        }
    }
    mod_timer(&mac->mac_phy_timer,(jiffies + mac->timer_freq));
    return 0;
}

void 
athr_gmac_timer_init(athr_gmac_t *mac) 
{
 
    if (mac_has_flag(mac,ETH_LINK_POLL)) {
        mac_timer[mac->mac_unit] = mac;
        mac->timer_freq = ATH_GMAC_POLL_LINK_FREQ; 
        enable_timer = 1;

        timer_workqueue_mac0 = create_singlethread_workqueue("timer_workqueue_mac0");
        if(timer_workqueue_mac0 == NULL) {
            printk(KERN_ERR "[%s] create_singlethread_workqueue(\"timer_workqueue_mac0\" failed\n", __FUNCTION__);
        }
#if (ATHR_GMAC_NMACS == 1)
        timer_workqueue_mac1 = create_singlethread_workqueue("timer_workqueue_mac1");
        if(timer_workqueue_mac1 == NULL) {
            printk(KERN_ERR "[%s] create_singlethread_workqueue(\"timer_workqueue_mac1\" failed\n", __FUNCTION__);
        }
#endif
    }
    if (mac_has_flag(mac,CHECK_DMA_STATUS)) {
        mac->timer_freq = ATH_GMAC_CHECK_DMA_FREQ; 
        enable_timer = 1;
    }
    if (mac_has_flag(mac,ETH_SOFT_LED)) {
        mac->timer_freq = ATH_GMAC_LED_BLINK_FREQ; 
        enable_timer = 1;
    }
    if (enable_timer) {
        init_timer(&mac->mac_phy_timer);
        mac->mac_phy_timer.data     = (unsigned long)mac;
        mac->mac_phy_timer.function = (void *)athr_gmac_timer_func;
        athr_gmac_timer_func(mac);
    }

}

void
athr_gmac_timer_del(athr_gmac_t *mac)
{
   if(enable_timer)
      del_timer(&mac->mac_phy_timer);
}
