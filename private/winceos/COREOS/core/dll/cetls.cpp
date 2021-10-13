//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//

#include <windows.h>
#include <coredll.h>
#include "kerncmn.h"

#define OwnCS(lpcs)    ((lpcs)->OwnerThread == (HANDLE) GetCurrentThreadId())

#ifdef KCOREDLL

#define LockLoader()
#define UnlockLoader()

#else  // KCOREDLL

#define LockLoader()      LockProcCS()
#define UnlockLoader()    UnlockProcCS()
#define OwnLoaderLock()   OwnProcCS()

#endif

// Node which handles the TLS values for a thread/slot combination.
typedef struct _CETLSNODE
{
    DLIST   link;   // Doubly-linked list to link all nodes (must be the first in the structure)
    DWORD   tid;
    DWORD   slot;    
    LPVOID  value;
} CETLSNODE, *PCETLSNODE;

// cleanup functions associated with each slot
PFN_CETLSFREE g_CeTlsSlotPfn[TLS_MINIMUM_AVAILABLE];

// all the tls values for all active threads which
// transitioned into a PSL server where cetls
// module is loaded. 
DLIST g_CeTlsActiveNodes;

// all the tls values for all exited threads which
// transitioned into a PSL server where cetls
// module is loaded. 
DLIST g_CeTlsFreeNodes;

// thread used to clean up tls values for a PSL threaddeseze
// which has exited.
HANDLE g_hCleanupEvent, g_hCleanupThread;

// CS protecting the tls nodes
CRITICAL_SECTION g_CeTlsLock;

// max tls slot used in current process
DWORD g_maxTlsSlot;

extern "C" BOOL xxx_KernelLibIoControl (HANDLE hLib, DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned);

__inline BOOL IsInRange (DWORD x, DWORD start, DWORD end)
{
    return (x >= start) && (x < end);
}

//
// match a given slot in the current thread
//
BOOL MatchNode( 
    PDLIST pItem, 
    LPVOID pEnumData 
    )
{
    PCETLSNODE pNode = (PCETLSNODE) pItem;    
    return ((pNode->tid == GetCurrentThreadId()) && (pNode->slot == (DWORD)pEnumData));
}

//
// match a given thread
//
BOOL MatchThread( 
    PDLIST pItem, 
    LPVOID pEnumData 
    )
{
    PCETLSNODE pNode = (PCETLSNODE) pItem;    
    return (pNode->tid == (DWORD)pEnumData);
}

//
// match a given slot
//
BOOL MatchSlot( 
    PDLIST pItem, 
    LPVOID pEnumData 
    )
{
    PCETLSNODE pNode = (PCETLSNODE) pItem;    
    return (pNode->slot == (DWORD)pEnumData);
}


/*++

Routine Description:

    This helper routine is used by TLS free code to call the
    cleanup function for a slot and remove the node mem.

Arguments:

    Item - Supplies a pointer to the thread node structure
    in the TLS active/free node list.

    EnumData - Slot index.

Return Value:

    TRUE - This will stop the enumeration code to break out
    of the loop.
    FALSE - This will continue the enumeration to go
    through the rest of the active/free nodes in the list.

--*/
BOOL DestroyNode( 
    PDLIST Item, 
    LPVOID EnumData 
    )
{
    PCETLSNODE Node = (PCETLSNODE) Item;
    DWORD slot = Node->slot;
    
    // call the cleanup function associated with this slot    
    if (g_CeTlsSlotPfn[slot] && Node->value) {
        LPDWORD tlsptr = (LPDWORD) UTlsPtr()[PRETLS_TLSBASE]; // get to the non-PSL tls area           
        tlsptr[slot] = (DWORD) Node->value; // to support TlsGetValue from cleanup function
        __try {
            (*g_CeTlsSlotPfn[slot]) (Node->tid, Node->value);
        } __except(EXCEPTION_EXECUTE_HANDLER) {
        }
        tlsptr[slot] = 0;        
    }

    // free the node
    LocalFree (Node);
    
    return FALSE;
}

/*++

Routine Description:

    This helper routine remove all items with matching slot
    values from source list and adds the nodes to the new
    destination list.

Arguments:

    Source - Supplies a pointer to the root node of the
    source list we are enumerating.

    Destination - Supplies a pointer to the root node of the
    destination list to which we are adding new nodes.
    
    slot - slot index.

Return Value:

    None.

--*/    
void MoveNodes(
    PDLIST Source,
    PDLIST Destination,
    DWORD slot
)
{
    PDLIST ptrav;
    for (ptrav = Source->pFwd; ptrav != Source; ptrav = ptrav->pFwd) {
        PCETLSNODE pNode = (PCETLSNODE) ptrav;
        if (pNode->slot == slot) {
            ptrav = pNode->link.pBack; // since we are deleting the current node below
            RemoveDList (&pNode->link);
            AddToDListHead (Destination, (PDLIST)pNode);
        }
    }
}

/*++

Routine Description:

    This is a worker thread to cleanup any nodes in the
    free nodes list. When a thread exits, that paritcular
    thread node is added to the free nodes list to be
    cleaned up in this thread. This thread calls the cleanup
    function for each TLS slots in every thread node and
    then releases the memory associated with the thread
    node.

Arguments:

    param - Unused.

Return Value:

    0 - Thread exit code.

--*/        
DWORD WINAPI CleanupThreadProc(
    LPVOID unused
    )
{
    DWORD dwWait = 0;

    DEBUGCHK (g_hCleanupEvent);
    DEBUGMSG (DBGLOADER, (L"+CeTls::CleanupThreadProc starting ...\r\n"));

    for (;;) {

        dwWait = WaitForSingleObject (g_hCleanupEvent, INFINITE);
        if (WAIT_OBJECT_0 != dwWait) {
            // wait failed; exit the thread
            break;
        }

        DLIST CeTlsFreeNodes;
        InitDList (&CeTlsFreeNodes);

        // copy the free nodes from global list to local copy
        EnterCriticalSection (&g_CeTlsLock);
        
        CeTlsFreeNodes = g_CeTlsFreeNodes;
        InitDList (&g_CeTlsFreeNodes); // this will effectively clear this list

        LeaveCriticalSection (&g_CeTlsLock);

        if (CeTlsFreeNodes.pBack != &g_CeTlsFreeNodes) {
            // list non-empty; adjust the end points and delete the nodes

            CeTlsFreeNodes.pBack->pFwd = CeTlsFreeNodes.pFwd->pBack = &CeTlsFreeNodes;            

            // take loader lock to prevent any dlls from unloading;
            // otherwise we might fault on the call to slot cleanup fn.
            LockLoader ();

            // destroy all nodes (calling the cleanup function for each node)
            DestroyDList (&CeTlsFreeNodes, DestroyNode, NULL);          

            UnlockLoader ();
        }    
    }

    DEBUGMSG (DBGLOADER, (L"-CeTls::CleanupThreadProc exiting ...\r\n"));
    
    return 0;
}

/*++

Routine Description:

    This routine is called by coredll on startup.

Arguments:

    None.

Return Value:

    TRUE - if initialized successfully.
    FALSE - otherwise; last error set.

--*/    
BOOL CeTlsCoreInit(
    void
    )
{    
    InitializeCriticalSection (&g_CeTlsLock);
    if (!g_CeTlsLock.hCrit) {
        return FALSE;
    }    

    g_hCleanupEvent = CreateEvent (NULL, 0, 0, NULL);
    if (!g_hCleanupEvent) {
        return FALSE;
    }
    
    // initialize the two lists: one to store active thread
    // tls values and one to store exited thread tls values
    InitDList (&g_CeTlsActiveNodes);
    InitDList (&g_CeTlsFreeNodes);

    return TRUE;
}

/*++

Routine Description:

    Kernel will call into this function when a thread is exiting; in this 
    API we will clean up any TLS slots for the given thread.

Arguments:

    Flags - PSL notification code; we track only thread detach.
    
    ProcId - process ID (unused)
    
    ThrdId - ID of the thread on which this PSL notification
    was generated.

Return Value:

    None.

--*/    
void
CeTlsFreeSlots(
    DWORD Flags, 
    DWORD ProcId, // unused 
    DWORD ThrdId
    )    
{
    if (DLL_TLS_DETACH == Flags && g_hCleanupThread) {
        // a thread is exiting; cleanup any tls slots for this thread in 
        // PSL context; this code only cleans up if the cleanup thread is
        // running in the current process. Cleanup thread will be running
        // in the current process only if there was a call to init cetls
        // in the current process (via CeTlsInitialize).

        PDLIST ptrav;
        BOOL fSetEvent = FALSE;
        
        EnterCriticalSection (&g_CeTlsLock);

        for (ptrav = g_CeTlsActiveNodes.pFwd; ptrav != &g_CeTlsActiveNodes; ptrav = ptrav->pFwd) {
            PCETLSNODE pNode = (PCETLSNODE) ptrav;
            if (pNode->tid == ThrdId) {
                ptrav = pNode->link.pBack; // since we are deleting the current node below
                RemoveDList (&pNode->link);                
                AddToDListHead (&g_CeTlsFreeNodes, &pNode->link);
                fSetEvent = TRUE;
            }
        }
                    
        LeaveCriticalSection (&g_CeTlsLock);

        if (fSetEvent) {
            DEBUGCHK (g_hCleanupThread);
            DEBUGCHK (g_hCleanupEvent);
            SetEvent (g_hCleanupEvent);        
        }            
    }
    
}

/*++

Routine Description:

    This helper routine is called to initialize the support of 
    cetls in the current process.

Arguments:

    None.

Return Value:

    TRUE - if initialized successfully.
    FALSE - otherwise; last error set.

--*/    
BOOL CeTlsInitialize(
    void
    )
{        
    DEBUGCHK (OwnCS(&g_CeTlsLock));
    
    if (!g_hCleanupThread) {

#ifndef KCOREDLL
        // it is not a good idea to create a thread while holding process
        // loader lock; if you hit this debug check, try to re-architect
        // the calling code.
        DEBUGMSG (OwnLoaderLock(), (L"Creating cetls monitor thread while holding loader lock. Generally not a good idea. If possible move the call to TlsSetValue out of dllmain.\r\n"));
#endif

        // create the cleanup thread        
        g_hCleanupThread = CreateThread (NULL, 0, CleanupThreadProc, NULL, 0, NULL);
        if (g_hCleanupThread) {
            // initialize cetls in the current process structure (this call will update current process structure)
            VERIFY (xxx_KernelLibIoControl ((HANDLE)KMOD_CORE, IOCTL_KLIB_SETPROCTLS, (LPVOID)CeTlsFreeSlots, sizeof(DWORD), NULL, 0, NULL));
        }
    }

    return (NULL != g_hCleanupThread);    
}

/*++

Routine Description:

    This routine returns a TLS value stored for the given
    slot in the current thread.

    TlsGetValue will call this function if the current thread 
    is in a PSL call.

Arguments:

    slot - index of the slot.
    pfNoTlsData - set to TRUE if slot range is valid but
    there is no tls data.

Return Value:

    lpvalue - value associated with the given slot in the
    current thread.
    NULL - no value is associated with this slot for the
    current thread.

--*/    
LPVOID CeTlsGetValue(
    DWORD slot,
    LPBOOL pfNoTlsData
    )
{
    LPVOID value = NULL; 
    *pfNoTlsData = FALSE;

    if (IsInRange (slot, TLSSLOT_NUMRES, TLS_MINIMUM_AVAILABLE) && g_CeTlsSlotPfn [slot]) {
    
        EnterCriticalSection (&g_CeTlsLock);   

        // get the existing node
        PCETLSNODE node = (PCETLSNODE) EnumerateDList (&g_CeTlsActiveNodes, MatchNode, (LPVOID)slot);    
        if (node) {
            value = node->value;
        } else {
            *pfNoTlsData = TRUE;
        }
        
        LeaveCriticalSection (&g_CeTlsLock);    

        if (!value)
            SetLastError (NO_ERROR);   
    }
    
    return value;
}

/*++

Routine Description:

    This routine sets a TLS value for the given slot in the 
    current thread.

    TlsSetValue will call this function for both local and PSL
    threads.

Arguments:

    slot - index of the slot.
    
    value - value to set to.

    fThrdInPSL - is the calling thread in a PSL context?

Return Value:

    lpvalue - value associated with the given slot in the
    current thread.
    NULL - no value is associated with this slot for the
    current thread.

--*/    
BOOL CeTlsSetValue(
    DWORD slot,
    LPVOID value,
    BOOL fThrdInPSL
    )
{
    BOOL fRet = IsInRange (slot, TLSSLOT_NUMRES, TLS_MINIMUM_AVAILABLE);

    if (fRet && g_CeTlsSlotPfn [slot]) {
       
        EnterCriticalSection (&g_CeTlsLock);      

        // initialize cetls support in the calling process if the set
        // call came in via PSL call.
        if (fThrdInPSL) {
            CeTlsInitialize ();
        }

        // find existing node
        PCETLSNODE node = (PCETLSNODE) EnumerateDList (&g_CeTlsActiveNodes, MatchNode, (LPVOID)slot);    
        
        if (!node) {
            // create a new node
            node = (PCETLSNODE) LocalAlloc (LMEM_ZEROINIT, sizeof(CETLSNODE));
            if (node) {
                node->tid = GetCurrentThreadId ();
                node->slot = slot;
                AddToDListHead (&g_CeTlsActiveNodes, &node->link);
            }
        }

        if (node) {
            // set the values
            node->value = value;
            fRet = TRUE;
        }

        LeaveCriticalSection (&g_CeTlsLock);        
    }
    
    return fRet;
}

/*++

Routine Description:

    This routine calls a cleanup function and frees the slot value in
    the current thread. Called from CeTlsFree.

    Modules should call this on local thread exit. Note that the
    process loader lock is held during this call.

Arguments:

    slot - index of the slot.
    
Return Value:

    TRUE - if there is a valid slot value stored for current thread.
    FALSE - otherwise.

--*/    
BOOL CeTlsFreeInCurrentThread(
    DWORD slot
    )
{
    BOOL fRet = IsInRange (slot, TLSSLOT_NUMRES, TLS_MINIMUM_AVAILABLE);

#ifndef KCOREDLL    
    if (fRet
        && !OwnLoaderLock() 
        && !IsExiting
        && g_CeTlsSlotPfn [slot]
        ) {
        ERRORMSG (1, (L"TLS slot has cleanup function associated with it (slot: 0x%8.8lx, func: 0x%8.8lx). In this case CeTlsFree should only be called from within dll entry point.\r\n", slot, g_CeTlsSlotPfn [slot]));
        fRet = FALSE;
    }
#endif
    
    if (fRet && g_CeTlsSlotPfn [slot]) {

        EnterCriticalSection (&g_CeTlsLock);      

        // find existing node
        PCETLSNODE node = (PCETLSNODE) EnumerateDList (&g_CeTlsActiveNodes, MatchNode, (LPVOID)slot);    
        if (node) {
            // remove the node from the list
            RemoveDList (&node->link);
        }

        LeaveCriticalSection (&g_CeTlsLock);       

        // this will call the cleanup function without holding the
        // cetls lock and also free the underlying node memory
        if (node) {
            DestroyNode ((PDLIST)node, NULL);                
        }
    }
    
    return fRet;
}

/*++

Routine Description:

    This routine calls a cleanup function and frees the slot value in
    the current thread. Called from thread exit notification from.

Arguments:

    none.
    
Return Value:

    FALSE / TRUE

--*/    
BOOL CeTlsThreadExit (
        void
    )
{
    BOOL fRet = FALSE;

#ifdef KCOREDLL    
    DWORD slot;
    for (slot = TLSSLOT_NUMRES; slot <= g_maxTlsSlot; ++slot) {       
        if (g_CeTlsSlotPfn[slot]) {
            // for kernel modules, tls value is in base tls
            LPVOID lpv = TlsGetValue (slot);
            if (lpv) {
                __try {
                    (*g_CeTlsSlotPfn[slot]) (GetCurrentThreadId(), lpv);
                } __except(EXCEPTION_EXECUTE_HANDLER) {
                }            
                TlsSetValue (slot, 0);
            }            
        }
    }
	fRet = true;
#endif // KCOREDLL    
    
    return fRet;
}


/*++

Routine Description:

    This routine frees the given slot in all threads managed by 
    tls in the current process. Called from TlsFree.

Arguments:

    slot - index of the slot.

Return Value:

    TRUE - if the given slot is valid and there is a cleanup 
    function for the slot.
    FALSE - if the given slot index is not valid.

--*/    
BOOL CeTlsFreeInAllThreads(
    DWORD slot
    )
{
    BOOL fRet = IsInRange (slot, TLSSLOT_NUMRES, TLS_MINIMUM_AVAILABLE);

#ifndef KCOREDLL    
    if (fRet
        && !OwnLoaderLock() 
        && !IsExiting
        && g_CeTlsSlotPfn [slot]
        ) {
        ERRORMSG (1, (L"TLS slot has cleanup function associated with it (slot: 0x%8.8lx, func: 0x%8.8lx). In this case TlsFree should only be called from within dll entry point.\r\n", slot, g_CeTlsSlotPfn [slot]));
        fRet = FALSE;
    }
#endif

    // called to free slot value for both local and PSL threads.
    if (fRet && g_CeTlsSlotPfn [slot]) {

        // for kcoredll version see CeTlsThreadExit which gets
        // called without holding loader critical section on thread
        // exit.
#ifndef KCOREDLL        
        if (!IsExiting) {        
            DLIST TempList;
            InitDList (&TempList);
        
            EnterCriticalSection (&g_CeTlsLock);

            // move all the current active/free nodes with matching slot ids
            // to the temporary list
            MoveNodes (&g_CeTlsActiveNodes, &TempList, slot);
            MoveNodes (&g_CeTlsFreeNodes, &TempList, slot);        

            LeaveCriticalSection (&g_CeTlsLock );   

            // call cleanup function and then destroy the nodes in
            // the temporary list without holding any tls locks
            DestroyDList (&TempList, DestroyNode, NULL);
        }
#endif        

        g_CeTlsSlotPfn [slot] = 0;
    }
    
    return fRet;
}

/*++

Routine Description:

    This routine associates a cleanup function with a slot; this 
    cleanup function will be called when the corresponding slot 
    is freed.

    This is called during DLL_PROCESS_ATTACH. The current 
    thread holds the loader lock when this API is called.

Arguments:

    slot - index of the slot.
    
    CleanupFunc - cleanup function to be associated with the given slot.

Return Value:

    TRUE - valid slot
    FALSE - otherwise.

--*/    
BOOL
CeTlsSetCleanupFunction(
    DWORD slot,
    PFNVOID CleanupFunc
    )
{
    BOOL fRet = IsInRange (slot, TLSSLOT_NUMRES, TLS_MINIMUM_AVAILABLE);

    if (fRet) {

        DEBUGCHK (CleanupFunc);
    
        EnterCriticalSection (&g_CeTlsLock);
        
        g_CeTlsSlotPfn[slot] = (PFN_CETLSFREE) CleanupFunc;   
        if (g_maxTlsSlot < slot)
            g_maxTlsSlot = slot;
        
        LeaveCriticalSection (&g_CeTlsLock);
    }

    return fRet;
}

