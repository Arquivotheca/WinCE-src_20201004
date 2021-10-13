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
/**     TITLE("Kernel API call handler")
 *++
 *
 *
 * Module Name:
 *
 *    apicall.c
 *
 * Abstract:
 *
 *  This file contains the implementation of API call handling
 *
 *--
 */

#include <windows.h>
#include <kernel.h>

PHDATA APISLockHandle (PAPISET pas, HANDLE hSrcProc, HANDLE h, LPVOID *pObj);
BOOL APISUnlockHandle (PAPISET pas, PHDATA phdLock);

#define ALIGN16_UP(x)       (((x) + 0xf) & ~0xf)
#define ALIGN16_DOWN(x)     ((x) & ~0xf)

//
// User mode server: for out parameters, we save the offset of "out parameters" on the server stack for 2 reasons
//          - it's safe. It's the source while copying out, and we can validate that the offset is valid before performing copy.
//          - it saves space on kernel stack so we can support more out parameters (6, instead of 4, if we put it on kernel stack)
//
// At the entrance to a user-mode server API, the stack will looks like this
//
//      |-----------------------|   <-- stack base (stack limit)
//      |                       |
//      |   unused stack for    |
//      | server, at least 32K  |
//      |                       |
//      |-----------------------|   <-- stack top when entering the user mode server
//      |                       |
//      |      API arguments    |
//      |                       |
//      |-----------------------|   <-- stack top
//      |                       |
//      |space for pointer args |
//      |  (copy-in/copy out)   |
//      |                       |
//      |-----------------------|   <-- out parameter offsets (use macro ARG_OFFSET_FROM_TLS to get here).
//      |                       |       Each entry here will be referring to an out pointer parameter in the
//      | OUT parameter offsets |       area above, such that we can copy-out on API return.
//      |                       |
//      |-----------------------|
//      | reserved (CRT,PRETLS) |
//      |-----------------------|   <-- TLS pointer
//      |                       |
//      | TLS and other OS info |
//      |                       |
//      |-----------------------|   <-- bottom of stack (== stack base + stack size)
//
//
//
#define ARG_OFFSET_FROM_TLS(tlsPtr)     ((LPDWORD) ALIGN16_DOWN ((DWORD) (tlsPtr) - SECURESTK_RESERVE - MAX_NUM_OUT_PTR_ARGS * sizeof(DWORD)))

// minimum stack left while calling into user mode server == 32K.
#define MIN_STACK_FOR_SERVER            (VM_BLOCK_SIZE >> 1)

static const PFNVOID APISetExtMethods[] = {
    (PFNVOID)APISClose,
    (PFNVOID)APISPreClose,
    (PFNVOID)APISRegister,
    (PFNVOID)APISCreateAPIHandle,
    (PFNVOID)APISVerify,
    (PFNVOID)NotSupported,
    (PFNVOID)NotSupported,
    (PFNVOID)NotSupported,
    (PFNVOID)APISSetErrorHandler,    
};

static const PFNVOID APISetIntMethods[] = {
    (PFNVOID)APISClose,
    (PFNVOID)APISPreClose,
    (PFNVOID)APISRegister,
    (PFNVOID)APISCreateAPIHandle,
    (PFNVOID)APISVerify,
    (PFNVOID)APISRegisterDirectMethods,
    (PFNVOID)APISLockHandle,
    (PFNVOID)APISUnlockHandle,
    (PFNVOID)APISSetErrorHandler,
};

static const ULONGLONG asetSigs [] = {
    FNSIG1 (DW),                    // CloseAPISet
    0,
    FNSIG2 (DW, DW),                // RegisterAPISet
    FNSIG2 (DW, DW),                // CreateAPIHandle (tread pvobj as DW, for no verification is needed)
    FNSIG2 (DW, DW),                // VerifyAPIHandle
    FNSIG2 (DW, DW),                // APISRegisterDirectMethods (exposed to K-Mode sever only)
    FNSIG4 (DW, DW, DW, O_PDW),     // LockAPIHandle (exposed to k-mode only)
    FNSIG2 (DW, DW),                // UnlockAPIHandle (exposed to k-mode only)
    FNSIG2 (DW, IO_PDW),            // SetAPIErrorHandler
};

ERRFALSE (sizeof(APISetExtMethods) == sizeof(APISetIntMethods));
ERRFALSE ((sizeof(APISetExtMethods) / sizeof(APISetExtMethods[0])) == (sizeof(asetSigs) / sizeof(asetSigs[0])));


const CINFO cinfAPISet = {
    "APIS",
    DISPATCH_KERNEL_PSL,
    HT_APISET,
    sizeof(APISetIntMethods)/sizeof(APISetIntMethods[0]),
    APISetExtMethods,
    APISetIntMethods,
    asetSigs,
    0,
    0,
    0,
};

// Array of pointers to system api sets.
const CINFO *SystemAPISets[NUM_API_SETS];

extern LPVOID pExitThread;
HANDLE g_hAPIRegEvt;

void APICallInit (void)
{
    /* setup the well known system API sets & API types */
    SystemAPISets[SH_WIN32] = &cinfWin32;
    SystemAPISets[SH_CURTHREAD] = &cinfThread;
    SystemAPISets[SH_CURPROC] = &cinfProc;
    SystemAPISets[SH_CURTOKEN] = &cinfToken;
    SystemAPISets[HT_EVENT] = &cinfEvent;
    SystemAPISets[HT_MUTEX] = &cinfMutex;
    SystemAPISets[HT_CRITSEC] = &cinfCRIT;
    SystemAPISets[HT_SEMAPHORE] = &cinfSem;
    SystemAPISets[HT_APISET] = &cinfAPISet;
    // SystemAPISets[HT_FILE] = &CinfFile;
    // SystemAPISets[HT_FIND] = &CinfFind;
    // SystemAPISets[HT_DBFILE] = &CinfDBFile;
    // SystemAPISets[HT_DBFIND] = &CinfDBFind;
    // SystemAPISets[HT_SOCKET] = &CinfSocket;
    // SystemAPISets[HT_WNETENUM] = &CinfWnetEnum;
    KInfoTable[KINX_APISETS] = (DWORD)SystemAPISets;

}

#define MAX_METHOD  (METHOD_MASK+1)

BOOL ValidateSignatures (const ULONGLONG *pu64Sigs, DWORD cFunctions)
{
    ULONGLONG u64Sig;
    DWORD     nArgs, dwCurArg;
    BOOL      fNeedSize = FALSE;
    DEBUGCHK (IsKModeAddr ((DWORD)pu64Sigs));
    
    while (!fNeedSize && cFunctions --) {
        u64Sig = pu64Sigs[cFunctions];
        nArgs  = (DWORD) u64Sig & ARG_TYPE_MASK;

        if (nArgs >= MAX_NUM_PSL_ARG) {
            // too many arguments
            RETAILMSG (1, (L"ERROR! Too many arguments for function idx = %d, nArgs = %d\r\n", cFunctions, nArgs));
            DEBUGCHK (0);
            return FALSE;            
        }

        while (nArgs --) {
            u64Sig >>= ARG_TYPE_BITS;
            dwCurArg = (DWORD) u64Sig & ARG_TYPE_MASK;

            if (ARG_DW == dwCurArg) {
                fNeedSize = FALSE;
            } else if (fNeedSize) {
                // invalid signature
                break;
            } else {
                switch (dwCurArg) {
                case ARG_I_PTR:
                case ARG_O_PTR:
                case ARG_IO_PTR:
                    fNeedSize = TRUE;
                    break;
                default:
                    break;
                }
            }
        }
    }

    if (fNeedSize) {
        // invalid signature detected 
        RETAILMSG (1, (L"ERROR! Invalid API Signature idx = %d, sig = 0x%I64X\r\n", cFunctions, pu64Sigs[cFunctions]));
        DEBUGCHK (0);
    }
    return !fNeedSize;
}

//------------------------------------------------------------------------------
//
//  @func HANDLE | NKCreateAPISet | Creates an API set
//  @rdesc Returns the handle to the API set
//  @parm char[4] | acName | class name (for debugging)
//  @parm USHORT | cFunctions | # of functions in set
//  @parm PFNVOID * | ppfnMethods | array of API functions
//  @parm PULONGLONG | pu64Sig | array of signatures for API functions
//  @comm Creates an API set from the list of functions passed in
//
//------------------------------------------------------------------------------
HANDLE NKCreateAPISet (
    char acName[4],                 /* 'class name' for debugging & QueryAPISetID() */
    USHORT cFunctions,              /* # of functions in set */
    const PFNVOID *ppfnMethods,     /* array of API functions */
    const ULONGLONG *pu64Sig        /* array of signatures for API functions */
    )
{
    PAPISET    pas   = NULL;
    HANDLE     hSet  = NULL;
    DWORD      dwErr = 0;
    PFNVOID   *ppfns = NULL;
    ULONGLONG *psigs = NULL;

    DEBUGCHK (cFunctions && (cFunctions <= MAX_METHOD));

    if (!(pas = NKmalloc (sizeof (APISET)))) {
        DEBUGMSG (1, (L"ERROR: NKCreateAPISet: out of memory 0\r\n"));
        dwErr = ERROR_NOT_ENOUGH_MEMORY;

    } else if (pActvProc != g_pprcNK) {
        // user mode server, verify/copy arguments
        ppfns = NKmalloc (cFunctions * sizeof (PFNVOID));
        psigs = NKmalloc (cFunctions * sizeof (ULONGLONG));

        if (!ppfns || !psigs) {
            DEBUGMSG (1, (L"ERROR: NKCreateAPISet: out of memory\r\n"));
            dwErr = ERROR_NOT_ENOUGH_MEMORY;
            
        } else if (!CeSafeCopyMemory (ppfns, ppfnMethods, cFunctions * sizeof (PFNVOID))
                || !CeSafeCopyMemory (psigs, pu64Sig, cFunctions * sizeof (ULONGLONG))) {
            DEBUGMSG (1, (L"ERROR: NKCreateAPISet: invalid parameter\r\n"));
            dwErr = ERROR_INVALID_PARAMETER;

        } else {

            ppfnMethods = ppfns;
            pu64Sig     = psigs;
        }
    }

    if (!dwErr) {
        pas->cinfo.disp     = (pActvProc == g_pprcNK)? DISPATCH_KERNEL_PSL : DISPATCH_PSL;
        pas->cinfo.type     = NUM_API_SETS;
        pas->cinfo.cMethods = cFunctions;
        pas->cinfo.ppfnExtMethods = pas->cinfo.ppfnIntMethods = ppfnMethods;
        pas->cinfo.pu64Sig  = pu64Sig;
        pas->cinfo.dwServerId = dwActvProcId;
        pas->cinfo.phdApiSet = NULL;
        pas->cinfo.pfnErrorHandler = NULL;
        *(PDWORD)pas->cinfo.acName = *(PDWORD)acName;
        pas->iReg           = -1;     /* not a registered handle */

        if (
#ifndef DEBUG
            // don't do vaildation for kernel PSL for perf
            (pActvProc != g_pprcNK) &&
#endif
            !ValidateSignatures (pu64Sig, cFunctions)) {
            dwErr = ERROR_INVALID_PARAMETER;
            
        } else if (!(hSet = HNDLCreateHandle (&cinfAPISet, pas, pActvProc))) {
            DEBUGMSG (1, (L"NKCreateAPISet: CreateHandle failed\r\n"));
            dwErr = ERROR_NOT_ENOUGH_MEMORY;
        }
    }

    if (dwErr) {
        DEBUGCHK (!hSet);
        NKfree (ppfns);
        NKfree (psigs);
        NKfree (pas);
        KSetLastError (pCurThread, dwErr);
    }

    DEBUGMSG (ZONE_OBJDISP, (L"NKCreateAPISet: returns %8.8lx\r\n", hSet));
    return hSet;
}

//
// External interface to NKCreateAPISet
//
HANDLE EXTCreateAPISet (
    char acName[4],                 /* 'class name' for debugging & QueryAPISetID() */
    USHORT cFunctions,              /* # of functions in set */
    const PFNVOID *ppfnMethods,     /* array of API functions */
    const ULONGLONG *pu64Sig        /* array of signatures for API functions */
    )
{
    HANDLE hSet = NULL;
    if (!cFunctions
        || (cFunctions > MAX_METHOD)
        || !IsValidUsrPtr (acName,      4,                               FALSE)
        || !IsValidUsrPtr (ppfnMethods, cFunctions * sizeof (PFNVOID),   FALSE)
        || !IsValidUsrPtr (pu64Sig,     cFunctions * sizeof (ULONGLONG), FALSE)
        ) {
        DEBUGMSG (1, (L"ERROR: NKCreateAPISet: invalid parameter 0\r\n"));
        SetLastError (ERROR_INVALID_PARAMETER);

    } else {
        hSet = NKCreateAPISet (acName, cFunctions, ppfnMethods, pu64Sig);
    }
    return hSet;
}

//------------------------------------------------------------------------------
// APISClose - delete an API Set
//------------------------------------------------------------------------------
void APISClose (PAPISET pas)
{
    if ((pas->iReg != -1) && !pas->cinfo.type) {
        SystemAPISets[pas->iReg] = 0;
    }

    DEBUGMSG (ZONE_OBJDISP, (L"APISClose: API set '%.4a' is being destroyed\r\n", pas->cinfo.acName));
    if (pas->cinfo.dwServerId && (pas->cinfo.dwServerId != g_pprcNK->dwId)) {
        if (pas->cinfo.ppfnExtMethods != pas->cinfo.ppfnIntMethods) {
            NKfree ((LPVOID) pas->cinfo.ppfnIntMethods);
        }
        NKfree ((LPVOID) pas->cinfo.ppfnExtMethods);
        NKfree ((LPVOID) pas->cinfo.pu64Sig);
    }
    NKfree (pas);
}

//------------------------------------------------------------------------------
// APISPreClose - handle server closed the handle to the API set
//------------------------------------------------------------------------------
void APISPreClose (PAPISET pas)
{
    DEBUGMSG (ZONE_OBJDISP, (L"APISPreClose: API set '%.4a' deregistered\r\n", pas->cinfo.acName));
    // change the server ID to be an invalid id, indicating invalid API set
    if (pas->cinfo.dwServerId && (pas->cinfo.dwServerId != g_pprcNK->dwId)) {
        pas->cinfo.dwServerId = (DWORD) INVALID_HANDLE_VALUE;
    } else {
        NKD (L"!!!Kernel mode PSL '%.4a' de-registered!!!\r\n", pas->cinfo.acName);
    }
}


// kernel mode server only - register a direct call function table 
BOOL APISRegisterDirectMethods (PAPISET pas, const PFNVOID *ppfnDirectMethods)
{
    DEBUGCHK (IsKModeAddr ((DWORD) ppfnDirectMethods));
    DEBUGCHK (pas->cinfo.ppfnIntMethods == pas->cinfo.ppfnExtMethods);

    if (ppfnDirectMethods) {
        pas->cinfo.ppfnIntMethods = ppfnDirectMethods;
    }
    return NULL != ppfnDirectMethods;
}

#define NUM_EXT_SETS        16

//------------------------------------------------------------------------------
//
//  @func BOOL | APISRegister | Registers a global APIset
//  @rdesc Returns TRUE on success, else FALSE
//  @parm HANDLE | hASet | handle to API set
//  @parm DWORD | dwSetID | constant identifying which API set to register as
//  @comm Associates the APIset with a slot in the global APIset table
//
//------------------------------------------------------------------------------
BOOL APISRegister (PAPISET pas, DWORD dwSetID)
{
    BOOL  fHandleBased = (dwSetID & REGISTER_APISET_TYPE);
    DWORD dwIDLimit    = fHandleBased? SH_FIRST_OS_API_SET : NUM_API_SETS;
    DWORD dwErr        = 0;

    dwSetID &= ~REGISTER_APISET_TYPE;

    DEBUGMSG (ZONE_OBJDISP, (L"APISRegister: Registering APISet '%.4a', dwSetID = 0x%x, fHandleBased = %x\r\n",
        pas->cinfo.acName, dwSetID, fHandleBased));

    LockLoader (g_pprcNK);        
    
    if (!dwSetID) {
        // search a empty slot based on handle-based or not
        DWORD dwBase = fHandleBased? SH_FIRST_EXT_HAPI_SET : SH_FIRST_EXT_API_SET;
        for (dwSetID = dwBase; dwSetID < dwIDLimit; dwSetID ++) {
            if (!SystemAPISets[dwSetID]) {
                break;
            }
        }
    }

    if ((-1 != pas->iReg) || (dwSetID >= dwIDLimit)) {
        dwErr = (-1 != pas->iReg)? ERROR_INVALID_PARAMETER : ERROR_NO_PROC_SLOTS;
    } else if (fHandleBased) {
        // handle based
        CINFO *pci = (CINFO *) SystemAPISets[dwSetID];
        if (!pci) {
            pci = (CINFO *) NKmalloc (sizeof (CINFO));
            if (!pci) {
                dwErr = ERROR_NOT_ENOUGH_MEMORY;
            } else {
                memset (pci, 0, sizeof (CINFO));
                pci->disp       = pas->cinfo.disp;
                pci->type       = (uchar) dwSetID;
                pci->cMethods   = pas->cinfo.cMethods;
                pci->dwServerId = (DWORD) INVALID_HANDLE_VALUE;
                *(LPDWORD)pci->acName = *(PDWORD)pas->cinfo.acName;
                SystemAPISets[dwSetID] = pci;
            }
            
        } else if (dwSetID != (DWORD) pci->type) {
            dwErr = ERROR_INVALID_PARAMETER;
        }

        DEBUGCHK (dwErr || pci);

        if (!dwErr) {
            pas->iReg       = dwSetID;
            pas->cinfo.type = (uchar)dwSetID;
        }

    } else if (dwSetID >= SH_FIRST_OS_API_SET) {
        // non-handle based
        if (SystemAPISets[dwSetID]) {
            dwErr = ERROR_INVALID_PARAMETER;
            
        } else {
            pas->cinfo.disp = (pas->cinfo.dwServerId == g_pprcNK->dwId) ? DISPATCH_I_KPSL : DISPATCH_I_PSL;
            pas->iReg = dwSetID;
            pas->cinfo.type = 0;
            SystemAPISets[dwSetID] = &pas->cinfo;
        }

    } else {
        // registering non-handle-based API in Handle-based API area
        dwErr = ERROR_NO_PROC_SLOTS;
    }

    UnlockLoader (g_pprcNK);        
    
    if (dwErr) {
        RETAILMSG(1, (TEXT("RegisterAPISet failed for api set id 0x%x, fHandleBased = %x.\r\n"), dwSetID, fHandleBased));
        SetLastError (dwErr);
    } else {
        // signal the "new API registered" event
        NKPulseEvent (g_pprcNK, g_hAPIRegEvt);
        DEBUGMSG(ZONE_OBJDISP, (TEXT("RegisterAPISet: API Set '%.4a', id = 0x%x registered.\r\n"), pas->cinfo.acName, dwSetID));
    }
    return !dwErr;
}


//------------------------------------------------------------------------------
// APISCreateAPIHandle - create a handle of a particular API set
//------------------------------------------------------------------------------
HANDLE APISCreateAPIHandle (PAPISET pas, LPVOID pvData)
{
    HANDLE hRet = NULL;

    // this is normal for handles using METHOD_CALL
    //DEBUGMSG (-1 == pas->iReg, (L"WARNING!! APISCreateAPIHandle: calling CreateAPIHandle on API Set '%.4a' that is not registered yet.\r\n", pas->cinfo.acName));

    DEBUGMSG (!pvData, (L"APISCreateAPIHandle: pvData == NULL\r\n"));

    DEBUGMSG (ZONE_OBJDISP, (L"Creating API Handle for APISet '%.4a' type %d\r\n", pas->cinfo.acName, pas->cinfo.type));

    switch (pas->cinfo.disp) {
    case DISPATCH_KERNEL_PSL:
    case DISPATCH_PSL:
        hRet = HNDLCreateHandle (&pas->cinfo, pvData, pActvProc);
        break;
    default:
        DEBUGMSG (1, (L"APISCreateAPIHandle Failed - invalid API set '%.4a' type %d\r\n", pas->cinfo.acName, pas->cinfo.type));
        KSetLastError(pCurThread, ERROR_INVALID_HANDLE);
    }

    return hRet;
}


LPVOID APISVerify (PAPISET pas, HANDLE h)
{
    LPVOID pvObj = NULL;
    DWORD  dwErr = ERROR_INVALID_HANDLE;
    PHDATA phd = LockHandleData (h, pActvProc);

    if (phd && (&pas->cinfo == phd->pci)) {
        pvObj = phd->pvObj;
        dwErr = 0;
    }

    UnlockHandleData (phd);
    SetLastError (dwErr);
    return pvObj;
}

PHDATA APISLockHandle (PAPISET pas, HANDLE hSrcProc, HANDLE h, LPVOID *ppObj)
{
    PHDATA phdSrcProc = LockHandleParam (hSrcProc, pActvProc);
    PHDATA phdLock;
    PPROCESS pprc = GetProcPtr (phdSrcProc);
    if (pprc
        && (phdLock = LockHandleData (h, pprc))
        && (&pas->cinfo == phdLock->pci)) {
        *ppObj = phdLock->pvObj;
    } else {
        phdLock = NULL;
    }
    UnlockHandleData (phdSrcProc);

    return phdLock;
}

BOOL APISUnlockHandle (PAPISET pas, PHDATA phdLock)
{
    DEBUGCHK (IsValidKPtr(phdLock));
    DEBUGCHK (&pas->cinfo == phdLock->pci);

    return (IsValidKPtr (phdLock) && (&pas->cinfo == phdLock->pci))
                ? UnlockHandleData (phdLock)
                : FALSE;
}

//------------------------------------------------------------------------------
//
//  @func BOOL | APISSetErrorHandler | Set an error handler for an API set
//  @rdesc Returns the TRUE if error handler is set; else FALSE (from the PSL layer)
//  @parm PAPISET | pas | Handle to the API set returned from CreateAPISet call
//  @parm PFNAPIERRHANDLER | pfnHandler | Error handler
//  @comm Registers a custom error handler for a API set. If user calls an API belonging
//  to this API set with invalid arguments, then the custom error handler is invoked on
//  the same thread. Any return value from the error handler is passed back to the caller.
//
//------------------------------------------------------------------------------
BOOL APISSetErrorHandler (PAPISET pas, PFNAPIERRHANDLER pfnHandler)
{
    pas->cinfo.pfnErrorHandler = pfnHandler;
    return TRUE;
}

//------------------------------------------------------------------------------
//
//  @func BOOL | APISQueryID | Lookup an API Set index
//  @rdesc Returns the index of the requested API set (-1 if not found)
//  @parm PCHAR | pName | the name of the API Set (4 characters or less)
//  @comm Looks in the table of registered API Sets to find an API set with
//        the requested name. If one is found, the index of that set is returned.
//        If no matching set is found, then -1 is returned.
//
//------------------------------------------------------------------------------
BOOL APISQueryID (char *pName, int* lpRetVal)
{
    const CINFO *pci;
    char acName[4];
    int inx;

    DEBUGCHK(lpRetVal);
    *lpRetVal = -1;
    
    for (inx = 0 ; inx < sizeof(acName) ; ++inx) {
        if ((acName[inx] = *pName) != 0)
            pName++;
    }
    if (*pName == 0) {
        for (inx = 0 ; inx < NUM_API_SETS ; ++inx) {
            if ((pci = SystemAPISets[inx]) != 0
                    && *(PDWORD)pci->acName == *(PDWORD)acName) {
                *lpRetVal = inx;
                break;
            }
        }
    }
    return TRUE;
}

ERRFALSE (0 == WAIT_OBJECT_0);
//
// NKWaitForAPIReady - waiting for an API set to be ready
//
BOOL IsSystemAPISetReady(DWORD dwAPISet)
{
    if (dwAPISet < NUM_API_SETS)
        return (NULL!=SystemAPISets[dwAPISet]);
    else
        return FALSE;
}
DWORD NKWaitForAPIReady (DWORD dwAPISet, DWORD dwTimeout)
{
    PHDATA phd = NULL;
    DWORD dwRet = WAIT_OBJECT_0;
    
    if (dwAPISet >= NUM_API_SETS) {
        dwRet = WAIT_FAILED;
        
    } else if (!dwTimeout) {
        // no timeout, return directly
        dwRet = SystemAPISets[dwAPISet]? WAIT_OBJECT_0 : WAIT_TIMEOUT;       
        
    } else if (!(phd = LockHandleParam (g_hAPIRegEvt, g_pprcNK))) {
        dwRet = WAIT_FAILED;
        
    } else {
        PROXY  proxy = {0};
        PTHREAD pth = pCurThread;
        CLEANEVENT _ce = {0};
        CLEANEVENT *lpce = &_ce;
        PPROXY pProx = &proxy;

        // initialize the timeout
        if (dwTimeout + 1 > 1) {  // the test fail only for 0xffffffff and 0
            dwTimeout = (dwTimeout < MAX_TIMEOUT)? (dwTimeout+1) : MAX_TIMEOUT;
        }

        // initialize the proxy
        pProx->pObject = phd->pvObj;
        pProx->bType = HT_MANUALEVENT;
        pProx->pTh = pth;
        pth->dwPendTime = dwTimeout;
        pth->dwPendWakeup = OEMGetTickCount () + dwTimeout;

        // initialize the thread
        KCall ((PKFN)WaitConfig, NULL, lpce);

        // Wait till our API set gets registered or timeout
        while (!dwRet && !SystemAPISets[dwAPISet]) {   

            // update the proxy and thread timeout
            pProx->wCount = pth->wCount;
            pProx->pQPrev = pProx->pQUp = pProx->pQDown = pProx->pQNext = 0;
            pProx->prio = (BYTE)GET_CPRIO(pth);            
            
            dwRet = KCall((PKFN)WaitForOneManualNonInterruptEvent, pProx, IsSystemAPISetReady, dwAPISet);            
            KCall((PKFN)DequeueFlatProxy, pProx);            
        }
            
        // cleanup
        pth->lpce = 0;
        UnlockHandleData (phd);
    }

    if (dwRet) {
        SetLastError (dwRet);
    }

    return dwRet;
}


//
// Notify ALL PSLs of process/thread create/delete and other system-wide events
//
void NKPSLNotify (DWORD dwFlags, DWORD dwProcId, DWORD dwThrdId)
{

    DWORD loop = NUM_API_SETS;
    const CINFO *pci;
    PPROCESS pprc;
    DEBUGMSG (ZONE_OBJDISP, (TEXT("NKPSLNotify: %8.8lx %8.8lx %8.8lx\r\n"), dwFlags, dwProcId, dwThrdId));
    pprc = SwitchActiveProcess (g_pprcNK);
    while (--loop >= SH_LAST_NOTIFY) {
        if (pci = SystemAPISets[loop]) {
            if (pci->dwServerId == g_pprcNK->dwId) {
                DEBUGMSG (ZONE_OBJDISP, (TEXT("NKPSLNotify: calling %8.8lx\r\n"), pci->ppfnIntMethods[0]));
                ((void (*) (DWORD, DWORD, DWORD)) pci->ppfnIntMethods[0]) (dwFlags, dwProcId, dwThrdId);
            } else {
                // u-mode server
                union {
                    DHCALL_STRUCT hc;
                    BYTE _b[sizeof(DHCALL_STRUCT)+2*sizeof(REGTYPE)];
                } u;

                u.hc.dwAPI    = 0;
                u.hc.dwErrRtn = 0;
                u.hc.cArgs    = 2;
                u.hc.args[0]  = dwFlags;
                u.hc.args[1]  = dwProcId;
                u.hc.args[2]  = dwThrdId;

                __try {
                    MDCallUserHAPI ((PHDATA) loop, &u.hc);
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                    DEBUGMSG (1, (L"ERROR Notifying PSL #%d\r\n", loop));
                }
                
            }
        }
    }
    SwitchActiveProcess (pprc);
}

PPROCESS LockServerWithId (DWORD dwProcId)
{
    PPROCESS pprc;
    if (dwProcId == g_pprcNK->dwId) {
        pprc = g_pprcNK;
    } else {
        PHDATA phd = LockHandleData ((HANDLE) dwProcId, g_pprcNK);
        if (pprc = GetProcPtr (phd)) {
            // increment the # of caller of the server process
            InterlockedIncrement (&pprc->nCallers);

            if (pprc->nCallers < 0) {
                // calling into a server that is exiting
                InterlockedDecrement (&pprc->nCallers);
                pprc = NULL;
            }
        }

        if (!pprc) {
            UnlockHandleData (phd);
        }
    }

    return pprc;
}

void UnlockServerWithId (DWORD dwProcId)
{
    if (dwProcId != g_pprcNK->dwId) {
        PHDATA phd = LockHandleData ((HANDLE) dwProcId, g_pprcNK);
        PPROCESS pprc = GetProcPtr (phd);
        DEBUGCHK (pprc);

        if (pprc) {
            // decrement the # of caller of the server process
            InterlockedDecrement (&pprc->nCallers);
            // unlock twice, for we locked it to get to phd
            DoUnlockHDATA (phd, -2*LOCKCNT_INCR);
        }
    }
}

#define SERVER_EXIT     0x80000000

#define MIN_SLEEP_TIME  500
#define MAX_SLEEP_TIME  30000

void WaitForCallerLeave (PPROCESS pprc)
{
    DWORD dwSleepTime = MIN_SLEEP_TIME;
    InterlockedExchangeAdd (&pprc->nCallers, SERVER_EXIT);

    while (pprc->nCallers != SERVER_EXIT) {
        DEBUGMSG (1, (L"PSL Server exiting - Waiting for callers to leave, nCallers = %8.8lx, %8.8lx\r\n", pprc->nCallers, pprc->dwId));
        Sleep (dwSleepTime);
        if (dwSleepTime < MAX_SLEEP_TIME) {
            dwSleepTime *= 2;
        }
    }
}

//------------------------------------------------------------------------------
//
//  BadHandleError - get error code based upon handle type
//
//  BadHandleError uses the handle type from the API call and the method
//  index to determine what value to return from an API call on an invalid handle.
//  If there is not default behavior, then an exception is raised.
//
//------------------------------------------------------------------------------
static DWORD BadHandleError (int type, long iMethod, const CINFO *pci) 
{
    RETAILMSG(1, (TEXT("Invalid handle: Set=%d '%.4a' Method=%d\r\n"), type,
        pci ? pci->acName : "NONE", iMethod));

    switch (type) {
    case HT_SOCKET:
        return WSAENOTSOCK;

    case HT_WNETENUM:
        return ERROR_INVALID_HANDLE;
        
    case HT_FILE:
        if (iMethod == 4 || iMethod == 5)
            return 0xFFFFFFFF;
        return 0;

    case HT_DBFILE:
        if (iMethod == 8 || iMethod == 10 || iMethod == 13 || iMethod == 21)
            return 0xFFFFFFFF;

        return FALSE;
        
    case HT_DBFIND:
    case HT_FIND:
        return FALSE;   // just return FALSE
    }

    // Fault the thread.
    return STATUS_INVALID_SYSTEM_SERVICE;
}



//------------------------------------------------------------------------------
// ErrorReturn - return a error value from an invalid API call
//
//  This function is invoked via ObjectCall as a kernel PSL. It extracts a
// return value from the CALLSTACK's access key.
//------------------------------------------------------------------------------
static DWORD ErrorReturn (DWORD dwErrRtn) 
{
    return dwErrRtn;
}


__inline void SaveDirectCall (PCALLSTACK pcstk)
{
    if (IsInDirectCall ()) {
        pcstk->dwPrcInfo |= CST_DIRECTCALL;
        ClearDirectCall ();
    }
}

//
// kernel API requires less space. We need to allow exception handler (RtlDispatchException)
// to call kernel API to dispatch excepiton. 
//
#define MIN_PSL_STACK       (2 * VM_PAGE_SIZE)          // at least 2 pages left before entering an API Call
#define MIN_KPSL_STACK      (1 * VM_PAGE_SIZE)          // at least 1 page left before entering a kernel API Call

__inline BOOL SecureStackSpaceLow (PCALLSTACK pcstk, DWORD dwServerId)
{
    LPDWORD pTlsSecure = pCurThread->tlsSecure;
    DWORD   cbMinLeft  = dwServerId? MIN_PSL_STACK : MIN_KPSL_STACK;
    DEBUGCHK (IsInRange ((DWORD) pcstk, pTlsSecure[PRETLS_STACKBASE], (DWORD) pTlsSecure));

    return ((DWORD) pcstk < (pTlsSecure[PRETLS_STACKBASE] + MIN_STACK_RESERVE + cbMinLeft));
}


static DWORD SetupCstk (PCALLSTACK pcstk)
{
    BOOL    dwErr   = 0;
    PTHREAD pth     = pCurThread;
    DWORD   dwUsrSP = pcstk->dwPrevSP;
    
#ifdef x86
    {
        // x86 specific - save registration pointer and terminate it
        NK_PCR  *pcr = TLS2PCR (((CST_MODE_FROM_USER & pcstk->dwPrcInfo)? pth->tlsNonSecure : pth->tlsSecure));

        pcstk->regs[REG_EXCPLIST] = pcr->ExceptionList;
        pcr->ExceptionList  = REGISTRATION_RECORD_PSL_BOUNDARY;
    }
#endif
    
    /** Setup a kernel PSL CALLSTACK structure. This is done here so that
     * any exceptions are handled correctly.
     */
    pcstk->phd      = NULL;
    pcstk->pprcLast = pActvProc;
    pcstk->pprcVM   = pth->pprcVM;
    pcstk->pOldTls  = pth->tlsNonSecure;
    pcstk->dwNewSP  = 0;

    SaveDirectCall (pcstk);

    /* Link the CALLSTACK struct at the head of the thread's list */
    //
    // NOTE: cannot generate exception before we link the callstack, for exception handler won't
    //       have the information to unwind correctly.
    //
    pcstk->pcstkNext = pth->pcstkTop;
    pth->pcstkTop = pcstk;

    // validate user stack pointer
    // NOTE we'll only access user stack from dwPrevSP sequentially, thus we only need to verify a DWORD
    if ((CST_MODE_FROM_USER & pcstk->dwPrcInfo)
        && (!IsStkAligned (dwUsrSP) || !IsValidUsrPtr ((LPVOID)dwUsrSP, sizeof(DWORD), TRUE))) {
        // back stack pointer. reset stack, and raise bad stack exception
        dwErr = STATUS_BAD_STACK;
        
    }
#ifdef x86
    // x86 - need to update return address
    else if (!CeSafeCopyMemory (&pcstk->retAddr, (LPCVOID) dwUsrSP, sizeof (RETADDR))) {

        // bad stack - can't read return address/exception-list from user stack.
        dwErr = STATUS_BAD_STACK;
        pcstk->retAddr = NULL;
    }
#endif

    DEBUGMSG(ZONE_OBJDISP||dwErr,
            (TEXT("SetupCstk: pcstk = %8.8lx, ra=%8.8lx mode=%x prevSP = %8.8lx, oldTLS = %8.8lx\r\n"),
            pcstk, pcstk->retAddr, pcstk->dwPrcInfo, pcstk->dwPrevSP, pcstk->pOldTls));

    return dwErr;
}

RETADDR SetupErrorReturn (PCALLSTACK pcstk, int iMethod, int iApiSet, DWORD dwErr, const CINFO *pci)
{
    RETADDR pfn;

    SwitchActiveProcess (pcstk->pprcLast);
    SwitchVM (pcstk->pprcVM);
    pcstk->dwPrcInfo &= ~CST_UMODE_SERVER;

    if (pcstk->phd) {
        UnlockHandleData (pcstk->phd);
        pcstk->phd = NULL;
    }

    if (pci && !pci->dwServerId) {
        // kernel API set; always return 0
        pcstk->args[0] = 0;
        KSetLastError (pCurThread, dwErr);
        pfn = (RETADDR) ErrorReturn;
        
    } else if ((ERROR_INVALID_HANDLE == dwErr)
        && ((pcstk->args[0] = BadHandleError (iApiSet, iMethod, pci)) != STATUS_INVALID_SYSTEM_SERVICE)) {
        // handle based calls that we know what's the error value to return, 
        KSetLastError (pCurThread, dwErr);
        pfn = (RETADDR) ErrorReturn;
        
    } else {
        // don't know what the return value should be, redirect the call the RaiseException.
        // In case of bad stack, reset the thread's stack ptr.
        if ((STATUS_BAD_STACK == dwErr) && (pcstk->dwPrcInfo & CST_MODE_FROM_USER)) {
            pcstk->dwPrevSP = ResetThreadStack (pCurThread);
        }
        DEBUGMSG (1, (L"SetupErrorReturn: Raising exception because of %s \r\n",
                  (STATUS_STACK_OVERFLOW == dwErr)? L"Secure Stack Overflow" : L"invalid argument"));
        pcstk->args[0] = dwErr;
        pcstk->args[1] = EXCEPTION_NONCONTINUABLE;
        pcstk->args[2] = pcstk->args[3] = 0;
        pfn = (RETADDR) CaptureContext;
    }

    pcstk->dwNewSP   = 0;
    return pfn;
}

static HANDLE GetHandleArg (PCALLSTACK pcstk)
{
    HANDLE h;
    
#ifdef x86
    // x86 argument is on user stack, need to be careful accessing it
    if (!CeSafeCopyMemory (&h, APIArgs (pcstk), sizeof (h))) {
        h = NULL;
    }
#else
    h = (HANDLE) pcstk->args[0];
#endif

    return h;
}

static BOOL CopyArgs (PCALLSTACK pcstk, int nArgs)
{
    BOOL fRet = TRUE;

#ifdef x86
    if (nArgs) {
        fRet = CeSafeCopyMemory (pcstk->args, APIArgs (pcstk), nArgs * REGSIZE);
    }
#else
    // all other architecture has the 1st 4 args in right place already
    if (nArgs > 4) {
#ifdef ARM
        LPVOID pArg4 = (LPVOID) pcstk->dwPrevSP;
#else
        LPVOID pArg4 = (LPVOID) (pcstk->dwPrevSP + 4 * REGSIZE);
#endif
        fRet = CeSafeCopyMemory (&pcstk->args[4], pArg4, (nArgs-4) * REGSIZE);
    }
#endif

    return fRet;
}

DWORD DecodeAPI (PCALLSTACK pcstk,
    int *piApiSet,          // OUT   - API set #
    PAPICALLINFO pApiInfo) // IN/OUT - dwMethod is filled up
{
    const CINFO *pci     = NULL;
    DWORD       dwErr    = 0;
    int         iMethod  = pApiInfo->dwMethod;
    int         iApiSet  = NUM_API_SETS;

    if (iMethod <= 0) {
        iMethod  = -iMethod;
        iApiSet  = (iMethod >> APISET_SHIFT) & APISET_MASK;
        iMethod &= METHOD_MASK;

        if (!iMethod                                    // index 0 is not allowed
            || !(pci = SystemAPISets[iApiSet])) {       // API set not registered

#ifdef DEBUG
            if (iMethod) {
                NKD (TEXT("!!DecodeAPI: Invalid API: iApiSet = %8.8lx '%.4a', iMethod = %8.8lx\n"),
                    iApiSet, pci ? pci->acName : "NONE", iMethod);
            } else {
                NKD (TEXT("!!DecodeAPI: calling close function (iMethod = 0) directly is not allowed\r\n"));
            }
#endif
            dwErr = STATUS_INVALID_SYSTEM_SERVICE;
            
        } else {
            DEBUGCHK(pci->type == 0 || pci->type == iApiSet);
            iApiSet = pci->type;
        }
    }

    if (iApiSet && !dwErr) {
        // handle based API sets

        if (iMethod <= 1) {
            DEBUGMSG (1, (TEXT("!!DecodeAPI: calling close/preclose function directly, not allowed\r\n"), iApiSet));
            dwErr = STATUS_INVALID_SYSTEM_SERVICE;

        } else {

            HANDLE h = GetHandleArg (pcstk);
            // Lock the handle and convert pseudo handle to real handles
            PHDATA phd  = LockHandleParam (h, pActvProc);
            
            // validate the handle is the reqested type
            if (!phd || (iApiSet != phd->pci->type)) {
                DEBUGMSG (1, (L"!!DecodeAPI: invalid handle! (h = %8.8lx, iApiSet = %8.8lx '%.4a', pActvProc->dwId = %8.8lx)\r\n",
                    h, iApiSet, phd ? phd->pci->acName : "NONE", pActvProc->dwId));
                UnlockHandleData (phd);
                dwErr = ERROR_INVALID_HANDLE;
                
            } else {
                pcstk->phd = phd;
                pci = phd->pci;
            }
        }
    }

    PREFAST_DEBUGCHK (dwErr || pci);

    if (!dwErr && SecureStackSpaceLow (pcstk, pci->dwServerId)) {
        dwErr = STATUS_STACK_OVERFLOW;
        pci   = NULL;       // force RaiseException
    }

    // update return value regardless error or not
    pApiInfo->dwMethod = iMethod; 
    pApiInfo->pci = pci;
    *piApiSet = iApiSet;

    if (!dwErr && (iMethod >= pci->cMethods)) {
        dwErr = STATUS_INVALID_SYSTEM_SERVICE;

    }
    DEBUGMSG(ZONE_OBJDISP && !dwErr,
            (TEXT("DecodeAPI: calling '%.4a' API #%d. disp=%d.\r\n"), pci->acName, iMethod, pci->disp));

    return dwErr;
}

#define MAX_TOTAL_ARG_SIZE    (VM_SHARED_HEAP_BASE)     // total size of argument cannot exceed 0x70000000

static DWORD ValidateArgs (REGTYPE *pArgs, LPDWORD pSizeArgs, int nArgs, ULONGLONG ui64Sig, BOOL fUMode, LPDWORD pdwTotalSize)
{
    int      idx;
    DWORD    dwCurSig;

    DEBUGCHK (nArgs < MAX_NUM_PSL_ARG);
    *pdwTotalSize = 0;

    for (idx = 0; ui64Sig && (idx < nArgs); idx ++, ui64Sig >>= ARG_TYPE_BITS) {

        // Need to specifically cast it to DWORD. The reason being that for 64-bit CPU,
        // the upper 32 bits can be anything.
        if ((DWORD) pArgs[idx]) {

            switch (dwCurSig = ((DWORD) ui64Sig & ARG_TYPE_MASK)) {
                
            case ARG_DW:
                continue;
                
            case ARG_IO_PDW:
            case ARG_O_PDW:
            case ARG_I_PDW:
                // ptr to dword
                pSizeArgs[idx] = sizeof (DWORD);
                break;
                
            case ARG_IO_PI64:
            case ARG_O_PI64:
                // ptr to I64
                pSizeArgs[idx] = sizeof (ULONGLONG);
                break;
                
            case ARG_IO_PTR:
            case ARG_I_PTR:
            case ARG_O_PTR:
                // ptr, whose size is in the next argument
                if (idx >= nArgs) {
                    DEBUGMSG (1, (L"ValidateArgs: Arg %u invalid pointer 0x%08x\r\n", idx, pArgs[idx]));
                    return ERROR_INVALID_PARAMETER;
                }
                pSizeArgs[idx] = (DWORD) pArgs[idx+1];
                break;
                
            case ARG_I_ASTR:
                // ascii string, validate starting address is a valid user ptr before getting length
                if (fUMode && !IsValidUsrPtr ((LPCVOID)pArgs[idx], 1, FALSE)) {
                    DEBUGMSG (1, (L"ValidateArgs: Arg %u invalid ASCII string pointer 0x%08x\r\n", idx, pArgs[idx]));
                    return ERROR_INVALID_PARAMETER;
                }
                pSizeArgs[idx] = pArgs[idx]? (strlen ((LPCSTR)pArgs[idx]) + 1) : 0;
                continue;
                
            case ARG_I_WSTR:
                // unicode string, validate starting address is a valid user ptr before getting length
                if (fUMode && !IsValidUsrPtr ((LPCVOID)pArgs[idx], 1, FALSE)) {
                    DEBUGMSG (1, (L"ValidateArgs: Arg %u invalid Unicode string pointer 0x%08x\r\n", idx, pArgs[idx]));
                    return ERROR_INVALID_PARAMETER;
                }
                pSizeArgs[idx] = pArgs[idx]? ((NKwcslen ((LPCWSTR)pArgs[idx]) + 1) << 1) : 0;  // * sizeof (WCHAR)
                continue;
            default:
                // unknown signature
                DEBUGMSG (1, (L"ValidateArgs: Arg %u invalid type\r\n", idx));
                return ERROR_INVALID_PARAMETER;
            }

            // validate the ptr is a valid user ptr
            if (fUMode && !IsValidUsrPtr ((LPCVOID)pArgs[idx], pSizeArgs[idx], dwCurSig & ARG_O_BIT)) {
                DEBUGMSG (1, (L"ValidateArgs: Arg %u invalid pointer 0x%08x\r\n", idx, pArgs[idx]));
                return ERROR_INVALID_PARAMETER;
            }

            // IsValidUsrPtr validated that the size given by user is >= 0 as an integer. So the following calculation
            // will never wrap around
            *pdwTotalSize += ALIGN16_UP (pSizeArgs[idx]);

            if (*pdwTotalSize >= MAX_TOTAL_ARG_SIZE) {
                DEBUGMSG (1, (L"ValidateArgs: total argument size too big, total size = 0x%08x\r\n", *pdwTotalSize));
                return ERROR_INVALID_PARAMETER;
            }
        }
    }

    return 0;
}

//
// User mode argument - always copy-in/copy-out
//
static DWORD SetupUmodeArgs (PPROCESS pprcServer, PARGINFO paif, PAPICALLINFO pApiInfo, LPDWORD pdwNewSP)
{
    DWORD  dwErr = 0;
    int nArgs = pApiInfo->nArgs;
    ULONGLONG u64Sig = pApiInfo->u64Sig;
    DWORD  cbStkSize = BLOCKALIGN_UP (pApiInfo->dwTotalSize + MIN_STACK_FOR_SERVER);
    LPBYTE pKrnStk   = VMCreateStack (g_pprcNK, cbStkSize);
    LPBYTE pSvrStk   = VMReserve (pprcServer, cbStkSize, MEM_AUTO_COMMIT, 0);

    DEBUGMSG (ZONE_OBJDISP, (L"SetupUmodeArgs, nArgs = %d, u64Sig = %I64x\r\n", nArgs, u64Sig));

    DEBUGMSG (pApiInfo->dwTotalSize > (VM_BLOCK_SIZE >> 1), (L"Large Usermode argument size found, total-size = 0x%x\r\n", pApiInfo->dwTotalSize));

    // create a stack in server process
    if (!pKrnStk || !pSvrStk) {
        dwErr = ERROR_NOT_ENOUGH_MEMORY;

    } else {
        LPDWORD     pKrnTls    = TLSPTR (pKrnStk, cbStkSize);
        LPDWORD     pSvrTls    = TLSPTR (pSvrStk, cbStkSize);
        LPDWORD     pdwDstOfst = ARG_OFFSET_FROM_TLS (pKrnTls);
        DWORD       dwKrnSP    = (DWORD) pdwDstOfst;
        PARGENTRY   pEnt;
        DWORD       cbSize;
        int         idx;

        paif->nArgs = 0;
        paif->wStkBaseInBlocks = (WORD) ((DWORD) pKrnStk >> VM_BLOCK_SHIFT);
        paif->wStkSizeInBlocks = (WORD) ((DWORD) cbStkSize >> VM_BLOCK_SHIFT);

        DEBUGCHK (!(dwKrnSP & STKMSK));

        for (idx = 0; u64Sig && (idx < nArgs); idx ++, u64Sig >>= ARG_TYPE_BITS) {

            if (cbSize = pApiInfo->ArgSize[idx]) {
                
                if (paif->nArgs >= MAX_NUM_OUT_PTR_ARGS) {
                    NKDbgPrintfW (L"!!!!API CONTAINS TOO MANY OUT-POINTER ARGUMENTS, NEED TO INCREASE MAX OR CHANGE API (argcnt = %d)!!!!\r\n", paif->nArgs);
                    DEBUGCHK (0);
                    dwErr = ERROR_INVALID_PARAMETER;
                    break;
                }

                dwKrnSP -= ALIGN16_UP (cbSize);     // 16 byte align the pointer

                DEBUGMSG (ZONE_OBJDISP, (L"SetupUmodeArgs: ptr arg%d, cbSize = %d, use CICO\r\n", idx, cbSize));

                // Copy parameter regardless it's in or out. The reason is, that on some
                // API specs, the out parameter should not be changed if the API failed. If we don't 
                // copy in the parameter, the out-only parameter can be changed when API failed.
                if (!CeSafeCopyMemory ((LPVOID) dwKrnSP, (LPVOID) pApiInfo->pArgs[idx], cbSize)) {
                    dwErr = ERROR_INVALID_PARAMETER;
                    break;
                }

                // setup information on ArgInfo structure for out parameter
                if ((DWORD) u64Sig & ARG_O_BIT) {
                    pdwDstOfst[paif->nArgs] = dwKrnSP - (DWORD) pKrnStk;
                    pEnt            = &paif->argent[paif->nArgs ++];
                    pEnt->ptr       = (LPVOID) pApiInfo->pArgs[idx];
                    pEnt->cbSize    = cbSize;
                }
                
                // replace argument with the new space allocated on server stack
                pApiInfo->pArgs[idx] = dwKrnSP - (DWORD) pKrnStk + (DWORD) pSvrStk;
                    

            } else if (u64Sig & (ARG_O_BIT|ARG_I_BIT)) {
                // zero sized ptr, zero the pointer itself
                pApiInfo->pArgs[idx] = 0;
            }
        }

        DEBUGCHK(pApiInfo->pci);
        if (dwErr && pApiInfo->pci->pfnErrorHandler) {
            // failed to setup arguments for the normal API call;
            // replace the call with the registered error handler for this API set            
            dwErr = 0;
            paif->nArgs = 0;
            pApiInfo->dwTotalSize = 0;
            pApiInfo->nArgs = 1;
            pApiInfo->u64Sig = FNSIG1 (DW);
            pApiInfo->pArgs[0] = pApiInfo->dwMethod;
            pApiInfo->ArgSize[0] = 0;
            pApiInfo->pfn = pApiInfo->pci->pfnErrorHandler;
            
        }
        
        // setup arguments on the server stack
        if (!dwErr) {

            DWORD   dwOfst; 

            if (nArgs) {
                // copy the 'fixed' arguments onto server stack
                dwKrnSP -= ALIGN16_UP (nArgs * sizeof(REGTYPE));            // 16 bytes align stack
                memcpy ((LPVOID) dwKrnSP, pApiInfo->pArgs, nArgs * sizeof(REGTYPE));

            }
#ifdef x86
            // terminate exception list
            TLS2PCR(pKrnTls)->ExceptionList = REGISTRATION_RECORD_PSL_BOUNDARY;

            // 'push' return address on server stack
            dwKrnSP -= REGSIZE;
            *(LPDWORD)dwKrnSP = SYSCALL_RETURN;
#endif
            dwOfst = PAGEALIGN_DOWN (dwKrnSP - (DWORD) pKrnStk);

            // fixup tls information
            pKrnTls[PRETLS_STACKBOUND] = dwOfst + (DWORD) pSvrStk;
            pKrnTls[PRETLS_STACKBASE]  = (DWORD) pSvrStk;
            pKrnTls[PRETLS_STACKSIZE]  = cbStkSize;
            pKrnTls[TLSSLOT_RUNTIME]   = (DWORD) CRTGlobFromTls (pSvrTls);

            // virtual copy to server process
            if (!VMFastCopy (pprcServer, (DWORD) pSvrStk + dwOfst, g_pprcNK, (DWORD) pKrnStk + dwOfst, cbStkSize - dwOfst, PAGE_READWRITE)) {
                dwErr = ERROR_NOT_ENOUGH_MEMORY;
            } else {
                // change non-secure TLS to the new TLS, and update newSP
                pCurThread->tlsNonSecure = pSvrTls;
                *pdwNewSP = dwKrnSP - (DWORD) pKrnStk + (DWORD) pSvrStk;
#ifdef ARM
                if (IsVirtualTaggedCache () && (pprcServer == pVMProc)) {
                    // this can only happen when kernel calls into user-mode server from a kernel thread. 
                    // We need to write-back cache for we're writing to stack via kernel address. In the case
                    // where server is not the current VM process, switch VM will write-back and discard all caches.
                    OEMCacheRangeFlush (pKrnStk + dwOfst, cbStkSize - dwOfst, CACHE_SYNC_WRITEBACK);
                }
#endif
            }

        }

    }

    if (dwErr) {
        // free stacks
        if (pSvrStk) {
            VERIFY (VMFreeAndRelease (pprcServer, pSvrStk, cbStkSize));
        }
        if (pKrnStk) {
            // don't put the stack back to cache list on error
            VERIFY (VMFreeAndRelease (g_pprcNK, pKrnStk, cbStkSize));
            // VMFreeStack (g_pprcNK, (DWORD) pKrnStk, cbStkSize);
        }

        paif->wStkBaseInBlocks = 0;
        paif->wStkSizeInBlocks = 0;
        *pdwNewSP = 0;
    }
    
    DEBUGMSG (ZONE_OBJDISP||dwErr, (L"SetupUmodeArgs: returns, dwErr = %8.8lx\r\n", dwErr));
    return dwErr;
}

DWORD DoValidateArgs (PCALLSTACK pcstk, PAPICALLINFO pApiInfo, DWORD fUMode)
{
    DWORD dwErr = 0;
    __try {
        dwErr = ValidateArgs (pApiInfo->pArgs, pApiInfo->ArgSize, pApiInfo->nArgs, pApiInfo->u64Sig, fUMode, &pApiInfo->dwTotalSize);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        dwErr = ERROR_INVALID_PARAMETER;
    }

    if (dwErr && pApiInfo->pci->pfnErrorHandler) {
        // if a custom error handler is set for this API set, return that handler
        pApiInfo->dwTotalSize = 0;
        pApiInfo->nArgs = 1;
        pApiInfo->u64Sig = FNSIG1 (DW);
        pApiInfo->pArgs[0] = pApiInfo->dwMethod;
        pApiInfo->ArgSize[0] = 0;
        pApiInfo->pfn = pApiInfo->pci->pfnErrorHandler;

        if (pcstk->phd) {
            UnlockHandleData (pcstk->phd);
            pcstk->phd = NULL;
        }

        dwErr = 0;
    }

    return dwErr;
}

static DWORD SetupArguments (PCALLSTACK pcstk, PAPICALLINFO pApiInfo)
{
    DWORD         dwErr   = 0;

    // set number of args and the method signatures
    pApiInfo->u64Sig = pApiInfo->pci->pu64Sig[pApiInfo->dwMethod];
    pApiInfo->nArgs = (int) pApiInfo->u64Sig & ARG_TYPE_MASK;

    // copy all arguments onto secure stack first
    if (!CopyArgs (pcstk, pApiInfo->nArgs)) {
        return STATUS_BAD_STACK;
    }
    pApiInfo->pArgs = pcstk->args;

    // validate argument, calculate lengths
    if (pApiInfo->u64Sig >>= ARG_TYPE_BITS) {        // >>4 to get rid of arg count from signature
        if (dwErr = DoValidateArgs (pcstk, pApiInfo,  pcstk->dwPrcInfo & CST_MODE_FROM_USER))
            return dwErr;
    }

    // replace arg0 for handle based API
    if (pcstk->phd) {
        pcstk->args[0] = (REGTYPE) pcstk->phd->pvObj;
    }

    if (pApiInfo->pci->dwServerId) {

        PPROCESS pServer = LockServerWithId (pApiInfo->pci->dwServerId);

        if (!pServer) {
            dwErr = ERROR_INVALID_HANDLE;
            
        } else if (pServer == g_pprcNK) {

            // kernel server, just switch active process
            SwitchActiveProcess (pServer);
            
        } else {

            ARGINFO aif;
            
            // user mode server
            pcstk->dwPrcInfo |= CST_UMODE_SERVER;

            if (pServer == pcstk->pprcLast) {

                // intra process PSL call. No need to marshal arguments
                DEBUGCHK (pcstk->pprcVM == pServer);
                
                pcstk->arginfo.nArgs = 0;
                pcstk->arginfo.wStkBaseInBlocks = 0;
                pcstk->arginfo.wStkSizeInBlocks = 0;
#ifdef ARM
                // ARM - arg0-3 are not on stack at the point of call
                pcstk->dwNewSP = pcstk->dwPrevSP - 4 * REGSIZE;
#else
                pcstk->dwNewSP = pcstk->dwPrevSP;
#endif
                __try {
#ifdef x86
                    // terminate exception list
                    TLS2PCR(pCurThread->tlsNonSecure)->ExceptionList = REGISTRATION_RECORD_PSL_BOUNDARY;
    
                    // replace return address with trap return
                    *(LPDWORD)pcstk->dwNewSP = SYSCALL_RETURN;

                    // replace arg0 for handle based API / error handler
                    if (pApiInfo->nArgs) {
                        *(LPDWORD)(pcstk->dwNewSP+4) = pcstk->args[0]; 
                    }
#else
                    if (pApiInfo->nArgs) {
                        memcpy ((LPVOID)pcstk->dwNewSP, pcstk->args, pApiInfo->nArgs*REGSIZE);
                    }
#endif
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                    dwErr = STATUS_BAD_STACK;
                    // error, unlock server
                    UnlockServerWithId (pServer->dwId);
                }


            // setup stack/arguments in server process
            }
            else if (dwErr = SetupUmodeArgs (pServer, &aif, pApiInfo, &pcstk->dwNewSP)) {

                // error (invalid arguments), unlock server
                UnlockServerWithId (pServer->dwId);

                pcstk->arginfo.nArgs = 0;

            } else {
                // successfully marshalled arguments, switch active/VM to server
                SwitchActiveProcess (pServer);
                SwitchVM (pServer); // this will cause cache flush on VIVT (ARM), which will make cache coherent.
                
                // save argument information in callstack structure
                pcstk->arginfo = aif;
                CELOG_ThreadMigrate(dwActvProcId);
            }

        }
    }
    
    return dwErr;
}


//
// pdwParam - contains the method to call on entrance,
//            contains the new SP on exit (0 if kernel mode server)
//
RETADDR ObjectCall (PCALLSTACK pcstk)
{
    DWORD       dwErr;
    int         iApiSet = 0;
    APICALLINFO ApiInfo = {0};    
    ApiInfo.dwMethod = pcstk->iMethod;
    
    randdw1 = ((randdw1<<5) | (randdw1>>27)) ^ (CurMSec & 0x1f);
    
    if (   (dwErr = SetupCstk (pcstk))
        || (dwErr = DecodeAPI (pcstk, &iApiSet, &ApiInfo))
        || (dwErr = SetupArguments (pcstk, &ApiInfo))) {    // NOTE: CopyArgument would switch to server process if needed

        ApiInfo.pfn = SetupErrorReturn (pcstk, ApiInfo.dwMethod, iApiSet, dwErr, ApiInfo.pci);

    } else if (!ApiInfo.pfn) {

#ifdef x86
        // setup floating point emulation handlers depending
        // on the PSL server current thread is migrating to.
        if (g_pCoredllFPEmul)
            SetFPEmul (pCurThread, (pcstk->dwPrcInfo & CST_UMODE_SERVER) ? USER_MODE : KERNEL_MODE);
#endif

        // call internal/external interface based on the caller's mode
        if (pcstk->dwPrcInfo & CST_MODE_FROM_USER) {
            ApiInfo.pfn = ApiInfo.pci->ppfnExtMethods[ApiInfo.dwMethod];
        } else {

            if (ApiInfo.pci->dwServerId) {
                SetDirectCall ();
            }
        
            ApiInfo.pfn = ApiInfo.pci->ppfnIntMethods[ApiInfo.dwMethod];
        }
        
#ifdef NKPROF
        if (g_ProfilerState.dwProfilerFlags & PROFILE_OBJCALL)
            ProfilerHit((UINT)ApiInfo.pfn);
#endif        
    }
    
    DEBUGMSG (ZONE_OBJDISP|dwErr, (L"ObjectCall: returning %8.8lx\r\n", ApiInfo.pfn));

    return ApiInfo.pfn;
}

RETADDR SetupCallToUserServer (PHDATA phd, PDHCALL_STRUCT phc, PCALLSTACK pcstk)
{
    PTHREAD       pth = pCurThread;
    DWORD         dwErr;
    PPROCESS     pServer;
    APICALLINFO ApiInfo = {0};

    // setup the api call info
    ApiInfo.dwMethod = (DWORD) phc->dwAPI;
    ApiInfo.nArgs = (int) phc->cArgs + 1; // the count passed in excludes the handle
    ApiInfo.pArgs = phc->args;

    DEBUGMSG (ZONE_OBJDISP, (L"SetupCallToUserServer: phd = %8.8lx, phc = %8.8lx, pcstk = %8.8lx\r\n", phd, phc, pcstk));

    if ((DWORD) phd < NUM_API_SETS) {
        // calling PSL notify of user mode server
        DEBUGCHK (2 == phc->cArgs);
        DEBUGCHK (0 == ApiInfo.dwMethod);
        DEBUGCHK (SH_LAST_NOTIFY <= (DWORD) phd);
        ApiInfo.pci = SystemAPISets[(DWORD) phd];
    } else {
        // calling handle based API
        ApiInfo.pci = phd->pci;
    }

    // fill in the info for pcstk, dwPrevSP and retAddr is filled before calling this function
    pcstk->dwPrcInfo = CST_UMODE_SERVER;
    pcstk->pcstkNext = pth->pcstkTop;
    pcstk->phd       = (ApiInfo.dwMethod > 1)? phd : NULL; // don't unlock handle on return when calling close and pre-close 
    pcstk->pOldTls   = pth->tlsNonSecure;
    pcstk->pprcLast  = pActvProc;
    pcstk->pprcVM    = pth->pprcVM;
    pcstk->dwNewSP   = 0;

    SaveDirectCall (pcstk);
    
    // link into threads callstack
    pth->pcstkTop    = pcstk;

    if (!(pServer = LockServerWithId (ApiInfo.pci->dwServerId))) {
        dwErr = ERROR_INVALID_HANDLE;
        
    } else {

        if ((DWORD) phd > NUM_API_SETS) {
            // setup call to handle based user API

            DEBUGCHK (pth->tlsPtr == pth->tlsSecure);
            DEBUGCHK (ApiInfo.pci->dwServerId && (ApiInfo.pci->dwServerId != g_pprcNK->dwId));

            // replace arg 0 with actual object
            phc->args[0] = (REGTYPE) phd->pvObj;
            ApiInfo.u64Sig = ApiInfo.pci->pu64Sig[ApiInfo.dwMethod] >> ARG_TYPE_BITS;    // >> 4 to get rid of arg count            
            dwErr = DoValidateArgs (pcstk, &ApiInfo, FALSE);

        } else {
            // PSL notify user mode server
            DEBUGMSG (ZONE_OBJDISP, (L"Notifying Usermode server %8.8lx\r\n", pServer->dwId));
            dwErr = 0;
        }

        if (!dwErr) {
            dwErr = SetupUmodeArgs (pServer, &pcstk->arginfo, &ApiInfo, &pcstk->dwNewSP);
        } 

        if (dwErr) {
            UnlockServerWithId (ApiInfo.pci->dwServerId);
            
        } else {

#ifdef x86
            // setup floating point emulation handlers for the current thread
            if (g_pCoredllFPEmul)        
                SetFPEmul (pth, USER_MODE);
#endif        

            // switch process to server
            SwitchActiveProcess (pServer);
            SwitchVM (pServer);
            CELOG_ThreadMigrate(dwActvProcId);
        }
    }

    if (dwErr) {
        pcstk->dwPrcInfo = 0;   // clear user mode server bit
        pcstk->dwNewSP = 0;     // new sp set to 0 to indicate kernel call
        pcstk->args[0] = phc->dwErrRtn;
        ApiInfo.pfn = (RETADDR) ErrorReturn;

    } else if (!ApiInfo.pfn) {
        // setup pfn to the external method if there was no error or if there is no error handler
        ApiInfo.pfn = ApiInfo.pci->ppfnExtMethods[ApiInfo.dwMethod];
    }

    DEBUGMSG (ZONE_OBJDISP, (L"SetupCallToUserServer: returning %8.8lx, dwErr = %8.8lx\r\n", ApiInfo.pfn, dwErr));

    return ApiInfo.pfn;
}

void UnwindOneCstk (PCALLSTACK pcstk, BOOL fCleanOnly)
{
    PTHREAD  pth     = pCurThread;
    PHDATA   phd     = pcstk->phd;

    // switch active process back to caller
    PPROCESS pprcServer = SwitchActiveProcess (pcstk->pprcLast);
    
    NKSetDirectCall (pcstk->dwPrcInfo & CST_DIRECTCALL);

    if (CST_UMODE_SERVER & pcstk->dwPrcInfo) {

        LPDWORD  pSvrTls = pth->tlsNonSecure;

        DEBUGCHK (pprcServer == pVMProc);

        // clear CST_UMODE_SERVER to avoid accidental double freeing
        pcstk->dwPrcInfo &= ~CST_UMODE_SERVER;

        //
        // if VM/TLS didn't change, we didn't do any marshaling for the call (i.e. server called
        // into itself, and there is no need to cleanup.)
        //
        if ((pprcServer != pcstk->pprcLast) || (pSvrTls != pcstk->pOldTls)) {

            PARGINFO  paif      = &pcstk->arginfo;
            LPBYTE    pKrnStk   = (LPBYTE) ((DWORD) paif->wStkBaseInBlocks << VM_BLOCK_SHIFT);
            DWORD     cbStkSize =          ((DWORD) paif->wStkSizeInBlocks << VM_BLOCK_SHIFT);
            LPDWORD   pKrnTls   = TLSPTR (pKrnStk, cbStkSize);
            LPBYTE    pSvrStk   = pKrnStk + (DWORD) pSvrTls - (DWORD) pKrnTls;

            DEBUGCHK (pKrnStk);
            DEBUGCHK (pVMProc == pprcServer);
            
            // release Server virtual Copy of the stack.
            VMFreeAndRelease (pprcServer, pSvrStk, cbStkSize);

#ifdef ARM
            // if VM doesn't switch, we need to explicitly discard cache, or we can run into cache coherent issue
            if (IsVirtualTaggedCache () && (pprcServer == pcstk->pprcVM)) {
                OEMCacheRangeFlush (NULL, 0, CACHE_SYNC_DISCARD);
            }
#endif
            // switch back to caller VM - this will cause cache to be flushed on VIVT cache
            SwitchVM (pcstk->pprcVM);

            // restore tlsNonSecure
            pth->tlsNonSecure = pcstk->pOldTls;
            
            if (!fCleanOnly) {
                // returning from user mode server, copy-out arguments
                PARGENTRY pEnt;
                int       idx, nArgs = paif->nArgs;
                LPDWORD   pdwSrcOfst = ARG_OFFSET_FROM_TLS (pKrnTls);
                DWORD     dwOfst;
                
                // copy-out or VirtualFree args
                for (idx = 0; idx < nArgs; idx ++) {

                    pEnt   = &paif->argent[idx];
                    dwOfst = pdwSrcOfst[idx];
                    
                    if ((dwOfst < cbStkSize) && (dwOfst + pEnt->cbSize < cbStkSize)) {

                        // copy-out
                        VERIFY (CeSafeCopyMemory (pEnt->ptr, pKrnStk + dwOfst, pEnt->cbSize));
                    } else {

                        // buffer overrrun in server, or malicious 'server' tempering with stack.
                        DEBUGCHK (0);
                        break;
                    }
                }
            }

            // update stack bound to use kernel stack
            pKrnTls[PRETLS_STACKBOUND] = (DWORD) pKrnStk + cbStkSize - VM_PAGE_SIZE;
            VERIFY (VMDecommit (g_pprcNK, pKrnStk, cbStkSize - VM_PAGE_SIZE, VM_PAGER_AUTO));

            // free the dynamically create stack
            VMFreeStack (g_pprcNK, (DWORD) pKrnStk, cbStkSize);

        }
        
        // unlock server process
        UnlockServerWithId (pprcServer->dwId);
        
        CELOG_ThreadMigrate(dwActvProcId);

    }
    else if (CST_CALLBACK & pcstk->dwPrcInfo) { 
        
        SwitchVM (pcstk->pprcVM);
        pth->tlsNonSecure = pcstk->pOldTls;
    }

    CleanupTokenList (pcstk);

    if (phd) {
        pcstk->phd = NULL;
        UnlockHandleData (phd);
    }

    // update pcstk->dwPrcInfo to the mode to return to
    pcstk->dwPrcInfo &= CST_MODE_FROM_USER;
    
#ifdef x86

    // x86 restore exception pointer on user stack
    TLS2PCR ((pcstk->dwPrcInfo? pth->tlsNonSecure : pth->tlsSecure))->ExceptionList = pcstk->regs[REG_EXCPLIST];

    // setup floating point emulation handlers for the current thread
    if (g_pCoredllFPEmul)        
        SetFPEmul (pth, (pcstk->dwPrcInfo) ? USER_MODE : KERNEL_MODE);

#endif

}


BOOL DelaySuspendOrTerminate (void)
{
    PTHREAD pth = pCurThread;
    BOOL    fTerminated = FALSE;

    while (TRUE) {
        
        if (GET_DYING(pth) && !GET_DEAD(pth)) {
            // being terminated - terminate now.
            pth->bPendSusp = 0;     // don't suspend if we were being suspended
            SET_DEAD(pth);
            CLEAR_USERBLOCK(pth);
            CLEAR_DEBUGWAIT(pth);
            fTerminated = TRUE;
            break;
        }

        if (!pth->bPendSusp) {
            break;
        }

        // delay suspended - suspend now.
        KCall ((PKFN) SuspendSelfIfNeeded);

    }
    
    return fTerminated;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void ServerCallReturn (void)
{
    PTHREAD     pth   = pCurThread;
    PCALLSTACK  pcstk = pth->pcstkTop;
    PPROCESS    pprc  = pcstk->pprcLast;

    DEBUGMSG (ZONE_OBJDISP, (L"SCR: pcstk = %8.8lx, pprc = %8.8lx\n", pcstk, pprc));

    UnwindOneCstk (pcstk, FALSE);

    if ((pprc == pth->pprcOwner)
        && (DelaySuspendOrTerminate ())) {
        pcstk->retAddr = (RETADDR)pExitThread;
    }

    /* unlink CALLSTACK structure from list */
    pth->pcstkTop = pcstk->pcstkNext;

    // restore TLS pointer
    KPlpvTls = pth->tlsPtr = (pcstk->dwPrcInfo? pth->tlsNonSecure : pth->tlsSecure);

    DEBUGMSG(ZONE_OBJDISP, (TEXT("SCRet: Before return pcstk->dwPrcInfo = %x, pcstk->dwPrevSP = %8.8lx, ra = %8.8lx, TLS = %8.8lx\n"), 
        pcstk->dwPrcInfo, pcstk->dwPrevSP, pcstk->retAddr, pth->tlsPtr));
}

#define ARG_SIZE    REGSIZE

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
RETADDR NKPrepareCallback (PCALLSTACK pNewcstk)
{
    PCALLSTACK    pcstk;
    PTHREAD       pth = pCurThread;
#ifdef ARM
    // ARM specific - the 4 arguments are pushed onto stack in NKPerformCallback, where the original SP doesn't take that 
    //                into account. need to decrement it by 16 to get to the 1st argument.
    REGTYPE       *pArgs = (REGTYPE*) (pNewcstk->dwPrevSP - 16); // argument list is on top of stack at the point of calling
#else
    REGTYPE       *pArgs = (REGTYPE*) pNewcstk->dwPrevSP;        // argument list is on top of stack at the point of calling
#endif
    PCALLBACKINFO pcbi;
    RETADDR       pfn = (RETADDR) ErrorReturn;

#ifdef MIPS
    DEBUGCHK (!((DWORD) pArgs & (sizeof (REG_TYPE) -1)));
#endif

#ifdef x86
    // x86 specific - return address on stack, advance pArgs to real arguments.
    pNewcstk->retAddr = (RETADDR) (*pArgs ++);
#endif
    pNewcstk->dwNewSP = 0;

    pcbi = Arg (pArgs, PCALLBACKINFO);
    WriteArg (pArgs, DWORD, (DWORD) pcbi->pvArg0);

    DEBUGMSG(ZONE_OBJDISP, (TEXT("NKPrepareCallback: pNewcstk = %8.8lx, pcbi = %8.8lx pArgs = %8.8lx, ra=%8.8lx\r\n"),
            pNewcstk, pcbi, pArgs, pNewcstk->retAddr));

    // only kernel can callback
    DEBUGCHK (pActvProc == g_pprcNK);
    DEBUGCHK (pth->tlsPtr == pth->tlsSecure);

    if ((DWORD) pcbi->pfn & 0x80000000) {
        // calling a kernel function, call direct
        DEBUGMSG(ZONE_OBJDISP, (TEXT("NKPrepareCallback: Kernel mode - Call direct to function at %8.8lx\r\n"), pcbi->pfn));
        pfn = (RETADDR) pcbi->pfn;

    } else {
        // user mode, find the entrance to kernel
        for (pcstk = pth->pcstkTop; pcstk && pcbi->hProc && (pcstk->pprcLast->dwId != (DWORD) pcbi->hProc); pcstk = pcstk->pcstkNext) {
            // empty body
        }

        if (!pcstk) {
            // CALL FORWARD!!!
            NKDbgPrintfW (L"Security Violation: Call Forward NOT SUPPORTED\r\n");
            DEBUGCHK (0);
            
        } else if (!pcbi->hProc || !pcbi->pfn) {
            // invalid arguments
            NKDbgPrintfW(L"NKPrepareCallback: invalid arguments hProc:%8.8lx pfn:%8.8lx\r\n", pcbi->hProc, pcbi->pfn);
            
        } else {

            REGTYPE *pUsrArgs = (REGTYPE *) (pcstk->dwPrevSP - 8 * ARG_SIZE);

            DEBUGMSG(ZONE_OBJDISP, (TEXT("NKPrepareCallback: pcstk = %8.8lx\r\n"), pcstk));
            DEBUGCHK (pcstk->pprcLast == pcstk->pprcVM);

            pNewcstk->pprcLast  = pActvProc;
            pNewcstk->pprcVM    = pth->pprcVM;
            pNewcstk->pOldTls   = pth->tlsNonSecure;
            pNewcstk->pcstkNext = pth->pcstkTop;
            pNewcstk->dwPrcInfo = CST_CALLBACK;     // callback always back to user mode
            pNewcstk->phd       = NULL;
            SaveDirectCall (pNewcstk);

            SwitchActiveProcess (pcstk->pprcLast);
            SwitchVM (pcstk->pprcVM);

            __try {
                // copy args to user stack
                memcpy (pUsrArgs, pArgs, 8 * ARG_SIZE);
#ifdef x86
                // x86 specific - update registration pointer, and update return address
                * (-- pUsrArgs) = SYSCALL_RETURN;               // return address is a trap (SYSCALL_RETURN)
#endif
                pNewcstk->args[0]   = pcbi->dwErrRtn;           // save error return value in case we get an unhandle excepiton in callback
                pfn                 = (RETADDR) pcbi->pfn;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                pfn = (RETADDR) ErrorReturn;
            }


            if ((RETADDR) ErrorReturn == pfn) {
                // error - restore VM/active process
                SwitchActiveProcess (pNewcstk->pprcLast);
                SwitchVM (pNewcstk->pprcVM);
                WriteArg (pArgs, DWORD, (DWORD) pcbi->dwErrRtn);
                
            } else {
                pNewcstk->dwNewSP = (DWORD) pUsrArgs;
                pth->tlsNonSecure = pcstk->pOldTls;
                pth->pcstkTop     = pNewcstk;
#ifdef x86
                // x86 specific - update registration pointer, and update return address
                TLS2PCR(pcstk->pOldTls)->ExceptionList = REGISTRATION_RECORD_PSL_BOUNDARY; // terminate user registation pointer

                // setup floating point emulation handlers for the current thread                
                if (g_pCoredllFPEmul)                        
                    SetFPEmul (pth, USER_MODE);
#endif
            }
        }

    }

    randdw1 = ((randdw1<<5) | (randdw1>>27)) ^ (CurMSec & 0x1f);

    DEBUGMSG (ZONE_OBJDISP, (L"Calling  callback function %8.8lx tls = %8.8lx, tlsNonSec = %8.8lx, tlsSec = %8.8lx, dwNewSP = %8.8lx\n",
                pfn, pth->tlsPtr, pth->tlsNonSecure, pth->tlsSecure, pNewcstk->dwNewSP));

    return pfn;
}

void CleanupAllCallstacks (PTHREAD pth)
{
    PCALLSTACK pcstk = pth->pcstkTop;

    DEBUGMSG (ZONE_OBJDISP, (L"Cleaning up callstacks for thread %8.8lx, pcstk=%8.8lx\r\n", pth, pcstk));

    DEBUGCHK (pth->tlsPtr == pth->tlsSecure);
    
    // Unlock all the handles in case we're terminated in the middle of handle-based API.
    // We also need to free temporary stack and VirtualCopy'd arguments created along the API calls
    for ( ; pcstk; pcstk = pcstk->pcstkNext) {
        UnwindOneCstk (pcstk, TRUE);
    }

    // user TLS set to original stack
    pth->tlsNonSecure = TLSPTR ((LPVOID) pth->dwOrigBase, pth->dwOrigStkSize);
    pth->pcstkTop = NULL;
}

//
// NKHandleCall - direct call from kcoredll to make a handle-based API call
// 
DWORD NKHandleCall (
        DWORD dwHtype,          // the expected handle type
        PDHCALL_STRUCT phc      // information of the api to call
        )
{
    DWORD  dwRet;
    DWORD  dwErr = 0;
    PHDATA phd   = LockHandleParam ((HANDLE) phc->args[0], pActvProc);
    DWORD  dwAPI = (DWORD) phc->dwAPI;
    const  CINFO *pci;
    BOOL   fForward = ((int) dwHtype < 0);
    
    DEBUGMSG (ZONE_OBJDISP, (L"NKHandleCall: dwHtype=%d, dwAPI=%d, cArgs=%d, h = %8.8lx, phd = %8.8lx\r\n", dwHtype, phc->dwAPI, phc->cArgs, phc->args[0], phd));

    if (fForward) {
        dwHtype = - (int) dwHtype;
    }

    DEBUGCHK (pActvProc == g_pprcNK);
    if (phd                                         // valid handle
        && ((pci = phd->pci)->type == dwHtype)      // correct handle type
        && (dwAPI > 1)                              // valid api index
        && (dwAPI < pci->cMethods)) {               // valid api index

        if (pci->dwServerId && (pci->dwServerId != g_pprcNK->dwId)) {
            // user mode server
            DEBUGMSG (ZONE_OBJDISP, (L"NKHandleCall: u-mode server\r\n"));
            __try {
                dwRet = MDCallUserHAPI (phd, phc);
                
                // phd is not unlocked here, it is unlocked in ServerCallReturn
                phd = NULL;

            } __except (EXCEPTION_EXECUTE_HANDLER) {
                dwErr = ERROR_INVALID_PARAMETER;
            }
           
        } else {

            BOOL fOldDirect = IsInDirectCall ();
            FARPROC pfnAPI = (FARPROC)pci->ppfnIntMethods[dwAPI];

            if (fForward && (NKGetDirectCallerProcessId () != g_pprcNK->dwId)) {
                // forwarding from external caller - call external method
                pfnAPI = (FARPROC)pci->ppfnExtMethods[dwAPI];   // call external method
                
            }

            if (!pfnAPI) {
                dwErr = ERROR_INVALID_PARAMETER;
                
            } else {
            
                if (!fForward && pci->dwServerId) {
                    SetDirectCall ();
                }
                
                // kmode server, just call direct
                DEBUGMSG (ZONE_OBJDISP, (L"NKHandleCall: kmode server, pfnAPI = 0x%x\r\n", pfnAPI));

                __try {
                    dwRet = MDCallKernelHAPI (pfnAPI, (DWORD) phc->cArgs, phd->pvObj, &phc->args[1]);
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                    dwErr = ERROR_INVALID_PARAMETER;
                }
                
                NKSetDirectCall (fOldDirect);
            }
        }
    } else {
        dwErr = ERROR_INVALID_HANDLE;
    }
    
    if (phd) {
        UnlockHandleData (phd);
    }

    if (dwErr) {
        KSetLastError (pCurThread, dwErr);
    }

    DEBUGMSG (ZONE_OBJDISP, (L"NKHandleCall returns 0x%x, dwErr= 0x%x\r\n", dwErr? (DWORD) phc->dwErrRtn : dwRet, dwErr));
    return dwErr? (DWORD) phc->dwErrRtn : dwRet;
}


//
// NKSetDirectCall - called by kcoredll whenever it makes a direct-call to an
// API in the kernel process, and when it completes the call.  Only used for
// non handle-based APIs.
// 
BOOL NKSetDirectCall (
    BOOL fNewValue
    )
{
    BOOL fOldValue = IsInDirectCall ();

    if (fNewValue) {
        SetDirectCall ();
    } else {
        ClearDirectCall ();
    }
    return fOldValue;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
PFNVOID 
DBG_CallCheck(
    PTHREAD pth,
    DWORD dwJumpAddress,
    PCONTEXT pCtx
    ) 
{
    return NULL;
}



