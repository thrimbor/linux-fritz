/******************************************************************************
** FILE NAME    : avm_busmaster_handler.c
** COMPANY      : AVM
** AUTHOR       : Heiko Blobner
** DESCRIPTION  : Unter anderem bei der Umschaltung des Taktes bei der z.B. 7320
**                ist es noetig, alle RAM-Zugriffe zu unterbinden.
**                Dieses Modul stellt einen Mechanismus zur Verfuegung, der
**                registrierte Busmaster stoppen und wieder laufenlassen kann.
**
**                Ablauf:
**                -'prepare_stop'
**                  Aufgabe dieses Aufrufes ist es, dass die Devices sich auf die
**                  Unterbrechung des RAM Zugriffs vorbereiten können (z.B. keine
**                  neuen Aktion, DMAs aufsetzen)
**
**                  Return-Werte:
**                   .0      ..keine Fehler
**                   .EBUSY  ..beschaeftigt, sofort nocheinmal probieren (max 2mal)
**                   .EAGAIN ..bereite Umschalten vor, nach  'schedule()' nochmal triggern
**                   .'else' ..Fehler (Vorgang abbrechen)
**
**                -'stop'
**                  RAM-Zugriffe werden gestoppt.
**
**                  Return-Werte:
**                   .0      ..keine Fehler
**                   .EBUSY  ..beschaeftigt, sofort nocheinmal probieren (max 2mal)
**                   .'else' ..Fehler (Vorgang abbrechen)
**
**                -'run'
**                  Auf das RAM kann wieder zugegrieffen werden.
**
**                  Return-Werte:
**                   .0      ..keine Fehler
**                   .'else' ..Fehler (Vorgang abbrechen)
**
*******************************************************************************/
#include <asm/avm_busmaster_handler.h>
#include <asm-generic/errno-base.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/vmalloc.h>

/*-Defines----------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#define AVM_BMASTER_DESCRIPTION    "AVM Busmaster Handler"
#define AVM_BMASTER_VERSION        "0.0.1"

/*-Debugging--------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
#define AVM_BMASTER_DBG

#define AVM_BMASTER_DBG_ERROR(...)      printk(KERN_ERR "E[%s:%u]: ", __FUNCTION__, __LINE__); printk(__VA_ARGS__); printk("\n");
#ifdef AVM_BMASTER_DBG
    #define AVM_BMASTER_DBG_WARN(...)   printk(KERN_ERR "W[%s:%u]: ", __FUNCTION__, __LINE__); printk(__VA_ARGS__); printk("\n");
    #define AVM_BMASTER_DBG_INFO(...)   printk(KERN_ERR "I[%s:%u]: ", __FUNCTION__, __LINE__); printk(__VA_ARGS__); printk("\n");
#else /*--- #ifdef AVM_BMASTER_DBG ---*/
    #define AVM_BMASTER_DBG_WARN(...)   ;
    #define AVM_BMASTER_DBG_INFO(...)   ;
#endif /*--- #else ---*/ /*--- #ifdef AVM_BMASTER_DBG ---*/

/*-Types------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
typedef struct busmaster_key {
    struct busmaster_key   *prev;
    struct busmaster_key   *next;

    char                    name[32];
    int                     (*callback)(enum _avm_busmaster_cmd cmd);
}busmaster_key_t;

busmaster_key_t *first_registered_busmaster;
DECLARE_MUTEX(bm_mutex);

/*-Functions--------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
void avm_register_busmaster       (char *name, int (*callback)(enum _avm_busmaster_cmd cmd));
void avm_release_busmaster        (char *name);
int  avm_send_cmd_to_all_busmaster(enum _avm_busmaster_cmd cmd);
int  avm_run_busmaster            (void);
int  avm_prepare_busmaster_stop   (void);
int  avm_stop_busmaster           (void);

//---------
void avm_register_busmaster(char *name, int (*callback)(enum _avm_busmaster_cmd cmd)) {
    busmaster_key_t *bm;

    //---
    if(name == NULL) {
        AVM_BMASTER_DBG_ERROR("failed. 'name' is null.");
        goto err;
    }
    if(callback == NULL) {
        AVM_BMASTER_DBG_ERROR("failed. 'callback' is null. (name: '%s')", name);
        goto err;
    }

    //---
    bm = vmalloc(sizeof(busmaster_key_t));
    if(bm == NULL) {
        AVM_BMASTER_DBG_ERROR("failed. Not possible to aquire memory to store registration. (name: '%s')", name);
        goto err;
    }
    memset(bm, 0, sizeof(busmaster_key_t));

    strncpy(bm->name, name, 32);
    bm->name[31] = 0;
    bm->callback = callback;

    if(down_interruptible(&bm_mutex)) {
        AVM_BMASTER_DBG_ERROR("failed. Interrupted from waiting semaphore. (name: '%s')", name);
        goto err;
    }

    if(first_registered_busmaster == NULL) {
        first_registered_busmaster = bm;
    } else {
        bm->next = first_registered_busmaster;
        first_registered_busmaster->prev = bm;
        first_registered_busmaster = bm;
    }

    up(&bm_mutex);

    AVM_BMASTER_DBG_INFO("Registered busmaster device: %s", name);

    return;
err:
    AVM_BMASTER_DBG_ERROR("failed. Device is not registered.");
}

//---------
void avm_release_busmaster(char *name) {
    busmaster_key_t *bm;

    //---
    if(name == NULL) {
        AVM_BMASTER_DBG_ERROR("failed. 'name' is null.");
        goto err;
    }

    //---
    bm = first_registered_busmaster;
    while(bm) {
        if(!strcmp(bm->name, name)) {
            break;
        }

        bm = bm->next;
    }

    if(bm) {
        busmaster_key_t *prev;

        prev = bm->prev;

        if(prev == NULL) {
            prev = first_registered_busmaster;
        }

        prev->next = bm->next;

        if(bm->next != NULL) {
            bm->next->prev = prev;
        }

    } else {
        AVM_BMASTER_DBG_INFO("No registered busmaster found with name: '%s'.", name);
    }

    AVM_BMASTER_DBG_INFO("Released busmaster device: %s", name);

    return;
err:
    AVM_BMASTER_DBG_ERROR("failed. Device is not released.");
}

//---------
int avm_send_cmd_to_all_busmaster(enum _avm_busmaster_cmd cmd) {
    busmaster_key_t *bm;
    uint8_t retry = 0;

    bm = first_registered_busmaster;
    while(bm) {
        int result = bm->callback(cmd);

        switch(cmd) {
            case avm_busmaster_prepare_stop:
                switch(result) {
                    case 0:
                        retry = 0;
                        bm = bm->next;
                        break;
                    case -EBUSY:
                        retry++;

                        if(retry > 5) {
                            AVM_BMASTER_DBG_ERROR("failed. Busmaster couldnt be stopped (-EBUSY). (Name: %s)", bm->name);
                            goto err;
                        }
                        break;
                    case -EAGAIN:
                        retry++;

                        if(retry > 2) {
                            AVM_BMASTER_DBG_ERROR("failed. Busmaster couldnt be stopped (-EAGAIN). (Name: %s)", bm->name);
                            goto err;
                        }
                        schedule();
                        break;
                    default:
                        AVM_BMASTER_DBG_ERROR("failed. Busmaster couldnt be stopped (-EAGAIN). (Name: %s, busmaster-Return: %d)", bm->name, result);
                        goto err;
                }
                break;
            case avm_busmaster_stop:
                switch(result) {
                    case 0:
                        retry = 0;
                        bm = bm->next;
                        break;
                    case -EBUSY:
                        retry++;

                        if(retry > 2) {
                            AVM_BMASTER_DBG_ERROR("failed. Busmaster couldnt be stopped (-EBUSY). (Name: %s)", bm->name);
                            goto err;
                        }
                        break;
                    default:
                        AVM_BMASTER_DBG_ERROR("failed. Busmaster couldnt be stopped (-EAGAIN). (Name: %s, busmaster-Return: %d)", bm->name, result);
                        goto err;
                }
                break;
            case avm_busmaster_run:
                switch(result) {
                    case 0:
                        retry = 0;
                        bm = bm->next;
                        break;
                    default:
                        AVM_BMASTER_DBG_ERROR("failed. Busmaster couldnt be stopped (-EAGAIN). (Name: %s, busmaster-Return: %d)", bm->name, result);
                        goto err;
                }
                break;
            default:
                AVM_BMASTER_DBG_ERROR("failed. Command unknown. (Cmd: %u)", cmd);
                goto err;
        }
    }

    return 0;
err:
    return -EFAULT;
}

//---------
int avm_run_busmaster(void) {
    return avm_send_cmd_to_all_busmaster(avm_busmaster_run);
}

//---------
int avm_prepare_busmaster_stop(void) {
    return avm_send_cmd_to_all_busmaster(avm_busmaster_prepare_stop);
}

//---------
int avm_stop_busmaster(void) {
    return avm_send_cmd_to_all_busmaster(avm_busmaster_stop);
}

