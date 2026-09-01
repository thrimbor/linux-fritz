#include <linux/kernel.h>
#include <linux/string.h>
#include "debug.h"
#include "appl.h"
#include "ca.h"

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
struct _ApplData *ApplData = NULL;
unsigned int MaxApplData   = 0;             /* Array, große bei Treiber Init gesetzt   */
unsigned int Karte;                         /* CAPI_INDEX                              */
struct semaphore Register_Release_Sema;     /* Semaphore für Register/Release Operation*/
struct semaphore ApplData_Sema;     /* Semaphore für Register/Release Operation*/
/*--- NU_SEMAPHORE S_ToLink; ---*/                      /* Semaphore zum Senden auf Linkinterface  */

void (*Stack_Register)(void *adata, unsigned int Mapper); /*  Callback Register im Stack       */
void (*Stack_Release)(void * adata);        /* Callback Release im Stack               */
void (*Stack_Shutdown)(void);               /* Callback in Stack (E1_Exit)             */

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
unsigned int Appl_Find_ApplId(enum _capi_source CapiSource, unsigned int ApplId) {
    unsigned int Count;
    struct _ApplData *A;
    down(&ApplData_Sema);
    A = &ApplData[0];
    for(Count = 0 ; Count < MaxApplData ; Count++) {
        if(A->InUse == _entry_in_use_ && A->ApplId == ApplId && A->CapiSource == CapiSource) {
            up(&ApplData_Sema);
            return Count + 1;
        }
        A++;
    }
    up(&ApplData_Sema);
    return 0;
}

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
unsigned int Appl_Find_EmptyApplId(void) {
    unsigned int Count;
    down(&ApplData_Sema);
    for(Count = 0 ; Count < MaxApplData ; Count++) {
        if(ApplData[Count].InUse == _entry_not_used_) {
            ApplData[Count].InUse = _entry_register_pending;
            up(&ApplData_Sema);
            return Count + 1;
        }
    }
    up(&ApplData_Sema);
    return 0;
}

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
struct _NCCIData  *Appl_Find_NCCIData(struct _ApplData *A, unsigned int NCCI) {
    struct _NCCIData *N;
    unsigned int      Count;

    N = &A->NCCIData[0];
    if(N == NULL)
        return NULL;

    for(Count = 0 ; Count < A->NCCIs ; Count++) {
        if(N->InUse == _entry_in_use_ && N->NCCI == NCCI)
            return N;
        N++;
    }
    return NULL;
}

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
char *Appl_PrintOneAppl(struct _ApplData *A) {
    struct _NCCIData *N;
    unsigned int Count;
    static char Buffer[256];

    sprintf(Buffer, "MapperId %u (Host %u", A->Nr + 1, A->ApplId);

    sprintf(Buffer + strlen(Buffer), ") NCCIs %u B3BlockSize %u WindowSize %u", A->NCCIs, A->B3BlockSize, A->WindowSize);

    N = A->NCCIData;
    if(N == NULL)
        return Buffer;

    for(Count = 0 ; Count < A->NCCIs ; Count++, A++) {
        if(N->InUse == _entry_not_used_)
            continue;
        sprintf(Buffer + strlen(Buffer), "\nNCCI 0x%8x WindowSize %u/%u ", N->NCCI, N->RxWindowSize, N->TxWindowSize);
        sprintf(Buffer + strlen(Buffer), "Rx[%c%c%c%c%c%c%c%c] ",
            (N->RxWindowSize > 0) ? (N->RxBuffer[0].InUse == _entry_in_use_ ? 'A' : 'F') : '.',
            (N->RxWindowSize > 1) ? (N->RxBuffer[1].InUse == _entry_in_use_ ? 'A' : 'F') : '.',
            (N->RxWindowSize > 2) ? (N->RxBuffer[2].InUse == _entry_in_use_ ? 'A' : 'F') : '.',
            (N->RxWindowSize > 3) ? (N->RxBuffer[3].InUse == _entry_in_use_ ? 'A' : 'F') : '.',
            (N->RxWindowSize > 4) ? (N->RxBuffer[4].InUse == _entry_in_use_ ? 'A' : 'F') : '.',
            (N->RxWindowSize > 5) ? (N->RxBuffer[5].InUse == _entry_in_use_ ? 'A' : 'F') : '.',
            (N->RxWindowSize > 6) ? (N->RxBuffer[6].InUse == _entry_in_use_ ? 'A' : 'F') : '.',
            (N->RxWindowSize > 7) ? (N->RxBuffer[7].InUse == _entry_in_use_ ? 'A' : 'F') : '.'
        );
        sprintf(Buffer + strlen(Buffer), "Tx[%c%c%c%c%c%c%c%c]\n",
            (N->TxWindowSize > 0) ? (N->TxBuffer[0].InUse == _entry_in_use_ ? 'A' : 'F') : '.',
            (N->TxWindowSize > 1) ? (N->TxBuffer[1].InUse == _entry_in_use_ ? 'A' : 'F') : '.',
            (N->TxWindowSize > 2) ? (N->TxBuffer[2].InUse == _entry_in_use_ ? 'A' : 'F') : '.',
            (N->TxWindowSize > 3) ? (N->TxBuffer[3].InUse == _entry_in_use_ ? 'A' : 'F') : '.',
            (N->TxWindowSize > 4) ? (N->TxBuffer[4].InUse == _entry_in_use_ ? 'A' : 'F') : '.',
            (N->TxWindowSize > 5) ? (N->TxBuffer[5].InUse == _entry_in_use_ ? 'A' : 'F') : '.',
            (N->TxWindowSize > 6) ? (N->TxBuffer[6].InUse == _entry_in_use_ ? 'A' : 'F') : '.',
            (N->TxWindowSize > 7) ? (N->TxBuffer[7].InUse == _entry_in_use_ ? 'A' : 'F') : '.'
        );
    }
    return Buffer;
}

/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
void Debug_PrintAppls(unsigned int MapperId) {
    unsigned int Count;
    struct _ApplData *A;
    if(MapperId > 0) {
        A = &ApplData[MapperId - 1];
        if(A->InUse == _entry_not_used_)
            printk("\nMapperId %d not registered", MapperId);
#ifndef NDEBUG
        else
            DEB_INFO("%s", Appl_PrintOneAppl(A));
#endif/*--- #ifndef NDEBUG ---*/
        return;
    }
    A = &ApplData[0];
    for(Count = 0 ; Count < MaxApplData ; Count++, A++) {
#ifndef NDEBUG
        if(A->InUse == _entry_in_use_)
            DEB_INFO("%s", Appl_PrintOneAppl(A));
#endif/*--- #ifndef NDEBUG ---*/
    }
}

