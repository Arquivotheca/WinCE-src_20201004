//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Module Name:  
    ethdbg.c
    
Abstract:  
    Routines for kernel debug services over Ethernet (KITL).  For platforms
    that have a debug ethernet controller, this will allow the following
    services to be configured to run over the ethernet:

    o  Debug messages
    o  Kernel debugger
    o  Text shell and PPFS file system
    o  Other user defined applications (e.g. profiling, test apps, etc)
    
    To provide reliability and data integrity, communication with the desktop is
    done via UDP, with the KITL protocol on top to provide reliability.  For efficiency,
    the KITL protocol maintains a simple sliding window algorithm, so that multiple
    packets may be outstanding
    
Functions:


Notes: 

--*/
#include <windows.h>
#include <types.h>
#include <nkintr.h>
#include <kitl.h>
#include <kitlpriv.h>
#include "kitlp.h"
#include "kernel.h"

DWORD SC_WaitForMultiple(DWORD cObjects, CONST HANDLE *lphObjects, BOOL fWaitAll, DWORD dwTimeout);
void SwitchToKernel(PCALLSTACK pcstk);
void SwitchBack();

// Priority for IST.  Since the SMC chip has a fairly small Rx packet buffer, 
// run at high priority to avoid overruns.  Note the OEM can modify this value in
// the OAL (OEMInit()).
#define KITL_IST_PRIORITY    130
#define KITL_TIMER_PRIORITY  247    // lowest RT priority
DWORD g_dwKITLThreadPriority = KITL_IST_PRIORITY;       
DWORD g_dwKITLTimerThreadPriority = KITL_TIMER_PRIORITY;        

// Priority for timer thread.  This just processes retransmissions, can run at
// lower priority.

// Bitmask which indicates global state
DWORD KITLGlobalState;

// Structures to maintain state of registered clients. Reserve space for the
// three default services, plus some for user defined applications.  The ServiceId
// identifiers are indices into this array.

// we now do dynamic allocation for clients. The name of a service, except 
// for default services, is now used to hash into the structure to provide
// some level of uniqueness of service id. Note that client structure will
// NOT be de-allocated once allocated.
#define HEAP_KITLCLIENT         HEAP_MUTEX
ERRFALSE(sizeof(KITL_CLIENT) <= sizeof(MUTEX));
KITL_CLIENT DfltClnts[NUM_DFLT_KITL_SERVICES];
PKITL_CLIENT KITLClients[MAX_KITL_CLIENTS];

// Buffers for default services - since PPFS and windbg use byte at a time operations, we need
// to format commands in a buffer before sending to other side.  Since both PPFS and windbg are
// serialized while sending commands, we can share a single buffer for send and receive.
static UCHAR PpfsFmtBuf[(KITL_MAX_DATA_SIZE+3)&~3];  // make it DWORD aligned
static UCHAR KdbgFmtBuf[(KITL_MAX_DATA_SIZE+3)&~3];  // make it DWORD aligned


// Receive buffers for ISR routine.  Need two, because we may have filled a buffer in the
// ISR, then get swapped out and another thread may need to do polled I/O.
static UCHAR ISTRecvBuf[KITL_MTU];

KITLTRANSPORT Kitl;

HANDLE hIntrThread;
HANDLE hTimerThread;

// To prevent interleaving of our debug output
CRITICAL_SECTION KITLODScs;

// cs to pretect timer list
CRITICAL_SECTION TimerListCS;

// critical section in place of KCall
CRITICAL_SECTION KITLKCallcs;

#define KITL_BUFFER_POOL_SIZE  (Kitl.WindowSize * 2 * KITL_MTU)

// cs to protect client list
extern CRITICAL_SECTION KITLcs;
extern BOOL fKITLcsInitialized;
void ResetClientState(KITL_CLIENT *pClient);
void SetClientEvent(KITL_CLIENT *pClient, HANDLE hEvent);

// Local functions
static BOOL StartKitl (BOOL fInit);
static BOOL  RegisterClientPart2(UCHAR Id);
static void  EnableInts();
static DWORD KITLInterruptThread(DWORD fEnableInts);
static BOOL  DoRegisterClient (UCHAR *pId, char *ServiceName, UCHAR Flags, UCHAR WindowSize, UCHAR *pBufferPool);

/*
 * @doc  KITL
 * @func VOID | KITLSetDebug |  Controls debug print output for KITL subsystem.
 * @comm Used to debug the KITL routines (only enabled for DEBUG versions of ethdbg.dll).
 *       Debug messages are written to the debug serial port, based on the value
 *       of the DebugZoneMask parameter. Some of the more useful zones are ZONE_INIT 
 *       (messages related to client registration), ZONE_WARNING (unusual conditions
 *       and lost frames), and ZONE_FRAMEDUMP (dump of all packets sent - verbose!).
 *       See ethdbg.h for all zone definitions.  
 */
DWORD KITLDebugZone = ZONE_INIT|ZONE_ERROR;//|ZONE_WARNING|ZONE_FRAMEDUMP|ZONE_RECV;
void KITLSetDebug (DWORD DebugZoneMask)
{
    KITLOutputDebugString("Changed KITL zone mask to 0x%X\n",DebugZoneMask);
    KITLDebugZone = DebugZoneMask;
}

BOOL IsDesktopDbgrExist () 
{
    return (DfltClnts[KERNEL_SVC_KDBG].State & KITL_CLIENT_REGISTERED);
}

//
// NewClient: create a new client structure and initialized it
//
static PKITL_CLIENT NewClient (UCHAR uId, LPCSTR pszSvcName, BOOL fAlloc)
{
    DEBUGCHK(IS_VALID_ID(uId));
    DEBUGCHK (!KITLClients[uId]);
    if (!fAlloc) {
        DEBUGCHK(IS_DFLT_SVC(uId));
        KITLClients[uId] = &DfltClnts[uId];
    } else if (!(KITLClients[uId] = (PKITL_CLIENT) AllocMem (HEAP_KITLCLIENT))) {
        return NULL;
    }
    memset (KITLClients[uId], 0, sizeof(KITL_CLIENT));
    KITLClients[uId]->ServiceId = uId;
    strcpy (KITLClients[uId]->ServiceName, pszSvcName);

    return KITLClients[uId];
}

//
// ChkDfltSvc: return id if it's a default service, -1 if not
//
int ChkDfltSvc (LPCSTR ServiceName)
{
    int i;
    for (i = 0; i < NUM_DFLT_KITL_SERVICES; i ++) {
        if (strcmp (KITLClients[i]->ServiceName, ServiceName) == 0)
            return i;
    }
    return -1;
}

/* KITLRegisterDfltClient
 *
 *   Register one of the 3 default services (DBGMSG, PPSH, KDBG).  These are registered
 *   differently from other KITL services - the window size and buffer pools are
 *   supplied in the HAL.  Additionally, we provide a couple of buffers for use by the
 *   kernel in formatting KITL messages - Since PPSH and KDBG use byte at a time I/O,
 *   these buffers are used to gather up to KITL_MAX_DATA_SIZE bytes to ship across KITL.
 */
BOOL
KITLRegisterDfltClient(
    UCHAR Service,         // IN - Service ID (one of KITL_SVC_ defs from ethdbg.h)
    UCHAR Flags,           // IN - Flags (one of KITL_CFGFL_ defs from ethdbg.h)
    UCHAR **ppTxBuffer,    // OUT- Receives pointer to formatting buffer, if appropriate
    UCHAR **ppRxBuffer)    // OUT- Receives pointer to formatting buffer, if appropriate
{
    UCHAR Id, *pBufferPool;
    KITL_CLIENT *pClient;
    BOOL fRet = TRUE;

    KITL_DEBUGMSG(ZONE_INIT,("+KITLRegisterDfltClient, service:%u\n",Service));

    if (!IS_DFLT_SVC(Service)) {
        return FALSE;
    }

    if (fKITLcsInitialized && !InSysCall ()) {
        EnterCriticalSection (&KITLcs);
    }
    if (!StartKitl (FALSE)) {
        KITLOutputDebugString ("!KITLRegisterDfltClient: Fail to initialize KITL\n");
        fRet = FALSE;
    } else {
    
        pClient = KITLClients[Service];
        DEBUGCHK (pClient);
    
        switch (Service) {
        case KITL_SVC_DBGMSG:
            // start DBGMSG service?
            if (!(Kitl.dwBootFlags & KITL_FL_DBGMSG)) {
                fRet = FALSE;
                goto exit;
            }
            pBufferPool = (LPBYTE) Kitl.dwPhysBuffer;
            // No special buffer needed for debug messages
            break;
        case KITL_SVC_PPSH:
            // start PPSH service?
            if (!(Kitl.dwBootFlags & KITL_FL_PPSH)) {
                fRet = FALSE;
                goto exit;
            }
            pBufferPool = (LPBYTE) (Kitl.dwPhysBuffer + KITL_BUFFER_POOL_SIZE);
            *ppTxBuffer = *ppRxBuffer = PpfsFmtBuf;
            break;
        case KITL_SVC_KDBG:
            // start KD service?
            if (!(Kitl.dwBootFlags & KITL_FL_KDBG)) {
                fRet = FALSE;
                goto exit;
            }
            pBufferPool = (LPBYTE) (Kitl.dwPhysBuffer + 2 * KITL_BUFFER_POOL_SIZE);
            *ppTxBuffer = *ppRxBuffer = KdbgFmtBuf;
            break;
        default:
            fRet = FALSE;
        }
    }
    if (fRet)
        fRet = DoRegisterClient(&Id, pClient->ServiceName, Flags, Kitl.WindowSize, pBufferPool);
exit:
    if (fKITLcsInitialized && !InSysCall ()) {
        LeaveCriticalSection (&KITLcs);
    }
    return fRet;
}

/* 
 * @func  BOOL | KITLRegisterClient | Register a client for debug Ethernet (KITL) service.
 * @rdesc Returns TRUE if registration succeeds, FALSE if another client has already 
 *        registered for service, or error occurs connecting to peer.
 * @comm  Function to register a client for an KITL service. Each service has a unique 
 *        name which identifies it.  Certain service names are reserved for default OS
 *        services (e.g. "DBGMSG").  Other names may be used for user defined applications.
 *        Registration will not complete until a peer client has registered for this service.
 *        If the peer address is known (e.g. for default OS clients, eshell sends this 
 *        information in JUMPIMG command when device is booted), then we'll start sending 
 *        config messages to the other side.  Otherwise, a passive connect is performed, 
 *        and we wait for the peer to connect to us.  For user applications, this means that
 *        this function will block until the peer application is started on the desktop.
 * @xref  <f KITLDeregisterClient>
 */
BOOL
KITLRegisterClient(
    UCHAR *pId,         // @parm [OUT] - Receives identifier to pass in to KITLSend/Recv 
    char  *ServiceName, // @parm [IN]  - Service name (NULL terminated, up to MAX_SVCNAME_LEN chars)
    UCHAR Flags,        // @parm [IN]  - KITL_CFGFL_ flags, defined in ethdbg.h
    UCHAR WindowSize,   // @parm [IN]  - Protocol window size (default is 8)
    UCHAR *pBufferPool) // @parm [IN]  - Buffer pool to use for KITL packet buffers - must be WindowSize*2*1500 bytes, unless STOP_AND_WAIT flag is set, in which
                        //               case, need only be 2*1500 bytes. For default OS services, may be NULL.
{
    BOOL fRet = FALSE;
    KITL_DEBUGMSG(ZONE_INIT,("+KITLRegisterClient(%s): Flags: %u, WindowSize: %u, BufferPool: %X\n",
                             ServiceName,Flags,WindowSize,pBufferPool));

    EnterCriticalSection (&KITLcs);
    
    if (StartKitl (FALSE))
        fRet = DoRegisterClient (pId, ServiceName, Flags, WindowSize, pBufferPool);

    LeaveCriticalSection (&KITLcs);
    KITL_DEBUGMSG(ZONE_INIT,("-KITLRegisterClient(%s): Flags: %u, WindowSize: %u, BufferPool: %X\n",
                             ServiceName,Flags,WindowSize,pBufferPool));
    return fRet;
}        
        
static BOOL DoRegisterClient(
    UCHAR *pId,         // @parm [OUT] - Receives identifier to pass in to KITLSend/Recv 
    char  *ServiceName, // @parm [IN]  - Service name (NULL terminated, up to MAX_SVCNAME_LEN chars)
    UCHAR Flags,        // @parm [IN]  - KITL_CFGFL_ flags, defined in ethdbg.h
    UCHAR WindowSize,   // @parm [IN]  - Protocol window size (default is 8)
    UCHAR *pBufferPool) // @parm [IN]  - Buffer pool to use for KITL packet buffers - must be WindowSize*2*1500 bytes, unless STOP_AND_WAIT flag is set, in which
                        //               case, need only be 2*1500 bytes. For default OS services, may be NULL.
{
    KITL_CLIENT *pClient = NULL;
    int i, iStart;

    if (!(KITLGlobalState & KITL_ST_ADAPTER_INITIALIZED))
        return FALSE;

    // Validate params
    if (!pId || !ServiceName || (strlen (ServiceName) > MAX_SVC_NAMELEN)) {
        KITLOutputDebugString("!DoRegisterClient(%s): Invalid parameter\n",ServiceName);
        return FALSE;
    }
    if (!pBufferPool) {
        KITLOutputDebugString("!DoRegisterClient(%s): Need to supply memory for buffer pool\n",ServiceName);
        return FALSE;
    }
    if (!WindowSize || (WindowSize > KITL_MAX_WINDOW_SIZE)) {
        KITLOutputDebugString("!DoRegisterClient(%s): Invalid window size %u\n",ServiceName, WindowSize);
        return FALSE;
    }

    // Determine where to start looking
    if ((i = ChkDfltSvc (ServiceName)) < 0) {
        i = HASH(ServiceName[0]);
    }
    iStart = i;

    // look for a slot in the client structure for the newly registered client
    do {
        if (!KITLClients[i]) {
            pClient = NewClient ((UCHAR) i, ServiceName, TRUE);
        } else if (strcmp (KITLClients[i]->ServiceName, ServiceName) == 0) {
            // same service has registered before
            if (!(KITLClients[i]->State & (KITL_CLIENT_REGISTERED|KITL_CLIENT_REGISTERING))) {
                pClient = KITLClients[i];
            } else if (!(Flags & KITL_CFGFL_MULTIINST)) {
                // duplicate registration -- fail the call
                break;
            }
        }

        if (pClient) {
            pClient->State |= KITL_CLIENT_REGISTERING;
        } else if (i < NUM_DFLT_KITL_SERVICES) {
            // no dup-registration for default services
            break;
        } else if (MAX_KITL_CLIENTS == ++ i) {
            i = NUM_DFLT_KITL_SERVICES;
        }
    } while (!pClient && (i != iStart));
    
    if (!pClient) {
        if (i == iStart) {
            KITLOutputDebugString("!KITLRegisterClient(%s): No available client structs\n",ServiceName);
        } else {
            KITLOutputDebugString("!KITLRegisterClient(%s): Client already registered (Id: %u)\n",ServiceName,i);
        }
        return FALSE;
    }

    // Set up client struct (ServiceId and State field are already valid)
    strcpy(pClient->ServiceName,ServiceName);
    pClient->State         |= KITL_CLIENT_REGISTERING;
    pClient->pTxBufferPool = MapPtr(pBufferPool);
    pClient->RxWinEnd      = WindowSize;
    pClient->WindowSize    = WindowSize;
    pClient->pRxBufferPool = pClient->pTxBufferPool + (WindowSize * KITL_MTU);
    pClient->CfgFlags      = Flags;

    // The service Id also functions as a client handle
    *pId = pClient->ServiceId;

    // KDBG is special case - it always runs with interrupts off and can't make sys calls,
    // so leave it in polling mode.
    if ((KITLGlobalState & KITL_ST_MULTITHREADED) && (pClient->ServiceId != KITL_SVC_KDBG))
        if (!RegisterClientPart2(*pId))
            return FALSE;

#if 0    
    //
    // we no long wait for client to connect to desktop during registration. The config exchange
    // will occur on the first KITLSend/KITLRecv call.
    //
    // Connect to desktop
    if (!ExchangeConfig(pClient))
        return FALSE;

    pClient->State &= ~KITL_CLIENT_REGISTERING;
    pClient->State |= KITL_CLIENT_REGISTERED;
#endif

    // waiting for config exchange now
    pClient->State |= KITL_WAIT_CFG;

    // just send a config message to the desktop. Okay if lost since we'll call into
    // Exchange config in case the client isn't connected when we call KITLSend/KITLRecv
    SendConfig (pClient, FALSE);
    
    return TRUE;
}

/* RegisterClientPart2
 *
 *  Finish initializing client data
 */
static BOOL
RegisterClientPart2(UCHAR Id)
{
    KITL_CLIENT *pClient = KITLClients[Id];
    CALLSTACK cstk;

    DEBUGCHK (pClient);
    KITL_DEBUGMSG(ZONE_INIT,("+RegisterClientPart2: Id 0x%X\n",Id));

    // if default clients switch to kernel, let kernel own all sync. objects in default clients
    if (IS_DFLT_SVC(Id))
        SwitchToKernel(&cstk);

    // Start retransmission timer thread if required
    if (!(KITLGlobalState & KITL_ST_TIMER_INIT)) {
        InitializeCriticalSection(&TimerListCS);
        if ((hTimerThread = CreateKernelThread((LPTHREAD_START_ROUTINE)TimerThread,0, (WORD)g_dwKITLTimerThreadPriority, 0)) == NULL) {
            DeleteCriticalSection(&TimerListCS);
            KITLOutputDebugString("Error creating timer thread\n");
            return FALSE;
        }
        KITLGlobalState |= KITL_ST_TIMER_INIT;
    }
    
    
    InitializeCriticalSection(&pClient->ClientCS);

    if (!(pClient->evCfg = CreateEvent(NULL,FALSE,FALSE,NULL))) {
        DeleteCriticalSection(&pClient->ClientCS);
        KITLOutputDebugString("!RegisterClient: Error in CreateEvent()\n");
        return FALSE;
    }
    if (!(pClient->evRecv = CreateEvent(NULL,FALSE,FALSE,NULL))) {
        DeleteCriticalSection(&pClient->ClientCS);
        CloseHandle(pClient->evCfg);
        KITLOutputDebugString("!RegisterClient: Error in CreateEvent()\n");
        return FALSE;
    }
    
    // Manual reset event, initially signalled
    if (!(pClient->evTxFull = CreateEvent(NULL,TRUE,TRUE,NULL))) {
        DeleteCriticalSection(&pClient->ClientCS);
        CloseHandle(pClient->evCfg);
        CloseHandle(pClient->evRecv);
        KITLOutputDebugString("!RegisterClient: Error in CreateEvent()\n");
        return FALSE;
    }

    // Set up process permissions, for accessing buffers. Only need to do this
    // for non-default services.
    if (!IS_DFLT_SVC(Id))
        pClient->ProcPerms = SC_GetCurrentPermissions();
    else
        SwitchBack();
    
    // If we need to turn on HW interrupts, do so now
    //if ((KITLGlobalState & KITL_ST_IST_STARTED) && !(KITLGlobalState & KITL_ST_INT_ENABLED))
    //    EnableInts();
    
    pClient->State |= KITL_USE_SYSCALLS;

    KITL_DEBUGMSG(ZONE_INIT,("-RegisterClientPart2\n"));
    return TRUE;
}

/* 
 * @func  BOOL | KITLDeregisterClient | Deregister debug Ethernet (KITL) client.
 * @rdesc Return TRUE if Id is a valid KITL client Id.
 * @comm  This function will deregister an KITL client.  Note that 
 *        no indication is given to the peer that we have disconnected, so if an 
 *        application needs to send this information, it must be done at a higher layer.
 * @xref  <f KITLRegisterClient>
 */
BOOL
KITLDeregisterClient(
    UCHAR Id)   // @parm [IN] - KITL client id.
{
    KITL_CLIENT *pClient;
    BOOL fRet = FALSE;
    UCHAR State;

    KITL_DEBUGMSG(ZONE_INIT,("+KITLDeregisterClient, Id:%u\n", Id));
    DEBUGCHK (!InSysCall ());
    if (!IS_VALID_ID(Id)) {
        KITL_DEBUGMSG(ZONE_WARNING, ("KITLDeregisterClient: invalid ID[%u]\n", Id));
        return FALSE;
    }

    EnterCriticalSection (&KITLcs);

    if ((pClient = KITLClients[Id]) != NULL) {
        EnterCriticalSection(&pClient->ClientCS);
        if (pClient->State & (KITL_CLIENT_REGISTERED | KITL_CLIENT_REGISTERING)) {

            DEBUGCHK (pClient->ServiceId == Id);

            // reset state before setting the events so the waiting 
            // thread knows that the client deregistered
            State = pClient->State;
            pClient->State = 0;
            ResetClientState (pClient);
            if (State & KITL_USE_SYSCALLS) {
                SetClientEvent(pClient,pClient->evCfg);
                SetClientEvent(pClient,pClient->evRecv);
            }

            if (pClient->evCfg)     CloseHandle(pClient->evCfg);
            if (pClient->evRecv)    CloseHandle(pClient->evRecv);
            if (pClient->evTxFull)  CloseHandle(pClient->evTxFull);
            // DeleteCriticalSection(&pClient->ClientCS);
            // pClient->ServiceName[0] = 0;
            fRet = TRUE;
        } else {
            KITL_DEBUGMSG(ZONE_WARNING, ("KITLDeregisterClient[%u]-already deregistered\n", Id));
        }
        LeaveCriticalSection(&pClient->ClientCS);
    }
    LeaveCriticalSection (&KITLcs);

    KITL_DEBUGMSG(ZONE_INIT,("-KITLDeregisterClient, Id:%u ret %d\n",Id, fRet));
    return fRet;
}


/* KITLInitializeInterrupt
 *
 *   This function is called by the kernel when the system is able to handle interrupts.
 *   Start our ISR thread, and unmask the HW interrupts.
 */
BOOL
KITLInitializeInterrupt()
{
    int i;

    if (!(KITLGlobalState & KITL_ST_ADAPTER_INITIALIZED))
        return FALSE;
    
    // Check if we are coming up for the first time, or resuming interrupts (e.g. when coming
    // back from OEMPowerOff)
    if (KITLGlobalState & KITL_ST_MULTITHREADED) {
        // Just enable interrupt and return
        EnableInts();
        return TRUE;
    }
    
    KITLOutputDebugString("KITL: Leaving polling mode...\n");

    InitializeCriticalSection(&KITLODScs);
    InitializeCriticalSection(&KITLKCallcs);

    KITLGlobalState |= KITL_ST_MULTITHREADED;
    
    KITL_DEBUGMSG(ZONE_INIT,("KITL Checking client registrations\n"));
    // Some clients may have registered already, finish initialization now that
    // the system is up. KDBG continues to run in polling mode.
    for (i=0; i< MAX_KITL_CLIENTS; i++) {
        if (KITLClients[i] && (i != KITL_SVC_KDBG)
            && (KITLClients[i]->State & (KITL_CLIENT_REGISTERED|KITL_CLIENT_REGISTERING))) {
            if (!RegisterClientPart2((UCHAR)i))
                return FALSE;
        }
    }

    // Start interrupt thread. If we have clients registered, also turn on receive interrupts
    // from the ethernet controller, otherwise leave them disabled. 
    if ((UCHAR) KITL_SYSINTR_NOINTR != Kitl.Interrupt) {
        KITL_DEBUGMSG(ZONE_INIT,("KITL Creating IST\n"));
        if ((hIntrThread = CreateKernelThread((LPTHREAD_START_ROUTINE)KITLInterruptThread,
                                              NULL, (WORD)g_dwKITLThreadPriority, 0)) == NULL) {
            KITLOutputDebugString("Error creating interrupt thread\n");
            return FALSE;
        }
    }

    return TRUE;
}

static BOOL ChkCnxDsktp (LPVOID pUnused)
{
    return (KITLGlobalState & KITL_ST_DESKTOP_CONNECTED);
}

static BOOL TranCnxDsktp (LPVOID pParam, BOOL fUseSysCalls)
{
    return KitlSendFrame (PpfsFmtBuf, (WORD) (DWORD) pParam);
}

//
// KITLConnectToDesktop: Exchanging transport information with desktop
//
//      This function calls the transport function to retrieve device transport info,
//      send it to desktop, getting the transport infor from the desktop, and tell 
//      the device transport about the desktop transport info.
//
static BOOL KITLConnectToDesktop (void)
{
    // we'll use the PpfsFmtBuf for send buffer since ppfs can't be started yet at this stage
    // 
    PKITL_HDR pHdr = (PKITL_HDR) (PpfsFmtBuf + Kitl.FrmHdrSize);
    PKITL_DEV_TRANSCFG pCfg = (PKITL_DEV_TRANSCFG) KITLDATA(pHdr);
    USHORT cbData = sizeof (PpfsFmtBuf) - Kitl.FrmHdrSize - sizeof (KITL_HDR) - sizeof (KITL_DEV_TRANSCFG);

    if (!Kitl.pfnGetDevCfg ((LPBYTE) (pCfg+1), &cbData))
        return FALSE;

    memset (pHdr, 0, sizeof (KITL_HDR));
    pHdr->Id = KITL_ID;
    pHdr->Service = KITL_SVC_ADMIN;
    pHdr->Cmd = KITL_CMD_TRAN_CONFIG;
    cbData += sizeof (KITL_DEV_TRANSCFG);
    pCfg->wLen = cbData;
    pCfg->wCpuId = KITL_CPUID;
    memcpy (pCfg->szDevName, Kitl.szName, KITL_MAX_DEV_NAMELEN);
    cbData += sizeof (KITL_HDR);

    return KitlSendFrame (PpfsFmtBuf, cbData)
        && KITLPollResponse (FALSE, ChkCnxDsktp, TranCnxDsktp, (LPVOID) cbData);
}

static BOOL StartKitl (BOOL fInit)
{
    // KITL already started?
    if (!fInit && (KITLGlobalState & KITL_ST_DESKTOP_CONNECTED)) {
        return TRUE;
    }

    /*
     * When this function is called, the kernel hasn't yet been initialized, 
     * so can't make any system calls.  Once the system has come up far
     * enough to handle system calls, KITLInitializeInterrupt() is called to complete
     * initialization.  This is indicated by the KITL_ST_MULTITHREADED flag in KITLGlobalState.
     */
    // Detect/initialize ethernet hardware, and return address information
    if (!OEMKitlInit (&Kitl))
        return FALSE;

    // verify that the Kitl structure is initialized.
    if (!Kitl.pfnDecode || !Kitl.pfnEncode || !Kitl.pfnEnableInt || !Kitl.pfnRecv || !Kitl.pfnSend
        || !Kitl.dwPhysBuffer || !Kitl.dwPhysBufLen || !Kitl.WindowSize || !Kitl.pfnGetDevCfg || !Kitl.pfnSetHostCfg) {
        return FALSE;
    }

    // Validate that address is not in free RAM area - the HAL should put it in a reserved
    // section of memory conditional on some environment var.
    if ((pTOC->ulRAMStart < Kitl.dwPhysBuffer + Kitl.dwPhysBufLen) 
        && (pTOC->ulRAMEnd > Kitl.dwPhysBuffer)) {
        KITLOutputDebugString("\r\n!Debug Ethernet packet buffers in free RAM area - must set IMGEBOOT=1\r\n");
        return FALSE;
    }

    if (Kitl.dwPhysBufLen < (DWORD) 3 * KITL_BUFFER_POOL_SIZE) {
        KITLOutputDebugString("\r\n!Debug Ethernet buffer size too small, must be at least 0x%x bytes (3 * WindowSize * 2 * KITL_MTU)\r\n",
            3 * KITL_BUFFER_POOL_SIZE);
        return FALSE;
    }

    KITLGlobalState |= KITL_ST_KITLSTARTED; // indicate (to kdstub) that KITL has started

    // If the initialized flag is already set, we are being called from the power on routine,
    // so reinit the HW, but not any state.
    if (!(KITLGlobalState & KITL_ST_ADAPTER_INITIALIZED)) {
        // perform the initial handshake with the desktop
        if (!KITLConnectToDesktop ()) {
            KITLOutputDebugString ("\r\n!Unable to establish KITL connection with desktop!\r\n");
            return FALSE;
        }
        
        // Set up kernel function pointers
        pKITLInitializeInterrupt = KITLInitializeInterrupt;
        pKITLSend = KITLSend;
        pKITLRecv = KITLRecv;

        KITLGlobalState |= KITL_ST_ADAPTER_INITIALIZED;

        if (Kitl.dwBootFlags & KITL_FL_DBGMSG)
            SetKernelCommDev (KERNEL_SVC_DBGMSG, KERNEL_COMM_ETHER);
        if (Kitl.dwBootFlags & KITL_FL_PPSH)
            SetKernelCommDev (KERNEL_SVC_PPSH, KERNEL_COMM_ETHER);
        if (Kitl.dwBootFlags & KITL_FL_KDBG)
            SetKernelCommDev (KERNEL_SVC_KDBG, KERNEL_COMM_ETHER);

        // only perform cleanboot if it's connected at boot. Cleanboot flag is
        // ignored if it's started dynamically.
        if (fInit && (Kitl.dwBootFlags & KITL_FL_CLEANBOOT)) {
            extern ROMHDR *const volatile pTOC;     // Gets replaced by RomLoader with real address
            // just clear the magic number (see SC_SetCleanRebootFlag)
            // NOTE: We can NOT call SC_SetCleanRebootFlag here since logPtr isn't
            // initialized yet.
            ((fslog_t *)((pTOC->ulRAMFree + MemForPT) | 0x20000000))->magic1 = 0;
        }
        // if OEM calls KitlInit (FALSE), KITLInitializeInterrupt will
        // not be called in SystemStartupFuc. We need to initialize
        // interrupt here (when RegisterClient is called)
        if (fKITLcsInitialized && !InSysCall ()) {
            KITLInitializeInterrupt ();
        }
    }

    LOG (KITLGlobalState);
    return TRUE;
}


// KITLIoctl:
//  return TRUE: success
//         FALSE: failed
//         ERROR_NO_SUPPORT: unsupported control code
DWORD KITLIoctl (DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned)
{
    DWORD retval;

    switch (dwIoControlCode) {
    case IOCTL_EDBG_REGISTER_CLIENT:
        retval = (KITLRegisterClient(lpInBuf, lpOutBuf, (UCHAR)nInBufSize, (UCHAR)nOutBufSize, (UCHAR *)lpBytesReturned));
        break;

    case IOCTL_EDBG_DEREGISTER_CLIENT:
        retval = (KITLDeregisterClient((UCHAR)nInBufSize));
        break;

    case IOCTL_EDBG_REGISTER_DFLT_CLIENT:
        retval = (KITLRegisterDfltClient((UCHAR)nInBufSize, (UCHAR)nOutBufSize, (UCHAR **)lpInBuf, (UCHAR **)lpOutBuf));
        break;

    case IOCTL_EDBG_SEND:
        retval = (KITLSend((UCHAR)nInBufSize, lpInBuf, nOutBufSize));
        break;

    case IOCTL_EDBG_RECV:
        retval = (KITLRecv((UCHAR)nInBufSize, lpInBuf, lpOutBuf, nOutBufSize));
        break;

    case IOCTL_EDBG_SET_DEBUG:
        KITLSetDebug(nInBufSize);
        retval = 1;
        break;

    case IOCTL_EDBG_GET_OUTPUT_DEBUG_FN:
        retval = 0;
        if (nOutBufSize >= sizeof(VOID*)) {
            *(VOID**)lpOutBuf = (VOID*)KITLOutputDebugString;
            retval = 1;
        }
        break;

    case IOCTL_EDBG_IS_STARTED:
	    retval = (KITLGlobalState & KITL_ST_KITLSTARTED) ? 1 : 0; // indicate (to kdstub) that KITL has started
	    break;
        
    default:
        return ERROR_NOT_SUPPORTED;
    }
    return retval;

}


/* 
 * KITLInit | Initialize debug Ethernet (KITL) subsystem.
 * 
 * This function is called from the HAL to initialize the structures and hardware
 * used for debug Ethernet routines.  It must be called before any other
 * KITL functions are called, or kernel services are redirected to Ethernet
 * via SetKernelCommDev().  This function may be called very early in
 * system initialization, such as in OEMInit().  It should also be called
 * any time the debug Ethernet hardware needs to be reinitialized, such as
 * returning from OEMPowerOff().
 *
 * Return Value:
 *   Return TRUE if initialization was successful, FALSE if an error occurred.
 */

extern BOOL (*pfnIsDesktopDbgrExist) ();
extern DWORD (* pKITLIoCtl) (DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned);

BOOL KitlInit (BOOL fStartKitl)
{
    // just initialize function pointers
    pKITLRegisterDfltClient  = KITLRegisterDfltClient;
    pKITLIoCtl = KITLIoctl;
    pfnIsDesktopDbgrExist = IsDesktopDbgrExist;

    // Initialize default clients
    NewClient (KITL_SVC_DBGMSG, KITL_SVCNAME_DBGMSG, FALSE);
    NewClient (KITL_SVC_PPSH,   KITL_SVCNAME_PPSH,   FALSE);
    NewClient (KITL_SVC_KDBG,   KITL_SVCNAME_KDBG,   FALSE);


    return fStartKitl? StartKitl (TRUE) : TRUE;
}

static void EnableInts()
{
    // Some platforms may not have available resources for an interrupt, in
    // which case we just continue to run in polling mode.
    if ((UCHAR) KITL_SYSINTR_NOINTR != Kitl.Interrupt) {
        KITL_DEBUGMSG(ZONE_INIT,("Enabling adapter ints...\n"));
        KCall((PKFN)Kitl.pfnEnableInt, TRUE);
        KITLGlobalState |= KITL_ST_INT_ENABLED;
    }
}

static DWORD KITLInterruptThread (DWORD Dummy)
{
    HANDLE hIntEvent;
    DWORD dwRet;

    KITL_DEBUGMSG(ZONE_INIT,("KITL Interrupt thread started (hTh: 0x%X, pTh: 0x%X), using SYSINTR %u\n",
                             hCurThread,pCurThread, Kitl.Interrupt));

    pCurThread->bDbgCnt = 1;   // no entry messages
    
    if ((hIntEvent = CreateEvent(0,FALSE,FALSE,EDBG_INTERRUPT_EVENT)) == NULL) {
        KITLOutputDebugString("KITL CreateEvent failed!\n");
        return 0;
    }
    if (!SC_InterruptInitialize(Kitl.Interrupt, hIntEvent, NULL,0)) {
        CloseHandle(hIntEvent);
        KITLOutputDebugString("KITL InterruptInitialize failed\n");
        return 0;
    }

    // always enable interrupt as long as OEM told us so
    EnableInts();
    
    KITLGlobalState |= KITL_ST_IST_STARTED;
    
    while ((dwRet = SC_WaitForMultiple (1,&hIntEvent,0,INFINITE)) == WAIT_OBJECT_0) {

        KITL_DEBUGLED(LED_IST_ENTRY,0);
        KITL_DEBUGMSG(ZONE_INTR,("KITL Interrupt event\n"));

        // no need to check pending, just call HandleRecvInterrupts because it'll
        // just return if there is no interrupt pending
        HandleRecvInterrupt(ISTRecvBuf,TRUE, NULL, NULL);

        SC_InterruptDone(Kitl.Interrupt);
        
        KITL_DEBUGMSG(ZONE_INTR,("Processed Interrupt event\n"));
    }
    KITLOutputDebugString("!KITL Interrupt thread got error in WaitForMultipleObjects: dwRet:%u, GLE:%u\n",
                          dwRet,GetLastError());
    return 0;
}

