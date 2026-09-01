#ifndef __LINUX_MTD_NAND_DIRECT_AVM_H
#define __LINUX_MTD_NAND_DIRECT_AVM_H

#include <linux/mtd/nand.h>

struct direct_avm_nand_platdata {
#if defined(CONFIG_MTD_NAND_DIRECT_AVM_ADDR)
	int	addr_nce;
	int	addr_nwp;
	int	addr_cle;
	int	addr_ale;
#endif /*--- #if defined(CONFIG_MTD_NAND_DIRECT_AVM_ADDR) ---*/
	int	gpio_rdy0;
#if defined(CONFIG_MTD_NAND_DIRECT_AVM_GPIO) || defined(CONFIG_MTD_NAND_COMPLETE_AVM)
#define AVM_NAND_RDY(x)         ((x)->plat.current_chip == 0 ? (x)->plat.gpio_rdy0 : (x)->plat.gpio_rdy1)
#define AVM_NAND_RDY_PLAT(x)    ((x)->current_chip == 0 ? (x)->gpio_rdy0 : (x)->gpio_rdy1)
	int	gpio_rdy1;
	int	gpio_nwp;
#define AVM_NAND_NCE(x)         ((x)->plat.current_chip == 0 ? (x)->plat.gpio_nce0 : (x)->plat.gpio_nce1)
#define AVM_NAND_NCE_PLAT(x)    ((x)->current_chip == 0 ? (x)->gpio_nce0 : (x)->gpio_nce1)
	int	gpio_nce0;
	int	gpio_nce1;
	int	gpio_ale;
	int	gpio_cle;
#endif /*--- #if defined(CONFIG_MTD_NAND_DIRECT_AVM_GPIO) || defined(CONFIG_MTD_NAND_COMPLETE_AVM) ---*/
	void	(*adjust_parts)(struct direct_avm_nand_platdata *, size_t);
	struct mtd_partition *parts;
	unsigned int num_parts;
	unsigned int options;
    unsigned int current_chip;
	int	chip_delay;
};

#endif /*--- #ifndef __LINUX_MTD_NAND_DIRECT_AVM_H ---*/
