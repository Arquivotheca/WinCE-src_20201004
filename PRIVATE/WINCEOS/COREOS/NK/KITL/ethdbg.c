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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
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
#include "kernel.h"
#include "kitlp.h"

struct KDataStruct *g_pKData;
PPROCESS g_pprcNK;

KITLPRIV g_kpriv;

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

#define IsSystemReady()     (KITLGlobalState & KITL_ST_SYSTEMSTARTED)

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
static UCHAR KdbgFmtBuf[(KITL_MAX_DATA_SIZE+3)&~3];  // make it DWORD aligned


// Receive buffers for ISR routine.  Need two, because we may have filled a buffer in the
// ISR, then get swapped out and another thread may need to do polled I/O.
static UCHAR ISTRecvBuf[KITL_MTU];
UCHAR SendFmtBuf[KITL_MTU];  // make it DWORD aligned

KITLTRANSPORT Kitl;

// To prevent interleaving of our debug output
CRITICAL_SECTION KITLODScs;

// cs to pretect timer list
CRITICAL_SECTION TimerListCS;

// critical section in place of KCall
CRITICAL_SECTION KITLKCallcs;

#define KITL_BUFFER_POOL_SIZE  (KITL_MAX_WINDOW_SIZE * 2 * KITL_MTU)

// buffer for default clients, must be DWORD aligned
static DWORD g_KitlBuffer[((KITL_BUFFER_POOL_SIZE * NUM_DFLT_KITL_SERVICES) + (sizeof(DWORD) - 1)) / sizeof(DWORD)];

// cs to protect client list
CRITICAL_SECTION KITLcs;
void ResetClientState(KITL_CLIENT *pClient);

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
void KITLSetDebug (DWORD DebugZoneMask)
{
#ifdef DEBUG
    KITLOutputDebugString("Changed KITL zone mask to 0x%X\n",DebugZoneMask);
    KITLDebugZone = DebugZoneMask;
#endif
}

BOOL IsDesktopDbgrExist (void)
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
    if (strlen(pszSvcName) >= MAX_SVC_NAMELEN)
        return NULL;

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

    if (IsSystemReady() && !InSysCall ()) {
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
    if (IsSystemReady () && !InSysCall ()) {
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

    DEBUGMSG (pBufferPool, (L"KITLRegisterClient: pBufferPool %8.8lx is no longer used. specify NULL\r\n", pBufferPool));

    if (pBufferPool = VMAlloc (g_pprcNK, NULL, 2 * KITL_MTU * WindowSize, MEM_COMMIT, PAGE_READWRITE)) {

        EnterCriticalSection (&KITLcs);

        if (StartKitl (FALSE)) 
            fRet = DoRegisterClient (pId, ServiceName, Flags, WindowSize, pBufferPool);        
        
        LeaveCriticalSection (&KITLcs);
    }
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
        KITLOutputDebugString("!DoRegisterClient(%s): Invalid parameter\n",ServiceName?ServiceName: "NULL");
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
    pClient->pTxBufferPool = pBufferPool;
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
    HANDLE   evCfg, evRecv, evTxFull;
    BOOL     fRet;
    PPROCESS pprc = pActvProc;

    DEBUGCHK (pClient);
    KITL_DEBUGMSG(ZONE_INIT,("+RegisterClientPart2: Id 0x%X\n",Id));

    // Start retransmission timer thread if required
    if (!(KITLGlobalState & KITL_ST_TIMER_INIT)) {
        HANDLE hTh;
        InitializeCriticalSection (&TimerListCS);
        if ((hTh = CreateKernelThread((LPTHREAD_START_ROUTINE)TimerThread,0, (WORD)g_dwKITLTimerThreadPriority, 0)) == NULL) {
            DeleteCriticalSection (&TimerListCS);
            KITLOutputDebugString("Error creating timer thread\n");
            return FALSE;
        }
        KITLOutputDebugString("Closing Handle of Timer Thread\n");
        HNDLCloseHandle (g_pprcNK, hTh);
        KITLGlobalState |= KITL_ST_TIMER_INIT;
    }

    evCfg    = NKCreateEvent (NULL, FALSE, FALSE, NULL);
    evRecv   = NKCreateEvent (NULL, FALSE, FALSE, NULL);
    evTxFull = NKCreateEvent (NULL, TRUE, TRUE, NULL);

    if (fRet = (evCfg && evRecv && evTxFull)) {
        pClient->phdCfgEvt    = LockHandleData (evCfg,    pprc);
        pClient->phdRecvEvt   = LockHandleData (evRecv,   pprc);
        pClient->phdTxFullEvt = LockHandleData (evTxFull, pprc);

        InitializeCriticalSection (&pClient->ClientCS);
        
        // If we need to turn on HW interrupts, do so now
        //if ((KITLGlobalState & KITL_ST_IST_STARTED) && !(KITLGlobalState & KITL_ST_INT_ENABLED))
        //    EnableInts();
        
        pClient->State |= KITL_USE_SYSCALLS;
        
    }

    // close all the handle even when we succeed, for we've already locked the handles and they
    // won't be destroyed.
    if (evCfg) {
        HNDLCloseHandle (pprc, evCfg);
    }
    if (evRecv) {
        HNDLCloseHandle (pprc, evRecv);
    }
    if (evTxFull) {
        HNDLCloseHandle (pprc, evTxFull);
    }

    KITL_DEBUGMSG(ZONE_INIT,("-RegisterClientPart2 returns %d\n", fRet));
    return fRet;
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
    PHDATA phd;

    KITL_DEBUGMSG(ZONE_INIT,("+KITLDeregisterClient, Id:%u\n", Id));
    DEBUGCHK (!InSysCall ());
    if (!IS_VALID_ID(Id)) {
        KITL_DEBUGMSG(ZONE_WARNING, ("KITLDeregisterClient: invalid ID[%u]\n", Id));
        return FALSE;
    }

    EnterCriticalSection (&KITLcs);

    if ((pClient = KITLClients[Id]) != NULL) {
        EnterCriticalSection (&pClient->ClientCS);
        
        if (pClient->State & (KITL_CLIENT_REGISTERED | KITL_CLIENT_REGISTERING)) {

            DEBUGCHK (pClient->ServiceId == Id);

            // reset state before setting the events so the waiting
            // thread knows that the client deregistered
            State = pClient->State;
            pClient->State = 0;
            ResetClientState (pClient);
            if (State & KITL_USE_SYSCALLS) {
                EVNTModify (GetObjPtr (pClient->phdCfgEvt), EVENT_SET);
                EVNTModify (GetObjPtr (pClient->phdRecvEvt), EVENT_SET);
            }

            if (phd = pClient->phdCfgEvt) {
                pClient->phdCfgEvt = NULL;
                UnlockHandleData (phd);
            }
            if (phd = pClient->phdRecvEvt) {
                pClient->phdRecvEvt = NULL;
                UnlockHandleData (phd);
            }
            if (phd = pClient->phdTxFullEvt) {
                pClient->phdTxFullEvt = NULL;
                UnlockHandleData (phd);
            }

            // release memory
            if (!IS_DFLT_SVC (Id)) {
                VMFreeAndRelease (g_pprcNK, pClient->pTxBufferPool, 2 * KITL_MTU * pClient->WindowSize);
            }
            
            // DeleteCriticalSection(&pClient->ClientCS);
            // pClient->ServiceName[0] = 0;
            fRet = TRUE;
        } else {
            KITL_DEBUGMSG(ZONE_WARNING, ("KITLDeregisterClient[%u]-already deregistered\n", Id));
        }
        LeaveCriticalSection (&pClient->ClientCS);
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

    if ((UCHAR) KITL_SYSINTR_NOINTR != Kitl.Interrupt) {
        KITLOutputDebugString("KITL: Leaving polling mode... 0x%x\n", &KITLODScs);
    }

    InitializeCriticalSection (&KITLODScs);
    InitializeCriticalSection (&KITLKCallcs);

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
        HANDLE hTh;
        KITL_DEBUGMSG(ZONE_INIT,("KITL Creating IST\n"));
        if ((hTh = CreateKernelThread((LPTHREAD_START_ROUTINE)KITLInterruptThread,
                                              NULL, (WORD)g_dwKITLThreadPriority, 0)) == NULL) {
            KITLOutputDebugString("Error creating interrupt thread\n");
            return FALSE;
        }
        HNDLCloseHandle (g_pprcNK, hTh);
    }

    return TRUE;
}

static BOOL ChkCnxDsktp (LPVOID pUnused)
{
    return (KITLGlobalState & KITL_ST_DESKTOP_CONNECTED);
}

static BOOL TranCnxDsktp (LPVOID pParam, BOOL fUseSysCalls)
{
    KITLOutputDebugString ("Connecting to Desktop .. resending\r\n");
    return KitlSendFrame (SendFmtBuf, (WORD) (DWORD) pParam);
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
    // we'll use the SendFmtBuf for send buffer since ppfs can't be started yet at this stage
    //
    PKITL_HDR pHdr = (PKITL_HDR) (SendFmtBuf + Kitl.FrmHdrSize);
    PKITL_DEV_TRANSCFG pCfg = (PKITL_DEV_TRANSCFG) KITLDATA(pHdr);
    USHORT cbData = sizeof (SendFmtBuf) - Kitl.FrmHdrSize - sizeof (KITL_HDR) - sizeof (KITL_DEV_TRANSCFG);

    KITLOutputDebugString ("Connecting to Desktop\r\n");
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

    return KitlSendFrame (SendFmtBuf, cbData)
        && KITLPollResponse (FALSE, ChkCnxDsktp, TranCnxDsktp, (LPVOID) cbData);
}


//------------------------------------------------------------------------------
// Print debug message using debug Ethernet card
//------------------------------------------------------------------------------
void
KITLWriteDebugString(
    LPCWSTR str
    ) 
{
    UCHAR FmtBuf[256+sizeof(KITL_DBGMSG_INFO)];
    KITL_DBGMSG_INFO *pDbgInfo = (KITL_DBGMSG_INFO *)FmtBuf;
    // Prepend a header so that application on the other side can get timing
    // and thread info.
    pDbgInfo->dwLen      = sizeof(KITL_DBGMSG_INFO);
    pDbgInfo->dwThreadId = dwCurThId;
    pDbgInfo->dwProcId   = dwActvProcId;
    pDbgInfo->dwTimeStamp = CurMSec;
    NKUnicodeToAscii(FmtBuf+sizeof(KITL_DBGMSG_INFO),str,256);
    KITLSend(KITL_SVC_DBGMSG, FmtBuf, sizeof(KITL_DBGMSG_INFO)+strlen(FmtBuf+sizeof(KITL_DBGMSG_INFO))+1);
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
    if (!Kitl.pfnDecode || !Kitl.pfnEncode || !Kitl.pfnRecv || !Kitl.pfnSend || !Kitl.pfnGetDevCfg || !Kitl.pfnSetHostCfg) {
        return FALSE;
    }

    // pfnEnableInt can not be null if using interrupt
    if (((UCHAR) KITL_SYSINTR_NOINTR != Kitl.Interrupt) && !Kitl.pfnEnableInt) {
        return FALSE;
    }

    if (Kitl.dwPhysBuffer || Kitl.dwPhysBufLen) {
        KITLOutputDebugString("\r\n!Kitl buffer specified by OAL is not required, ignore...\r\n");
    }

    Kitl.dwPhysBuffer = (DWORD)g_KitlBuffer;
    //Kitl.dwPhysBufLen = sizeof(g_KitlBuffer);
    Kitl.WindowSize = KITL_MAX_WINDOW_SIZE;

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
        KITLGlobalState |= KITL_ST_ADAPTER_INITIALIZED;

        if ((Kitl.dwBootFlags & KITL_FL_DBGMSG)
            && KITLRegisterDfltClient (KITL_SVC_DBGMSG, 0, NULL, NULL)) {
            g_pNKGlobal->pfnWriteDebugString = KITLWriteDebugString;
        }
        if (Kitl.dwBootFlags & KITL_FL_PPSH) {
            KITLRegisterDfltClient (KITL_SVC_PPSH, 0, NULL, NULL);
        }

        // only perform cleanboot if it's connected at boot. Cleanboot flag is
        // ignored if it's started dynamically.
        if (fInit && (Kitl.dwBootFlags & KITL_FL_CLEANBOOT)) {
            NKForceCleanBoot();
        }
        // if OEM calls KitlInit (FALSE), KITLInitializeInterrupt will
        // not be called in SystemStartupFuc. We need to initialize
        // interrupt here (when RegisterClient is called)
        if (IsSystemReady () && !InSysCall ()) {
            KITLInitializeInterrupt ();
        }
    }

    return TRUE;
}


//copies data from/to a user pointer
static int MemCopyUserPtr(void *userPtr, void* localPtr, size_t count, BOOL copyToUserPtr)
{

    int bytesCopied=0;
    
    //try to do memcpy
    __try{
        //check to see if it's a valid user ptr.
        if(IsValidUsrPtr(userPtr, count, copyToUserPtr))
        {        
            if(copyToUserPtr)
            {
                memcpy(userPtr, localPtr, count);                
            }
            else
            {
                memcpy(localPtr, userPtr, count);                        
            }
            bytesCopied = count;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    return bytesCopied;
}

static int MemCopyToUserPtr(void *userPtr, void* localPtr, size_t count)
{
    return MemCopyUserPtr(userPtr, localPtr, count, TRUE);
 
}

static int MemCopyFromUserPtr(void *userPtr, void* localPtr, size_t count)
{
    return MemCopyUserPtr(userPtr, localPtr, count, FALSE);
}

// ExtKITLIoctl:  Entry point for user level apps making kitl calls.  We must validate the parameters the user passes in.
//  return TRUE: success
//         FALSE: failed
//         ERROR_NO_SUPPORT: unsupported control code
BOOL ExtKITLIoctl (DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned)
{    
    BOOL retval = FALSE;
    
    switch (dwIoControlCode) {        
    case IOCTL_EDBG_REGISTER_CLIENT:
        {            
            PEDBG_REGISTER_CLIENT_IN pRegClientIn = (PEDBG_REGISTER_CLIENT_IN)lpInBuf;
            UCHAR *pOutId = (UCHAR *)lpOutBuf;  //output buffer to place the Id in.

            UCHAR Id = 0;  //Local copy of Id
            DWORD dwServiceNameLen = 0;
            UCHAR pszServiceName[MAX_SVC_NAMELEN];

            retval = FALSE;
            __try {
                
                //Validate the arguments
                if (IsValidUsrPtr(pRegClientIn->szServiceName, 1, FALSE)) {
                    dwServiceNameLen = strlen((LPCSTR)pRegClientIn->szServiceName);

                    if ((dwServiceNameLen < MAX_SVC_NAMELEN) &&
                          MemCopyFromUserPtr(pRegClientIn->szServiceName, pszServiceName, dwServiceNameLen)) {
                            //null terminate string.
                            pszServiceName[dwServiceNameLen] = 0;
                            retval = TRUE;
                    }

                }
            } __except (EXCEPTION_EXECUTE_HANDLER) {
            }

            if(!retval){
                NKSetLastError(ERROR_INVALID_PARAMETER);
                return retval;
            }

            retval = (KITLRegisterClient(&Id, pszServiceName, pRegClientIn->Flags, pRegClientIn->WindowSize, NULL));

            if (retval) {                
                if (!MemCopyToUserPtr(pOutId, &Id, sizeof(Id))) {
                    //failed to copy id to user -- deregister the client
                    retval = FALSE;
                    KITLDeregisterClient(Id);
                    NKSetLastError(ERROR_INVALID_PARAMETER);
                }
            }                       
        }
        break;

    case IOCTL_EDBG_DEREGISTER_CLIENT:
        {
            UCHAR Id = *(UCHAR*)lpInBuf;        
            retval = (KITLDeregisterClient(Id));
        }
        break;

    case IOCTL_EDBG_REGISTER_DFLT_CLIENT:
        {
            
            PEDBG_REGISTER_DFLT_CLIENT_IN pRegDfltClientIn = (PEDBG_REGISTER_DFLT_CLIENT_IN)lpInBuf;
            retval = (KITLRegisterDfltClient(pRegDfltClientIn->Service, pRegDfltClientIn->Flags, NULL, NULL));
        }
        break;

    case IOCTL_EDBG_SEND:
        {
            PEDBG_SEND_IN pEdbgSend = (PEDBG_SEND_IN)lpInBuf;

            UCHAR pInBuf[KITL_MAX_DATA_SIZE];
            retval = FALSE;

            //Verify that the data the user passed in is valid
            if ((pEdbgSend->dwUserDataLen <= KITL_MAX_DATA_SIZE) &&
                MemCopyFromUserPtr(pEdbgSend->pUserData, pInBuf, pEdbgSend->dwUserDataLen)) 
            {
                retval = TRUE;
            }

            if (!retval) {
                NKSetLastError(ERROR_INVALID_PARAMETER);            
                return retval;
            }

            retval = (KITLSend(pEdbgSend->Id, pInBuf, pEdbgSend->dwUserDataLen));

        }        
        break;

    case IOCTL_EDBG_RECV:
        {
            PEDBG_RECV_IN pEdbgRecvIn = (PEDBG_RECV_IN)lpInBuf;
            PEDBG_RECV_OUT pEdbgRecvOut = (PEDBG_RECV_OUT)lpOutBuf;
            
            UCHAR pOutBuf[KITL_MAX_DATA_SIZE];
            DWORD dwOutBufLen;
            retval = FALSE;

            //validate if len is valid
            if (MemCopyFromUserPtr(pEdbgRecvOut->pdwLen, &dwOutBufLen, sizeof(dwOutBufLen)))
            {
                    /*  The buffer length is valid -- Now validate the buffer.
                      *  We copy junk into the pRecvBuf to verify that the entire buffer is writable.
                      *  If we called KITLRecv, then wrote to the buffer, and it failed, we would screw up the
                      *  the state of the kitl protocol.  This minimizes the chance of this situation.
                      */
                    if ((dwOutBufLen <= KITL_MAX_DATA_SIZE) &&
                        MemCopyToUserPtr(pEdbgRecvOut->pRecvBuf, pOutBuf, dwOutBufLen))
                    {
                        retval = TRUE;
                    }
            }
                
            if (!retval) {
                NKSetLastError(ERROR_INVALID_PARAMETER);
                return retval;
            }
            
            retval = (KITLRecv(pEdbgRecvIn->Id, pOutBuf,  &dwOutBufLen, pEdbgRecvIn->dwTimeout));

            if (retval && dwOutBufLen) {
                //copy data back to user buffer
                if((dwOutBufLen <= KITL_MAX_DATA_SIZE) &&
                    MemCopyToUserPtr(pEdbgRecvOut->pdwLen, &dwOutBufLen, sizeof(dwOutBufLen)) &&
                    MemCopyToUserPtr(pEdbgRecvOut->pRecvBuf, pOutBuf, dwOutBufLen)){
                         retval = TRUE;
                }else{
                        NKSetLastError(ERROR_INVALID_PARAMETER);
                        retval = FALSE;                                
                }
            }
        }          
        break;

    case IOCTL_EDBG_SET_DEBUG:
        {
            DWORD dwZoneMask = *(DWORD *)lpInBuf;
            KITLSetDebug(dwZoneMask);
            retval = TRUE;
        }
        break;

    default:
        retval = OEMKitlIoctl (dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned);
        break;
    }

    return retval;

}

// KITLIoctl:
//  return TRUE: success
//         FALSE: failed
//         ERROR_NO_SUPPORT: unsupported control code
BOOL KITLIoctl (DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned)
{
    BOOL retval = FALSE;

    switch (dwIoControlCode) {
    case IOCTL_KITL_STARTUP:
        retval = OEMKitlStartup ();
        break;
        
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
        retval = TRUE;
        break;

    case IOCTL_EDBG_GET_OUTPUT_DEBUG_FN:
        if (nOutBufSize >= sizeof(VOID*)) {
            *(VOID**)lpOutBuf = (VOID*)KITLOutputDebugString;
            retval = TRUE;
        }
        break;

    case IOCTL_EDBG_IS_STARTED:
	    retval = ((KITLGlobalState & KITL_ST_KITLSTARTED) != 0); // indicate (to kdstub) that KITL has started
	    break;

    case IOCTL_KITL_IS_KDBG_REGISTERED:
        retval = IsDesktopDbgrExist ();
        break;
        
    case IOCTL_KITL_INTRINIT:
        retval = KITLInitializeInterrupt ();
        break;

    case IOCTL_KITL_POWER_CALL:
        if (lpInBuf && nInBufSize >= sizeof(BOOL)) {
            BOOL fPowerOn = *(BOOL*)lpInBuf;
            if (fPowerOn && Kitl.pfnPowerOn) {
                Kitl.pfnPowerOn();      
            } else if (!fPowerOn && Kitl.pfnPowerOff) {
                Kitl.pfnPowerOff();
            }
            retval = TRUE;
        }
        break;

    case IOCTL_HAL_POSTINIT:
        InitializeCriticalSection (&KITLcs);
        KITLGlobalState |= KITL_ST_SYSTEMSTARTED;

        if (KITLGlobalState & KITL_ST_ADAPTER_INITIALIZED) {
            KITLInitializeInterrupt ();
        }
        // fall through to call OEMKitlIoctl

    default:
        retval = OEMKitlIoctl (dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned);
        break;
    }

    return retval;

}


/*
 * KITLInit | Initialize debug Ethernet (KITL) subsystem.
 *
 * This function is called from the HAL to initialize the structures and hardware
 * used for debug Ethernet routines.  This function may be called very early in
 * system initialization, such as in OEMInit().  It should also be called
 * any time the debug Ethernet hardware needs to be reinitialized, such as
 * returning from OEMPowerOff().
 *
 * Return Value:
 *   Return TRUE if initialization was successful, FALSE if an error occurred.
 */

void InitTimerList (void);

BOOL KitlInit (BOOL fStartKitl)
{
    // Initialize default clients
    NewClient (KITL_SVC_DBGMSG, KITL_SVCNAME_DBGMSG, FALSE);
    NewClient (KITL_SVC_PPSH,   KITL_SVCNAME_PPSH,   FALSE);
    NewClient (KITL_SVC_KDBG,   KITL_SVCNAME_KDBG,   FALSE);

    InitTimerList ();

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
    PHDATA phdIntEvt;
    DWORD dwRet;

    KITL_DEBUGMSG(ZONE_INIT,("KITL Interrupt thread started (hTh: 0x%X, pTh: 0x%X), using SYSINTR %u\n",
                             dwCurThId, pCurThread, Kitl.Interrupt));

    dwIntrThId = dwCurThId;

    pCurThread->bDbgCnt = 1;   // no entry messages

    if ((hIntEvent = NKCreateEvent (0, FALSE, FALSE, EDBG_INTERRUPT_EVENT)) == NULL) {
        KITLOutputDebugString("KITL CreateEvent failed!\n");
        return 0;
    }
    if (!g_kpriv.pfnIntrInitialize (Kitl.Interrupt, hIntEvent, NULL, 0)) {
        HNDLCloseHandle (pActvProc, hIntEvent);
        KITLOutputDebugString("KITL InterruptInitialize failed\n");
        return 0;
    }

    VERIFY (phdIntEvt = LockHandleData (hIntEvent, g_pprcNK));

    // always enable interrupt as long as OEM told us so
    EnableInts();

    KITLGlobalState |= KITL_ST_IST_STARTED;

    while ((dwRet = DoWaitForObjects (1, &phdIntEvt, INFINITE)) == WAIT_OBJECT_0) {

        KITL_DEBUGLED (LED_IST_ENTRY,0);
        KITL_DEBUGMSG (ZONE_INTR,("KITL Interrupt event\n"));
        //KITLOutputDebugString ("+ ");

        // no need to check pending, just call HandleRecvInterrupts because it'll
        // just return if there is no interrupt pending
        HandleRecvInterrupt (ISTRecvBuf, TRUE, NULL, NULL);

        g_pOemGlobal->pfnInterruptDone (Kitl.Interrupt);

        //KITLOutputDebugString ("- ");
        KITL_DEBUGMSG(ZONE_INTR,("Processed Interrupt event\n"));
    }
    KITLOutputDebugString("!KITL Interrupt thread got error in WaitForMultipleObjects: dwRet:%u, GLE:%u\n",
                          dwRet,NKGetLastError());
    return 0;
}

#ifdef DEBUG

extern DBGPARAM dpCurSettings;

#endif


BOOL WINAPI KitlDllMain (HINSTANCE DllInstance, DWORD dwReason, LPVOID Reserved)
{
    if (DLL_PROCESS_ATTACH == dwReason) {
        PFN_KLIBIOCTL pfnKLibIoctl = (PFN_KLIBIOCTL) Reserved;
        g_kpriv.pfnExtKITLIoctl = ExtKITLIoctl;                
        (* pfnKLibIoctl) ((HANDLE)KMOD_KITL, IOCTL_KLIB_KITL_INIT, NULL, 0, &g_kpriv, sizeof (g_kpriv), NULL);
        g_pKData        = g_kpriv.pKData;
        g_pprcNK        = g_kpriv.pprcNK;
        g_pNKGlobal     = g_pKData->pNk;
        g_pOemGlobal    = g_pKData->pOem;
        g_pNKGlobal->pfnKITLIoctl = KITLIoctl;
#ifdef DEBUG
        g_pNKGlobal->pKITLDbgZone = &dpCurSettings;
#endif
    }

    return TRUE;
}

