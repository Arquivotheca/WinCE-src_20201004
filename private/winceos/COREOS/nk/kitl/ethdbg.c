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
#include <intsafe.h>
#include "kernel.h"
#include "kitlp.h"

#undef g_pKData

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
#define HEAP_KITLCLIENT         HEAP_MODULE
ERRFALSE(sizeof(KITL_CLIENT) <= sizeof(MODULE));
KITL_CLIENT DfltClnts[NUM_DFLT_KITL_SERVICES];
PKITL_CLIENT KITLClients[MAX_KITL_CLIENTS];

// Buffers for default services - since PPFS and windbg use byte at a time operations, we need
// to format commands in a buffer before sending to other side.  Since both PPFS and windbg are
// serialized while sending commands, we can share a single buffer for send and receive.
static UCHAR KdbgFmtBuf[(KITL_MAX_DATA_SIZE+3)&~3];  // make it DWORD aligned


// Extra receive buffer for ISR routine.  Need two, because we may have filled a buffer in the
// ISR, then get swapped out and another thread may need to do polled I/O.
UCHAR SendFmtBuf[KITL_MTU];  // make it DWORD aligned
UCHAR PollRecvBuf[KITL_MTU];

KITLTRANSPORT Kitl;

// To prevent interleaving of our debug output
CRITICAL_SECTION KITLODScs;

// cs to pretect timer list
CRITICAL_SECTION TimerListCS;

// critical section in place of KCall
CRITICAL_SECTION KITLTransportCS;

#define KITL_BUFFER_POOL_SIZE  (KITL_MAX_WINDOW_SIZE * 2 * KITL_MTU)

// buffer for default clients, must be DWORD aligned
static DWORD g_KitlBuffer[((KITL_BUFFER_POOL_SIZE * NUM_DFLT_KITL_SERVICES) + (sizeof(DWORD) - 1)) / sizeof(DWORD)];

// cs to protect client list
CRITICAL_SECTION KITLcs;
void ResetClientState(KITL_CLIENT *pClient);

// monitor thread related variables
#define KM_NO_MONITOR           0
#define KM_MONITOR_REGISTERING  1
#define KM_MONITOR_REGISTERED   2
static LONG          g_dwMonitorState = KM_NO_MONITOR;
static PFN_KLIBIOCTL g_pfnKLibIoctl;
static DWORD         g_dwSpinnerThreadId;
volatile const DWORD g_KitlMonitorDelay = 1000;

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

// If any safe CRT functions throw an unrecoverable error while in KITL,
// we will try to take down the device.
void
__cdecl
__crt_unrecoverable_error(
    const wchar_t *pszExpression,
    const wchar_t *pszFunction,
    const wchar_t *pszFile,
    unsigned int nLine,
    uintptr_t pReserved
    )
{
    KITL_RETAILMSG(ZONE_ERROR, ("Kitl: Hit __crt_unrecoverable_error.  Rebooting."));
    OEMIoControl (IOCTL_HAL_REBOOT, NULL, 0, NULL, 0, NULL);
    OEMIoControl (IOCTL_HAL_HALT, NULL, 0, NULL, 0, NULL);
    for(;;) {}
}

//
// NewClient: create a new client structure and initialize it's ServiceId
//              and ServiceInstanceId
//
static PKITL_CLIENT NewClient (UCHAR uId, BOOL fAlloc)
{
    PKITL_CLIENT pRetClient = NULL;

    DEBUGCHK(IS_VALID_ID(uId));
    DEBUGCHK (!KITLClients[uId]);

    if (fAlloc)
    {
        pRetClient = (PKITL_CLIENT) AllocMem (HEAP_KITLCLIENT);
    }
    else
    {
        DEBUGCHK(IS_DFLT_SVC(uId));
        pRetClient = &DfltClnts[uId];
    }

    if (pRetClient != NULL)
    {
        memset (pRetClient, 0, sizeof(KITL_CLIENT));
        pRetClient->ServiceId = uId;
        pRetClient->ServiceInstanceId = SVC_INST_ID_INVALID;

        KITLClients[uId] = pRetClient;
    }

    return pRetClient;
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
    BOOL fRet;

    KITL_DEBUGMSG(ZONE_INIT,("+KITLRegisterDfltClient, service:%u\n",Service));

    if ((KITLGlobalState & KITL_ST_KITLINITIALIZED) != KITL_ST_KITLINITIALIZED) {
        KITLOutputDebugString("\r\n!KITLRegisterDfltClient called before KitlInit. KITLGlobalState=%X\r\n", KITLGlobalState);
        fRet = FALSE;
        goto exit;
    }

    if (!IS_DFLT_SVC(Service)) {
        fRet = FALSE;
        goto exit;
    }

    if (IsSystemReady() && !InSysCall ()) {
        EnterCriticalSection (&KITLcs);
    }
    if (!StartKitl (FALSE)) {
        KITLOutputDebugString ("!KITLRegisterDfltClient: Fail to initialize KITL\n");
        fRet = FALSE;
        goto exitcs;
    }

    pClient = KITLClients[Service];
    DEBUGCHK (pClient);

    switch (Service) {
    case KITL_SVC_DBGMSG:
        // start DBGMSG service?
        if (!(Kitl.dwBootFlags & KITL_FL_DBGMSG)) {
            fRet = FALSE;
            goto exitcs;
        }
        pBufferPool = (LPBYTE) Kitl.dwPhysBuffer;
        // No special buffer needed for debug messages
        break;
    case KITL_SVC_PPSH:
        // start PPSH service?
        if (!(Kitl.dwBootFlags & KITL_FL_PPSH)) {
            fRet = FALSE;
            goto exitcs;
        }
        pBufferPool = (LPBYTE) (Kitl.dwPhysBuffer + KITL_BUFFER_POOL_SIZE);
        break;
    case KITL_SVC_KDBG:
        // start KD service?
        if (!(Kitl.dwBootFlags & KITL_FL_KDBG)) {
            fRet = FALSE;
            goto exitcs;
        }
        pBufferPool = (LPBYTE) (Kitl.dwPhysBuffer + 2 * KITL_BUFFER_POOL_SIZE);
        *ppTxBuffer = *ppRxBuffer = KdbgFmtBuf;
        break;
    default:
        fRet = FALSE;
        goto exitcs;
        break;
    }

    fRet = DoRegisterClient(&Id, pClient->ServiceName, Flags, Kitl.WindowSize, pBufferPool);

exitcs:
    if (IsSystemReady () && !InSysCall ()) {
        LeaveCriticalSection (&KITLcs);
    }
exit:
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
    char  *ServiceName, // @parm [IN]  - Service name (NULL terminated, up to MAX_SVCNAME_LEN chars
                        //                  for single-instance services, MAX_MULTIINST_SVC_NAMELEN
                        //                  for multi-instance services)
    UCHAR Flags,        // @parm [IN]  - KITL_CFGFL_ flags, defined in ethdbg.h
    UCHAR WindowSize,   // @parm [IN]  - Protocol window size (default is 8)
    UCHAR *pBufferPool) // @parm [IN]  - Buffer pool to use for KITL packet buffers - must be WindowSize*2*1500 bytes, unless STOP_AND_WAIT flag is set, in which
                        //               case, need only be 2*1500 bytes. For default OS services, may be NULL.
{
    BOOL fRet = FALSE;
    size_t BufferPoolSize;

    size_t cchNameMax = 0;
    size_t nNameLen = 0;

    if ((KITLGlobalState & KITL_ST_KITLINITIALIZED) != KITL_ST_KITLINITIALIZED) {
        KITLOutputDebugString("\r\n!KITLRegisterClient called before KitlInit. KITLGlobalState=%X\r\n", KITLGlobalState);
        return FALSE;
    }

    if (pId == NULL)
    {
        KITLOutputDebugString("!KITLRegisterClient: Invalid parameter (pId == NULL).\n");
        NKSetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if (ServiceName == NULL)
    {
        KITLOutputDebugString("!KITLRegisterClient: Invalid parameter (ServiceName == NULL).\n");
        NKSetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    cchNameMax = IS_SVC_MULTIINST (Flags) ? (MAX_MULTIINST_SVC_NAMELEN + 1) : (MAX_SVC_NAMELEN + 1);

    nNameLen = strnlen (ServiceName, cchNameMax);

    if (nNameLen >= cchNameMax)
    {
        KITLOutputDebugString("!KITLRegisterClient: Invalid parameter (ServiceName is too long.).\n");
        NKSetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if (WindowSize == 0 || WindowSize > KITL_MAX_WINDOW_SIZE) {
        KITLOutputDebugString("!KITLRegisterClient(%s): Invalid window size %u\n",ServiceName, WindowSize);
        NKSetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    KITL_DEBUGMSG(ZONE_INIT,("+KITLRegisterClient(%s): Flags: %u, WindowSize: %u, BufferPool: %X\n",
                             ServiceName,Flags,WindowSize,pBufferPool));

    DEBUGMSG (pBufferPool, (L"KITLRegisterClient: pBufferPool %8.8lx is no longer used. specify NULL\r\n", pBufferPool));

    //  BufferPoolSize = 2 * KITL_MTU * WindowSize;
    if (FAILED(SizeTMult(2, KITL_MTU, &BufferPoolSize))) {
        NKSetLastError(ERROR_ARITHMETIC_OVERFLOW);
        return FALSE;
    }
    if (FAILED(SizeTMult(BufferPoolSize, (size_t) WindowSize, &BufferPoolSize))) {
        NKSetLastError(ERROR_ARITHMETIC_OVERFLOW);
        return FALSE;
    }

    pBufferPool = VMAlloc(g_pprcNK, NULL, BufferPoolSize, MEM_COMMIT, PAGE_READWRITE);
    if(pBufferPool != NULL) {

        EnterCriticalSection (&KITLcs);

        if (StartKitl (FALSE)) 
            fRet = DoRegisterClient (pId, ServiceName, Flags, WindowSize, pBufferPool);        

        LeaveCriticalSection (&KITLcs);
        if (!fRet) {
            // Total failure.  Client record failed to initialize and
            // this buffer really needs to be freed.
            VMFreeAndRelease (g_pprcNK, pBufferPool, BufferPoolSize);
        }
    }
    KITL_DEBUGMSG(ZONE_INIT,("-KITLRegisterClient(%s): Flags: %u, WindowSize: %u, BufferPool: %X\n",
                             ServiceName,Flags,WindowSize,pBufferPool));
    return fRet;
}

// NOTE: Algorithm used by AllocClient for choosing next instance IDs must match the algorithm used by 
//       FindNextAvailableInstance in device-side KITL.  Don't update the algorithm here without matching
//       it there (and keep back-compat in mind if updating the algorithm).  Also, don't change the name
//       of AllocClient without updating the back-reference over on FindNextAvailableInstance
//
static DWORD AllocClient(KITL_CLIENT ** const ppClient, const LPCSTR ServiceName, const UCHAR Flags)
{
    DWORD ec = ERROR_GEN_FAILURE;

    const int iInvalid = -1;
    int iSelectedClientSlot = iInvalid;

    BOOLEAN abIsInstanceUsed[MAX_MULTIINST_SVC_INSTANCES] = {FALSE};


    DEBUGCHK ((ppClient != NULL) && (ServiceName != NULL));
    KITL_DEBUGMSG(ZONE_INIT, ("+AllocClient: ServiceName: '%s', Flags: 0x%B.\n", ServiceName, Flags));


    iSelectedClientSlot = ChkDfltSvc (ServiceName);

    if (iSelectedClientSlot >= 0)
    {
        // Is a default service

        DEBUGCHK (KITLClients[iSelectedClientSlot] != NULL);
        DEBUGCHK (strcmp (KITLClients[iSelectedClientSlot]->ServiceName, ServiceName) == 0);
        DEBUGCHK (KITLClients[iSelectedClientSlot]->ServiceId == iSelectedClientSlot);
        DEBUGCHK (KITLClients[iSelectedClientSlot]->ServiceInstanceId == SVC_INST_ID_INVALID);
        DEBUGCHK (KITLClients[iSelectedClientSlot]->CfgFlags == Flags);
        DEBUGCHK ( !IS_SVC_MULTIINST(Flags) );

        if (IS_UNUSED (KITLClients[iSelectedClientSlot]->State))
        {
            (*ppClient) = KITLClients[iSelectedClientSlot];
            ec = ERROR_SUCCESS;
        }
        else
        {
            KITL_DEBUGMSG(ZONE_WARNING, ("!AllocClient: Client already registered.  ServiceName: '%s'\n", ServiceName));
            ec = ERROR_ALREADY_INITIALIZED;
        }
    }
    else
    {
        // Is a non-default service, find a client slot for it...

        const int iMin = NUM_DFLT_KITL_SERVICES;
        const int iMax = MAX_KITL_CLIENTS - 1;
        const int iStart = HASH(ServiceName[0]);

        int iCur = iStart;


        // Search the list of clients.  Look for the first available slot as well as any
        // collisions with existing services that prevent this client's alloc.

        iSelectedClientSlot = iInvalid;

        for(;;)
        {
            if (KITLClients[iCur] == NULL)
            {
                if (iSelectedClientSlot == iInvalid)
                {
                    iSelectedClientSlot = iCur;
                }

                // Because clients are allocated contiguously and are never deallocated,
                // as soon as we hit a NULL client slot, we know that no further slots are
                // allocated with the same client name currently being requested.  No need
                // to continue searching for collisions.

                ec = ERROR_SUCCESS;
                break;
            }
            else if (IS_UNUSED(KITLClients[iCur]->State))
            {
                if (iSelectedClientSlot == iInvalid)
                {
                    iSelectedClientSlot = iCur;
                }

                // Fall through to continue searching for existing instances
            }
            else if (strcmp (KITLClients[iCur]->ServiceName, ServiceName) == 0)
            {
                if ((KITLClients[iCur]->State & KITL_CLIENT_DEREGISTERING) == KITL_CLIENT_DEREGISTERING)
                {
                    // Found client by same name that's in use, but since it's being deregistered, it can
                    // be ignored... nothing to do.

                    // Fall through to continue searching for existing instances
                }
                else if ( IS_SVC_MULTIINST(Flags) != IS_SVC_MULTIINST(KITLClients[iCur]->CfgFlags) )
                {
                    KITL_DEBUGMSG(ZONE_WARNING, ("!AllocClient: Cannot change multi-instance property of live service.  ServiceName: '%s'\n"
                                                        "    KITLClients[iCur]->CfgFlags:   0x%B\n"
                                                        "    Flags:                         0x%B\n",
                                                    ServiceName,
                                                    KITLClients[iCur]->CfgFlags,
                                                    Flags));

                    ec = ERROR_INVALID_PARAMETER;
                    break;
                }
                else if ( !IS_SVC_MULTIINST(Flags) )
                {
                    KITL_DEBUGMSG(ZONE_WARNING, ("!AllocClient: Client already registered.  ServiceName: '%s'\n", ServiceName));

                    ec = ERROR_ALREADY_INITIALIZED;
                    break;
                }
                else
                {
                    // Found existing instance of multi-instance service.  Record its instance ID
                    // to avoid its reuse

                    DEBUGCHK( IS_SVC_INST_ID_VALID (KITLClients[iCur]->ServiceInstanceId) );

                    abIsInstanceUsed[KITLClients[iCur]->ServiceInstanceId] = TRUE;

                    // Fall through to continue searching for existing instances
                }
            }
            else
            {
                // Found a used client by a different name... nothing to do.

                // Fall through to continue searching for existing instances
            }


            iCur = (iCur == iMax) ? iMin : (iCur + 1);

            if (iCur == iStart)
            {
                // Wrapped back to start.

                if (iSelectedClientSlot == iInvalid)
                {
                    KITL_DEBUGMSG(ZONE_WARNING, ("!AllocClient: No available client slots.  ServiceName: '%s'\n", ServiceName));
                    ec = ERROR_OUTOFMEMORY;
                }
                else
                {
                    ec = ERROR_SUCCESS;
                }

                break;
            }
        }

        if (ec == ERROR_SUCCESS)
        {
            DEBUGCHK ((iSelectedClientSlot >= iMin) && (iSelectedClientSlot <= iMax));

            if (KITLClients[iSelectedClientSlot] == NULL)
            {
                (*ppClient) = NewClient ((UCHAR) iSelectedClientSlot, TRUE);
                DEBUGCHK((*ppClient) == KITLClients[iSelectedClientSlot]);

                if ((*ppClient) == NULL)
                {
                    KITL_DEBUGMSG(ZONE_WARNING, ("!AllocClient: Couldn't allocate memory for client.  ServiceName: '%s'\n", ServiceName));
                    ec = ERROR_OUTOFMEMORY;
                }
            }
            else
            {
                (*ppClient) = KITLClients[iSelectedClientSlot];
            }
        }
    }

    if (ec == ERROR_SUCCESS)
    {
        DEBUGCHK (IS_VALID_ID (iSelectedClientSlot));
        DEBUGCHK (KITLClients[iSelectedClientSlot] != NULL);
        DEBUGCHK (KITLClients[iSelectedClientSlot] == (*ppClient));
        DEBUGCHK (IS_UNUSED (KITLClients[iSelectedClientSlot]->State) );

        if ( IS_SVC_MULTIINST(Flags) )
        {
            SVCINSTID siiFirstAvailable = SVC_INST_ID_INVALID;
            int iCurInstId = 0;

            while ((iCurInstId < MAX_MULTIINST_SVC_INSTANCES) && (siiFirstAvailable == SVC_INST_ID_INVALID))
            {
                if( !abIsInstanceUsed[iCurInstId] )
                {
                    siiFirstAvailable = (SVCINSTID)iCurInstId;
                }

                iCurInstId++;
            }

            // As long as MAX_MULTIINST_SVC_INSTANCES >= MAX_KITL_CLIENTS, we shouldn't need to worry about
            // running out of instance IDs.
            DEBUGCHK( MAX_MULTIINST_SVC_INSTANCES >= MAX_KITL_CLIENTS );
            DEBUGCHK( IS_SVC_INST_ID_VALID(siiFirstAvailable) );

            KITLClients[iSelectedClientSlot]->ServiceInstanceId = siiFirstAvailable;

            KITL_DEBUGMSG(ZONE_INIT, (" AllocClient: Allocating multi-instance service.\n"
                                        "    ServiceName:           '%s'\n"
                                        "    ServiceId:             %u\n"
                                        "    ServiceInstanceId:     %u\n"
                                        "    Flags:                 0x%B\n",
                                    ServiceName,
                                    KITLClients[iSelectedClientSlot]->ServiceId,
                                    KITLClients[iSelectedClientSlot]->ServiceInstanceId,
                                    Flags));
        }
        else
        {
            KITLClients[iSelectedClientSlot]->ServiceInstanceId = SVC_INST_ID_INVALID;

            KITL_DEBUGMSG(ZONE_INIT, (" AllocClient: Allocating single-instance service.\n"
                                        "    ServiceName:           '%s'\n"
                                        "    ServiceId:             %u\n"
                                        "    ServiceInstanceId:     %u\n"
                                        "    Flags:                 0x%B\n",
                                    ServiceName,
                                    KITLClients[iSelectedClientSlot]->ServiceId,
                                    KITLClients[iSelectedClientSlot]->ServiceInstanceId,
                                    Flags));
        }
    }

    if (ec != ERROR_SUCCESS)
    {
        (*ppClient) = NULL;
    }

    KITL_DEBUGMSG(ZONE_INIT, ("-AllocClient: ec: 0x%X, ppClient: 0x%X.\n", ec, ppClient));
    return ec;
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
    DWORD ec = ERROR_GEN_FAILURE;

    if (!(KITLGlobalState & KITL_ST_ADAPTER_INITIALIZED)) {
        NKSetLastError(ERROR_NOT_READY);
        return FALSE;
    }

    ec = AllocClient(&pClient, ServiceName, Flags);
    if (ec != ERROR_SUCCESS)
    {
        NKSetLastError(ec);
        return FALSE;
    }

    // Fill in client state...

    // AllocClient should set ServiceId
    DEBUGCHK(IS_VALID_ID (pClient->ServiceId));

    // AllocClient should set ServiceInstanceId
    DEBUGCHK(
                (  IS_SVC_MULTIINST(Flags) && IS_SVC_INST_ID_VALID (pClient->ServiceInstanceId) )
             || ( !IS_SVC_MULTIINST(Flags) && (pClient->ServiceInstanceId == SVC_INST_ID_INVALID) )  );

    strncpy_s(pClient->ServiceName, _countof(pClient->ServiceName), ServiceName, _TRUNCATE);
    pClient->State = KITL_CLIENT_REGISTERING;

    // pClient->pTxBufferPool will be overwritten + lost.
    DEBUGCHK(IS_DFLT_SVC(pClient->ServiceId) || pClient->pTxBufferPool == NULL);
    pClient->pTxBufferPool = pBufferPool;
    pClient->RxWinEnd      = WindowSize;
    pClient->WindowSize    = WindowSize;
    DEBUGCHK(IS_DFLT_SVC(pClient->ServiceId) || pClient->pRxBufferPool == NULL);
    pClient->pRxBufferPool = pClient->pTxBufferPool + (WindowSize * KITL_MTU);
    pClient->CfgFlags      = Flags;
    DEBUGCHK(pClient->cRef == 0);
    pClient->cRef          = 0;

    // The service Id also functions as a client handle
    *pId = pClient->ServiceId;

    // KDBG is special case - it always runs with interrupts off and can't make sys calls,
    // so leave it in polling mode.
    if (pClient->ServiceId == KITL_SVC_KDBG) {
        KITLKdRegister ();
    } else if ((KITLGlobalState & KITL_ST_MULTITHREADED)
               && !RegisterClientPart2(*pId)) {
        // Mark pClient and free up the slot for later on.
        pClient->State         = 0;
        pClient->pTxBufferPool = NULL;
        pClient->pRxBufferPool = NULL;
        return FALSE;
    }

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
    KITL_DEBUGMSG(ZONE_INIT,("+RegisterClientPart2: Id %d\n",Id));

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

    fRet = (evCfg && evRecv && evTxFull);
    if (fRet) {
        pClient->phdCfgEvt    = LockHandleData (evCfg,    pprc);
        pClient->phdRecvEvt   = LockHandleData (evRecv,   pprc);
        pClient->phdTxFullEvt = LockHandleData (evTxFull, pprc);

        InitializeCriticalSection (&pClient->ClientCS);
        
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
    DWORD dwErr = ERROR_SUCCESS;

    KITL_DEBUGMSG(ZONE_INIT,("+KITLDeregisterClient, Id:%d\n", Id));
    DEBUGCHK (!InSysCall ());

    // Prevent caller from specifying a bad ID.
    if (!IS_VALID_ID(Id)) {
        KITL_DEBUGMSG(ZONE_WARNING, ("KITLDeregisterClient: invalid ID[%u]\n", Id));
        NKSetLastError(ERROR_INVALID_INDEX);
        return FALSE;
    }

    if (IS_DFLT_SVC(Id)) {
        // TODO: We really need to prevent this.  Conceivably, some app
        // can just come in and disable a default stream.
        KITL_DEBUGMSG(ZONE_WARNING,("KITLDeregisterClient(%d): Disabling a default service.\n",Id));
        // NKSetLastError(ERROR_ACCESS_DENIED);
        // return FALSE;
    }

    EnterCriticalSection (&KITLcs);
    
    if ((pClient = KITLClients[Id]) != NULL) {

        if (pClient->State & (KITL_CLIENT_REGISTERED |
                              KITL_CLIENT_REGISTERING |
                              KITL_CLIENT_DEREGISTERING)) {
            DEBUGCHK (pClient->ServiceId == Id);

            // Client is still valid.  Nothing got freed yet.
            EnterCriticalSection (&pClient->ClientCS);

            // reset state before setting the events so the waiting
            // thread knows that the client deregistered

            State = pClient->State;
            pClient->State = KITL_CLIENT_DEREGISTERING;
            ResetClientState (pClient);

            if (State & KITL_USE_SYSCALLS) {
                // Wake up any thread that are waiting to share
                // configuration information, waiting on receive,
                // or waiting for room in the window to post a
                // send.
                //
                // Let them get out if possible before nuking this
                // structure.
                KITLSetEvent(pClient->phdCfgEvt);
                KITLSetEvent(pClient->phdRecvEvt);
                KITLSetEvent(pClient->phdTxFullEvt);

                pClient->State |= KITL_USE_SYSCALLS;
            }

            if (InterlockedCompareExchange(&pClient->cRef, 0, 0) == 0) {
                // Actual clean up.  Invoke this only if there are no
                // other threads using this KITL_CLIENT.
                pClient->State = 0;
                
                if (pClient->phdCfgEvt) {
                    UnlockHandleData (pClient->phdCfgEvt);
                    pClient->phdCfgEvt = NULL;
                }
                if (pClient->phdRecvEvt) {
                    UnlockHandleData (pClient->phdRecvEvt);
                    pClient->phdRecvEvt = NULL;
                }
                if (pClient->phdTxFullEvt) {
                    UnlockHandleData (pClient->phdTxFullEvt);
                    pClient->phdTxFullEvt = NULL;
                }

                // release memory
                if (!IS_DFLT_SVC (Id)) {
                    VMFreeAndRelease (g_pprcNK, pClient->pTxBufferPool, 2 * KITL_MTU * pClient->WindowSize);
                    pClient->pTxBufferPool = NULL;
                    pClient->pRxBufferPool = NULL;
                }

                DeleteCriticalSection(&pClient->ClientCS);
                
                // TODO: See if we can remove this in the future.  The
                // side effect of this is that streams with the same
                // name tend to use the same client record.
                // pClient->ServiceName[0] = 0;
            } else {
                // Someone is still using KITLSend or KITLRecv.  Not a
                // hard error.  Let caller know to retry the disconnect.
                LeaveCriticalSection (&pClient->ClientCS);
                dwErr = ERROR_RETRY;
            }
        } else {
            KITL_DEBUGMSG(ZONE_WARNING, ("KITLDeregisterClient[%u]-already deregistered\n", Id));
            dwErr = ERROR_INVALID_INDEX;
        }
    } else {
        dwErr = ERROR_INVALID_INDEX;
    }

    LeaveCriticalSection (&KITLcs);

    fRet = (dwErr == ERROR_SUCCESS);
    if (!fRet) {
        NKSetLastError(dwErr);
    }

    KITL_DEBUGMSG(ZONE_INIT,("-KITLDeregisterClient, Id:%u ret %d\n", Id, fRet));
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
    InitializeCriticalSection (&KITLTransportCS);

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


static BOOL StartKitl (BOOL fInit)
{
    BOOL fKitlInitSuccess = FALSE;

    // KITL already started?
    if (!fInit && (KITLGlobalState & KITL_ST_DESKTOP_CONNECTED)) {
        return TRUE;
    }

    // KITL initialized?
    if ((KITLGlobalState & KITL_ST_KITLINITIALIZED) != KITL_ST_KITLINITIALIZED) {
        KITLOutputDebugString("\r\n!StartKitl called before KitlInit. KITLGlobalState=%X\r\n", KITLGlobalState);
        return FALSE;
    }

    /*
     * When this function is called, the kernel hasn't yet been initialized,
     * so can't make any system calls.  Once the system has come up far
     * enough to handle system calls, KITLInitializeInterrupt() is called to complete
     * initialization.  This is indicated by the KITL_ST_MULTITHREADED flag in KITLGlobalState.
     */

    // Detect/initialize ethernet hardware, and return address information
    while(!fKitlInitSuccess) {
        fKitlInitSuccess = OEMKitlInit (&Kitl);
        if (!fKitlInitSuccess) {
            const int cSleepBeforeRetryMs = 10000;

            KITLOutputDebugString("\r\nOEMKitlInit failed - trying again.\r\n");
            Sleep(cSleepBeforeRetryMs);
        }
    }

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
            char *pOutId = (char *)lpOutBuf;  //output buffer to place the Id in.

            UCHAR Id = 0;  //Local copy of Id
            char pszServiceName[MAX_SVC_NAMELEN + 1];

            //Validate the arguments
            retval = IsValidUsrPtr(pRegClientIn->szServiceName, 1, FALSE);

            // make a local copy of the service name
            if (retval) {
                __try {
                    strncpy_s(pszServiceName, _countof(pszServiceName), pRegClientIn->szServiceName, _TRUNCATE);
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                    retval = FALSE;
                }
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

            // Verify that the data the user passed in is valid
            // NOTE: externel send to KD/DBGMSG is not allowed
            retval =   (KITL_SVC_KDBG != pEdbgSend->Id)
                    && (KITL_SVC_DBGMSG != pEdbgSend->Id)
                    && (pEdbgSend->dwUserDataLen <= KITL_MAX_DATA_SIZE)
                    && MemCopyFromUserPtr(pEdbgSend->pUserData, pInBuf, pEdbgSend->dwUserDataLen);

            if (!retval) {
                NKSetLastError(ERROR_INVALID_PARAMETER);            
                break;
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
            // Verify that the data the user passed in is valid
            // NOTE: externel send to KD/DBGMSG is not allowed
            retval =   (KITL_SVC_KDBG != pEdbgRecvIn->Id)
                    && (KITL_SVC_DBGMSG != pEdbgRecvIn->Id)
                    && MemCopyFromUserPtr (pEdbgRecvOut->pdwLen, &dwOutBufLen, sizeof(dwOutBufLen))
                    && (dwOutBufLen <= KITL_MAX_DATA_SIZE)
                    /*  The buffer length is valid -- Now validate the buffer.
                     *  We copy junk into the pRecvBuf to verify that the entire buffer is writable.
                     *  If we called KITLRecv, then wrote to the buffer, and it failed, we would screw up the
                     *  the state of the kitl protocol.  This minimizes the chance of this situation.
                     */
                    && MemCopyToUserPtr(pEdbgRecvOut->pRecvBuf, pOutBuf, dwOutBufLen);
                
            if (!retval) {
                NKSetLastError(ERROR_INVALID_PARAMETER);
                break;
            }

            retval = KITLRecv(pEdbgRecvIn->Id, pOutBuf,  &dwOutBufLen, pEdbgRecvIn->dwTimeout);

            if (retval && dwOutBufLen) {
                // copy data back to user buffer
                retval =   (dwOutBufLen <= KITL_MAX_DATA_SIZE)
                        && MemCopyToUserPtr(pEdbgRecvOut->pdwLen, &dwOutBufLen, sizeof(dwOutBufLen))
                        && MemCopyToUserPtr(pEdbgRecvOut->pRecvBuf, pOutBuf, dwOutBufLen);
            }
        }          
        break;

    case IOCTL_KITL_GET_CAPABILITIES:
        {
            PKITL_GET_CAPABILITIES_OUT pKitlGetCapabilitiesOut = (PKITL_GET_CAPABILITIES_OUT)lpOutBuf;
            DWORD dwBytesWritten = 0;

            retval = FALSE;

            if ((pKitlGetCapabilitiesOut != NULL) && (nOutBufSize == sizeof(*pKitlGetCapabilitiesOut)))
            {
                pKitlGetCapabilitiesOut->dwCapabilities = (DWORD) (kcStandard | kcSupportsMultiInstanceServices);
                dwBytesWritten = sizeof(*pKitlGetCapabilitiesOut);
                retval = TRUE;
            }

            if (lpBytesReturned != NULL)
            {
                (*lpBytesReturned) = dwBytesWritten;
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

    case IOCTL_EDBG_IS_STARTED:
        {
            retval = ((KITLGlobalState & KITL_ST_KITLSTARTED) != 0); // indicate (to kdstub) that KITL has started
        }
        break;

    case IOCTL_KITL_REGISTER_MONITOR_THREAD:
        // just set the flag, registration will occur on the next KITL interrupt
        g_dwMonitorState = KM_MONITOR_REGISTERING;
        break;

    default:
        retval = OEMKitlIoctl (dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned);
        break;
    }

    return retval;

}

extern void NKInitInterlockedFuncs (void);

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

    case IOCTL_KITL_GET_CAPABILITIES:
        {
            PKITL_GET_CAPABILITIES_OUT pKitlGetCapabilitiesOut = (PKITL_GET_CAPABILITIES_OUT)lpOutBuf;
            DWORD dwBytesWritten = 0;

            retval = FALSE;

            if ((pKitlGetCapabilitiesOut != NULL) && (nOutBufSize == sizeof(*pKitlGetCapabilitiesOut)))
            {
                pKitlGetCapabilitiesOut->dwCapabilities = (DWORD) (kcStandard | kcSupportsMultiInstanceServices);
                dwBytesWritten = sizeof(*pKitlGetCapabilitiesOut);
                retval = TRUE;
            }

            if (lpBytesReturned != NULL)
            {
                (*lpBytesReturned) = dwBytesWritten;
            }
        }
        break;

    case IOCTL_EDBG_SET_DEBUG:
        KITLSetDebug(nInBufSize);
        retval = TRUE;
        break;

    case IOCTL_EDBG_GET_OUTPUT_DEBUG_FN:
        if (nOutBufSize >= sizeof(VOID*)) {
#pragma warning(push)
#pragma warning(disable:4054)
            *(VOID**)lpOutBuf = (VOID*)KITLOutputDebugString;
#pragma warning(pop)
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
        NKInitInterlockedFuncs ();
        InitializeCriticalSection (&KITLcs);
        KITLGlobalState |= KITL_ST_SYSTEMSTARTED;

        if (KITLGlobalState & KITL_ST_ADAPTER_INITIALIZED) {
            KITLInitializeInterrupt ();
        }

        // call OEM kitl ioctl
        OEMKitlIoctl (dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned);
        
        // set last error to not supported such that we'll call OEMIOControl
        SetLastError (ERROR_NOT_SUPPORTED);
        break;

    case IOCTL_KITL_REGISTER_MONITOR_THREAD:
        // just set the flag, registration will occur on the next KITL interrupt
        g_dwMonitorState = KM_MONITOR_REGISTERING;
        break;

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

    NewClient (KITL_SVC_DBGMSG, FALSE);
    strncpy_s(KITLClients[KITL_SVC_DBGMSG]->ServiceName, _countof(KITLClients[KITL_SVC_DBGMSG]->ServiceName), KITL_SVCNAME_DBGMSG, _TRUNCATE);

    NewClient (KITL_SVC_PPSH, FALSE);
    strncpy_s(KITLClients[KITL_SVC_PPSH]->ServiceName, _countof(KITLClients[KITL_SVC_PPSH]->ServiceName), KITL_SVCNAME_PPSH, _TRUNCATE);

    NewClient (KITL_SVC_KDBG, FALSE);
    strncpy_s(KITLClients[KITL_SVC_KDBG]->ServiceName, _countof(KITLClients[KITL_SVC_KDBG]->ServiceName), KITL_SVCNAME_KDBG, _TRUNCATE);

    KITLOutputDebugString ("g_kitlLock = 0x%x\r\n", &g_kitlLock);
    InitTimerList ();

    KITLGlobalState |= KITL_ST_KITLINITIALIZED; // Record call to KitlInit.

    return fStartKitl? StartKitl (TRUE) : TRUE;
}

static void EnableInts()
{
    // Some platforms may not have available resources for an interrupt, in
    // which case we just continue to run in polling mode.
    if ((UCHAR) KITL_SYSINTR_NOINTR != Kitl.Interrupt) {
        KITL_DEBUGMSG(ZONE_INIT,("Enabling adapter ints...\n"));
        KitlEnableInt (TRUE);
        KITLGlobalState |= KITL_ST_INT_ENABLED;
    }
}

static DWORD KITLInterruptThread (DWORD Dummy)
{
    HANDLE hIntEvent;
    PHDATA phdIntEvt;
    DWORD dwRet;
    UCHAR ISTRecvBuf[KITL_MTU];
    DWORD dwMaxDelay = g_KitlMonitorDelay;
    DWORD dwSpinnerThreadId;

    pCurThread->bDbgCnt = 1;   // no entry messages - must be set before any debug message

    KITL_DEBUGMSG(ZONE_INIT,("KITL Interrupt thread started (hTh: 0x%X, pTh: 0x%X), using SYSINTR %u\n",
                             dwCurThId, pCurThread, Kitl.Interrupt));

    KITLOutputDebugString ("&g_kitlLock = 0x%x\r\n", &g_kitlLock);

    dwIntrThId = dwCurThId;

    if ((hIntEvent = NKCreateEvent (0, FALSE, FALSE, EDBG_INTERRUPT_EVENT)) == NULL) {
        KITLOutputDebugString("KITL CreateEvent failed!\n");
        return 0;
    }
    if (!g_kpriv.pfnIntrInitialize (Kitl.Interrupt, hIntEvent, NULL, 0)) {
        HNDLCloseHandle (pActvProc, hIntEvent);
        KITLOutputDebugString("KITL InterruptInitialize failed\n");
        return 0;
    }

    phdIntEvt = LockHandleData(hIntEvent, g_pprcNK);
    DEBUGCHK(phdIntEvt);

    // always enable interrupt as long as OEM told us so
    EnableInts();

    KITLGlobalState |= KITL_ST_IST_STARTED;


    while ((dwRet = DoWaitForObjects (1, &phdIntEvt, INFINITE)) == WAIT_OBJECT_0) {

        KITL_DEBUGLED (LED_IST_ENTRY,0);
        KITL_DEBUGMSG (ZONE_INTR,("KITL Interrupt event\n"));
        //KITLOutputDebugString ("+ ");

        // no need to check pending, just call HandleRecvInterrupts because it'll
        // just return if there is no interrupt pending
        while (HandleRecvInterrupt (ISTRecvBuf)) {
            // empty body
        }

        if (KM_MONITOR_REGISTERING == g_dwMonitorState) {
            dwMaxDelay = g_KitlMonitorDelay;
            // register KITL IST as monitor thread
            (* g_pfnKLibIoctl) ((HANDLE)KMOD_CORE,
                                IOCTL_KLIB_REGISTER_MONITOR,
                                &dwMaxDelay,
                                sizeof (DWORD),
                                &g_dwSpinnerThreadId,
                                sizeof (DWORD),
                                NULL);
            g_dwMonitorState = KM_MONITOR_REGISTERED;
            g_dwSpinnerThreadId = 0;

        }

        if (KM_MONITOR_REGISTERED == g_dwMonitorState) {

            dwSpinnerThreadId = InterlockedExchange ((PLONG) &g_dwSpinnerThreadId, 0);
            if (dwSpinnerThreadId) {
                KITLOutputDebugString ("!!! High Priority Spinner detected, thread id = 0x%x !!!\r\n", dwSpinnerThreadId);
                if (IsDesktopDbgrExist ()) {
                    DebugBreak ();
                }
            }
        }

        INTRDone(Kitl.Interrupt);

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


BOOL WINAPI KitlDllMain (HINSTANCE DllInstance, DWORD dwReason, PFN_KLIBIOCTL pfnKLibIoctl)
{
    if (DLL_PROCESS_ATTACH == dwReason) {
        g_pfnKLibIoctl = pfnKLibIoctl;
        g_kpriv.pfnExtKITLIoctl = ExtKITLIoctl;                
        (* g_pfnKLibIoctl) ((HANDLE)KMOD_KITL, IOCTL_KLIB_KITL_INIT, NULL, 0, &g_kpriv, sizeof (g_kpriv), NULL);
        g_pKData        = g_kpriv.pKData;
        g_pprcNK        = g_kpriv.pprcNK;
        g_pNKGlobal     = g_pKData->pNk;
        g_pOemGlobal    = g_pKData->pOem;
        g_pNKGlobal->pfnKITLIoctl = KITLIoctl;
        InitializeKitlSpinLock ();
        // Copy address back to kernel for debugger.
        *g_kpriv.ppKitlSpinLock = &g_kitlLock;
        
#ifdef DEBUG
        g_pNKGlobal->pKITLDbgZone = &dpCurSettings;
#endif
    }

    return TRUE;
}


//
// Attempt to guard the entrance into KITLSend and KITLRecv in such a
// way that if another thread were to call or was already calling
// KITLDeregisterClient, things won't break.
//
BOOL KITLTryAcquireClient(KITL_CLIENT *pClient)
{
    BOOL fCanEnter = FALSE;
    BOOL const fBlock = !InSysCall() &&
                        UseSysCall(pClient) &&
                        (KITLGlobalState & KITL_ST_INT_ENABLED);

    if (fBlock) {
        EnterCriticalSection(&KITLcs);
    }
    
    fCanEnter = (pClient->State & (KITL_CLIENT_REGISTERED |
                                   KITL_CLIENT_REGISTERING)) &&
                !(pClient->State & KITL_CLIENT_DEREGISTERING);
    if (fCanEnter) {
        InterlockedIncrement(&pClient->cRef);
    }

    if (fBlock) {
        LeaveCriticalSection(&KITLcs);
    }

    return fCanEnter;
}


//
// Decrement the kitl client inuse count.
//
void KITLReleaseClient(KITL_CLIENT *pClient)
{
    InterlockedDecrement(&pClient->cRef);
}

