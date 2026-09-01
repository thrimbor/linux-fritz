#define SIOCDEVPRIVATE  0x89F0  /* to 89FF */
#define S26_RD_PHY  (SIOCDEVPRIVATE | 0x1)
#define S26_WR_PHY  (SIOCDEVPRIVATE | 0x2)
#define S26_FORCE_PHY  (SIOCDEVPRIVATE | 0x3)

struct eth_diag {
    char    ad_name[IFNAMSIZ];      /* if name, e.g. "eth0" */
    union {
        u_int16_t portnum;           /* pack to fit, yech */
        u_int8_t duplex;
    }ed_u;
    u_int32_t phy_reg;
    u_int   val;
    caddr_t ad_in_data;
    caddr_t ad_out_data;
};

