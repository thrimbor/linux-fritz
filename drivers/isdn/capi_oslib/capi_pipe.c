#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/wait.h>

#include "debug.h"
#include <linux/new_capi.h>
#include <linux/capi_oslib.h>
#include <linux/semaphore.h>


#include "capi_pipe.h"
#include "appl.h"
#include "ca.h"

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct workqueue_struct *pipe_workqueue;

/*------------------------------------------------------------------------------------------*\
 * mit RxBuffer == NULL wird die Länge der im Buffer befindelichen CapiMessage returned
\*------------------------------------------------------------------------------------------*/
int Capi_Receive_From_Pipe(struct capi_pipe *P, unsigned char *RxBuffer, unsigned int RxBufferLen, unsigned long *received, unsigned int Suspend) {
    unsigned int Len, CopyLength;

    if(P->delete_pending) {
        return CAPI_PIPE_DELETED;
    }

    if(P->with_lock) {
        down(&(P->Lock));
    }

Capi_Receive_From_Pipe_restart:
    /*--- if(RxBuffer) ---*/
        /*--- DEB_INFO("[Capi_Receive_From_Pipe] write=%u read=%u free=%u\n", P->WritePos, P->ReadPos, P->Free); ---*/

    if(P->WritePos == P->ReadPos) {
        if(Suspend == CAPI_SUSPEND) {
            atomic_inc(&(P->rx_waiting));
        }
        if(P->with_lock)
            up(&(P->Lock));
        if(Suspend == CAPI_SUSPEND) {
            down(&(P->rx_Wait));
            if(P->with_lock) {
                down(&(P->Lock));
            }
            atomic_dec(&(P->rx_waiting));
            if(P->delete_pending) {
                if(P->with_lock)
                    up(&(P->Lock));
                complete(&P->complete);
                return CAPI_PIPE_DELETED;
            }
            goto Capi_Receive_From_Pipe_restart;
        }
        return RxBuffer == NULL ? 0 : CAPI_PIPE_EMPTY;
    }

    Len = *(unsigned int *)&(P->Buffer[P->ReadPos]);
    *received = Len;
    if(RxBuffer == NULL) {
        if(P->with_lock)
            up(&(P->Lock));
        return Len;
    }

    if(Len > RxBufferLen) {
        if(P->with_lock)
            up(&(P->Lock));
        return CAPI_PIPE_BUFFER_TO_SMALL;
    }

    P->ReadPos += sizeof(unsigned int);
    P->Free    += sizeof(unsigned int);
    if(P->ReadPos >= P->BufferLen)
        P->ReadPos = 0;

    /*--------------------------------------------------------------------------------------*\
     * pruefen ob in zwei Abschnitten kopiert werden muss
    \*--------------------------------------------------------------------------------------*/
    if(Len > P->BufferLen - P->ReadPos) {
        unsigned int part_Len = P->BufferLen - P->ReadPos;
        memcpy(RxBuffer, &(P->Buffer[P->ReadPos]), part_Len);
        P->ReadPos = 0;
        P->Free  += part_Len;
        RxBuffer += part_Len;
        Len -= part_Len;
    }

    /*--------------------------------------------------------------------------------------*\
     * den rest kopieren
    \*--------------------------------------------------------------------------------------*/
    CopyLength = Len;
    Len += sizeof(unsigned int) - 1;    /*--- align auf sizeof(unsigned int) ---*/
    Len &= ~(sizeof(unsigned int) - 1);

    memcpy(RxBuffer, &(P->Buffer[P->ReadPos]), CopyLength);
    P->ReadPos += Len;
    P->Free    += Len;
    if(P->ReadPos >= P->BufferLen)
        P->ReadPos = 0;

    if(atomic_read(&(P->tx_waiting))) {
        up(&(P->tx_Wait));
    }
    if(P->with_lock)
        up(&(P->Lock));

    if(P->tx_wait_queue)
        wake_up_interruptible(P->tx_wait_queue);
        /*--- wake_up(P->tx_wait_queue); ---*/
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int Capi_Send_To_Pipe(struct capi_pipe *P, unsigned char *TxBuffer, unsigned int TxBufferLen, unsigned int Suspend) {

    if(P->delete_pending) {
        return CAPI_PIPE_DELETED;
    }

    if(P->with_lock) {
        down(&(P->Lock));
    }

Capi_Send_To_Pipe_restart:

    /*--- DEB_INFO("[Capi_Send_To_Pipe] write=%u read=%u free=%u\n", P->WritePos, P->ReadPos, P->Free); ---*/

    /*--------------------------------------------------------------------------------------*\
    \*--------------------------------------------------------------------------------------*/
    if(TxBufferLen + 3 + sizeof(unsigned int) >= P->Free) {
        if(Suspend == CAPI_SUSPEND) {
            atomic_inc(&(P->tx_waiting));
        }
        if(P->with_lock)
            up(&(P->Lock));
        if(Suspend == CAPI_SUSPEND) {
            down(&(P->tx_Wait));
            if(P->with_lock) {
                down(&(P->Lock));
            }
            atomic_dec(&(P->tx_waiting));
            if(P->delete_pending) {
                if(P->with_lock)
                    up(&(P->Lock));
                complete(&P->complete);
                return CAPI_PIPE_DELETED;
            }
            goto Capi_Send_To_Pipe_restart;
        }
        return CAPI_PIPE_FULL;
    }

    /*--------------------------------------------------------------------------------------*\
     * Laenge speichern
    \*--------------------------------------------------------------------------------------*/
    *(unsigned int *)&(P->Buffer[P->WritePos]) = TxBufferLen;
    P->WritePos += sizeof(unsigned int);
    P->Free     -= sizeof(unsigned int);
    if(P->WritePos >= P->BufferLen)
        P->WritePos = 0;

    TxBufferLen += sizeof(unsigned int) - 1;    /*--- align auf sizeof(unsigned int) ---*/
    TxBufferLen &= ~(sizeof(unsigned int) - 1);

    /*--------------------------------------------------------------------------------------*\
     * pruefen ob in zwei Abschnitten kopiert werden muss
    \*--------------------------------------------------------------------------------------*/
    if(TxBufferLen > P->BufferLen - P->WritePos) {
        unsigned int Len = P->BufferLen - P->WritePos;
        memcpy(&(P->Buffer[P->WritePos]), TxBuffer, Len);
        P->WritePos = 0;
        P->Free -= Len;
        TxBufferLen -= Len;
        TxBuffer += Len;
    }

    /*--------------------------------------------------------------------------------------*\
     * den rest kopieren
    \*--------------------------------------------------------------------------------------*/
    memcpy(&(P->Buffer[P->WritePos]), TxBuffer, TxBufferLen);
    P->WritePos += TxBufferLen;
    P->Free     -= TxBufferLen;
    if(P->WritePos >= P->BufferLen)
        P->WritePos = 0;

    if(atomic_read(&(P->rx_waiting))) {
        up(&(P->rx_Wait));
    }
    if(P->with_lock)
        up(&(P->Lock));

    if(P->rx_wait_queue)
        wake_up_interruptible(P->rx_wait_queue);
        /*--- wake_up(P->rx_wait_queue); ---*/

    if(P->scheduler_rx_work && pipe_workqueue)
        queue_work_on(PCMLINK_TASKLET_CONTROL_CPU, pipe_workqueue, P->scheduler_rx_work);

    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int Capi_Create_Pipe(struct capi_pipe *P, char *Name, unsigned char *Buffer, unsigned int BufferLen, int type __attribute__((unused)), unsigned int MaxMessageLen, int Lock) {
    int len;
    len = strlen(Name);
    P->Name = CA_MALLOC(len + 1);
    if(P->Name) {
        strcpy(P->Name, Name);
    }
    P->Buffer = Buffer;
    P->BufferLen = BufferLen;
    P->MaxMessageLen = MaxMessageLen;
    P->Free = BufferLen - sizeof(unsigned int);
    P->ReadPos = 0;
    P->WritePos = 0;
    P->with_lock = Lock;
    if(P->with_lock == CAPI_LOCK)
        sema_init(&(P->Lock), 1);
    sema_init(&(P->rx_Wait), 0);  /* blockieren beim ersten mal */
    sema_init(&(P->tx_Wait), 0);  /* blockieren beim ersten mal */
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int Capi_Delete_Pipe(struct capi_pipe *P) {
    P->delete_pending = 1;
    init_completion(&P->complete);
    while(atomic_read(&P->rx_waiting) || atomic_read(&P->tx_waiting)) {
        if(atomic_read(&P->rx_waiting))
            up(&P->rx_Wait);
        if(atomic_read(&P->tx_waiting))
            up(&P->tx_Wait);
        wait_for_completion(&P->complete);
    }
    if(P->Name) {
        CA_FREE(P->Name);
        P->Name = NULL;
    }
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int Capi_Pipe_Options(struct capi_pipe *P, wait_queue_head_t *rx_wait_queue, wait_queue_head_t *tx_wait_queue) {
    P->rx_wait_queue = rx_wait_queue;
    P->tx_wait_queue = tx_wait_queue;
    return 0;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
char *Capi_Pipe_Status(struct capi_pipe *P) {
    static char Buffer[80];
    if(P == NULL)
        return "no pipe";

    DEB_ERR("[Capi_Pipe_Status] Pipe=0x%p\n", P);

    sprintf(Buffer, "Pipe(%s) wr=%d rd=%d free=%d size=%d", 
            P->Name ? P->Name : "noname", P->WritePos, P->ReadPos, P->Free, P->BufferLen);
    return Buffer;
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
int Capi_Pipe_Init(void) {
    if(pipe_workqueue == NULL) {
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 19)
        pipe_workqueue = create_singlethread_workqueue("capioslib_pipe");
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
        pipe_workqueue = alloc_workqueue("capi_pipew", WQ_MEM_RECLAIM | WQ_HIGHPRI, 1);
#else/*--- #if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 19) ---*/
        pipe_workqueue = create_rt_workqueue("capi_pipew");
#endif
    }
    if(pipe_workqueue == NULL)
        return -EFAULT;
    return 0;
}
