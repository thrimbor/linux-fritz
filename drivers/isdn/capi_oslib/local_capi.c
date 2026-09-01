#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/semaphore.h>

#include "debug.h"
#include <linux/new_capi.h>
#include <linux/capi_oslib.h>

#include "consts.h"
#include "appl.h"
#include "ca.h"
#include "host.h"
#include "capi_pipe.h"
#include "capi_events.h"
#include "zugriff.h"
#include "local_capi.h"

unsigned char *CAPI_Version;  /*--- CAPI_Version[MAX_CONTROLLERS][256] ---*/


/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
#define SYSTEM_ERROR(str, ret) if(Status != 0) { printk(KERN_ERR str); return ret; }
#define SYSTEM_ERROR_VOID(str) if(Status != 0) { printk(KERN_ERR str); return; }

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
struct _local_capi_appl *LocalCapiAppl[SOURCE_ANZAHL];
struct semaphore         LocalCapiRegisterSema;
struct semaphore         LocalCapiReleaseSema;
unsigned int             LocalCapiMaxAppls;

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
static inline unsigned char *LOCAL_CAPI_GET_DATA_B3_REQ_DATA(unsigned char *Msg);

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
void LOCAL_CAPI_INIT(enum _capi_source capi_source) {
    unsigned int Count;

    if(capi_source == SOURCE_UNKNOWN) {
        sema_init(&LocalCapiRegisterSema, 1);
        sema_init(&LocalCapiReleaseSema, 0);
        LocalCapiMaxAppls = LOCAL_CAPI_APPLIKATIONS;
        DEB_INFO("LOCAL_CAPI_INIT: first done\n");
    }

    LocalCapiAppl[capi_source] = (struct _local_capi_appl *)CA_MALLOC(sizeof(struct _local_capi_appl) * LocalCapiMaxAppls);
    
    for(Count = 0 ; Count < LocalCapiMaxAppls; Count++)
        LocalCapiAppl[capi_source][Count].Kennung = LOCAL_APPL_KENNUNG;

    DEB_INFO("LOCAL_CAPI_INIT: done\n");
    return;
}

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
void LOCAL_CAPI_MESSAGE(enum _capi_source capi_source, unsigned int ApplId, unsigned char *Msg, unsigned int MsgLen) {
    struct _local_capi_appl *LA;
    int                      Status;
    unsigned int             Suspend;

#if defined(DEBUG_LOCAL_CAPI_DATA)
    {
        unsigned int Count;
        DEB_INFO("CAPI_MESSAGE(ApplId=%d, Msg=%x, MsgLen=%u): ",  ApplId, Msg, MsgLen);
        for(Count = 0 ; Count < MsgLen ; Count++) {
            printk("%02x ", Msg[Count]);
        }
        printk("\n");
    }
#endif /*--- #if defined(DEBUG_LOCAL_CAPI_DATA) ---*/

    LA = &LocalCapiAppl[capi_source][ApplId - 1];
    if(LA->InUse != _entry_in_use_) {
        DEB_WARN("CAPI_MESSAGE(ApplId=%d, *Msg, MsgLen=%u) not registered\n",  ApplId, MsgLen);
        return;
    }

#if defined(USE_WORKQUEUES) || defined(USE_THREAD)
    Suspend = CAPI_SUSPEND;
#endif
#if defined(USE_TASKLETS)
    Suspend = CAPI_NO_SUSPEND;
#endif
    Status = Capi_Send_To_Pipe(LA->Pipe, Msg, MsgLen, Suspend); /*--- maximal 100 ms warten dann message lost ---*/
    if(Status == CAPI_PIPE_TIMEOUT || Status == CAPI_PIPE_FULL) {
        DEB_WARN("to-appl-message-pipe overflow %s\n", Capi_Pipe_Status(LA->Pipe));
        LA->MessageLost++;
    } else if(Status != 0) {
        DEB_ERR("LOCAL_CAPI_MESSAGE(%s, %u) Send_To_Pipe(LA->Pipe, Msg, %u, Suspend) failed, Status = %d\n", __FILE__, __LINE__, MsgLen, Status);
        return;
    }
    if(LA->SignalEvent) {
        Status = Capi_Set_Events(LA->SignalEvent, LA->EventBit, CAPI_EVENT_OR);
        DEB_WARN("CAPI_MESSAGE(ApplId=%d, *Msg, MsgLen=%u) signal event failed\n",  ApplId, MsgLen);
    }
    return;
}


/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
unsigned int LOCAL_CAPI_REGISTER(enum _capi_source capi_source, unsigned int MessageBufferSize, unsigned int MaxNCCIs, unsigned int WindowSize, unsigned int B3BlockSize, unsigned int *ApplId) {
    unsigned int             Count;
    unsigned int             Len, MaxLen;
    struct _local_capi_appl *LA = NULL;
    int                      Status;
    char                     Name[40];
    
    if(LocalCapiAppl[capi_source] == NULL) {
        DEB_ERR("CAPI for source=%u not installed\n", capi_source);
        return ERR_IllegalController;
    }

    DEB_INFO("LOCAL_CAPI_REGISTER(%s, MessageBufferSize=%u, MaxNCCIs=%u, WindowSize=%u, B3BlockSize=%u, ApplId=...)\n",
        capi_source_name[capi_source], MessageBufferSize, MaxNCCIs, WindowSize, B3BlockSize);

    down(&LocalCapiRegisterSema);
    
    /*--- find free applid ---*/
    for(Count = 0 ; Count < LocalCapiMaxAppls ; Count++) {
        if(LocalCapiAppl[capi_source][Count].InUse == _entry_not_used_) {
            LA = &LocalCapiAppl[capi_source][Count];
            break;
        }
    }
    if(Count == LocalCapiMaxAppls || LA == NULL) {
        DEB_WARN("LOCAL_CAPI_REGISTER: ERR_ToManyApplications\n");
        up(&LocalCapiRegisterSema);
        return ERR_ToManyApplications;
    }

    *ApplId            = Count + 1;
    memset(LA, 0 , sizeof(struct _local_capi_appl));
    LA->Kennung        = LOCAL_APPL_KENNUNG;
    
    Len                = MessageBufferSize + 256;
    MaxLen             = min(MessageBufferSize, MAX_CAPI_MESSAGE_SIZE);
    LA->MaxMessageSize = MaxLen;
    LA->MessageBuffer  = CA_MALLOC(Len);
    LA->TmpBufferSize  = 0;
    LA->TmpBuffer      = NULL;
    LA->SignalEvent    = NULL;

    sprintf(Name, "%s-%s-%u", capi_source_name[capi_source], current->comm, Count + 1);

    LA->PipePointer    = CA_MALLOC(Len);
    LA->Pipe           = CA_MALLOC(sizeof(struct capi_pipe) * 1);
    Status = Capi_Create_Pipe(LA->Pipe, Name, LA->PipePointer, Len, CAPI_VARIABLE_SIZE, MaxLen, CAPI_LOCK); 
    if(Status != 0) {
        printk(KERN_ERR "create applid receive pipe failed");
        up(&LocalCapiRegisterSema);
        return ERR_ResourceError;
    }

    up(&LocalCapiRegisterSema);

    HOST_REGISTER(capi_source, *ApplId, 0, MaxNCCIs, WindowSize, B3BlockSize);
    /*--- capi_oslib_dump_open_data_list("LOCAL_CAPI_REGISTER"); ---*/

    LA->InUse = _entry_in_use_;  /* jetzt erst */
    DEB_INFO("[LOCAL_CAPI_REGISTER] success\n");
    return ERR_NoError;
}

/*------------------------------------------------------------------------------------------*\
 * Der Buffer mitt B3BlockSize * 8 * MaxNCCIs gross sein
\*------------------------------------------------------------------------------------------*/
int LOCAL_CAPI_REGISTER_B3_BUFFER(enum _capi_source capi_source, unsigned int ApplId, struct _adr_b3_ind_data *b3Buffers, unsigned int BufferAnzahl, void (*release_buffers)(void *), void *context) {
    struct _local_capi_appl *LA;

    DEB_INFO("LOCAL_CAPI_REGISTER_B3_BUFFER(%s, ApplId=%u)\n", capi_source_name[capi_source], ApplId);

    if(LocalCapiAppl[capi_source] == NULL) {
        DEB_ERR("[LOCAL_CAPI_REGISTER_B3_BUFFER] CAPI for source=%u not installed\n", capi_source);
        return ERR_IllegalController;
    }

    if(ApplId > LocalCapiMaxAppls) {
        DEB_ERR("[LOCAL_CAPI_REGISTER_B3_BUFFER] illegal ApplId %d\n", ApplId);
        return ERR_IllegalApplId;
    }

    LA = &LocalCapiAppl[capi_source][ApplId - 1];
    if(LA->InUse != _entry_in_use_) {
        DEB_WARN("[LOCAL_CAPI_REGISTER_B3_BUFFER] ApplId %d not registered\n", ApplId);
        return ERR_IllegalApplId;
    }

    return HOST_REGISTER_B3_BUFFER(capi_source, ApplId, b3Buffers, BufferAnzahl, release_buffers, context);
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
struct capi_pipe *LOCAL_CAPI_GET_MESSAGE_WAIT_QUEUE(enum _capi_source capi_source, unsigned int ApplId, wait_queue_head_t *rx_wait_queue, wait_queue_head_t *tx_wait_queue) {
    struct _local_capi_appl *LA;
    DEB_INFO("LOCAL_CAPI_GET_MESSAGE_WAIT_QUEUE(%s, ApplId=%u)\n", capi_source_name[capi_source], ApplId);

    if(LocalCapiAppl[capi_source] == NULL) {
        DEB_ERR("[LOCAL_CAPI_GET_MESSAGE_WAIT_QUEUE] CAPI for source=%u not installed\n", capi_source);
        return NULL;
    }

    if(ApplId > LocalCapiMaxAppls) {
        DEB_ERR("[LOCAL_CAPI_GET_MESSAGE_WAIT_QUEUE] illegal ApplId %d\n", ApplId);
        return NULL;
    }

    LA = &LocalCapiAppl[capi_source][ApplId - 1];
    if(LA->InUse != _entry_in_use_) {
        DEB_WARN("[LOCAL_CAPI_GET_MESSAGE_WAIT_QUEUE] ApplId %d not registered\n", ApplId);
        return NULL;
    }
    Capi_Pipe_Options(LA->Pipe, rx_wait_queue, tx_wait_queue);
    return LA->Pipe;
}

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
unsigned int LOCAL_CAPI_RELEASE(enum _capi_source capi_source, unsigned int ApplId) {
    unsigned char            RELEASE_REQ[8];
    struct _local_capi_appl *LA;
    unsigned int ret;

    DEB_INFO("LOCAL_CAPI_RELEASE(%s, ApplId=%u)\n", capi_source_name[capi_source], ApplId);
    /*--- capi_oslib_dump_open_data_list("LOCAL_CAPI_RELEASE"); ---*/

    if(LocalCapiAppl[capi_source] == NULL) {
        DEB_ERR("[LOCAL_CAPI_RELEASE] CAPI for source=%u not installed\n", capi_source);
        return ERR_IllegalController;
    }

    if(ApplId > LocalCapiMaxAppls) {
        DEB_ERR("[LOCAL_CAPI_RELEASE] illegal ApplId %d\n", ApplId);
        return ERR_IllegalApplId;
    }

    LA = &LocalCapiAppl[capi_source][ApplId - 1];
    if(LA->InUse != _entry_in_use_) {
        DEB_WARN("[LOCAL_CAPI_RELEASE] ApplId %d not registered\n", ApplId);
        return ERR_IllegalApplId;
    }
    LA->InUse = _entry_release_pending;

    SET_WORD( (RELEASE_REQ+0), sizeof(RELEASE_REQ));      /*----- Len --###*/
    SET_WORD( (RELEASE_REQ+2), (unsigned short)ApplId);
    *(unsigned char  *)(RELEASE_REQ+4) = 0xfe;                     /*----- command --###*/
    *(unsigned char  *)(RELEASE_REQ+5) = 0x80;                     /*----- subcommand --###*/

    ret = (unsigned int)HOST_MESSAGE (capi_source, RELEASE_REQ, NULL);
    down(&LocalCapiReleaseSema);

    return ret;
}

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
unsigned int LOCAL_CAPI_RELEASE_CONF(enum _capi_source capi_source, unsigned int ApplId) {
    struct _local_capi_appl *LA;
    int                      Status;

    DEB_INFO("LOCAL_CAPI_RELEASE_CONF(%s, ApplId=%u)\n", capi_source_name[capi_source], ApplId);

    down(&LocalCapiRegisterSema);

    LA = &LocalCapiAppl[capi_source][ApplId - 1];

    if(LA->InUse == _entry_not_used_) {
        up(&LocalCapiRegisterSema);
        return ERR_IllegalApplId;
    }

    Status = Capi_Delete_Pipe(LA->Pipe);

    if(Status != 0) {
        printk(KERN_ERR "delete LA->Pipe failed");
    }
    CA_FREE(LA->PipePointer);
    CA_FREE(LA->Pipe);
    CA_FREE(LA->MessageBuffer);

    if(LA->TmpBufferSize && LA->TmpBuffer) {
        CA_FREE(LA->TmpBuffer);
        LA->TmpBuffer = NULL;
    }

    memset(LA, 0 , sizeof(struct _local_capi_appl)); /*--- LA->InUse = _entry_not_used_; ---*/

    LA->Kennung = LOCAL_APPL_KENNUNG;

    up(&LocalCapiRegisterSema);
    up(&LocalCapiReleaseSema);

    return 0x0000;
}

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
#if !defined(NO_BCHANNEL)
static inline unsigned char *LOCAL_CAPI_GET_DATA_B3_REQ_DATA(unsigned char *Msg) {
    unsigned int Buffer;
    /*--- intel byte order ---*/
    Buffer  = Msg[12];
    Buffer |= Msg[13] << 8;
    Buffer |= Msg[14] << 16;
    Buffer |= Msg[15] << 24;
    return (unsigned char *)Buffer;
}
#endif /*--- #if !defined(NO_BCHANNEL) ---*/

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
unsigned char *LOCAL_CAPI_GET_BUFFER(enum _capi_source capi_source, unsigned int ApplId, unsigned int Size) {
    struct _local_capi_appl *LA;
    DEB_INFO("CAPI_GET_BUFFER(ApplId=%u, Size=%u\n", ApplId, Size);

    LA = &LocalCapiAppl[capi_source][ApplId - 1];
    if(LA->InUse != _entry_in_use_) {
        return (unsigned char *)0;
    }
    if(LA->TmpBufferSize != Size) {
        if(LA->TmpBuffer) {
            CA_FREE(LA->TmpBuffer);
        }
        LA->TmpBuffer = CA_MALLOC(Size);
        if(LA->TmpBuffer)
            LA->TmpBufferSize = Size;
    }
    return LA->TmpBuffer;
}

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
unsigned int LOCAL_CAPI_PUT_MESSAGE(enum _capi_source capi_source, unsigned int ApplId, unsigned char *CapiMessage) {
    /*--- unsigned char *Buffer; ---*/
    struct _local_capi_appl *LA;
#if defined(OSLIB_DEBUG)
    unsigned int len = copy_word_from_le_unaligned(CapiMessage);
    CapiTrace(CapiMessage, printk);
#endif/*--- #if defined(OSLIB_DEBUG) ---*/

    LA = &LocalCapiAppl[capi_source][ApplId - 1];
    if(LA->InUse != _entry_in_use_) {
        return ERR_IllegalApplId;
    }

#if !defined(NO_BCHANNEL) && 0
    /*--------------------------------------------------------------------------------------*\
     * DATA_B3_REQ:
     * da der Pointer schon korrekt ist muss er nicht mehr umgesetzt werden
    \*--------------------------------------------------------------------------------------*/
    if(CA_IS_DATA_B3_REQ(CapiMessage)) {
        /*--- DEB_INFO(" IS_DATA_B3_REQ == TRUE "); ---*/
        Buffer = LOCAL_CAPI_GET_DATA_B3_REQ_DATA(CapiMessage);
        return HOST_MESSAGE(capi_source, CapiMessage, Buffer);
    } 
#endif /*--- #if !defined(NO_BCHANNEL) ---*/
    return HOST_MESSAGE(capi_source, CapiMessage, NULL);

}

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
#if !defined(NO_BCHANNEL)
unsigned char *LOCAL_CAPI_NEW_DATA_B3_REQ_BUFFER(enum _capi_source capi_source, unsigned int ApplId, unsigned int NCCI) {
    unsigned int MapperId;
    MapperId = Appl_Find_ApplId(capi_source, ApplId);     /*--- Mapper Id ermitteln ---*/
    if(MapperId == 0)
        return NULL;
    return CA_NEW_DATA_B3_REQ(MapperId, NCCI);
}
#endif /*--- #if !defined(NO_BCHANNEL) ---*/

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
unsigned int LOCAL_CAPI_SET_NOTIFY(enum _capi_source capi_source, unsigned int ApplId, struct work_struct *scheduler_rx_work) {
    struct _local_capi_appl *LA;

    LA = &LocalCapiAppl[capi_source][ApplId - 1];
    if(LA->InUse != _entry_in_use_) {
        return ERR_IllegalApplId;
    }
    LA->Pipe->scheduler_rx_work = scheduler_rx_work;
    return ERR_NoError;
}

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
unsigned int LOCAL_CAPI_SET_SIGNAL(enum _capi_source capi_source, unsigned int ApplId, struct capi_events *pEvent, unsigned int Bit) {
    struct _local_capi_appl *LA;

    LA = &LocalCapiAppl[capi_source][ApplId - 1];
    if(LA->InUse != _entry_in_use_) {
        return ERR_IllegalApplId;
    }
    LA->SignalEvent = pEvent;
    LA->EventBit    = Bit;
    return ERR_NoError;
}

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
unsigned int LOCAL_CAPI_GET_MESSAGE(enum _capi_source capi_source, unsigned int ApplId, unsigned char **pCapiMessage, unsigned int Suspend) {
    int                      Status;
    unsigned long            Count;
    struct _local_capi_appl *LA;

    if(pCapiMessage == NULL) {
        /*--- DEB_INFO("CAPI_GET_MESSAGE(ApplId=%u, ...) (check message avail)\n", ApplId); ---*/
        ;
    /*--- } else { ---*/
        /*--- DEB_INFO("CAPI_GET_MESSAGE(ApplId=%u, *pCapiMessage=%p ...., Suspend=%d)\n", ApplId, *pCapiMessage, Suspend); ---*/
    }

    if(ApplId > LocalCapiMaxAppls)
        return ERR_IllegalApplId;

    LA = &LocalCapiAppl[capi_source][ApplId - 1];
    if(LA->InUse != _entry_in_use_) {
        return ERR_IllegalApplId;
    }

    if(pCapiMessage == NULL) {
        return Capi_Receive_From_Pipe(LA->Pipe, NULL, 0, &Count, CAPI_NO_SUSPEND);
    }

    if(*pCapiMessage == NULL)
        *pCapiMessage = LA->MessageBuffer;

    if(*pCapiMessage == NULL) {
        return ERR_ResourceError;
    }

    Status = Capi_Receive_From_Pipe(LA->Pipe, *pCapiMessage, LA->MaxMessageSize, &Count, Suspend);
    if(Status == CAPI_PIPE_EMPTY || Status == CAPI_PIPE_DELETED || Status == CAPI_PIPE_TIMEOUT) {
        return ERR_QueueEmpty;
    }
#if defined(CAPIOSLIB_CHECK_LATENCY)
    /*--- capi_check_latency(ApplId, (char *)__func__, 1); ---*/
#endif/*--- #if defined(CAPIOSLIB_CHECK_LATENCY) ---*/
    if(Status != 0) {
        /*--- DEB_WARN("ApplId %u Suspend %x local_capi_appl (%#x) =[%*B]\n", ApplId, Suspend, A, sizeof(*A), A); ---*/
        SYSTEM_ERROR("receive from LA->Pipe failed", ERR_ResourceError);
    }

    /*--- DEB_INFO("CAPI_GET_MESSAGE():%s\n", CAPI_MESSAGE_NAME((*pCapiMessage)[4], (*pCapiMessage)[5])); ---*/

    if(LA->MessageLost) {
        LA->MessageLost = 0;
        return ERR_MessageLost;
    }

    if(Count == 0)
        return ERR_QueueEmpty;

    {
#if defined(OSLIB_DEBUG)
        unsigned int len = copy_word_from_le_unaligned(*pCapiMessage);
        CapiTrace(*pCapiMessage, printk);
#endif/*--- #if defined(OSLIB_DEBUG) ---*/
    }
    return ERR_NoError;
}

