/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  Copyright (C) 2001 Rusty Russell.
 *  Copyright (C) 2003, 2004 Ralf Baechle (ralf@linux-mips.org)
 *  Copyright (C) 2005 Thiemo Seufer
 */

#undef DEBUG

#include <linux/moduleloader.h>
#include <linux/elf.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/zugriff.h>
#include <linux/spinlock.h>
#include <asm/pgtable.h>	/* MODULE_START */
#include <linux/avm_kernel_config.h>

#define CONFIG_INSMOD_KSEG0 1

struct mips_hi16 {
	struct mips_hi16 *next;
	Elf_Addr *addr;
	Elf_Addr value;
};

static struct mips_hi16 *mips_hi16_list;

static LIST_HEAD(dbe_list);
static DEFINE_SPINLOCK(dbe_lock);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct _module_alloc_size_list {
    unsigned long size;
    unsigned int alloc;
    unsigned long addr;
    char name[64];
    enum _module_alloc_type_ type;
} module_alloc_size_list[50];

unsigned long module_alloc_size_list_base;
unsigned int module_alloc_size_list_size;

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
char *module_alloc_find_module_name(char *buff, char *end, unsigned long addr) {
    unsigned int i;
    unsigned int len;
    for(i = 0 ; i < sizeof(module_alloc_size_list) / sizeof(module_alloc_size_list[0]) ; i++) {
        if(module_alloc_size_list[i].alloc == 0)
            continue;
        if(addr < module_alloc_size_list[i].addr)
            continue;
        if(addr > module_alloc_size_list[i].size + module_alloc_size_list[i].addr)
            continue;
        len = snprintf(buff, end - buff, "0x%08lx (%s + 0x%lx)   [%s]", addr, 
                    module_alloc_size_list[i].name,
                    addr - module_alloc_size_list[i].addr,
                    module_alloc_size_list[i].name);
        return buff + len;
    }
    len = snprintf(buff, end - buff, "0x%08lx", addr);
    return buff + len;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void module_alloc_size_list_free(unsigned long addr) {
    unsigned int i;
    for(i = 0 ; i < sizeof(module_alloc_size_list) / sizeof(module_alloc_size_list[0]) ; i++) {
        if(module_alloc_size_list[i].addr == addr) {
            if(module_alloc_size_list[i].alloc == 1) {
                module_alloc_size_list[i].alloc = 0;
                return;
            }
        }
    }
    /*--- printk(KERN_ERR "[module-alloc-by-name] pointer 0x%lx isn't in kseg0-module-list\n", addr); ---*/
    return;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#if defined(CONFIG_CHECK_TIMER_ON_FREED_MODULE)
int module_alloc_check_pointer(unsigned long addr, char **name) {
    unsigned int i;
    *name = NULL;
    for(i = 0 ; i < sizeof(module_alloc_size_list) / sizeof(module_alloc_size_list[0]) ; i++) {
        if(module_alloc_size_list[i].addr > addr) {
            continue;
        }
        if(module_alloc_size_list[i].addr + module_alloc_size_list[i].size <= addr) {
            continue;
        }
        if(module_alloc_size_list[i].alloc == 1) {
            return 0;
        }
        *name = module_alloc_size_list[i].name;
        return -1;
    }
    /*--- printk(KERN_ERR "[module-alloc-by-name] pointer 0x%lx isn't in kseg0-module-list\n", addr); ---*/
    return 1;
}
#endif /*--- #if defined(CONFIG_CHECK_TIMER_ON_FREED_MODULE) ---*/

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
char *module_load_black_list[] = { 
    /*--- "pcmlink", "isdn_fbox_fon5", "capi_codec", "usbcore", "ifxusb_host", NULL ---*/
    NULL
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
unsigned long module_alloc_size_list_alloc(unsigned long size, char *name, enum _module_alloc_type_ type) {
    unsigned int i;
    char **p;

    if((module_alloc_size_list_size == 0) || (module_alloc_size_list_base == 0))
        return 0L;

    /*--- printk(KERN_ERR "[module-alloc-by-name] module '%s' size 0x%lx type %s\n", name, size, type == module_alloc_type_core ? "core" : "init"); ---*/
    p = module_load_black_list;
    while(*p) {
        if(!strcmp(*p, name)) {
            printk(KERN_ERR "[module-alloc-by-name] module '%s' on black list, use normal alloc\n", name);
            return 0UL;
        }
        p++;
    }

    /*--- printk(KERN_ERR "[module-alloc-by-name] next base is 0x%lx, rest size is 0x%x\n", module_alloc_size_list_base, module_alloc_size_list_size); ---*/

    size +=  ((1 << PAGE_SHIFT) - 1);
    size &= ~((1 << PAGE_SHIFT) - 1);

    for(i = 0 ; i < sizeof(module_alloc_size_list) / sizeof(module_alloc_size_list[0]) ; i++) {
        if(!strcmp(module_alloc_size_list[i].name, name)) { /*--- name gefunden ---*/
            if(module_alloc_size_list[i].type == type) {
                /*--- printk(KERN_ERR "[module-alloc-by-name] module '%s' found\n", name); ---*/
                break;
            }
        }
        if(module_alloc_size_list[i].name[0] == '\0') {
            strcpy(module_alloc_size_list[i].name, name);
            module_alloc_size_list[i].type = type;
            /*--- printk(KERN_ERR "[module-alloc-by-name] new module '%s' will use entry %d\n", name, i); ---*/
            break;
        }
    }
    if(i == sizeof(module_alloc_size_list) / sizeof(module_alloc_size_list[0])) {
        printk(KERN_ERR "[module-alloc-by-name] module alloc table full\n");
        return 0UL;
    }

    if(module_alloc_size_list[i].alloc != 0) {
        printk(KERN_ERR "[module-alloc-by-name] segment for %s is %s\n", name,
            module_alloc_size_list[i].alloc == 1 ? "already allocated" :
            module_alloc_size_list[i].alloc == 2 ? "locked, because of size" : "unknown locked");
        return 0UL;
    }

    /*--- wenn noch keine addresse, dann festlegen ---*/
    if(module_alloc_size_list[i].addr == 0) {
        module_alloc_size_list[i].size = size;
        if(size > module_alloc_size_list_size) {
            printk(KERN_ERR "[module-alloc-by-name] no kseg0-space for module '%s' (0x%lx bytes) -> use ksseg\n", name, size);
            module_alloc_size_list[i].alloc = 2;
            return 0UL;
        }
        module_alloc_size_list[i].addr = module_alloc_size_list_base;

        module_alloc_size_list_size -= size;
        module_alloc_size_list_base += size;
        module_alloc_size_list[i].alloc = 1;

        printk(KERN_ERR "[module-alloc-by-name] give 0x%lx bytes at 0x%lx to module '%s' (0x%x bytes left)\n", 
            module_alloc_size_list[i].size, module_alloc_size_list[i].addr, module_alloc_size_list[i].name,
            module_alloc_size_list_size 
            );
        if(kernel_modulmemory_config) {
            struct _kernel_modulmemory_config *C = kernel_modulmemory_config;
            while(C->name) {
                if(!strcmp(C->name, module_alloc_size_list[i].name)) {
                    printk(KERN_ERR "[module-alloc-by-name] 0x%lx bytes used, 0x%x bytes expected\n", 
                        module_alloc_size_list[i].size, C->size);
                    break;
                }
                C++;
            }
        }
    }

    /*--- segment groesse überprüfen ---*/
    if(module_alloc_size_list[i].size != size) {
        printk(KERN_ERR "[module-alloc-by-name] invalid size change 0x%lx bytes != 0x%lx bytes (module '%s')\n", 
            module_alloc_size_list[i].size, size, module_alloc_size_list[i].name);

        if(module_alloc_size_list[i].size < size) {
            module_alloc_size_list[i].size = size;
        }
        module_alloc_size_list[i].alloc = 2;
        return 0UL;
    }
    return module_alloc_size_list[i].addr;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void *module_alloc(unsigned long size, char *name __attribute__ ((unused)), enum _module_alloc_type_ type __attribute__ ((unused))) {
    void *ptr;
    switch(type) {
        case module_alloc_type_init:
            ptr = vmalloc(size);
            break;
        case module_alloc_type_core:
            ptr = (void *)module_alloc_size_list_alloc(size, name, type);
            if(ptr == NULL)
                ptr = vmalloc(size);
            break;
        default:
        case module_alloc_type_page:
            ptr = vmalloc(size);
            break;
    }
    /*--- do_memory_check(); ---*/
    return ptr;
}

/* Free memory returned from module_alloc */
void module_free(struct module *mod, void *module_region) {
    if((((unsigned long)module_region) & 0xE0000000) == 0xC0000000) {
        vfree(module_region);
        return;
    }
    module_alloc_size_list_free((unsigned long)module_region);
    return;
}

int module_frob_arch_sections(Elf_Ehdr *hdr, Elf_Shdr *sechdrs,
			      char *secstrings, struct module *mod)
{
	return 0;
}

static int apply_r_mips_none(struct module *me, u32 *location, Elf_Addr v)
{
	return 0;
}

static int apply_r_mips_32_rel(struct module *me, u32 *location, Elf_Addr v)
{
    if((u32)location & 0x3) {
        /*--- supress unaligned trap-handling ---*/
        u32 tmp = extract_unaligned_dword(location);
        set_unaligned_dword(location, tmp + v);
    } else {
        *location += v;
    }
	return 0;
}

static int apply_r_mips_32_rela(struct module *me, u32 *location, Elf_Addr v)
{
    if((u32)location & 0x3) {
        /*--- supress unaligned trap-handling ---*/
        set_unaligned_dword(location, v);
    } else {
        *location = v;
    }
	return 0;
}

static int apply_r_mips_26_rel(struct module *me, u32 *location, Elf_Addr v)
{
	if (v % 4) {
		pr_err("module %s: dangerous R_MIPS_26 REL relocation\n",
		       me->name);
		return -ENOEXEC;
	}

	if ((v & 0xf0000000) != (((unsigned long)location + 4) & 0xf0000000)) {
		printk(KERN_ERR
		       "module %s: relocation overflow\n",
		       me->name);
		return -ENOEXEC;
	}

	*location = (*location & ~0x03ffffff) |
	            ((*location + (v >> 2)) & 0x03ffffff);

	return 0;
}

static int apply_r_mips_26_rela(struct module *me, u32 *location, Elf_Addr v)
{
	if (v % 4) {
		pr_err("module %s: dangerous R_MIPS_26 RELArelocation\n",
		       me->name);
		return -ENOEXEC;
	}

	if ((v & 0xf0000000) != (((unsigned long)location + 4) & 0xf0000000)) {
		printk(KERN_ERR
		       "module %s: relocation overflow\n",
		       me->name);
		return -ENOEXEC;
	}

	*location = (*location & ~0x03ffffff) | ((v >> 2) & 0x03ffffff);

	return 0;
}

static int apply_r_mips_hi16_rel(struct module *me, u32 *location, Elf_Addr v)
{
	struct mips_hi16 *n;

	/*
	 * We cannot relocate this one now because we don't know the value of
	 * the carry we need to add.  Save the information, and let LO16 do the
	 * actual relocation.
	 */
	n = kmalloc(sizeof *n, GFP_KERNEL);
	if (!n)
		return -ENOMEM;

	n->addr = (Elf_Addr *)location;
	n->value = v;
	n->next = mips_hi16_list;
	mips_hi16_list = n;

	return 0;
}

static int apply_r_mips_hi16_rela(struct module *me, u32 *location, Elf_Addr v)
{
	*location = (*location & 0xffff0000) |
	            ((((long long) v + 0x8000LL) >> 16) & 0xffff);

	return 0;
}

static int apply_r_mips_lo16_rel(struct module *me, u32 *location, Elf_Addr v)
{
	unsigned long insnlo = *location;
	Elf_Addr val, vallo;

	/* Sign extend the addend we extract from the lo insn.  */
	vallo = ((insnlo & 0xffff) ^ 0x8000) - 0x8000;

	if (mips_hi16_list != NULL) {
		struct mips_hi16 *l;

		l = mips_hi16_list;
		while (l != NULL) {
			struct mips_hi16 *next;
			unsigned long insn;

			/*
			 * The value for the HI16 had best be the same.
			 */
			if (v != l->value)
				goto out_danger;

			/*
			 * Do the HI16 relocation.  Note that we actually don't
			 * need to know anything about the LO16 itself, except
			 * where to find the low 16 bits of the addend needed
			 * by the LO16.
			 */
			insn = *l->addr;
			val = ((insn & 0xffff) << 16) + vallo;
			val += v;

			/*
			 * Account for the sign extension that will happen in
			 * the low bits.
			 */
			val = ((val >> 16) + ((val & 0x8000) != 0)) & 0xffff;

			insn = (insn & ~0xffff) | val;
			*l->addr = insn;

			next = l->next;
			kfree(l);
			l = next;
		}

		mips_hi16_list = NULL;
	}

	/*
	 * Ok, we're done with the HI16 relocs.  Now deal with the LO16.
	 */
	val = v + vallo;
	insnlo = (insnlo & ~0xffff) | (val & 0xffff);
	*location = insnlo;

	return 0;

out_danger:
	pr_err("module %s: dangerous R_MIPS_LO16 REL relocation\n", me->name);

	return -ENOEXEC;
}

static int apply_r_mips_lo16_rela(struct module *me, u32 *location, Elf_Addr v)
{
	*location = (*location & 0xffff0000) | (v & 0xffff);

	return 0;
}

static int apply_r_mips_64_rela(struct module *me, u32 *location, Elf_Addr v)
{
	*(Elf_Addr *)location = v;

	return 0;
}

static int apply_r_mips_higher_rela(struct module *me, u32 *location,
				    Elf_Addr v)
{
	*location = (*location & 0xffff0000) |
	            ((((long long) v + 0x80008000LL) >> 32) & 0xffff);

	return 0;
}

static int apply_r_mips_highest_rela(struct module *me, u32 *location,
				     Elf_Addr v)
{
	*location = (*location & 0xffff0000) |
	            ((((long long) v + 0x800080008000LL) >> 48) & 0xffff);

	return 0;
}

static int (*reloc_handlers_rel[]) (struct module *me, u32 *location,
				Elf_Addr v) = {
	[R_MIPS_NONE]		= apply_r_mips_none,
	[R_MIPS_32]		= apply_r_mips_32_rel,
	[R_MIPS_26]		= apply_r_mips_26_rel,
	[R_MIPS_HI16]		= apply_r_mips_hi16_rel,
	[R_MIPS_LO16]		= apply_r_mips_lo16_rel
};

static int (*reloc_handlers_rela[]) (struct module *me, u32 *location,
				Elf_Addr v) = {
	[R_MIPS_NONE]		= apply_r_mips_none,
	[R_MIPS_32]		= apply_r_mips_32_rela,
	[R_MIPS_26]		= apply_r_mips_26_rela,
	[R_MIPS_HI16]		= apply_r_mips_hi16_rela,
	[R_MIPS_LO16]		= apply_r_mips_lo16_rela,
	[R_MIPS_64]		= apply_r_mips_64_rela,
	[R_MIPS_HIGHER]		= apply_r_mips_higher_rela,
	[R_MIPS_HIGHEST]	= apply_r_mips_highest_rela
};

int apply_relocate(Elf_Shdr *sechdrs, const char *strtab,
		   unsigned int symindex, unsigned int relsec,
		   struct module *me)
{
	Elf_Mips_Rel *rel = (void *) sechdrs[relsec].sh_addr;
	Elf_Sym *sym;
	u32 *location;
	unsigned int i;
	Elf_Addr v;
	int res;

	pr_debug("Applying relocate section %u to %u\n", relsec,
	       sechdrs[relsec].sh_info);

	for (i = 0; i < sechdrs[relsec].sh_size / sizeof(*rel); i++) {
		/* This is where to make the change */
		location = (void *)sechdrs[sechdrs[relsec].sh_info].sh_addr
			+ rel[i].r_offset;
		/* This is the symbol it is referring to */
		sym = (Elf_Sym *)sechdrs[symindex].sh_addr
			+ ELF_MIPS_R_SYM(rel[i]);
		if (IS_ERR_VALUE(sym->st_value)) {
			/* Ignore unresolved weak symbol */
			if (ELF_ST_BIND(sym->st_info) == STB_WEAK)
				continue;
			printk(KERN_WARNING "%s: Unknown symbol %s\n",
			       me->name, strtab + sym->st_name);
			return -ENOENT;
		}

		v = sym->st_value;

		res = reloc_handlers_rel[ELF_MIPS_R_TYPE(rel[i])](me, location, v);
		if (res)
			return res;
	}

	return 0;
}

int apply_relocate_add(Elf_Shdr *sechdrs, const char *strtab,
		       unsigned int symindex, unsigned int relsec,
		       struct module *me)
{
	Elf_Mips_Rela *rel = (void *) sechdrs[relsec].sh_addr;
	Elf_Sym *sym;
	u32 *location;
	unsigned int i;
	Elf_Addr v;
	int res;

	pr_debug("Applying relocate section %u to %u\n", relsec,
	       sechdrs[relsec].sh_info);

	for (i = 0; i < sechdrs[relsec].sh_size / sizeof(*rel); i++) {
		/* This is where to make the change */
		location = (void *)sechdrs[sechdrs[relsec].sh_info].sh_addr
			+ rel[i].r_offset;
		/* This is the symbol it is referring to */
		sym = (Elf_Sym *)sechdrs[symindex].sh_addr
			+ ELF_MIPS_R_SYM(rel[i]);
		if (IS_ERR_VALUE(sym->st_value)) {
			/* Ignore unresolved weak symbol */
			if (ELF_ST_BIND(sym->st_info) == STB_WEAK)
				continue;
			printk(KERN_WARNING "%s: Unknown symbol %s\n",
			       me->name, strtab + sym->st_name);
			return -ENOENT;
		}

		v = sym->st_value + rel[i].r_addend;

		res = reloc_handlers_rela[ELF_MIPS_R_TYPE(rel[i])](me, location, v);
		if (res)
			return res;
	}

	return 0;
}

/* Given an address, look for it in the module exception tables. */
const struct exception_table_entry *search_module_dbetables(unsigned long addr)
{
	unsigned long flags;
	const struct exception_table_entry *e = NULL;
	struct mod_arch_specific *dbe;

	spin_lock_irqsave(&dbe_lock, flags);
	list_for_each_entry(dbe, &dbe_list, dbe_list) {
		e = search_extable(dbe->dbe_start, dbe->dbe_end - 1, addr);
		if (e)
			break;
	}
	spin_unlock_irqrestore(&dbe_lock, flags);

	/* Now, if we found one, we are running inside it now, hence
           we cannot unload the module, hence no refcnt needed. */
	return e;
}

/* Put in dbe list if necessary. */
int module_finalize(const Elf_Ehdr *hdr,
		    const Elf_Shdr *sechdrs,
		    struct module *me)
{
	const Elf_Shdr *s;
	char *secstrings = (void *)hdr + sechdrs[hdr->e_shstrndx].sh_offset;

	INIT_LIST_HEAD(&me->arch.dbe_list);
	for (s = sechdrs; s < sechdrs + hdr->e_shnum; s++) {
		if (strcmp("__dbe_table", secstrings + s->sh_name) != 0)
			continue;
		me->arch.dbe_start = (void *)s->sh_addr;
		me->arch.dbe_end = (void *)s->sh_addr + s->sh_size;
		spin_lock_irq(&dbe_lock);
		list_add(&me->arch.dbe_list, &dbe_list);
		spin_unlock_irq(&dbe_lock);
	}
	return 0;
}

void module_arch_cleanup(struct module *mod)
{
	spin_lock_irq(&dbe_lock);
	list_del(&mod->arch.dbe_list);
	spin_unlock_irq(&dbe_lock);
}
