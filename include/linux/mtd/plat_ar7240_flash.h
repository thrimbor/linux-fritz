
#ifndef __PLAT_AR7240_FLASH__
#define __PLAT_AR7240_FLASH__

#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>


struct ar7240_flash_data {
	unsigned int		flash_size;
	unsigned int		nr_parts;
	struct mtd_partition	*parts;
	const char		**probes;
};

#endif 
