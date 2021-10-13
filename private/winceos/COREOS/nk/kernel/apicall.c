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

void GoToUserTime(void);
void GoToKernTime(void);

#define ALIGN16_UP(x)       (((x) + 0xf) & ~0xf)
#define ALIGN16_DOWN(x)     ((x) & ~0xf)

// highest slot so far registered for non-handle based API set
DWORD g_idxMaxApiSet;

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
    (PFNVOID)EXTAPISRegisterDirectMethods,
    (PFNVOID)NotSupported,
    (PFNVOID)NotSupported,
    (PFNVOID)APISSetErrorHandler,    
    (PFNVOID)EXTAPISRegisterAccessMask,
    (PFNVOID)APISCreateAPIHandleWithAccess,
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
    (PFNVOID)APISRegisterAccessMask,
    (PFNVOID)APISCreateAPIHandleWithAccess,
};

static const ULONGLONG asetSigs [] = {
    FNSIG1 (DW),                    // CloseAPISet
    0,
    FNSIG2 (DW, DW),                // RegisterAPISet
    FNSIG2 (DW, DW),                // CreateAPIHandle (tread pvobj as DW, for no verification is needed)
    FNSIG2 (DW, DW),                // VerifyAPIHandle
    FNSIG2 (DW, I_PDW),             // RegisterDirectMethods
    FNSIG4 (DW, DW, DW, O_PDW),     // LockAPIHandle (exposed to k-mode only)
    FNSIG2 (DW, DW),                // UnlockAPIHandle (exposed to k-mode only)
    FNSIG2 (DW, IO_PDW),            // SetAPIErrorHandler
    FNSIG3 (DW, I_PTR, DW),         // CeRegisterAccessMask  
    FNSIG4 (DW, DW, DW, DW)         // CreateAPIHandleWithAccess
};

ERRFALSE (sizeof(APISetExtMethods) == sizeof(APISetIntMethods));
ERRFALSE ((sizeof(APISetExtMethods) / sizeof(APISetExtMethods[0])) == (sizeof(asetSigs) / sizeof(asetSigs[0])));


const CINFO cinfAPISet = {
    { 'A', 'P', 'I', 'S' },
    DISPATCH_KERNEL_PSL,
    HT_APISET,
    sizeof(APISetIntMethods)/sizeof(APISetIntMethods[0]),
    APISetExtMethods,
    APISetIntMethods,
    asetSigs,
    0,
    0,
    0,
    0
};

// Array of pointers to system api sets.
const CINFO *SystemAPISets[NUM_API_SETS];

PHDATA g_phdAPIRegEvt;

#define MAX_METHOD  (METHOD_MASK+1)

//
// PSL server list support
//
extern CRITICAL_SECTION PslServerCS;
PPSLSERVER g_PslServerList;
int g_nPslServers;

//
// remove a matching node from the PSL servers list
//
void RemovePslServer (DWORD ProcessId)
{
    PPSLSERVER *ppServer, pServer;    
    
    EnterCriticalSection (&PslServerCS);

    // find the node
    for (ppServer = &g_PslServerList; NULL != (pServer = *ppServer); ppServer = &pServer->pNext) {
        if (pServer->ProcessId == ProcessId) {
            // found, remove from list
            *ppServer = pServer->pNext;
            FreeMem (pServer, HEAP_PSLSERVER);
            DEBUGCHK (g_nPslServers);
            g_nPslServers--;
            break;

        } else if (pServer->ProcessId < ProcessId) {
            // sorted list; moved beyond the match point
            // should not happen if the process has a tls
            // cleanup function, there should be an entry
            DEBUGCHK (0);
            break;
        }
    }    
    
    LeaveCriticalSection (&PslServerCS);
}

//
// add a new node to the PSL servers list; no-op if existing node
//
BOOL AddPslServer (DWORD ProcessId)
{
    BOOL fRet = TRUE;
    PPSLSERVER *ppPrevious = &g_PslServerList, pCurrent;
    
    DEBUGCHK (OwnCS (&PslServerCS));

    // check if we have an entry in the PSL server list; if not add one. 
    // list is sorted; currently the search is a simple linear search since 
    // the number of PSL servers is usually a very small number.
    while ((NULL != (pCurrent = *ppPrevious)) && (ProcessId < pCurrent->ProcessId)) {
        ppPrevious = &pCurrent->pNext;
        pCurrent = pCurrent->pNext;
    }

    if (!pCurrent || (pCurrent->ProcessId != ProcessId)) {
        // add new node after *ppPrevious and before pCurrent
        PPSLSERVER pNewNode = AllocMem (HEAP_PSLSERVER);
        if (pNewNode) {
            pNewNode->ProcessId = ProcessId;
            pNewNode->pNext = pCurrent;
            *ppPrevious = pNewNode;
            g_nPslServers++;

        } else {
            // OOM
            fRet = FALSE;
        }
    }
    
    return fRet;   
}

// set the TLS cleanup function for the current process
BOOL SetProcTlsCleanup (FARPROC lpv)
{
    BOOL fRet = FALSE;
    PPROCESS pprc = pActvProc;
    
    if (!pprc->pfnTlsCleanup && (pprc != g_pprcNK)) {
        
        EnterCriticalSection (&PslServerCS);
        fRet = AddPslServer (pprc->dwId);
        if (fRet) 
            pprc->pfnTlsCleanup = (PFNVOID) lpv;
        LeaveCriticalSection (&PslServerCS);

    } else {
        // TLS cleanup is supported only for user
        // mode PSL servers.
        DEBUGCHK (0);
    }

    return fRet;    
}

//
// called on bootup to verify static PSL server signatures
//
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

PPROCESS LockServerWithId (DWORD dwProcId)
{
    PPROCESS pprc;
    if (dwProcId == g_pprcNK->dwId) {
        pprc = g_pprcNK;
    } else {
        PHDATA phd = LockHandleData ((HANDLE) dwProcId, g_pprcNK);
        pprc = GetProcPtr (phd);
        if (pprc) {
            if (IsProcessTerminated (pprc)) {
                pprc = NULL;
            } else {
                // increment the # of caller of the server process
                InterlockedIncrement (&pprc->nCallers);

                if (pprc->nCallers < 0) {
                    // calling into a server that is exiting
                    InterlockedDecrement (&pprc->nCallers);
                    pprc = NULL;
                }
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

void APICallInit (void)
{
    /* setup the well known system API sets & API types */
    SystemAPISets[SH_WIN32] = &cinfWin32;
    SystemAPISets[SH_CURTHREAD] = &cinfThread;
    SystemAPISets[SH_CURPROC] = &cinfProc;
    SystemAPISets[HT_EVENT] = &cinfEvent;
    SystemAPISets[HT_MUTEX] = &cinfMutex;
    SystemAPISets[HT_CRITSEC] = &cinfCRIT;    
    SystemAPISets[HT_SEMAPHORE] = &cinfSem;
    SystemAPISets[HT_APISET] = &cinfAPISet; 
    
#ifdef DEBUG
    {
        int i;
        for (i = 0; i < NUM_API_SETS; ++i) {
            if (SystemAPISets[i]) {
                ValidateSignatures(SystemAPISets[i]->pu64Sig, SystemAPISets[i]->cMethods);
            }        
        }
    }
#endif

    KInfoTable[KINX_APISETS] = (DWORD)SystemAPISets;
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
    PPROCESS   pprc  = pActvProc;
    PAPISET    pas   = NKmalloc (sizeof (APISET));
    HANDLE     hSet  = NULL;
    DWORD      dwErr = 0;
    PFNVOID   *ppfns = NULL;
    ULONGLONG *psigs = NULL;

    DEBUGCHK (cFunctions && (cFunctions <= MAX_METHOD));

    if (!pas) {
        DEBUGMSG (ZONE_ERROR, (L"ERROR: NKCreateAPISet - out of memory 0\r\n"));
        dwErr = ERROR_NOT_ENOUGH_MEMORY;

    } else if (pprc != g_pprcNK) {
        // user mode server, verify/copy arguments
        ppfns = NKmalloc (cFunctions * sizeof (PFNVOID));
        psigs = NKmalloc (cFunctions * sizeof (ULONGLONG));

        if (!ppfns || !psigs) {
            DEBUGMSG (ZONE_ERROR, (L"ERROR: NKCreateAPISet - out of memory\r\n"));
            dwErr = ERROR_NOT_ENOUGH_MEMORY;
            
        } else if (!CeSafeCopyMemory (ppfns, ppfnMethods, cFunctions * sizeof (PFNVOID))
                || !CeSafeCopyMemory (psigs, pu64Sig, cFunctions * sizeof (ULONGLONG))) {
            DEBUGMSG (ZONE_ERROR, (L"ERROR: NKCreateAPISet - invalid parameter\r\n"));
            dwErr = ERROR_INVALID_PARAMETER;

        } else {

            ppfnMethods = ppfns;
            pu64Sig     = psigs;
        }
    }

    if (!dwErr) {
        pas->cinfo.disp     = (pprc == g_pprcNK)? DISPATCH_KERNEL_PSL : DISPATCH_PSL;
        pas->cinfo.type     = NUM_API_SETS;
        pas->cinfo.cMethods = cFunctions;
        pas->cinfo.ppfnExtMethods = pas->cinfo.ppfnIntMethods = ppfnMethods;
        pas->cinfo.pu64Sig  = pu64Sig;
        pas->cinfo.dwServerId = dwActvProcId;
        pas->cinfo.phdApiSet = NULL;
        pas->cinfo.pfnErrorHandler = NULL;
        pas->cinfo.lprgAccessMask = NULL;
        *(PDWORD)pas->cinfo.acName = *(PDWORD)acName;
        pas->iReg           = -1;     /* not a registered handle */

        if (
#ifndef DEBUG
            // don't do vaildation for kernel PSL for perf
            (pprc != g_pprcNK) &&
#endif
            !ValidateSignatures (pu64Sig, cFunctions)) {
            dwErr = ERROR_INVALID_PARAMETER;
            
        } else if (NULL == (hSet = HNDLCreateHandle (&cinfAPISet, pas, pprc, 0))) {
            DEBUGMSG (ZONE_ERROR, (L"ERROR: NKCreateAPISet - CreateHandle failed\r\n"));
            dwErr = ERROR_NOT_ENOUGH_MEMORY;
            
        } else if (pprc != g_pprcNK) {
            // user mode PSL - track HDATA object of the api set for ref-counting
            // we will remove this lock count in pre-close function
            pas->cinfo.phdApiSet = LockHandleData (hSet, pprc);
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
        DEBUGMSG (ZONE_ERROR, (L"ERROR: NKCreateAPISet - invalid parameter 0\r\n"));
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
        EnterCriticalSection (&PslServerCS);
        SystemAPISets[pas->iReg] = 0;
        LeaveCriticalSection (&PslServerCS);
    }

    DEBUGMSG (ZONE_OBJDISP, (L"APISClose: API set '%.4a' is being destroyed\r\n", pas->cinfo.acName));
    if (pas->cinfo.dwServerId && (pas->cinfo.dwServerId != g_pprcNK->dwId)) {
        if (pas->cinfo.ppfnExtMethods != pas->cinfo.ppfnIntMethods) {
            NKfree ((LPVOID) pas->cinfo.ppfnIntMethods);
        }
        if (pas->cinfo.lprgAccessMask)
            NKfree ((LPVOID)pas->cinfo.lprgAccessMask);
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
    
    if (pas->cinfo.dwServerId && (pas->cinfo.dwServerId != g_pprcNK->dwId)) {        
        // change the server ID to be an invalid id, indicating invalid API set       
        pas->cinfo.dwServerId = (DWORD) INVALID_HANDLE_VALUE;
        // remove the lock added in create api set call
        UnlockHandleData(pas->cinfo.phdApiSet);
        
    } else {
        NKD (L"!!!Kernel mode PSL '%.4a' de-registered!!!\r\n", pas->cinfo.acName);
    }
}

// kernel mode call  - register a direct call function table 
BOOL APISRegisterDirectMethods (PAPISET pas, const PFNVOID *ppfnDirectMethods)
{
    DEBUGCHK (IsKModeAddr ((DWORD) ppfnDirectMethods));
    DEBUGCHK (pas->cinfo.ppfnIntMethods == pas->cinfo.ppfnExtMethods);

    if (ppfnDirectMethods) {
        pas->cinfo.ppfnIntMethods = ppfnDirectMethods;
    }
    return NULL != ppfnDirectMethods;
}

// user mode call - register a direct call function table 
BOOL EXTAPISRegisterDirectMethods (PAPISET pas, const PFNVOID *ppfnDirectMethods)
{
    BOOL fRet = FALSE;
    ushort cMethods = pas->cinfo.cMethods;
    
    if (!IsValidUsrPtr (ppfnDirectMethods, cMethods * sizeof (PFNVOID), FALSE)) {
        DEBUGMSG (ZONE_ERROR, (L"ERROR: RegisterDirectMethods - invalid parameter\r\n"));
        SetLastError (ERROR_INVALID_PARAMETER);

    } else {
        PFNVOID *ppfns = NKmalloc (cMethods * sizeof (PFNVOID));

        if (!ppfns) {
            DEBUGMSG (ZONE_ERROR, (L"ERROR: RegisterDirectMethods - out of memory\r\n"));
            SetLastError(ERROR_NOT_ENOUGH_MEMORY);
            
        } else if (!CeSafeCopyMemory (ppfns, ppfnDirectMethods, cMethods * sizeof (PFNVOID))
                || (FALSE == (fRet = APISRegisterDirectMethods (pas, ppfns)))) {
            NKfree (ppfns);
            SetLastError (ERROR_INVALID_PARAMETER);
        }
    }
    
    return fRet;
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
    PTHREAD pth        = pCurThread;

    dwSetID &= ~REGISTER_APISET_TYPE;

    DEBUGMSG (ZONE_OBJDISP, (L"APISRegister: Registering APISet '%.4a', dwSetID = 0x%x, fHandleBased = %x\r\n",
        pas->cinfo.acName, dwSetID, fHandleBased));

    DEBUGCHK (!dwSetID || (dwSetID < dwIDLimit) || (!fHandleBased && (dwSetID >= SH_FIRST_OS_API_SET)));

    // Policy check if registration is for non handle based
    if ((dwSetID >= dwIDLimit)
        || (!fHandleBased && dwSetID && (dwSetID < SH_FIRST_OS_API_SET))) {
        dwErr = ERROR_INVALID_PARAMETER;

    } else {
        EnterCriticalSection (&PslServerCS);
        
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

                if (dwSetID > g_idxMaxApiSet)
                    g_idxMaxApiSet = dwSetID;
            }

        } else {
            // registering non-handle-based API in Handle-based API area
            dwErr = ERROR_NO_PROC_SLOTS;
        }

        LeaveCriticalSection (&PslServerCS);
    }
    
    if (dwErr) {
        RETAILMSG(1, (TEXT("RegisterAPISet failed for api set id 0x%x, fHandleBased = %x.\r\n"), dwSetID, fHandleBased));
        KSetLastError (pth, dwErr);
        
    } else {
        // signal the "new API registered" event
        ((PPROCESS)pActvProc)->fFlags |= PROC_PSL_SERVER;
        EVNTModify (GetEventPtr (g_phdAPIRegEvt), EVENT_PULSE);
        DEBUGMSG(ZONE_OBJDISP, (TEXT("RegisterAPISet: API Set '%.4a', id = 0x%x registered.\r\n"), pas->cinfo.acName, dwSetID));
    }
    return !dwErr;
}


//------------------------------------------------------------------------------
// APISCreateAPIHandle - create a handle of a particular API set
// Note: access mask for the new handle is set to 0. This means any handle based
// API calls using this handle might fail if access check is enabled for this API 
// set. If handle should be opened with specific access, use the API which takes
// access mask also -- CreateAPIHandleWithAccess.
//------------------------------------------------------------------------------
HANDLE APISCreateAPIHandle (PAPISET pas, LPVOID pvData)
{
    HANDLE hRet = NULL;

    DEBUGMSG (!pvData, (L"APISCreateAPIHandle: pvData == NULL\r\n"));

    DEBUGMSG (ZONE_OBJDISP, (L"Creating API Handle for APISet '%.4a' type %d\r\n", pas->cinfo.acName, pas->cinfo.type));

    switch (pas->cinfo.disp) {
    case DISPATCH_KERNEL_PSL:
    case DISPATCH_PSL:
        hRet = HNDLCreateHandle (&pas->cinfo, pvData, pActvProc, 0);
        break;
    default:
        DEBUGMSG (ZONE_ERROR, (L"ERROR: APISCreateAPIHandle Failed - invalid API set '%.4a' type %d\r\n", pas->cinfo.acName, pas->cinfo.type));
        KSetLastError(pCurThread, ERROR_INVALID_HANDLE);
    }

    return hRet;
}

//------------------------------------------------------------------------------
// APISCreateAPIHandleWithAccess - create a handle of a particular API set and 
// associate access mask with the new handle created.
//------------------------------------------------------------------------------
HANDLE APISCreateAPIHandleWithAccess (PAPISET pas, LPVOID pvData, DWORD dwAccessMask, HANDLE hTargetProcess)
{
    HANDLE hRet = NULL;
    PHDATA phdTargetProc = LockHandleParam (hTargetProcess, pActvProc);
    PPROCESS pprc = GetProcPtr (phdTargetProc);

    DEBUGMSG (!pvData, (L"APISCreateAPIHandleWithAccess: pvData == NULL\r\n"));

    if(pprc) {
        DEBUGMSG (ZONE_OBJDISP, (L"Creating API Handle for APISet '%.4a' type %d with access 0x%8.8lx\r\n", pas->cinfo.acName, pas->cinfo.type, dwAccessMask));

        switch (pas->cinfo.disp) {
        case DISPATCH_KERNEL_PSL:
        case DISPATCH_PSL:
            hRet = HNDLCreateHandle (&pas->cinfo, pvData, pprc, dwAccessMask);
            break;
        default:
            DEBUGMSG (ZONE_ERROR, (L"ERROR: APISCreateAPIHandleWithAccess Failed - invalid API set '%.4a' type %d with access 0x%8.8lx\r\n", pas->cinfo.acName, pas->cinfo.type, dwAccessMask));
            KSetLastError(pCurThread, ERROR_INVALID_HANDLE);
        }
    } else {
        // hTargetProcess is invalid
        KSetLastError(pCurThread, ERROR_INVALID_HANDLE);
    }

    UnlockHandleData(phdTargetProc);

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
    PHDATA   phdSrcProc = LockHandleParam (hSrcProc, pActvProc);
    PPROCESS pprc       = GetProcPtr (phdSrcProc);
    PHDATA   phdLock    = pprc? LockHandleData (h, pprc) : NULL;
    if (phdLock) {
        if (&pas->cinfo == phdLock->pci) {
            *ppObj = phdLock->pvObj;
        } else {
            UnlockHandleData (phdLock);
            phdLock = NULL;
        }
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
BOOL APISQueryID (const char *pName, int* lpRetVal)
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

//------------------------------------------------------------------------------
//
//  @func BOOL | APISRegisterAccessMask | Set access mask for all APIs in the 
//  API set. Internal version of the API. Check the external API for comments.
//
//------------------------------------------------------------------------------
BOOL APISRegisterAccessMask (
    PAPISET pas, 
    const DWORD *lprgAccessMask, 
    DWORD cAccessMask) /* size of lprgAccessMask in bytes */
{
    DWORD dwErr = 0;

    DEBUGCHK (pas->cinfo.cMethods == cAccessMask/sizeof(DWORD));
    DEBUGCHK (!pas->cinfo.lprgAccessMask);
    DEBUGCHK (IsKModeAddr ((DWORD)lprgAccessMask));

    if(!pas->cinfo.pfnErrorHandler) {
        // Not allowed to register an access mask table unless the API already
        // has a registered error handler function.
        DEBUGMSG (ZONE_ERROR, (L"ERROR: APISRegisterAcessMask Failed - API set '%.4a' has no error handler\r\n", pas->cinfo.acName));
        dwErr = ERROR_NOT_SUPPORTED;        
    } else {
        pas->cinfo.lprgAccessMask = lprgAccessMask;
    }

    if(dwErr) {
        KSetLastError(pCurThread, ERROR_NOT_SUPPORTED);
    }

    return !dwErr;
}

//------------------------------------------------------------------------------
//
//  @func BOOL | EXTAPISRegisterAccessMask | Set access mask for all APIs in the 
//  API set.
//  @rdesc Returns the TRUE if access mask is set; else FALSE (from the PSL layer)
//  @parm PAPISET | pas | Handle to the API set returned from CreateAPISet call. 
//  This has to be a handle based API set.
//  @parm LPDWORD | lpdwAccessMaskArray | array which has access mask for every 
//  API.
//  @parm DWORD | cbAccessMask | size of array lpdwAccessMaskArray in bytes
//  @comm Registers a custom access mask for every API in the given API set. OS 
//  will validate that the handle used to make an API call within this API set 
//  is opened with at-least the requires access for that API. If the access 
//  check fails, then OS will call any registered error handler for this API set;
//  for kernel mode PSL servers OS will return the error value stored in the 
//  handle data.
//
//------------------------------------------------------------------------------
BOOL EXTAPISRegisterAccessMask (
    PAPISET pas, 
    const DWORD *lprgAccessMask, 
    DWORD cbAccessMask
    )
{
    BOOL fRet = FALSE;
    DWORD dwError = 0;
    LPDWORD lprgLocalAccessMask = NULL; // local copy of the access mask
    
    if (pas->cinfo.lprgAccessMask
        || (pas->cinfo.cMethods != cbAccessMask/sizeof(DWORD))        
        ) {
        dwError = ERROR_INVALID_PARAMETER;

    } else {
        lprgLocalAccessMask = NKmalloc (cbAccessMask);
        if (!lprgLocalAccessMask) {
            dwError = ERROR_OUTOFMEMORY;
            
        } else if (!CeSafeCopyMemory (lprgLocalAccessMask, lprgAccessMask, cbAccessMask)) {
            dwError = ERROR_INVALID_PARAMETER;
            
        } else {   
            fRet = APISRegisterAccessMask (pas, lprgLocalAccessMask, cbAccessMask);
        }
    }

    if (!fRet) {

        SetLastError (dwError);
        DEBUGMSG (ZONE_ERROR, (L"ERROR: EXTAPISRegisterAccessMask - GLE: 0x%8.8lx\r\n", dwError));        
        
        if (lprgLocalAccessMask) {
            NKfree (lprgLocalAccessMask);
        }
    }
    
    return fRet;
}

ERRFALSE (0 == WAIT_OBJECT_0);
//
// NKWaitForAPIReady - waiting for an API set to be ready
//
BOOL IsSystemAPISetReady(DWORD dwAPISet)
{
    return (dwAPISet < NUM_API_SETS)? (NULL!=SystemAPISets[dwAPISet]) : FALSE;
}


//
// call either TLS notify or PSL notify for a PSL server
//
static void CallUserNotify (DWORD dwFlags, DWORD dwProcId, DWORD dwThrdId, DWORD pHD)
{
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
        MDCallUserHAPI ((PHDATA) pHD, &u.hc);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        DEBUGMSG (ZONE_ERROR, (L"ERROR: CallUserNotify failed - Notifying PSL 0x%8.8lx\r\n", dwProcId));
    }
}

#define MAX_STATIC_PSL_SERVERS 64

//
// Notify ALL PSLs of thread delete so that TLS can be cleaned up for PSL threads
//
void NKTLSNotify (DWORD dwFlags, DWORD dwProcId, DWORD dwThrdId)
{
    int idxServer = 0;
    DWORD PslServerIds[MAX_STATIC_PSL_SERVERS] = {0};
    LPDWORD PPslServerIds = &PslServerIds[0];
    
    DEBUGMSG (ZONE_OBJDISP, (TEXT("NKTLSNotify: %8.8lx %8.8lx %8.8lx\r\n"), dwFlags, dwProcId, dwThrdId));

    if (((DLL_THREAD_DETACH != dwFlags) && (DLL_PROCESS_DETACH != dwFlags))
        || !(pCurThread->tlsSecure[TLSSLOT_KERNEL] & TLSKERN_TLSCALLINPSL)) {        
        // this is not a thread exit or this thread has never called TLS in a PSL context
        return;
    }
    
    EnterCriticalSection (&PslServerCS);
    
    if (g_nPslServers > MAX_STATIC_PSL_SERVERS) {
        PPslServerIds = NKmalloc (sizeof(DWORD) * g_nPslServers);
    }

    // get the current snapshot of the psl server list
    if (g_nPslServers && PPslServerIds) {
        PPSLSERVER pNode = g_PslServerList;
        while (pNode) {
            PREFAST_DEBUGCHK (idxServer < g_nPslServers);
            DEBUGCHK (g_pprcNK->dwId != pNode->ProcessId); // currently this is supported only for user mode PSL servers
            PPslServerIds[idxServer++] = pNode->ProcessId;
            pNode = pNode->pNext;
        }
        
        DEBUGCHK (idxServer == g_nPslServers);  
    }
    
    LeaveCriticalSection (&PslServerCS);

    // ok, now we have the snapshot, call into each PSLs TLS notify function
    while (--idxServer >= 0) {
        CallUserNotify (DLL_TLS_DETACH, PPslServerIds[idxServer], dwThrdId, SH_LAST_NOTIFY + 1);
    }

    // cleanup
    if (PPslServerIds && (PPslServerIds != &PslServerIds[0])) {
        NKfree (PPslServerIds);
    }
    
}

//
// Notify ALL PSLs of process/thread create/delete and other system-wide events
//
void NKPSLNotify (DWORD dwFlags, DWORD dwProcId, DWORD dwThrdId)
{
    BOOL bFaulted;
    DWORD loop;
    const CINFO *pci;
    PPROCESS    pprc, pprcExiting = NULL;
    PHDATA      phdExiting = NULL;
    const CINFO *pLocalSystemApiSets[NUM_API_SETS-SH_LAST_NOTIFY];
    BOOL        fSkipNotify = FALSE;
    
    DEBUGMSG (ZONE_OBJDISP, (TEXT("NKPSLNotify: %8.8lx %8.8lx %8.8lx\r\n"), dwFlags, dwProcId, dwThrdId));

    pprc = SwitchActiveProcess (g_pprcNK);  

    // first clear any TLS values in all PSL servers
    NKTLSNotify (dwFlags, dwProcId, dwThrdId);

    // lock the current snapshot of non-handle api sets
    loop = SH_LAST_NOTIFY;    
    EnterCriticalSection (&PslServerCS);

    if (DLL_PROCESS_EXITING == dwFlags) {
        phdExiting  = LockHandleData ((HANDLE)dwProcId, g_pprcNK);
        pprcExiting = GetProcPtr (phdExiting);
        if (  !pprcExiting                                      // process is already gone?
            || pprcExiting->fNotifyExiting                      // it's in the middle of exiting notfication?
            || (!dwThrdId && !IsProcessNormal (pprcExiting))) { // coming from TerminateProcess and the process
                                                                // to be terminated already start exiting?
            // no need to do process exiting notification
            fSkipNotify = TRUE;
        } else {
            pprcExiting->fNotifyExiting = TRUE;
        }
    }

    if (!fSkipNotify) {
    while (loop <= g_idxMaxApiSet) {
        pci = SystemAPISets[loop];
        if (pci && pci->phdApiSet) {
            DEBUGCHK (pci->dwServerId != g_pprcNK->dwId);
            LockHDATA (pci->phdApiSet);            
        }
        pLocalSystemApiSets[loop-SH_LAST_NOTIFY] = pci;
        loop++;
    }
    }
    LeaveCriticalSection (&PslServerCS);
    
    // next call PSL notification for all non-handle based API sets
    while (--loop >= SH_LAST_NOTIFY) {
        if (NULL != (pci = pLocalSystemApiSets[loop-SH_LAST_NOTIFY])) {
            if (pci->dwServerId == g_pprcNK->dwId) {
                DEBUGMSG (ZONE_OBJDISP, (TEXT("NKPSLNotify: calling %8.8lx\r\n"), pci->ppfnIntMethods[0]));
                bFaulted = FALSE;
                __try {
                    ((void (*) (DWORD, DWORD, DWORD)) pci->ppfnIntMethods[0]) (dwFlags, dwProcId, dwThrdId);                
                } __except(EXCEPTION_EXECUTE_HANDLER)
                {
                    bFaulted = TRUE;
                }
                if (bFaulted)
                {
                    ASSERT(FALSE);
                    RETAILMSG(1, (TEXT("\r\n\r\n***NKPSLNotify: Faulted trying to call server 0x%08X. Aborting this PSL!\r\n\r\n"), pci->dwServerId));
                    EnterCriticalSection (&PslServerCS);
                    SystemAPISets[loop] = NULL;
                    LeaveCriticalSection (&PslServerCS);
                    pLocalSystemApiSets[loop-SH_LAST_NOTIFY] = NULL;
                }

            } else {            
                // u-mode server
                CallUserNotify (dwFlags, dwProcId, dwThrdId, loop);
                UnlockHandleData(pci->phdApiSet);
            }
        }        
    }        

    if (phdExiting) {
        if (pprcExiting && !fSkipNotify) {
            // no need to hold PslSeverCS to clear fNotifyExiting. The reason:
            // 1) only the thread that set fNotifiyExiting will get here.
            // 2) race after clear is equivalent to race before clear and should be handled correctly.
            pprcExiting->fNotifyExiting = FALSE;
        }
        UnlockHandleData (phdExiting);
    }

    SwitchActiveProcess (pprc);
}

#define SERVER_EXIT     0x80000000

#define MIN_SLEEP_TIME  500
#define MAX_SLEEP_TIME  30000

void WaitForCallerLeave (PPROCESS pprc)
{
    DWORD dwSleepTime = MIN_SLEEP_TIME;
    InterlockedExchangeAdd (&pprc->nCallers, SERVER_EXIT);

    while (pprc->nCallers != SERVER_EXIT) {
        DEBUGMSG (ZONE_ENTRY, (L"PSL Server exiting - Waiting for callers to leave, nCallers = %8.8lx, %8.8lx\r\n", pprc->nCallers, pprc->dwId));
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
    DEBUGMSG(ZONE_OBJDISP, (TEXT("Invalid handle: Set=%d '%.4a' Method=%d\r\n"), type,
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
    return (DWORD) STATUS_INVALID_SYSTEM_SERVICE;
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
    const DWORD *pTlsSecure = pCurThread->tlsSecure;
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
        // cannot use safe mem cpy as it changes registration ptr
        NK_PCR  *pcr = TLS2PCR (((CST_MODE_FROM_USER & pcstk->dwPrcInfo)? pth->tlsNonSecure : pth->tlsSecure));

        pcstk->regs[REG_EXCPLIST] = pcr->ExceptionList;
        pcr->ExceptionList  = (DWORD) REGISTRATION_RECORD_PSL_BOUNDARY;
    }
#endif
    
    /** Setup a kernel PSL CALLSTACK structure. This is done here so that
     * any exceptions are handled correctly.
     */
    pcstk->phd       = NULL;
    pcstk->pprcLast  = pActvProc;
    pcstk->pprcVM    = pth->pprcVM;
    pcstk->pOldTls   = pth->tlsNonSecure;
    pcstk->dwNewSP   = 0;
    pcstk->phdApiSet = 0;

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
        dwErr = (DWORD) STATUS_BAD_STACK;
        
    }
#ifdef x86
    // x86 - need to update return address
    else if (!CeSafeCopyMemory (&pcstk->retAddr, (LPCVOID) dwUsrSP, sizeof (RETADDR))) {

        // bad stack - can't read return address/exception-list from user stack.
        dwErr = (DWORD) STATUS_BAD_STACK;
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
    PTHREAD pth = pCurThread;

    SwitchActiveProcess (pcstk->pprcLast);
    SwitchVM (pcstk->pprcVM);
    pcstk->dwPrcInfo &= ~CST_UMODE_SERVER;

    if (pcstk->phd) {
        UnlockHandleData (pcstk->phd);
        pcstk->phd = NULL;
    }

    // speical handling for threads been terminated to guarantee thread termiantion.
    if (GET_DYING (pth) && !GET_DEAD (pth) && !pcstk->pcstkNext) {
        // thread been terminated
        pfn = (RETADDR) NKExitThread;
        
    } else if (pci && !pci->dwServerId) {
        // kernel API set; always return 0
        pcstk->args[0] = 0;
        KSetLastError (pth, dwErr);
        pfn = (RETADDR) ErrorReturn;
        
    } else if ((ERROR_INVALID_HANDLE == dwErr)
        && ((pcstk->args[0] = BadHandleError (iApiSet, iMethod, pci)) != (DWORD) STATUS_INVALID_SYSTEM_SERVICE)) {
        // handle based calls that we know what's the error value to return, 
        KSetLastError (pth, dwErr);
        pfn = (RETADDR) ErrorReturn;
        
    } else {
        // don't know what the return value should be, redirect the call the RaiseException.
        // In case of bad stack, reset the thread's stack ptr.
        if (((DWORD) STATUS_BAD_STACK == dwErr) && (pcstk->dwPrcInfo & CST_MODE_FROM_USER)) {
            pcstk->dwPrevSP = ResetThreadStack (pth);
        }
        DEBUGMSG (ZONE_ERROR, (L"ERROR: SetupErrorReturn - Raising exception because of %s \r\n",
                  ((DWORD) STATUS_STACK_OVERFLOW == dwErr)? L"Secure Stack Overflow" : L"invalid argument"));
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
    h = (HANDLE) (LONG) pcstk->args[0];
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

        if (!iMethod                                            // index 0 is not allowed
            || (NULL == (pci = SystemAPISets[iApiSet]))) {      // API set not registered

#ifdef DEBUG
            if (iMethod) {
                NKD (TEXT("!!DecodeAPI: Invalid API: iApiSet = %8.8lx '%.4a', iMethod = %8.8lx\n"),
                    iApiSet, pci ? pci->acName : "NONE", iMethod);
            } else {
                NKD (TEXT("!!DecodeAPI: calling close function (iMethod = 0) directly is not allowed\r\n"));
            }
#endif
            dwErr = (DWORD) STATUS_INVALID_SYSTEM_SERVICE;
            
        } else {
            DEBUGCHK(pci->type == 0 || pci->type == iApiSet);
            iApiSet = pci->type;
        }
    }

    if (iApiSet && !dwErr) {
        // handle based API sets

        if (iMethod <= 1) {
            DEBUGMSG (ZONE_ERROR, (TEXT("ERROR: DecodeAPI - calling close/preclose function directly, not allowed\r\n"), iApiSet));
            dwErr = (DWORD) STATUS_INVALID_SYSTEM_SERVICE;

        } else {

            HANDLE h = GetHandleArg (pcstk);
            // Lock the handle and convert pseudo handle to real handles
            PHDATA phd  = LockHandleParam (h, pActvProc);
            
            // validate the handle is the reqested type
            if (!phd || (iApiSet != phd->pci->type)) {
                DEBUGMSG (h && (INVALID_HANDLE_VALUE != h) && ZONE_ERROR,
                    (L"ERROR: DecodeAPI - invalid handle! (h = %8.8lx, iApiSet = %8.8lx '%.4a', pActvProc->dwId = %8.8lx)\r\n",
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
        dwErr = (DWORD) STATUS_STACK_OVERFLOW;
        pci   = NULL;       // force RaiseException
    }

    // update return value regardless error or not
    pApiInfo->dwMethod = iMethod; 
    pApiInfo->pci = pci;
    *piApiSet = iApiSet;

    if (!dwErr && (iMethod >= pci->cMethods)) {
        dwErr = (DWORD) STATUS_INVALID_SYSTEM_SERVICE;

    }
    DEBUGMSG(ZONE_OBJDISP && !dwErr,
            (TEXT("DecodeAPI: calling '%.4a' API #%d. disp=%d.\r\n"), pci->acName, iMethod, pci->disp));

    return dwErr;
}

#define MAX_TOTAL_ARG_SIZE    (VM_SHARED_HEAP_BASE)     // total size of argument cannot exceed 0x70000000

static DWORD ValidateArgs (const REGTYPE *pArgs, __out_ecount (nArgs) LPDWORD pSizeArgs, int nArgs, ULONGLONG ui64Sig, BOOL fUMode, LPDWORD pdwTotalSize)
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
                    DEBUGMSG (ZONE_ERROR, (L"ERROR: ValidateArgs - Arg %u invalid pointer 0x%08x\r\n", idx, pArgs[idx]));
                    return ERROR_INVALID_PARAMETER;
                }
                pSizeArgs[idx] = (DWORD) pArgs[idx+1];
                break;
                
            case ARG_I_ASTR:
                // ascii string, validate starting address is a valid user ptr before getting length
                if (fUMode && !IsValidUsrPtr ((LPCVOID)(LONG)pArgs[idx], 1, FALSE)) {
                    DEBUGMSG (ZONE_ERROR, (L"ERROR: ValidateArgs - Arg %u invalid ASCII string pointer 0x%08x\r\n", idx, pArgs[idx]));
                    return ERROR_INVALID_PARAMETER;
                }
                pSizeArgs[idx] = pArgs[idx]? (strlen ((LPCSTR)(LONG)pArgs[idx]) + 1) : 0;
                break;
                
            case ARG_I_WSTR:
                // unicode string, validate starting address is a valid user ptr before getting length
                if (fUMode && !IsValidUsrPtr ((LPCVOID)(LONG)pArgs[idx], 1, FALSE)) {
                    DEBUGMSG (ZONE_ERROR, (L"ERROR: ValidateArgs - Arg %u invalid Unicode string pointer 0x%08x\r\n", idx, pArgs[idx]));
                    return ERROR_INVALID_PARAMETER;
                }
                pSizeArgs[idx] = pArgs[idx]? ((NKwcslen ((LPCWSTR)(LONG)pArgs[idx]) + 1) << 1) : 0;  // * sizeof (WCHAR)
                break;
                
            default:
                // unknown signature
                DEBUGMSG (ZONE_ERROR, (L"ERROR: ValidateArgs - Arg %u invalid type\r\n", idx));
                return ERROR_INVALID_PARAMETER;
            }

            // validate the ptr is a valid user ptr
            if (fUMode && !IsValidUsrPtr ((LPCVOID)(LONG)pArgs[idx], pSizeArgs[idx], dwCurSig & ARG_O_BIT)) {
                DEBUGMSG (ZONE_ERROR, (L"ERROR: ValidateArgs - Arg %u invalid pointer 0x%08x\r\n", idx, pArgs[idx]));
                return ERROR_INVALID_PARAMETER;
            }

            // IsValidUsrPtr validated that the size given by user is >= 0 as an integer. So the following calculation
            // will never wrap around
            *pdwTotalSize += ALIGN16_UP (pSizeArgs[idx]);

            if (*pdwTotalSize >= MAX_TOTAL_ARG_SIZE) {
                DEBUGMSG (ZONE_ERROR, (L"ERROR: ValidateArgs - total argument size too big, total size = 0x%08x\r\n", *pdwTotalSize));
                return ERROR_INVALID_PARAMETER;
            }
        }
    }

    return 0;
}

// Update TLS bits in the current thread
static void UpdateProcTls (PPROCESS pServer, PTHREAD pth)
{
    DEBUGCHK (pServer != g_pprcNK);

    if (pth->pprcOwner == pServer) {
        // use the original tls
        pth->tlsNonSecure[PRETLS_TLSBASE] = (DWORD) TLSPTR(pth->dwOrigBase, pth->dwOrigStkSize);
        pth->tlsNonSecure[TLSSLOT_KERNEL] &= ~TLSKERN_THRDINPSL;

    } else{
        // thread is in psl
        pth->tlsNonSecure[TLSSLOT_KERNEL] |= TLSKERN_THRDINPSL;       
    }

    // propogate the # of sys callers tls setting
    pth->tlsNonSecure[TLSSLOT_SECMODE] = pth->tlsSecure[TLSSLOT_SECMODE];
}

//
// Common setup when calling into a user mode server.
// Applies to both handle/non-based user mode servers.
//
static void SetupUModeCmn (PPROCESS pServer, PTHREAD pth, PAPICALLINFO pApiInfo, PCALLSTACK pcstk)
{
    // update thread default user tls
    UpdateProcTls (pServer, pth);

    // lock the api set for the duration of this call
    // pApiInfo->pci == NULL for TLS cleanup API call
    if (pApiInfo->pci) {
        pcstk->phdApiSet = pApiInfo->pci->phdApiSet;
        DEBUGCHK (pcstk->phdApiSet);
        LockHDATA (pcstk->phdApiSet);
    }
}

//
// User mode argument - always copy-in/copy-out
//
static DWORD SetupUmodeArgs (PPROCESS pprcServer, PARGINFO paif, PAPICALLINFO pApiInfo, LPDWORD pdwNewSP)
{
    DWORD  dwErr = 0;
    PTHREAD pth = pCurThread;
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

            cbSize = pApiInfo->ArgSize[idx];
            if (cbSize) {
                
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
                if (!CeSafeCopyMemory ((LPVOID) dwKrnSP, (LPVOID) (LONG) pApiInfo->pArgs[idx], cbSize)) {
                    dwErr = ERROR_INVALID_PARAMETER;
                    break;
                }

                // setup information on ArgInfo structure for out parameter
                if ((DWORD) u64Sig & ARG_O_BIT) {
                    pdwDstOfst[paif->nArgs] = dwKrnSP - (DWORD) pKrnStk;
                    pEnt            = &paif->argent[paif->nArgs ++];
                    pEnt->ptr       = (LPVOID) (LONG) pApiInfo->pArgs[idx];
                    pEnt->cbSize    = cbSize;
                }
                
                // replace argument with the new space allocated on server stack
                pApiInfo->pArgs[idx] = dwKrnSP - (DWORD) pKrnStk + (DWORD) pSvrStk;
                    

            } else if (u64Sig & (ARG_O_BIT|ARG_I_BIT)) {
                // zero sized ptr, zero the pointer itself
                pApiInfo->pArgs[idx] = 0;
            }
        }

        // pApiInfo->pci could be NULL if the call is from PSLNotify and the
        // calling function is the TLS cleanup function call.
        if (dwErr && pApiInfo->pci && pApiInfo->pci->pfnErrorHandler) {
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
            TLS2PCR(pKrnTls)->ExceptionList = (DWORD) REGISTRATION_RECORD_PSL_BOUNDARY;

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
            pKrnTls[PRETLS_TLSBASE]    = (DWORD) pSvrTls;

#ifdef ARM
            if (IsVirtualTaggedCache () && (pprcServer == pVMProc)) {
                // this can only happen when kernel calls into user-mode server from a kernel thread. 
                // We need to write-back cache for we're writing to stack via kernel address. In the case
                // where server is not the current VM process, switch VM will write-back and discard all caches.
                OEMCacheRangeFlush (pKrnStk + dwOfst, cbStkSize - dwOfst, CACHE_SYNC_DISCARD);
            }
#endif
            // virtual copy to server process
            if (!VMFastCopy (pprcServer, (DWORD) pSvrStk + dwOfst, g_pprcNK, (DWORD) pKrnStk + dwOfst, cbStkSize - dwOfst, PAGE_READWRITE)) {
                dwErr = ERROR_NOT_ENOUGH_MEMORY;
            } else {
                // change non-secure TLS to the new TLS, and update newSP
                pth->tlsNonSecure = pSvrTls;
                *pdwNewSP = dwKrnSP - (DWORD) pKrnStk + (DWORD) pSvrStk;
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

    //
    // check the arguments
    //
    if(pApiInfo->u64Sig) {
        __try {
            dwErr = ValidateArgs (pApiInfo->pArgs, pApiInfo->ArgSize, pApiInfo->nArgs, pApiInfo->u64Sig, fUMode, &pApiInfo->dwTotalSize);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            dwErr = ERROR_INVALID_PARAMETER;
        }
    }

    //
    // If failed argument or access check route the call to error handler if one exists
    // for this API set; otherwise kernel will return failure to the caller.
    //
    if (dwErr && pApiInfo->pci->pfnErrorHandler) {
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

        // PSL servers can query the last error to decide if the failure was due to:
        // a) invalid parameters: last error set to ERROR_INVALID_PARAMETER
        // b) access check failure: last error set to ERROR_ACCESS_DENIED
        KSetLastError (pCurThread, dwErr);
        dwErr = 0;
    }

    return dwErr;
}

static DWORD SetupArguments (PCALLSTACK pcstk, PAPICALLINFO pApiInfo)
{
    DWORD         dwErr   = 0;
    PTHREAD       pth = pCurThread;

    // set number of args and the method signatures
    pApiInfo->u64Sig = pApiInfo->pci->pu64Sig[pApiInfo->dwMethod];
    pApiInfo->nArgs = (int) pApiInfo->u64Sig & ARG_TYPE_MASK;

    // copy all arguments onto secure stack first
    if (!CopyArgs (pcstk, pApiInfo->nArgs)) {
        return (DWORD) STATUS_BAD_STACK;
    }
    pApiInfo->pArgs = pcstk->args;
    pApiInfo->u64Sig >>= ARG_TYPE_BITS; // >>4 to get rid of arg count from signature
 
    // validate argument, calculate lengths
    dwErr = DoValidateArgs (pcstk, pApiInfo,  pcstk->dwPrcInfo & CST_MODE_FROM_USER);
    if (dwErr) {
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
                    TLS2PCR(pth->tlsNonSecure)->ExceptionList = (DWORD) REGISTRATION_RECORD_PSL_BOUNDARY;
    
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
                    SetupUModeCmn (pServer, pth, pApiInfo, pcstk);

                } __except (EXCEPTION_EXECUTE_HANDLER) {
                    dwErr = (DWORD) STATUS_BAD_STACK;
                    // error, unlock server
                    UnlockServerWithId (pServer->dwId);
                }


            // setup stack/arguments in server process
            }
            else if (0 != (dwErr = SetupUmodeArgs (pServer, &aif, pApiInfo, &pcstk->dwNewSP))) {

                // error (invalid arguments), unlock server
                UnlockServerWithId (pServer->dwId);

                pcstk->arginfo.nArgs = 0;

            } else {
                // successfully marshalled arguments, switch active/VM to server
                SwitchActiveProcess (pServer);
                SwitchVM (pServer); // this will cause cache flush on VIVT (ARM), which will make cache coherent.
                SetupUModeCmn (pServer, pth, pApiInfo, pcstk);
                
                // save argument information in callstack structure
                pcstk->arginfo = aif;

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

    PcbSetTlsPtr (pCurThread->tlsSecure);
    
    randdw1 = ((randdw1<<5) | (randdw1>>27)) ^ (CurMSec & 0x1f);

    if (GET_TIMEMODE(pCurThread) == TIMEMODE_USER) {
        // from user mode; switch to kernel time
        GoToKernTime();
    }
    
    if (   (0 != (dwErr = SetupCstk (pcstk)))
        || (0 != (dwErr = DecodeAPI (pcstk, &iApiSet, &ApiInfo)))
        || (0 != (dwErr = SetupArguments (pcstk, &ApiInfo)))) {    // NOTE: CopyArgument would switch to server process if needed

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

            DEBUGMSG (ZONE_OBJDISP, (L"Calling API with trap interface from kernel mode. Return Address = %8.8lx\r\n", pcstk->retAddr));

            if (ApiInfo.pci->dwServerId) {
                SetDirectCall ();
            }
        
            ApiInfo.pfn = ApiInfo.pci->ppfnIntMethods[ApiInfo.dwMethod];

            if (!IsKModeAddr ((DWORD)pcstk->retAddr)) {
            
                //
                // If the following debug break is hit, the thread was running user code in kernel mode, which is a 
                // SEVERE SECURITY HOLE. Possible reasons:
                // - user mode thread calls into kernel mode server, passing a function pointer. And the kernel mode server simply 
                //   calls the function without using PerformCallBack.
                // - un-initialized function pointer in kernel mode, which happens to contain to a user mode address.
                // - corrupted function table in kernel servers.
                //
                NKD (L"!! ERROR - SECURIIY VIOLATION !! ----------------------------------------------------------------------\r\n");
                NKD (L"!! ERROR - SECURIIY VIOLATION !! Running user code in kernel Mode (0x%8.8lx), Thread terminated!!\r\n", pcstk->retAddr);
                NKD (L"!! ERROR - SECURIIY VIOLATION !! ----------------------------------------------------------------------\r\n");
                DebugBreak ();

                // terminate the thread
                ApiInfo.pfn = SetupErrorReturn (pcstk, ApiInfo.dwMethod, iApiSet, ERROR_ACCESS_DENIED, ApiInfo.pci);
                SCHL_SetThreadToDie (pCurThread, 0, NULL);
            }
        }
        
        if (g_ProfilerState.dwProfilerFlags & PROFILE_OBJCALL)
            ProfilerHit((UINT)ApiInfo.pfn);
    }

    if (pcstk->dwNewSP) {
        // call to user mode
        GoToUserTime();
    }
    
    DEBUGMSG (ZONE_OBJDISP, (L"ObjectCall: returning %8.8lx\r\n", ApiInfo.pfn));

    return ApiInfo.pfn;
}

RETADDR SetupCallToUserServer (PHDATA phd, PDHCALL_STRUCT phc, PCALLSTACK pcstk)
{
    PTHREAD       pth = pCurThread;
    DWORD         dwErr;
    PPROCESS      pServer;
    APICALLINFO   ApiInfo = {0};
    DWORD         ServerId;
    BOOL          fProcTlsCleanup = FALSE;
    PPROCESS      pprcActv = pActvProc;
    
    // setup the api call info
    ApiInfo.dwMethod = (DWORD) phc->dwAPI;
    ApiInfo.nArgs = (int) phc->cArgs + 1; // the count passed in excludes the handle
    ApiInfo.pArgs = phc->args;

    DEBUGMSG (ZONE_OBJDISP, (L"SetupCallToUserServer: phd = %8.8lx, phc = %8.8lx, pcstk = %8.8lx\r\n", phd, phc, pcstk));

    if ((DWORD) phd < NUM_API_SETS) {
        // calling PSL notify of user mode server
        DEBUGCHK (2 == phc->cArgs);
        DEBUGCHK (0 == ApiInfo.dwMethod);
        DEBUGCHK ((DLL_TLS_DETACH == phc->args[0]) || (SH_LAST_NOTIFY <= (DWORD) phd));
        if (DLL_TLS_DETACH == phc->args[0]) {
            fProcTlsCleanup = TRUE;
            ServerId = (DWORD) phc->args[1]; // PSL process id is passed in as second argument
        } else {
            ApiInfo.pci = SystemAPISets[(DWORD) phd];
            ServerId = ApiInfo.pci->dwServerId;
        }
    } else {
        // calling handle based API
        ApiInfo.pci = phd->pci;
        ServerId = ApiInfo.pci->dwServerId;
    }

    // fill in the info for pcstk, dwPrevSP and retAddr is filled before calling this function
    pcstk->dwPrcInfo = CST_UMODE_SERVER;
    pcstk->pcstkNext = pth->pcstkTop;
    pcstk->phd       = (ApiInfo.dwMethod > 1)? phd : NULL; // don't unlock handle on return when calling close and pre-close 
    pcstk->pOldTls   = pth->tlsNonSecure;
    pcstk->pprcLast  = pActvProc;
    pcstk->pprcVM    = pth->pprcVM;
    pcstk->dwNewSP   = 0;
    pcstk->phdApiSet = 0;

    SaveDirectCall (pcstk);
    
    // link into threads callstack
    pth->pcstkTop    = pcstk;
    
    pServer = LockServerWithId (ServerId);
    if (!pServer) {
        dwErr = ERROR_INVALID_HANDLE;
        
    } else {

        if ((DWORD) phd > NUM_API_SETS) {
            // setup call to handle based user API

            DEBUGCHK (pth->tlsPtr == pth->tlsSecure);
            DEBUGCHK (ServerId && (ServerId != g_pprcNK->dwId));

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
            UnlockServerWithId (ServerId);
            
        } else {

#ifdef x86
            // setup floating point emulation handlers for the current thread
            if (g_pCoredllFPEmul)        
                SetFPEmul (pth, USER_MODE);
#endif        

            // switch process to server
            SwitchActiveProcess (pServer);
            SwitchVM (pServer);
            SetupUModeCmn (pServer, pth, &ApiInfo, pcstk);
            GoToUserTime(); // accumulate kernel time            
        }
    }

    if (dwErr) {
        pcstk->dwPrcInfo = 0;   // clear user mode server bit
        pcstk->dwNewSP = 0;     // new sp set to 0 to indicate kernel call
        pcstk->args[0] = phc->dwErrRtn;
        ApiInfo.pfn = (RETADDR) ErrorReturn;

    } else if (!ApiInfo.pfn) {
        // setup pfn to the external method if there was no error or if there is no error handler
        DEBUGCHK (!fProcTlsCleanup || pServer->pfnTlsCleanup);
        ApiInfo.pfn = (fProcTlsCleanup) ? pServer->pfnTlsCleanup : ((pprcActv == g_pprcNK) ? ApiInfo.pci->ppfnIntMethods[ApiInfo.dwMethod] : ApiInfo.pci->ppfnExtMethods[ApiInfo.dwMethod]);
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

    if (GET_TIMEMODE(pth) == TIMEMODE_USER) {
        // from user mode; switch to kernel time
        GoToKernTime();
    }

    if (CST_UMODE_SERVER & pcstk->dwPrcInfo) {
    
        // returning from a user mode server (PSL call)

        const DWORD *pSvrTls = pth->tlsNonSecure;
        
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
            
            // release Server virtual Copy of the stack.
            VMFreeAndRelease (pprcServer, pSvrStk, cbStkSize);

            // switch back to caller VM - this will cause cache to be flushed on VIVT cache
            SwitchVM (pcstk->pprcVM);
            
            // restore tlsNonSecure
            pth->tlsNonSecure = pcstk->pOldTls;
            
            if (!fCleanOnly) {
                // returning from user mode server, copy-out arguments
                PARGENTRY pEnt;
                int       idx, nArgs = paif->nArgs;
                const DWORD *pdwSrcOfst = ARG_OFFSET_FROM_TLS (pKrnTls);
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

            // Carry forward the bit which is set if there was a TLS call
            // on this thread while the thread is in a PSL context. We use
            // this information to cleanup TLS on thread exit.
            pth->tlsSecure[TLSSLOT_KERNEL] |= (pKrnTls[TLSSLOT_KERNEL] & TLSKERN_TLSCALLINPSL);

            // update stack bound to use kernel stack
            pKrnTls[PRETLS_STACKBOUND] = (DWORD) pKrnStk + cbStkSize - VM_PAGE_SIZE;
            VERIFY (VMDecommit (g_pprcNK, pKrnStk, cbStkSize - VM_PAGE_SIZE, VM_PAGER_AUTO));

            // free the dynamically create stack
            VMFreeStack (g_pprcNK, (DWORD) pKrnStk, cbStkSize);

        }

        // unlock api set
        if (pcstk->phdApiSet) {
            UnlockHandleData (pcstk->phdApiSet);
        }

        // unlock server process
        UnlockServerWithId (pprcServer->dwId);
    }
    else if (CST_CALLBACK & pcstk->dwPrcInfo) { 
        
        // returning from a perform callback
        
        SwitchVM (pcstk->pprcVM);
        pth->tlsNonSecure = pcstk->pOldTls;
    }

    if (phd) {
        pcstk->phd = NULL;
        UnlockHandleData (phd);
    }

    // update pcstk->dwPrcInfo to the mode to return to
    pcstk->dwPrcInfo &= CST_MODE_FROM_USER;

    if (pcstk->dwPrcInfo) {
        // returning to user mode
        UpdateProcTls (pcstk->pprcLast, pth);
        GoToUserTime();
    }

#ifdef x86
{
    NK_PCR *pcr = TLS2PCR ((pcstk->dwPrcInfo? pth->tlsNonSecure : pth->tlsSecure));
    
    // x86 restore exception pointer on user stack
    // cannot use safe mem cpy as it changes registration ptr
    pcr->ExceptionList = pcstk->regs[REG_EXCPLIST];

    // setup floating point emulation handlers for the current thread
    if (g_pCoredllFPEmul)        
        SetFPEmul (pth, (pcstk->dwPrcInfo) ? USER_MODE : KERNEL_MODE);
}
#endif

}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void ServerCallReturn (void)
{
    PTHREAD pth        = pCurThread;
    PCALLSTACK pcstk   = pth->pcstkTop;
    BOOL fRtnFromOwner = (pActvProc == pth->pprcOwner);        // pActvProc can change in UnwindOneCstk, get get here
    BOOL fRtnFromCB    = (CST_CALLBACK & pcstk->dwPrcInfo);    // pcstk->dwPrcInfo can change in UnwindOneCstk, get get here
    BOOL fRtnToOwner   = (pcstk->pprcLast == pth->pprcOwner);

    DEBUGMSG (ZONE_OBJDISP, (L"SCR: pcstk = %8.8lx, returning to process = %8.8lx\n", pcstk, pcstk->pprcLast));

    PcbSetTlsPtr (pth->tlsSecure);

    UnwindOneCstk (pcstk, FALSE);

    for ( ; ; ) {
        if (GET_DYING (pth) && !GET_DEAD (pth)) {

            // current thread is been terminated
            if (fRtnToOwner) {
                DEBUGCHK (!GET_USERBLOCK (pth));
                if (pcstk->pcstkNext) {
                    // callstack not empty, just skip the callback by setting return address to SYSCALL_RETURN
                    pcstk->retAddr = (RETADDR) SYSCALL_RETURN;
                } else {
                    // callstack empty, terminate the thread
                    pcstk->retAddr = (RETADDR) TrapAddrExitThread;
                    SET_DEAD (pth);
                }
                // don't allow suspending a thread been terminated, or the thread could've never exited.
                pth->bPendSusp   = 0;

            } else if (fRtnFromCB && fRtnFromOwner) {
                // returning from callback from the owner process of a thread been termianted, indicate callback failed.
                PCALLBACKINFO pcbi = (PCALLBACKINFO) pcstk->args[0];
                pcbi->dwErrRtn = ERROR_OPERATION_ABORTED;
            }
            break;
        }

        if (!pth->bPendSusp || !fRtnToOwner) {
            break;
        }

        // delay suspended - suspend now.
        SCHL_SuspendSelfIfNeeded ();
        CLEAR_USERBLOCK (pth);
    }

    /* unlink CALLSTACK structure from list */
    pth->pcstkTop = pcstk->pcstkNext;

    // restore TLS pointer
    pth->tlsPtr = (pcstk->dwPrcInfo? pth->tlsNonSecure : pth->tlsSecure);
    PcbSetTlsPtr (pth->tlsPtr);

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
    pcbi->dwErrRtn = 0;

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
            pcbi->dwErrRtn = ERROR_ACCESS_DENIED;
            DEBUGCHK (0);
            
        } else if (!pcbi->hProc || !pcbi->pfn) {
            // invalid arguments
            NKDbgPrintfW(L"NKPrepareCallback: invalid arguments hProc:%8.8lx pfn:%8.8lx\r\n", pcbi->hProc, pcbi->pfn);
            pcbi->dwErrRtn = ERROR_INVALID_PARAMETER;

        } else if (   IsProcessTerminated (pcstk->pprcLast)
                   || (GET_DYING (pth) && !GET_DEAD (pth) && (pcstk->pprcLast == pth->pprcOwner))) {
            // thread being termianted and calling back to owner
            RETAILMSG (1, (L"NKPrepareCallback: calling back on a dying thread, proc-id = %8.8lx\r\n", pth->pprcOwner->dwId));
            pcbi->dwErrRtn = ERROR_OPERATION_ABORTED;
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
                pNewcstk->args[0]   = (REGTYPE) pcbi;           // save pcbi at arg[0]
                pfn                 = (RETADDR) pcbi->pfn;
                GoToUserTime(); // accumulate kernel time so far
                
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                pfn = (RETADDR) ErrorReturn;
            }


            if ((RETADDR) ErrorReturn == pfn) {
                // error - restore VM/active process
                SwitchActiveProcess (pNewcstk->pprcLast);
                SwitchVM (pNewcstk->pprcVM);
                pcbi->dwErrRtn = ERROR_INVALID_PARAMETER;
                
            } else {
                pNewcstk->dwNewSP = (DWORD) pUsrArgs;
                pth->tlsNonSecure = pcstk->pOldTls;
                pth->pcstkTop     = pNewcstk;
                UpdateProcTls (pcstk->pprcLast, pth);
                
#ifdef x86
                // x86 specific - update registration pointer, and update return address
                TLS2PCR(pcstk->pOldTls)->ExceptionList = (DWORD) REGISTRATION_RECORD_PSL_BOUNDARY; // terminate user registation pointer

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
    DWORD  dwRet = 0;
    DWORD  dwErr = 0;
    PHDATA phd   = LockHandleParam ((HANDLE) (LONG) phc->args[0], pActvProc);
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



