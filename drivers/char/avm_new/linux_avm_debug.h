#ifndef _linux_avm_debug_h_
#define _linux_avm_debug_h_
/*--------------------------------------------------------------------------------*\
 * die /dev/debug-Schnittstelle
\*--------------------------------------------------------------------------------*/
#include <linux/types.h>
#include <stdarg.h>

/*--------------------------------------------------------------------------------*\
 * alle Daten koennen mit cat /dev/debug abgeholt werden
\*--------------------------------------------------------------------------------*/
void avm_DebugPrintf(const char *format, ...);
int avm_DebugvPrintf(unsigned *Mode, const char *format, va_list marker);

/*-------------------------------------------------------------------------------------*\
 * Debug-Funktion am Treiber anmelden
 * prefix: der Inputdaten werden nach diesem Prefix durchsucht, und bei Match 
 * wird die CallbackFkt aufgerufen
 * um also den string 'blabla=haha' zum Treiber angemeldet mit prefix 'unicate_' zu transportieren
 * ist einfach ein "echo unicate_blabla=haha >/dev/debug" auf der Konsole auszufuehren
 * ret: handle (fuer UnRegister)
\*-------------------------------------------------------------------------------------*/
void *avm_DebugCallRegister(char *prefix, void (*CallBackDebug)(char *string, void *refdata), void *refdata);

/*--------------------------------------------------------------------------------*\
 * Debug-Funktion am Treiber abmelden
\*--------------------------------------------------------------------------------*/
void avm_DebugCallUnRegister(void *handle);

/*--------------------------------------------------------------------------------*\
 * signal 0 ...31 
 * signal: 0 supportdata per pushmail
\*--------------------------------------------------------------------------------*/
void avm_DebugSignal(unsigned int signal);
void avm_DebugSignalLog(unsigned int signal, char *fmt, ...);

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct file;
struct inode;

typedef ssize_t (*avm_debug_write_t)(struct file *, const char *, size_t , loff_t *);
typedef ssize_t (*avm_debug_read_t)(struct file *, char *, size_t , loff_t *);
typedef int (*avm_debug_open_t)(struct inode *, struct file *);
typedef int (*avm_debug_close_t)(struct inode *, struct file *);
typedef long (*avm_debug_ioctl_t)(struct file *, unsigned, unsigned long);

int avm_debug_register_minor(int minor, 
                             avm_debug_open_t,
                             avm_debug_close_t,
                             avm_debug_write_t,
                             avm_debug_read_t,
                             avm_debug_ioctl_t);

int avm_debug_release_minor(int);

/*------------------------------------------------------------------------------------------*\
 * Genutzte Minor Nummern
\*------------------------------------------------------------------------------------------*/
#define NLR_AUDIO_MINOR            1

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#define AVM_DEBUG_MAX_MINOR        1
#define AVM_DEBUG_MINOR_COUNT (AVM_DEBUG_MAX_MINOR+1)

#endif /*--- #ifndef _linux_avm_debug_h_ ---*/
