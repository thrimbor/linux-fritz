/*------------------------------------------------------------------------------------------*\
 *
\*------------------------------------------------------------------------------------------*/
#ifndef _ENV_H_
#define _ENV_H_

/*------------------------------------------------------------------------------------------*\
 * [0] : 123
 * [1] : 1=paged flash, 0 restricted flash
 * [2] : 0=normal audio I2C adr, 1=altenative audio I2C adr
 * [3] : unused
 * [4] : unused
\*------------------------------------------------------------------------------------------*/
extern unsigned int davinci_revision[5];

enum _env_location {
    ENV_LOCATION_FLASH = 0,
    ENV_LOCATION_PHY_RAM = 1,
    ENV_LOCATION_VIRT_RAM = 2
};
extern void env_init(int *penv, enum _env_location );
extern char *prom_getenv(char *envname);
extern char *getcmdline(void);

#include <asm/prealloc_memory.h>
#endif

