#if ! defined(_ATH_I2C_H_)
#define _ATH_I2C_H_

#define I2C_BASE_SLAVE_ADDR                     0x20
#define I2C_10BIT_BASE_ADDR                     0x180 //0x280

#define I2C_LOCK_INIT(_sc)                      spin_lock_init(&(_sc)->i2c_lock)
#define I2C_LOCK_DESTROY(_sc)
#define I2C_LOCK(_sc)                           spin_lock_irqsave(&(_sc)->i2c_lock, (_sc)->i2c_lockflags)
#define I2C_UNLOCK(_sc)                         spin_unlock_irqrestore(&(_sc)->i2c_lock, (_sc)->i2c_lockflags)

/* I2C control register */
#define ATH_I2C_CON_SLAVE_DISABLE               (0x1 << 6)
#define ATH_I2C_CON_RESTART_EN                  (0x1 << 5)
#define ATH_I2C_CON_10BITADDR_MASTER            (0x1 << 4)
#define ATH_I2C_CON_IC_10BITADDR_SLAVE          (0x1 << 3)
#define ATH_I2C_CON_SPEED_HS                    (0x3 << 1)
#define ATH_I2C_CON_SPEED_FS                    (0x2 << 1)
#define ATH_I2C_CON_SPEED_SS                    (0x1 << 1)
#define ATH_I2C_CON_SPEED_MASK                  (0x3 << 1)
#define ATH_I2C_CON_MASTER_MODE                 (0x1 << 0)

/* Register Values */
#define I2C_ENABLE                              (1 << 0)
#define I2C_DISABLE                             (0 << 0)
#define I2C_SRESET                              (1 << 0)

/*I2C Status Register*/
#define ATH_I2C_STAT_RFF                        (0x1 << 4)
#define ATH_I2C_STAT_RFNE                       (0x1 << 3)
#define ATH_I2C_STAT_TFE                        (0x1 << 2)
#define ATH_I2C_STAT_TFNF                       (0x1 << 1)
#define ATH_I2C_STAT_ACTIVITY                   (0x1 << 0)

/* Intr mask bits */
#define ATH_I2C_INTR_MASK_GENCALL               (0x1 << 11)
#define ATH_I2C_INTR_MASK_START                 (0x1 << 10)
#define ATH_I2C_INTR_MASK_STOP                  (0x1 << 9)
#define ATH_I2C_INTR_MASK_ACTIVITY              (0x1 << 8)
#define ATH_I2C_INTR_MASK_RXDONE                (0x1 << 7)
#define ATH_I2C_INTR_MASK_TXABRT                (0x1 << 6)
#define ATH_I2C_INTR_MASK_RDREQ                 (0x1 << 5)
#define ATH_I2C_INTR_MASK_TXEMPTY               (0x1 << 4)
#define ATH_I2C_INTR_MASK_TXOVER                (0x1 << 3)
#define ATH_I2C_INTR_MASK_RXFULL                (0x1 << 2)
#define ATH_I2C_INTR_MASK_RXOVER                (0x1 << 1)
#define ATH_I2C_INTR_MASK_RXUNDER               (0x1 << 0)

/* Clock Cnt values. Refer to I2C controller datasheet */
#define ATH_I2C_SS_SCL_HCNT_40                  0xA0
#define ATH_I2C_SS_SCL_LCNT_40                  0xBC
#define ATH_I2C_SS_SCL_HCNT_100                 0x190
#define ATH_I2C_SS_SCL_LCNT_100                 0x1D6
#define ATH_I2C_FS_SCL_HCNT_40                  0x18
#define ATH_I2C_FS_SCL_LCNT_40                  0x34
#define ATH_I2C_FS_SCL_HCNT_100                 0x3C
#define ATH_I2C_FS_SCL_LCNT_100                 0x82
#define ATH_I2C_HS_SCL_HCNT_40                  0x5
#define ATH_I2C_HS_SCL_LCNT_40                  0xD
#define ATH_I2C_HS_SCL_HCNT_100                 0xC
#define ATH_I2C_HS_SCL_LCNT_100                 0x20

/* Param Max values */
#define I2C_DATASIZE_MAX                        65536           /* Transmission data internal buffer size */
#define I2C_OFFSET_MAX                          8               /* Offset max size */

/* Max data */
//#define MAX_RX_DATA                             4
#define MAX_RX_DATA                             12
#define MAX_TX_DATA                             4
#define MAX_ADDRESS_OFFSET_BYTES                2
#define MAX_LEN                                 12
#define MAX_READ_COUNT                          10

/*RX/TX Data Buffer and Command Register*/
#define ATH_I2C_CMD_READ                        (0x1 << 8)
#define ATH_I2C_DAT_MASK                        0xff

/* I2C All function Return Values */
#define _I2C_OK                                 0x00000000
#define _I2C_ERR                                0x00000001
#define _I2C_ERR_PARAM                          0x00000002
#define _I2C_ERR_BUSY                           0x00000004
#define _I2C_ERR_SEM                            0x00000008
#define _I2C_ERR_IOCTL_IO                       0x00000100
#define _I2C_ERR_IOCTL_PARAM                    0x00000200
#define _I2C_ERR_IOCTL_ADDR                     0x00000400
#define _I2C_ERR_IOCTL_ARG                      0x00000800
#define _I2C_ERR_ABORT                          0x00010000
#define _I2C_ERR_NACK                           0x00020000
#define _I2C_ERR_UNDER                          0x00040000
#define _I2C_ERR_OVER                           0x00080000

/* I2C structure to hold variables and locks */
typedef struct ath_i2c_dev {
	struct i2c_adapter      adapter;
	void __iomem            *iobase;
	struct resource         *ioarea;
	int                     irq;

	spinlock_t              i2c_lock;
	unsigned long           i2c_lockflags;

	struct completion       wait;

    unsigned char           *buffer;
    int                     bufferlen;
    int                     rx_len;

    unsigned int            error;

} ath_i2c_dev_t;

#endif /*--- #if defined _ATH_I2C_H_ ---*/
