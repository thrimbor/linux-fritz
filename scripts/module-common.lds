/*
 * Common module linker script, always used when linking a module.
 * Archs are free to supply their own linker scripts.  ld will
 * combine them automatically.
 */


SECTIONS {
	/* Bug table for data bus errors */
	__bug_debug_table : {
		__start___module_bug_debug_table = .;
		*(__bug_debug_table)
		__stop___module_bug_debug_table = .;
	}
	/DISCARD/ : { *(.discard) }
}
