/*------------------------------------------------------------------------------------------*\
 *   
 *   Copyright (C) 2006 AVM GmbH <fritzbox_info@avm.de>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
\*------------------------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------------------------*\
\*-----------------------------------------------------------------------------------------------*/
#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <asm/fcntl.h>
#include <asm/ioctl.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
#include <linux/cdev.h>
#include <asm/mach_avm.h>
#else
#endif

#include <linux/fs.h>
/*--- #include <asm/semaphore.h> ---*/
#include <asm/errno.h>
/*--- #include <linux/wait.h> ---*/
/*--- #include <linux/vmalloc.h> ---*/
/*--- #include <linux/poll.h> ---*/

#include <linux/avm_event.h>
#include <linux/avm_led.h>
#include "avm_sammel.h"
#include "avm_led.h"

#define SKIP_SPACES(p) while((p) && *(p) && ((*(p) == ' ') || (*(p) == '\t'))) (p)++;
#define SKIP_WORD(p) while((p) && *(p) && (*(p) != ' ') && (*(p) != '\t') && (*(p) != '=') && (*(p) != ',')) (p)++;
#define SKIP_KOMMA(p)  while((p) && *(p) && (*(p) != ',')) (p)++; if(*(p) == ',') (p)++;

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_led_monitor_level = 0;

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_led_write_check_special_char(char **p, char check_char) {
    SKIP_SPACES(*p); /* ggf. spaces */
    if(**p != check_char) 
        return 1;
    (*p)++;
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_led_write_check_special_string(char **p, char *check_string) {
    int len = strlen(check_string);
    SKIP_SPACES(*p); /* ggf. spaces */
    if(strncmp(*p, check_string, len))
        return 1;
    (*p) += len;
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_led_write_get_next_number(char **p, int *pNumber) {
    SKIP_SPACES(*p);
    if(**p == '\0')
        return -1;
    *pNumber = simple_strtol(*p, NULL, 0);
    SKIP_WORD(*p);  /* zahl ueberspringen */
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_led_write_get_next_wort(char **p, char *name, int name_len) {
    SKIP_SPACES(*p);
    if(**p == '\0')
        return -1;
    while(**p && 
            (**p != ' ') && 
            (**p != '\t') && 
            (**p != ',') && 
            (**p != '=') && 
            (**p != '\r') && 
            (**p != '\n') && 
            (name_len > 1))
        *name++ = *(*p)++, name_len--;
    *name = '\0';
    SKIP_WORD(*p);  /* wort ueberspringen */
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int avm_led_write_extract_name_and_instance(char **p, char *name, int name_len, int *pInstance) {
    if(avm_led_write_get_next_wort(p, name, name_len)) {
        DEB_ERR("[avm_led]: avm_led_write_extract_name_and_instance illegal string\n");
        return -1;
    }
    if(avm_led_write_check_special_char(p, ',')) {
        DEB_ERR("[avm_led]: avm_led_write_extract_name_and_instance \",\" missing\n");
        return -1;
    }
    if(avm_led_write_get_next_number(p, pInstance)) {
        DEB_ERR("[avm_led]: avm_led_write_extract_name_and_instance: instance missing\n");
        return -1;
    }
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
ssize_t avm_led_write(struct file *filp, const char *write_buffer, size_t write_length, loff_t *write_pos) {
    char Buffer[256], *p;
    unsigned int org_write_length;

    if(write_pos != NULL) {
        DEB_INFO("[%s]: write_length = %u *write_pos = 0x%LX\n", "avm_led_write", write_length, *write_pos);
    }

    org_write_length = write_length;

    if(write_length >= sizeof(Buffer)) {
        write_length = sizeof(Buffer) - 1;
        DEB_NOTE("[avm_led] long line reduce to %u bytes\n", write_length);
    }
    
    if(filp == NULL) {
        memcpy(Buffer, write_buffer, write_length);
    } else {
        if(copy_from_user(Buffer, write_buffer, write_length)) {
            DEB_ERR("[%s]: avm_led_write: copy_from_user failed\n", "avm_led_write");
            return -EFAULT;
        }
    }

    if(write_pos != NULL) {
        *write_pos += write_length;
    }

    /*--------------------------------------------------------------------------------------*\
    \*--------------------------------------------------------------------------------------*/
    Buffer[write_length] = '\0';
    p = strchr(Buffer, 0x0A);
    if(p) {
        *p = '\0';
        write_length = strlen(Buffer) + 1;
        DEB_NOTE("[avm_led] multi line reduce to %u bytes\n", write_length);
    }
    p = Buffer;

    /*--------------------------------------------------------------------------------------*\
     * cmd extraieren
    \*--------------------------------------------------------------------------------------*/
    SKIP_SPACES(p);
    DEB_NOTE("[avm_led]: process input \"%s\"\n", p);
    
    /*--------------------------------------------------------------------------------------*\
    \*--------------------------------------------------------------------------------------*/
    if(!strncmp("MONITOR ", p, 8)) {
        p += 8;
        if(avm_led_write_check_special_char(&p, '=')) {
monitor_format_error:
            DEB_ERR("[avm_led] format error: \"MONITOR = <level>\"%s\n", avm_led_monitor_user());
            return -EFAULT;
        }
        if(avm_led_write_get_next_number(&p, &avm_led_monitor_level)) {
            DEB_ERR("[avm_led] get monitor level failed\n");
            goto monitor_format_error;
        }
        avm_led_show_virt_led(-1);
    /*--------------------------------------------------------------------------------------*\
    \*--------------------------------------------------------------------------------------*/
    } else if(!strncmp("DEF ", p, 4)) {
        /*----------------------------------------------------------------------------------*\
         * DEF Name,Instanz = low_level_driver_index, gpio_number, physical_pin_number, physical_pin_name
        \*----------------------------------------------------------------------------------*/
        char Name[64];
        int instance, handle;
        int low_level_driver_index, gpio_number, pin_number;

        p += 4;
        if(avm_led_write_extract_name_and_instance(&p, Name, sizeof(Name), &instance)) {
def_format_error:
            DEB_ERR("[avm_led] format error: \"DEF Name,Instanz = low_level_driver_index, gpio_number, physical_pin_number, physical_pin_name\"%s\n", avm_led_monitor_user());
            return -EFAULT;
        }
        handle = avm_led_get_virt_led_handle(Name, instance);
        if((handle > 0) || (handle < -255)) {
            DEB_ERR("[avm_led] \"%s\",%u ! already defined\n", Name, instance);
            goto def_format_error;
        }
        if(avm_led_write_check_special_char(&p, '=')) {
            DEB_ERR("[avm_led] syntax error \"=\" missing\n");
            goto def_format_error;
        }
        if(avm_led_write_get_next_number(&p, &low_level_driver_index)) {
            DEB_ERR("[avm_led] get low_level_driver_index failed\n");
            goto def_format_error;
        }
        if(avm_led_write_check_special_char(&p, ',')) {
            DEB_ERR("[avm_led] komman after low_level_driver_index missing\n");
            goto def_format_error;
        }
        if(avm_led_write_get_next_number(&p, &gpio_number)) {
            DEB_ERR("[avm_led] get gpio_number failed\n");
            goto def_format_error;
        }
        if(avm_led_write_check_special_char(&p, ',')) {
            DEB_ERR("[avm_led] komman after gpio_number missing\n");
            goto def_format_error;
        }
        if(avm_led_write_get_next_number(&p, &pin_number)) {
            DEB_ERR("[avm_led] get pin_number failed\n");
            goto def_format_error;
        }
        if(avm_led_write_check_special_char(&p, ',')) {
            DEB_ERR("[avm_led] komman after pin_number missing\n");
            goto def_format_error;
        }
        DEB_NOTE("[avm_led] ADD %s,%u \n"
                 "\tlow_level_driver_index = %u\n"
                 "\tgpio number = %u\n"
                 "\tpin number = %u\n"
                 "\tpin name = %s\n",
                 Name, instance, low_level_driver_index, 
                 gpio_number, pin_number, p);

        handle = avm_led_register_led(Name, instance, low_level_driver_index, gpio_number, pin_number, p);
        if((handle < 0) && (handle > -255)) {
            DEB_ERR("[avm_led] format error: \"DEF Name,Instanz = low_level_driver_index, gpio_number, physical_pin_number, physical_pin_name\"%s\n", avm_led_monitor_user());
            goto def_format_error;
        }

    /*--------------------------------------------------------------------------------------*\
    \*--------------------------------------------------------------------------------------*/
    } else if(!strncmp("UNDEF ", p, 6)) {
        /*----------------------------------------------------------------------------------*\
         * UNDEF Name,Instanz
        \*----------------------------------------------------------------------------------*/
        char Name[64];
        int instance, handle;
        p += 6;
        if(avm_led_write_extract_name_and_instance(&p, Name, sizeof(Name), &instance)) {
undef_format_error:
            DEB_ERR("[avm_led] format error: \"UNDEF Name,Instanz\"%s\n", avm_led_monitor_user());
            return -EFAULT;
        }
        handle = avm_led_get_virt_led_handle(Name, instance);
        if((handle < 0) && (handle > -255)) {
            DEB_ERR("[avm_led] virt led not registered\n");
            goto undef_format_error;
        }
        avm_led_release_led(handle);

    /*--------------------------------------------------------------------------------------*\
    \*--------------------------------------------------------------------------------------*/
    } else if(!strncmp("STATES ", p, 7)) {
        /*----------------------------------------------------------------------------------*\
         * UNDEF Name,Instanz
        \*----------------------------------------------------------------------------------*/
        char Name[64];
        int instance, handle;
        int from_state, to_state;
        p += 7;
        if(avm_led_write_extract_name_and_instance(&p, Name, sizeof(Name), &instance)) {
states_format_error:
            DEB_ERR("[avm_led] format error: \"STATES Name,Instanz = A->B,C->D,...\"%s\n", avm_led_monitor_user());
            return -EFAULT;
        }
        handle = avm_led_get_virt_led_handle(Name, instance);
        if((handle < 0) && (handle > -255)) {
            DEB_ERR("[avm_led] virt led not registered\n");
            goto states_format_error;
        }
        if(avm_led_write_check_special_char(&p, '=')) {
            DEB_ERR("[avm_led] syntax error \"=\" missing\n");
            goto states_format_error;
        }
        DEB_NOTE("[avm_led] STATES %s,%u = %s\n", 
                avm_led_virt_led_name(handle), avm_led_virt_led_instance(handle), p);

        while(p) {
            if(avm_led_write_get_next_number(&p, &from_state)) {
                DEB_ERR("[avm_led] get low_level_driver_index failed\n");
                goto states_format_error;
            }
            if(avm_led_write_check_special_string(&p, "->")) {
                DEB_ERR("[avm_led] syntax error \"->\" missing\n");
                goto states_format_error;
            }
            if(avm_led_write_get_next_number(&p, &to_state)) {
                DEB_ERR("[avm_led] get low_level_driver_index failed\n");
                goto states_format_error;
            }

            avm_led_map_states(handle, from_state, to_state);

            if(avm_led_write_check_special_char(&p, ',')) {
                break;
            }
        }


    /*--------------------------------------------------------------------------------------*\
    \*--------------------------------------------------------------------------------------*/
    } else if(!strncmp("SET ", p, 4) || !strncmp("EVENT ", p, 6)) {
        /*----------------------------------------------------------------------------------*\
         * SET Name,Instanz = State
        \*----------------------------------------------------------------------------------*/
        char Name[64];
        int instance, handle, state;
        int with_event = strncmp("SET ", p, 4);
        if(with_event)
            p += 6;
        else
            p += 4;

        if(avm_led_write_extract_name_and_instance(&p, Name, sizeof(Name), &instance)) {
set_format_error:
            DEB_ERR("[avm_led] format error: \"SET Name,Instanz = state\"%s\n", avm_led_monitor_user());
            return -EFAULT;
        }
        handle = avm_led_get_virt_led_handle(Name, instance);
        if((handle < 0) && (handle > -255)) {
            DEB_ERR("[avm_led] virt led not registered\n");
            goto set_format_error;
        }
        if(avm_led_write_check_special_char(&p, '=')) {
            DEB_ERR("[avm_led] syntax error \"=\" missing\n");
            goto def_format_error;
        }
        if(avm_led_write_get_next_number(&p, &state)) {
            DEB_ERR("[avm_led] format error: state missing\n");
            goto set_format_error;
        }
        DEB_NOTE("[avm_led] %s %s,%u = %u\n", 
                with_event ? "EVENT" : "SET",
                avm_led_virt_led_name(handle), avm_led_virt_led_instance(handle), state);

        avm_led_virt_led_action(handle, state);
        if(with_event) {
            unsigned int event_len;
            /* write_length beschreibt die Laenge des LED-Kontrollstrings. Nach dem Ende wird *\
             * auf die 4-Byte-Grenze ein Alignment durchgefuehrt, um die folgenden binaeren   *
            \* Eventdaten verarbeiten zu koennen                                              */
            write_length +=  3;
            write_length &= ~3;
            write_buffer += write_length;
            if(org_write_length >= write_length + sizeof(unsigned int)) {
                struct _avm_virt_led *V = (struct _avm_virt_led *) handle;

                if(filp == NULL) {
                    memcpy(&event_len, write_buffer, sizeof(unsigned int));
                } else {
                    if(copy_from_user(&event_len, write_buffer, sizeof(unsigned int))) {
                        DEB_ERR("[%s]: avm_led_write: copy_from_user failed (event_len)\n", "avm_led_write");
                        return -EFAULT;
                    }
                }
                write_buffer += sizeof(unsigned int);
                if(org_write_length >= write_length + sizeof(unsigned int) + event_len) {
                    if(V->event_ptr != NULL)
                        kfree(V->event_ptr);
                    V->event_ptr = NULL;
                    V->event_length = 0;

                    V->event_ptr = (struct _avm_event_led_status *) kmalloc(sizeof(struct _avm_event_led_status) + event_len, GFP_ATOMIC);
                    if(V->event_ptr == NULL) {
                        DEB_ERR("[avm_led]: no mem (alloc %u bytes)\n", sizeof(struct _avm_event_led_status) + event_len);
                        return -ENOMEM;
                    }

                    V->event_ptr->header.id = avm_event_id_led_status;
#if defined(CONFIG_MIPS_AVALANCHE_LED) || defined(CONFIG_MIPS_AVALANCHE_LED_MODULE)
                    V->event_ptr->led = avalanche_led_get_old_id_from_handle(handle);
                    if(V->event_ptr->led == 0) {
                        V->event_ptr->led = avalanche_led_get_new_id_from_handle(handle);
                    }
#else/*--- #if defined(CONFIG_MIPS_AVALANCHE_LED) || defined(CONFIG_MIPS_AVALANCHE_LED_MODULE) ---*/
                    V->event_ptr->led = avalanche_led_get_new_id_from_handle(handle);
#endif/*--- #else ---*//*--- #if defined(CONFIG_MIPS_AVALANCHE_LED) || defined(CONFIG_MIPS_AVALANCHE_LED_MODULE) ---*/
                    V->event_ptr->state = state;
                    V->event_ptr->param_len = event_len;
                    if(filp == NULL) {
                        memcpy(V->event_ptr->params, write_buffer, event_len);
                    } else {
                        if(copy_from_user(V->event_ptr->params, write_buffer, event_len)) {
                            DEB_ERR("[%s]: avm_led_write: copy_from_user failed (event)\n", "avm_led_write");
                            return -EFAULT;
                        }
                    }
                    V->event_length = sizeof(struct _avm_event_led_status) + event_len;

                    avm_led_status_event(V);
                    write_length = org_write_length;
                }
            } else {
                DEB_ERR("[avm_led] EVENT Error no event length\n");
            }
        }

    /*--------------------------------------------------------------------------------------*\
    \*--------------------------------------------------------------------------------------*/
    } else if(!strncmp("MAP ", p, 4) || !strncmp("DOUBLE ", p, 7) ) {
        /*----------------------------------------------------------------------------------*\
         * MAP VonName,VonInstanz TO AufName,AufInstanz
        \*----------------------------------------------------------------------------------*/
        char from_Name[64];
        char to_Name[64];
        int from_instance, from_handle;
        int to_instance, to_handle;
        int both = strncmp("MAP ", p, 4);
        if(both)
            p += 7;
        else
            p += 4;

        if(avm_led_write_extract_name_and_instance(&p, from_Name, sizeof(from_Name), &from_instance)) {
map_format_error:
            DEB_ERR("[avm_led] format error: \"MAP Name,Instanz TO Name,Instanz\"%s\n", avm_led_monitor_user());
            return -EFAULT;
        }
        from_handle = avm_led_get_virt_led_handle(from_Name, from_instance);
        if((from_handle < 0) && (from_handle > -255)) {
            DEB_ERR("[avm_led] from virt led ('%s', %u) not registered\n", from_Name, from_instance);
            goto map_format_error;
        }

        p = strstr(p, "TO");
        if(p == NULL) {
            DEB_ERR("[avm_led] keyword TO missing\n");
            goto map_format_error;
        }
        p += 2;

        if(avm_led_write_extract_name_and_instance(&p, to_Name, sizeof(to_Name), &to_instance)) {
            DEB_ERR("[avm_led] to name,instance not found\n");
            goto map_format_error;
        }
        to_handle = avm_led_get_virt_led_handle(to_Name, to_instance);
        if((to_handle < 0) && (to_handle > -255)) {
            DEB_ERR("[avm_led] to virt led not registered\n");
            goto map_format_error;
        }
        DEB_NOTE("[avm_led] %s %s,%u TO %s,%u\n", 
                both ? "DOUBLE" : "MAP",
                avm_led_virt_led_name(from_handle), avm_led_virt_led_instance(from_handle),
                avm_led_virt_led_name(to_handle), avm_led_virt_led_instance(to_handle));

        if(avm_led_map_led(from_handle, to_handle, both)) {
            DEB_ERR("[avm_led] map failed\n");
            goto map_format_error;
        }

    /*--------------------------------------------------------------------------------------*\
    \*--------------------------------------------------------------------------------------*/
    } else if(!strncmp("UNMAP ", p, 6)) {
        /*----------------------------------------------------------------------------------*\
         * UNMAP Name,Instanz
        \*----------------------------------------------------------------------------------*/
        char Name[64];
        int instance, handle;
        p += 6;
        if(avm_led_write_extract_name_and_instance(&p, Name, sizeof(Name), &instance)) {
unmap_format_error:
            DEB_ERR("[avm_led] format error: \"UNMAP Name,Instanz\"%s\n", avm_led_monitor_user());
            return -EFAULT;
        }
        handle = avm_led_get_virt_led_handle(Name, instance);
        if((handle < 0) && (handle > -255)) {
            DEB_ERR("[avm_led] virt led not registered\n");
            goto unmap_format_error;
        }

        DEB_NOTE("[avm_led] UNMAP %s,%u\n", 
                avm_led_virt_led_name(handle), avm_led_virt_led_instance(handle));

        if(avm_led_map_led(handle, 0, 0)) {
            DEB_ERR("[avm_led] map virt led failed \n");
            goto unmap_format_error;
        }

    /*--------------------------------------------------------------------------------------*\
    \*--------------------------------------------------------------------------------------*/
    } else if(
            !strncmp("MASTER_ENABLE ", p, 7 + 7) || 
            !strncmp("MASTER_DISABLE ", p, 8 + 7) ||
            !strncmp("ENABLE ", p, 7) || 
            !strncmp("DISABLE ", p, 8)
            ) {
        /*----------------------------------------------------------------------------------*\
         * UNMAP Name,Instanz
        \*----------------------------------------------------------------------------------*/
        char Name[64];
        int instance, handle;
        int master = !strncmp("MASTER_", p, 7) ? 1 : 0;
        int enable;
        p += master ? 7 : 0;
        enable = !strncmp("ENABLE ", p, 7) ? 1 : 0;
        p += enable ? 7 : 8;

        if(avm_led_write_extract_name_and_instance(&p, Name, sizeof(Name), &instance)) {
ctrl_format_error:
            DEB_ERR("[avm_led] format error: \"%s%s Name,Instanz\"%s\n", 
                    master ? "MASTER_" : "",
                    enable ? "ENABLE" : "DISABLE",
                    avm_led_monitor_user());
            return -EFAULT;
        }
        handle = avm_led_get_virt_led_handle(Name, instance);
        if((handle < 0) && (handle > -255)) {
            DEB_ERR("[avm_led] virt led not registered\n");
            goto ctrl_format_error;
        }

        DEB_NOTE("[avm_led] %s%s %s,%u\n", 
                master ? "MASTER_" : "",
                enable ? "ENABLE" : "DISABLE",
                avm_led_virt_led_name(handle), avm_led_virt_led_instance(handle));

        avm_led_virt_led_ctrl(handle, master, enable);

    /*--------------------------------------------------------------------------------------*\
    \*--------------------------------------------------------------------------------------*/
#if defined(AVM_LED_DEBUG)
    } else if(!strncmp("INFO ", p, 5)) {
        /*----------------------------------------------------------------------------------*\
         * INFO Name,Instanz
        \*----------------------------------------------------------------------------------*/
        char Name[64];
        int instance, handle;
        p += 5;
        if((p[0] == 'A') && (p[1] == 'L') & (p[2] == 'L')) {
            avm_led_show_virt_led(-1);
        } else {
            if(avm_led_write_extract_name_and_instance(&p, Name, sizeof(Name), &instance)) {
info_format_error:
                DEB_ERR("[avm_led] format error: \"INFO Name,Instanz\"%s\n", avm_led_monitor_user());
                return -EFAULT;
            }
            handle = avm_led_get_virt_led_handle(Name, instance);
            if((handle < 0) && (handle > -255)) {
                DEB_ERR("[avm_led] virt led not registered\n");
                goto info_format_error;
            }
            avm_led_show_virt_led(handle);
        }
#endif /*--- #if defined(AVM_LED_DEBUG) ---*/
#if defined(CONFIG_MIPS_AVALANCHE_LED) || defined(CONFIG_MIPS_AVALANCHE_LED_MODULE)
    /*--------------------------------------------------------------------------------------*\
     * old compatible led layer
    \*--------------------------------------------------------------------------------------*/
    } else {
        int number, state, handle;
        char Name[64];
        DEB_NOTE("[avm_led] emulate old shell format. input string = \"%s\"\n", p);

        if(avm_led_write_extract_name_and_instance(&p, Name, sizeof(Name), &state)) {
old_format_error:
            DEB_ERR("[avm_led] old format error: \"old_id,state\"%s\n", avm_led_monitor_user());
            return -EFAULT;
        }
        number = simple_strtol(Name, NULL, 0);
        handle = avalanche_led_get_handle_from_old_id(number);
        if(handle == 0) {
            DEB_ERR("[avm_led] illegal old handle\n");
            goto old_format_error;
        }

        DEB_NOTE("[avm_led] (emulate) SET %s,%u = %u\n", 
                avm_led_virt_led_name(handle), avm_led_virt_led_instance(handle), state);

        avalanche_led_action((void *) handle, state - 1);
#endif /*--- #if defined(CONFIG_MIPS_AVALANCHE_LED) || defined(CONFIG_MIPS_AVALANCHE_LED_MODULE) ---*/
    }
    /*--------------------------------------------------------------------------------------*\
    \*--------------------------------------------------------------------------------------*/

    return write_length;
}

