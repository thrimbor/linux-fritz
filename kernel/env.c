#include <linux/autoconf.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/module.h>

#if defined(CONFIG_ARM)
#include <asm/arch/io.h>
#endif /*--- #if defined(CONFIG_ARM) ---*/
#include <linux/env.h>

#if defined(CONFIG_MIPS)
#include <asm/prom.h>
#define EMIF_ADDRESS
#define phys_to_virt        /*--- ist nur beim DAVINCI notwendig ---*/

#ifdef CONFIG_MIPS_UR8
#include <asm/setup.h>

char *dst_env[64 * 2] __attribute__ ((section(".init.data")));
char *dst_config[4] __attribute__ ((section(".init.data")));
char dst_env_buff[2048] __attribute__((section(".init.data")));
char *dst_cmdline[COMMAND_LINE_SIZE] __attribute__ ((section(".init.data")));
char dst_cmdline_buff[64 *1024] __attribute__((section(".init.data")));
#endif /*--- #ifdef CONFIG_MIPS_UR8 ---*/
#endif /*--- #if defined(CONFIG_MIPS) ---*/

/*--- #define DEBUG_PROM_INIT ---*/

#if defined(DEBUG_PROM_INIT)
	#define PROM_PRINTK(...) printk(__VA_ARGS__)
#else
	#define PROM_PRINTK(...)
#endif


#if defined(CONFIG_ARCH_PUMA5)
#warning: todo ? prom_getenv in arch/arm/kernel/setup.c ?!
#else 
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#define prom_envp(index) ((char *)(long)_prom_envp[(index)])

static char env_buffer[2048];
static char *_local_envp[64 * 2];
unsigned int fritz_box_hw_revision = 0;
int *_prom_envp;

#if defined(DEBUG_PROM_INIT)
void print_env( char **myenv ){
    int i = 0;
    PROM_PRINTK( "myenvp=%p\n", myenv );
    while ( myenv[i] && myenv[i+1] ) {
        PROM_PRINTK( "(%p)%s : (%p)%s\n", myenv[i], myenv[i], myenv[i+1],myenv[i+1]);
        i += 2;
    }
}

void print_cmdline( int argc, char **argv ){
    
    int i = 0;
    PROM_PRINTK( "argc=%d argv=%p\n", argc, argv );
    for (i = 0; i < argc; i++) {
       PROM_PRINTK ( "[%d]: %s \n", i, argv[i]);
    }
}

extern unsigned long fw_arg0, fw_arg1;
#endif /*--- #if defined(DEBUG_PROM_INIT) ---*/

void __init env_init(int *fw_arg2, enum _env_location env_location) {
    unsigned int i;
    char *p;
    static unsigned int once = 0;
    if(once)
        return;
    once = 1;

    _prom_envp = fw_arg2;

#if defined(DEBUG_PROM_INIT)
    print_env( (char **)_prom_envp );
    print_cmdline( fw_arg0, (char**)fw_arg1 );
#endif /*--- #if defined(DEBUG_PROM_INIT) ---*/

#if defined(CONFIG_MIPS_OHIO) || defined(CONFIG_MIPS_AR7) || defined(CONFIG_MACH_ATHEROS)
    PROM_PRINTK("[prom_init] 0\n");
#else
#if defined(CONFIG_MIPS_UR8)
    PROM_PRINTK("[prom_init] fw_arg2 0x%p\n", _prom_envp);
    PROM_PRINTK("[prom_init] dst_env 0x%p\n", dst_env);
    PROM_PRINTK("[prom_init] fw_arg1 0x%p\n", fw_arg1);
    PROM_PRINTK("[prom_init] dst_cmdline%p\n", dst_cmdline);
#endif /*--- #if defined(CONFIG_MIPS_UR8) ---*/
    PROM_PRINTK("[prom_init] 0x%p [0]\n", _prom_envp[0]);
    PROM_PRINTK("[prom_init] 0x%p [1]\n", _prom_envp[1]);
    PROM_PRINTK("[prom_init] 0x%p [2]\n", _prom_envp[2]);
    if(((_prom_envp[1] & 0xC0000000) == 0x80000000) && ((_prom_envp[2] & 0xC0000000) == 0x80000000)) {
        PROM_PRINTK("[prom_init] switch to ram location\n");
        env_location = ENV_LOCATION_PHY_RAM;
    } else {
        PROM_PRINTK("[prom_init] switch to flash location\n");
        env_location = ENV_LOCATION_FLASH;
    }
#endif /*--- #if defined(CONFIG_MIPS_OHIO) || defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_UR8) ---*/
    /*--------------------------------------------------------------------------------------*\
    \*--------------------------------------------------------------------------------------*/
    env_buffer[0] = '\0';
    p = env_buffer;

    /*--------------------------------------------------------------------------------------*\
     * copy envp values from urlader memory to (non init) kernel memory
    \*--------------------------------------------------------------------------------------*/
    for(i = 0 ; _prom_envp && _prom_envp[i] && _prom_envp[i + 1] ; i += 2) {
#if defined(DEBUG_PROM_INIT)
#if defined(CONFIG_MIPS_OHIO) || defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_UR8) || defined(CONFIG_MACH_ATHEROS)
        PROM_PRINTK("[prom_init] envp[%u]= \"%s\"=\"%s\" \n", i, (char *)_prom_envp[i], (char *)_prom_envp[i + 1]);
#else
        switch(env_location) {
            case ENV_LOCATION_FLASH:
                PROM_PRINTK("[prom_init] envp[%u]= \"%s\"=\"%s\" \n", i, (char *)phys_to_virt(_prom_envp[i]), (char *)EMIF_ADDRESS(_prom_envp[i + 1]));
                break;
            case ENV_LOCATION_PHY_RAM:
                PROM_PRINTK("[prom_init] envp[%u]= \"%s\"=\"%s\" \n", i, (char *)phys_to_virt(_prom_envp[i]), (char *)phys_to_virt(_prom_envp[i + 1]));
                break;
        }
#endif /*--- #if defined(CONFIG_MIPS_OHIO) || defined(CONFIG_MIPS_AR7) || defined(CONFIG_MIPS_UR8) ---*/
#endif /*--- #if defined(DEBUG_PROM_INIT) ---*/
        _local_envp[i] = p;
        strcat(p, (char *)phys_to_virt(_prom_envp[i]));
        p += strlen((char *)phys_to_virt(_prom_envp[i])) + 1;
        while((unsigned int)p & 0x3)  /*--- align auf naechstes wort ---*/
            *p++ = '\0';

        _local_envp[i + 1] = p;
        switch(env_location) {
            case ENV_LOCATION_FLASH:
                strcat(p, (char *)EMIF_ADDRESS(_prom_envp[i + 1]));
                p += strlen((char *)EMIF_ADDRESS(_prom_envp[i + 1])) + 1;
                break;
            case ENV_LOCATION_PHY_RAM:
                strcat(p, (char *)phys_to_virt(_prom_envp[i + 1]));
                p += strlen((char *)phys_to_virt(_prom_envp[i + 1])) + 1;
                break;
            case ENV_LOCATION_VIRT_RAM:
                strcat(p, (char *)(_prom_envp[i + 1]));
                p += strlen((char *)(_prom_envp[i + 1])) + 1;
                break;
        }
        while((unsigned int)p & 0x3)  /*--- align auf naechstes wort ---*/
            *p++ = '\0';
    }
    _local_envp[i] = NULL;
    _local_envp[i + 1] = NULL;
    _prom_envp = (int *)_local_envp;

    fritz_box_hw_revision = simple_strtoul(prom_getenv("HWRevision"), NULL, 10);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
char *prom_getenv(char *envname)
{
    /*
     * Return a pointer to the given environment variable.
     * In 64-bit mode: we're using 64-bit pointers, but all pointers
     * in the PROM structures are only 32-bit, so we need some
     * workarounds, if we are running in 64-bit mode.
     */
    int i, index=0;

    if(!_prom_envp)
        return NULL;

    i = strlen(envname);

    while (prom_envp(index)) {
        if(strncmp(envname, prom_envp(index), i) == 0) {
            return (prom_envp(index+1));
        }
        index += 2;
    }

    return NULL;
}

EXPORT_SYMBOL(prom_getenv);

#endif

