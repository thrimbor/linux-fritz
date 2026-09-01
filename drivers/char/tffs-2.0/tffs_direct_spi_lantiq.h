/*------------------------------------------------------------------------------------------*\
 *
\*------------------------------------------------------------------------------------------*/
#if !defined(_HW_SPI_H_)
#define _HW_SPI_H_

#define DATA_WIDTH	         8
#define RXFIFO_SIZE          8
#define TXFIFO_SIZE          8

#define SFLASH_BAUDRATE	     2000000

enum spi_cs {
    IFX_SSC_CS1 = 0, 
    IFX_SSC_CS2,
    IFX_SSC_CS3,
    IFX_SSC_CS4,
    IFX_SSC_CS5,
    IFX_SSC_CS6,
    IFX_SSC_CS7,
};

enum spi_cs_enum {
    select,
    deselect
};

#define SPI_CLK_OFF         0x00
#define SPI_CFG_OFF         0x04
#define SPI_CMD_OFF         0x08
#define SPI_STAT_OFF        0x0C
#define SPI_DATA_OFF        0x10


#define SPI_CLK_EN          (1<<31)

#define SPI_CMD_READ        (1 << 16)
#define SPI_CMD_WRITE       (2 << 16)
#define SPI_WORD_LEN_8      ((8-1) << 19)  
#define SPI_WORD_LEN_16     ((16-1) << 19)  
#define SPI_WORD_LEN_24     ((24-1) << 19)  
#define SPI_WORD_LEN_32     ((32-1) << 19)  
#define SPI_USE_CS0         (0 << 28)
#define SPI_USE_CS1         (1 << 28)
#define SPI_USE_CS2         (2 << 28)
#define SPI_USE_CS3         (3 << 28)

#define SPI_MAX_FRAME_LEN   4096        /*--- 11 Bit Framelänge, es können 32 Bit pro Frame übertragen werden ---*/

/*------------------------------------------------------------------------------------------*\
 * SPI_CON_REGISTER
\*------------------------------------------------------------------------------------------*/
#define SPI_CON_TXOFF       1
#define SPI_CON_RXOFF       2


#if !defined(_ASSEMBLER_)
union _spi_con {
    volatile unsigned int Register;
    struct __spi_con {
        volatile unsigned int reserved2 : 7;
        volatile unsigned int em        : 1;
        volatile unsigned int idle      : 1;
        volatile unsigned int enbv      : 1;
        volatile unsigned int reserved1 : 1;
        volatile unsigned int bm        : 5;
        volatile unsigned int reserved0 : 1;
        volatile unsigned int cdel      : 2;
        volatile unsigned int ruen      : 1;
        volatile unsigned int tuen      : 1;
        volatile unsigned int aen       : 1;
        volatile unsigned int ren       : 1;
        volatile unsigned int ten       : 1;
        volatile unsigned int lb        : 1;
        volatile unsigned int po        : 1;
        volatile unsigned int ph        : 1;
        volatile unsigned int hb        : 1;
        volatile unsigned int csben     : 1;
        volatile unsigned int csbinv    : 1;
        volatile unsigned int rxoff     : 1;
        volatile unsigned int txoff     : 1;
    } Bits;
};

#define CON_TXOFF   (1 << 0)
#define CON_RXOFF   (1 << 1)
#define CON_HB      (1 << 4)
#define CON_PH      (1 << 5)
#define CON_BM_8BIT (7 << 16)
#define CON_BM_16BIT (15 << 16)
#define CON_BM_32BIT (31 << 16)
#define CON_ENBV    (1 << 22)

union _spi_status {
    volatile unsigned int Register;
    struct __spi_stat {
        volatile unsigned int rxeom     : 1;
        volatile unsigned int rxbv      : 3;
        volatile unsigned int txeom     : 1;
        volatile unsigned int txbv      : 3;
        unsigned int reserved2          : 3;
        volatile unsigned int bc        : 5;
        unsigned int reserved1          : 2;
        volatile unsigned int bsy       : 1;
        volatile unsigned int rue       : 1;
        volatile unsigned int tue       : 1;
        volatile unsigned int ae        : 1;
        volatile unsigned int re        : 1;
        volatile unsigned int te        : 1;
        volatile unsigned int me        : 1;
        unsigned int reserved0          : 4;
        volatile unsigned int ssel      : 1;
        volatile unsigned int ms        : 1;
        volatile unsigned int en        : 1;
    } Bits;
};

union _spi_fifo_ctrl {
    volatile unsigned int Register;
    struct __spi_fifo_ctrl {
        unsigned int reserved1          : 18;
        volatile unsigned int intlevel  : 6;
        unsigned int reserved0          : 6;
        volatile unsigned int flush     : 1;
        volatile unsigned int en        : 1;
    } Bits;
};

union _spi_fifo_stat {
    volatile unsigned int Register;
    struct __spi_fifo_stat {
        unsigned int reserved1          : 18;
        volatile unsigned int txffl     : 6;
        unsigned int reserved0          : 2;
        volatile unsigned int rxffl     : 6;
    } Bits;
};

union _spi_pisel {
    volatile unsigned int Register;
    struct __spi_pisel {
        unsigned int reserved0        : 29;
        volatile unsigned int cis     : 1;
        volatile unsigned int sis     : 1;
        volatile unsigned int mis     : 1;
    } Bits;
};

union _spi_sfcon {
    volatile unsigned int Register;
    struct __spi_sfcon {
        volatile unsigned int plen      : 10;
        unsigned int reserved1          : 1;
        volatile unsigned int stop      : 1;
        volatile unsigned int iclk      : 2;
        volatile unsigned int idat      : 2;
        volatile unsigned int dlen      : 12;
        volatile unsigned int iaen      : 1;
        volatile unsigned int iben      : 1;
        unsigned int reserved0          : 1;
        volatile unsigned int sfen      : 1;
    } Bits;
};

union _spi_sfstat {
    volatile unsigned int Register;
    struct __spi_sfstat {
        volatile unsigned int pcnt      : 10;
        unsigned int reserved1          : 6;
        volatile unsigned int dcnt      : 12;
        unsigned int reserved0          : 2;
        volatile unsigned int pbsy      : 1;
        volatile unsigned int dbsy      : 1;
    } Bits;
};

union _spi_id {
    volatile unsigned int Register;
    struct __spi_id {
        unsigned int reserved0          : 2;
        volatile unsigned int txfs      : 6;
        unsigned int reserved1          : 2;
        volatile unsigned int rxfs      : 6;
        volatile unsigned int id        : 8;
        unsigned int reserved2          : 2;
        volatile unsigned int cfg       : 1;
        volatile unsigned int rev       : 5;
    } Bits;
};

union _spi_data {
    volatile unsigned int Register;
    struct __spi_data_short {
        volatile unsigned short reserved;
        volatile unsigned short Bytes;
    } Short;
    struct __spi_data_char {
        volatile unsigned char reserved[3];
        volatile unsigned char Byte;
    } Char;
};

struct _spi_register {
    volatile unsigned int CLC;                           /*--- 0x00 ---*/
    union _spi_pisel        PISEL;                         /*--- 0x04 ---*/
    union _spi_id           ID;                            /*--- 0x08 ---*/
    unsigned int reserved_0x0c;                     /*--- 0x0C ---*/
    union _spi_con          CON;                           /*--- 0x10 ---*/
    union _spi_status       STAT;                          /*--- 0x14 ---*/
    volatile unsigned int WHBSTATE;                      /*--- 0x18 ---*/
    unsigned int reserved_0x1c;                     /*--- 0x1C ---*/
    union _spi_data TB;                            /*--- 0x20 ---*/
    volatile unsigned int RB;                            /*--- 0x24 ---*/
    unsigned int reserved_0x28;                     /*--- 0x28 ---*/
    unsigned int reserved_0x2C;                     /*--- 0x2C ---*/
    union _spi_fifo_ctrl    RXFCON;                        /*--- 0x30 ---*/
    union _spi_fifo_ctrl    TXFCON;                        /*--- 0x34 ---*/
    union _spi_fifo_stat    FSTAT;                         /*--- 0x38 ---*/
    unsigned int reserved_0x3C;                     /*--- 0x3C ---*/
    volatile unsigned int BRT;                           /*--- 0x40 ---*/
    volatile unsigned int BRSTAT;                        /*--- 0x44 ---*/
    unsigned int reserved_0x3C_0x60[(0x60-0x48)/4];     /*--- 0x48-0x60 ---*/
    union _spi_sfcon        SFCON;                         /*--- 0x60 ---*/
    union _spi_sfstat       SFSTAT;                        /*--- 0x64 ---*/
    unsigned int reserved_0x68;                     /*--- 0x68 ---*/
    unsigned int reserved_0x6C;                     /*--- 0x6C ---*/
    volatile unsigned int GPOCON;                        /*--- 0x70 ---*/
    volatile unsigned int GPOSTAT;                       /*--- 0x74 ---*/
    volatile unsigned int FGPO;                          /*--- 0x78 ---*/
    unsigned int reserved_0x7C;                     /*--- 0x7C ---*/
    volatile unsigned int RXREQ;                         /*--- 0x80 ---*/
    volatile unsigned int RXCNT;                         /*--- 0x84 ---*/
    unsigned int reserved_0x88_0xEC[(0xEC-0x88)/4];     /*--- 0x88-0xEC ---*/
    volatile unsigned int DMACON;                        /*--- 0xEC ---*/
    unsigned int reserved_0xF0;                     /*--- 0xF0 ---*/
    volatile unsigned int IRNEN;                         /*--- 0xF4 ---*/
    volatile unsigned int IRNICR;                        /*--- 0xF8 ---*/
    volatile unsigned int IRNCR;                         /*--- 0xFC ---*/
};

#endif /*--- #if !defined(_ASSEMBLER_) ---*/


/*------------------------------------------------------------------------------------------*\
 * aus urlader => include/flash16.h
\*------------------------------------------------------------------------------------------*/
#if defined(CONFIG_ARCH_PUMA5) || defined(CONFIG_AR9) || defined(CONFIG_VR9)  || defined(CONFIG_AR10) || defined(CONFIG_ARCH_AR724x) || defined(CONFIG_ARCH_AR934x)
    #define SPI_CMD_SHIFT_24   0
#else
    #define SPI_CMD_SHIFT_24   24
#endif
#define SPI_WRITE_ENABLE                    (0x06 << SPI_CMD_SHIFT_24)       /*--- write enable ---*/
#define SPI_WRITE_DISABLE                   (0x04 << SPI_CMD_SHIFT_24)       /*--- write disable ---*/
#define SPI_READ_ID                         (0x9F << 24)       /*--- read identification ---*/
#define SPI_READ_STATUS                     (0x05 << SPI_CMD_SHIFT_24)       /*--- read status register ---*/
#define SPI_WRITE_STATUS                    (0x01 << 24)       /*--- write status register ---*/
#define SPI_READ                            (0x03 << 24)       /*--- read data ---*/
#define SPI_FASTREAD                        (0x0B << 24)       /*--- fast read data ---*/
#define SPI_PARALLEL_MODE                   (0x55 << 24)       /*--- parallel mode ---*/
#define SPI_SECTOR_ERASE                    (0xD8 << 24)       /*--- sector erase ---*/ 
#define SPI_CHIP_ERASE                      (0xD7 << 24)       /*--- chip erase ---*/
#define SPI_PAGE_PROGRAM                    (0x02 << 24)       /*--- page program ---*/
#define SPI_DEEP_POWER_DOWN                 (0xB9 << 24)       /*--- deep power down ---*/
#define SPI_ENTER_4K                        (0xA5 << 24)       /*--- enter 4kb ---*/
#define SPI_EXIT_4K                         (0xB5 << 24)       /*--- exit 4kb ---*/
#define SPI_RELEASE_POWER_DOWN              (0xAB << 24)       /*--- release from deep power-down ---*/
#define SPI_READ_ELECTRONIK_ID  SPI_REALEASE_POWER_DOWN        /*--- read electronic id ---*/
#define SPI_READ_ELECTRONIC_ID_MANUFACTURE  (0x9F << SPI_CMD_SHIFT_24)       /*--- read electronic manufacturer id & device id ---*/

/*--- status bits ---*/ 
#define WIP                 (1 << 0)
#define WEL                 (1 << 1)
#define BP0                 (1 << 2)
#define BP1                 (1 << 3)
#define BP2                 (1 << 4)
#define BP3                 (1 << 5)
#define PROG_ERROR          (1 << 6)
#define REG_WRITE_PROTECT   (1 << 7)

#define SPI_READ_ELECTRONIC_ID_MANUFACTURE  (0x9F << SPI_CMD_SHIFT_24)       /*--- read electronic manufacturer id & device id ---*/
#define MX_MANUFACTURE_ID                       0xC2      
#define MX_MEMORY_TYPE_SPI                      0x20      /*--- MX25L6405 ---*/
#define SPANSION_MANUFACTURE_ID                 0x01
#define SPANSION_MEMORY_TYPE_SPI                0x02      /*--- S25FL064A ---*/

#define FLASH_SUCCESS           0
#define FLASH_INVAL_ADDR        0x8002
#define FLASH_ID_FAILED         0x8003
#define FLASH_QUERY_FAILED      0x8004
#define FLASH_ERASE_REGIONS     0x8005
#define FLASH_MX_BOTTOM_FAILED  0x8006


/*------------------------------------------------------------------------------------------*\
 * neue Fehler für Flashfunktionen - werden negativ ausgegeben
 * z.B. -FLASH_ERASE_ERROR
\*------------------------------------------------------------------------------------------*/
#define FLASH_NO_ERROR          0
#define FLASH_ERASE_ERROR       1
#define FLASH_WRITE_ERROR       2
#define FLASH_BAD_BLOCK         3
#define FLASH_VERIFY_ERROR      4
#define FLASH_SINGLEBIT_ERROR   5
#define FLASH_MULTIBIT_ERROR    6
#define FLASH_PARAMETER_ERROR   7


struct _CFI_Erase_Regions_ {
    unsigned short  blk_num;
    unsigned int    blk_size;
    unsigned int    size;
};
 
struct _CFI_Device_Geometry_ {
    unsigned short  flash_interface_size;
    unsigned short  interface_code;
    unsigned short  writeBuffer_size;
    unsigned char   num_erase_regions;
    struct _CFI_Erase_Regions_ erase_regions[4];
};

struct tffs_spi_flash_device {
    unsigned char   Blockmode;
    unsigned int    Size;
    struct _CFI_Device_Geometry_    Geometry;
};


#endif
