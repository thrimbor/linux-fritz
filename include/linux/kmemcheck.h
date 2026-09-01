#ifndef LINUX_KMEMCHECK_H
#define LINUX_KMEMCHECK_H

#include <linux/mm_types.h>
#include <linux/types.h>

#ifdef CONFIG_KMEMCHECK
extern int kmemcheck_enabled;

/* The slab-related functions. */
void kmemcheck_alloc_shadow(struct page *page, int order, gfp_t flags, int node);
void kmemcheck_free_shadow(struct page *page, int order);
void kmemcheck_slab_alloc(struct kmem_cache *s, gfp_t gfpflags, void *object,
			  size_t size);
void kmemcheck_slab_free(struct kmem_cache *s, void *object, size_t size);

void kmemcheck_pagealloc_alloc(struct page *p, unsigned int order,
			       gfp_t gfpflags);

void kmemcheck_show_pages(struct page *p, unsigned int n);
void kmemcheck_hide_pages(struct page *p, unsigned int n);

bool kmemcheck_page_is_tracked(struct page *p);

void kmemcheck_mark_unallocated(void *address, unsigned int n);
void kmemcheck_mark_uninitialized(void *address, unsigned int n);
void kmemcheck_mark_initialized(void *address, unsigned int n);
void kmemcheck_mark_freed(void *address, unsigned int n);

void kmemcheck_mark_unallocated_pages(struct page *p, unsigned int n);
void kmemcheck_mark_uninitialized_pages(struct page *p, unsigned int n);
void kmemcheck_mark_initialized_pages(struct page *p, unsigned int n);

int kmemcheck_show_addr(unsigned long address);
int kmemcheck_hide_addr(unsigned long address);

bool kmemcheck_is_obj_initialized(unsigned long addr, size_t size);

#else
#define kmemcheck_enabled 0

static inline void
kmemcheck_alloc_shadow(struct page *page __attribute__ ((unused)),
                       int order __attribute__ ((unused)), 
                       gfp_t flags __attribute__ ((unused)),
                       int node __attribute__ ((unused)))
{
}

static inline void
kmemcheck_free_shadow(struct page *page __attribute__ ((unused)), int order __attribute__ ((unused)))
{
}

static inline void
kmemcheck_slab_alloc(struct kmem_cache *s __attribute__ ((unused)),
                     gfp_t gfpflags __attribute__ ((unused)),
                     void *object __attribute__ ((unused)),
                     size_t size __attribute__ ((unused)))
{
}

static inline void kmemcheck_slab_free(struct kmem_cache *s __attribute__ ((unused)),
                                       void *object __attribute__ ((unused)),
                                       size_t size __attribute__ ((unused)))
{
}

static inline void kmemcheck_pagealloc_alloc(struct page *p __attribute__ ((unused)),
	unsigned int order __attribute__ ((unused)), gfp_t gfpflags __attribute__ ((unused)))
{
}

static inline bool kmemcheck_page_is_tracked(struct page *p __attribute__ ((unused)))
{
	return false;
}

static inline void kmemcheck_mark_unallocated(void *address __attribute__ ((unused)), unsigned int n __attribute__ ((unused)))
{
}

static inline void kmemcheck_mark_uninitialized(void *address __attribute__ ((unused)), unsigned int n __attribute__ ((unused)))
{
}

static inline void kmemcheck_mark_initialized(void *address __attribute__ ((unused)), unsigned int n __attribute__ ((unused)))
{
}

static inline void kmemcheck_mark_freed(void *address __attribute__ ((unused)), unsigned int n __attribute__ ((unused)))
{
}

static inline void kmemcheck_mark_unallocated_pages(struct page *p __attribute__ ((unused)),
						    unsigned int n __attribute__ ((unused)))
{
}

static inline void kmemcheck_mark_uninitialized_pages(struct page *p __attribute__ ((unused)),
						      unsigned int n __attribute__ ((unused)))
{
}

static inline void kmemcheck_mark_initialized_pages(struct page *p __attribute__ ((unused)),
						    unsigned int n __attribute__ ((unused)))
{
}

static inline bool kmemcheck_is_obj_initialized(unsigned long addr __attribute__ ((unused)), size_t size __attribute__ ((unused)))
{
	return true;
}

#endif /* CONFIG_KMEMCHECK */

/*
 * Bitfield annotations
 *
 * How to use: If you have a struct using bitfields, for example
 *
 *     struct a {
 *             int x:8, y:8;
 *     };
 *
 * then this should be rewritten as
 *
 *     struct a {
 *             kmemcheck_bitfield_begin(flags);
 *             int x:8, y:8;
 *             kmemcheck_bitfield_end(flags);
 *     };
 *
 * Now the "flags_begin" and "flags_end" members may be used to refer to the
 * beginning and end, respectively, of the bitfield (and things like
 * &x.flags_begin is allowed). As soon as the struct is allocated, the bit-
 * fields should be annotated:
 *
 *     struct a *a = kmalloc(sizeof(struct a), GFP_KERNEL);
 *     kmemcheck_annotate_bitfield(a, flags);
 *
 * Note: We provide the same definitions for both kmemcheck and non-
 * kmemcheck kernels. This makes it harder to introduce accidental errors. It
 * is also allowed to pass NULL pointers to kmemcheck_annotate_bitfield().
 */
#define kmemcheck_bitfield_begin(name)	\
	int name##_begin[0];

#define kmemcheck_bitfield_end(name)	\
	int name##_end[0];

#define kmemcheck_annotate_bitfield(ptr, name)				\
	do {								\
		int _n;							\
									\
		if (!ptr)						\
			break;						\
									\
		_n = (long) &((ptr)->name##_end)			\
			- (long) &((ptr)->name##_begin);		\
		MAYBE_BUILD_BUG_ON(_n < 0);				\
									\
		kmemcheck_mark_initialized(&((ptr)->name##_begin), _n);	\
	} while (0)

#define kmemcheck_annotate_variable(var)				\
	do {								\
		kmemcheck_mark_initialized(&(var), sizeof(var));	\
	} while (0)							\

#endif /* LINUX_KMEMCHECK_H */
