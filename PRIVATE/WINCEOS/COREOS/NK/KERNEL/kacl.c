//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <kernel.h>
#include <tokpriv.h>
#include <aclpriv.h>

const PFNVOID TokenMthds[] = {
    (PFNVOID)SC_TokenCloseHandle,
    (PFNVOID)0,
    (PFNVOID)SC_Impersonate,    // impersonate a token
    (PFNVOID)SC_AccessCheck,    // access check for that can't be done in-proc
    (PFNVOID)SC_PrivilegeCheck, // privilege check for that can't be done in-proc
};

const CINFO cinfToken = {
    "TOKN",
    DISPATCH_KERNEL_PSL,
    SH_CURTOKEN,
    sizeof(TokenMthds)/sizeof(TokenMthds[0]),
    TokenMthds
};

BOOL FreeTokenMemory (LPVOID pTok);


//
// Helper function for token closing
//
BOOL CloseTokenInProcess (HANDLE hTok, PPROCESS pprc)
{
    LPVOID pTok = HandleToToKen (hTok);
    if (DecRef (hTok, pprc, FALSE)) {
        FreeTokenMemory (pTok);
        FreeHandle (hTok);
    }
    return NULL != pTok;
}

//
// Create a token, should only be called from filesys
//
HANDLE SC_CreateToken (LPVOID pTok, DWORD dwFlags)
{
    TRUSTED_API (L"Sc_CreateToken", NULL);
    return IsInSharedSection (pTok)? AllocHandle (&cinfToken, pTok, (dwFlags & TF_OWNED_BY_KERNEL)? ProcArray : pCurProc) : NULL;
}

//
// revert to self
//
BOOL SC_RevertToSelf (void)
{
    PTOKENLIST ptl = (PTOKENLIST) pCurThread->hTok;
    // we uses the last 2 bits to determine if it's a list or just the token itself
    // handle always have bit 1 set, and list will not (it's dword aligned)
    if (((DWORD) ptl & 3) != 2) {
        DEBUGCHK (ptl);
        pCurThread->hTok = (HANDLE) ptl->pNext;
        if (TOKEN_SYSTEM != ptl->hTok) {
            // decrement ref count of NK
            CloseTokenInProcess (ptl->hTok, &ProcArray[0]);
        }
        FreeMem (ptl, HEAP_TOKLIST);
        hCurTok = KThrdToken (pCurThread);
        KInfoTable[KINX_PTR_CURTOKEN] = (DWORD) HandleToToKen (hCurTok);
        return TRUE;
    }
    return FALSE;
}

//
// close a token handle
//
BOOL   SC_TokenCloseHandle (HANDLE hTok)
{
    return CloseTokenInProcess (hTok, pCurProc);
}

//
// duplicate a token, flag must be 0 for now
//
BOOL   SC_CeDuplicateToken (HANDLE hTok, DWORD dwFlags, PHANDLE phRet)
{
    DWORD dwErr = 0;

    // validate parameter
    if (dwFlags || !SC_MapPtrWithSize (phRet, sizeof(DWORD), hCurProc)) {
        dwErr = ERROR_INVALID_PARAMETER;
        
    } else if (TOKEN_SYSTEM != hTok) {
        if (!HandleToToKen (hTok)) {
            dwErr = ERROR_INVALID_HANDLE;
        } else {
            IncRef (hTok, pCurProc);
        }
    }

    if (dwErr) {
        KSetLastError (pCurThread, dwErr);
    } else {
        *phRet = hTok;
    }
    
    return !dwErr;
}


//
// check if we have impersonate privilege - TBD
BOOL HasImpersonatePriv (PTHREAD pth)
{
    HANDLE      hTok = KThrdToken (pth);
    PTOKENINFO  pTok;
    if (TOKEN_SYSTEM == hTok)
        return TRUE;

    if (!(pTok = HandleToToKen (hTok))) {
        DEBUGCHK (0);   // should've never happen
        return FALSE;
    }

    return (CEPRI_IMPERSONATE & pTok->allBasicPrivileges);
}


//
// impersonate a token
//
BOOL DoImpersonateToken (HANDLE hTok)
{
    PTOKENLIST ptl = (PTOKENLIST) AllocMem (HEAP_TOKLIST);
    if (ptl) {
        if (TOKEN_SYSTEM != hTok) {
            // increment ref count of NK to this token
            IncRef (hTok, &ProcArray[0]);
        }
        hCurTok = ptl->hTok = hTok;
        KInfoTable[KINX_PTR_CURTOKEN] = (DWORD) HandleToToKen (hTok);
        ptl->pNext = (PTOKENLIST) pCurThread->hTok;
        pCurThread->hTok = (HANDLE) ptl;
        return TRUE;
    }
    KSetLastError (pCurThread, ERROR_OUTOFMEMORY);
    return FALSE;
}

//
// Impersonate current process
//
BOOL SC_CeImpersonateCurrProc (void)
{
    return DoImpersonateToken (pCurProc->hTok);
}

//
// impersonate a token
//
BOOL SC_Impersonate (HANDLE hTok)
{
    if (HasImpersonatePriv (pCurThread) && (TOKEN_SYSTEM != hTok)) {
        return DoImpersonateToken (hTok);
    }
    KSetLastError (pCurThread, ERROR_ACCESS_DENIED);
    return FALSE;
}

#include "../../../core/acl/accchk.c"
//
// free the security descriptor in kernel objects
//
BOOL FreePSD (HANDLE hObj)
{
    LPName psd = (LPName) GetUserInfo (hObj);
    if (psd) {
        SetUserInfo (hObj, 0);
        FreeName (psd);
    }
    return TRUE;
}

//
// perform ACL check on kernel objects
//
BOOL ACLCheck (HANDLE hObj)
{
    LPName psd = (LPName) GetUserInfo (hObj);
    HANDLE hTok = KThrdToken (pCurThread);
    return (psd && (TOKEN_SYSTEM != hTok))
        ? DoAccessCheck (HandleToToKen (hTok), (PCSECDESCHDR) psd->name, STANDARD_RIGHTS_REQUIRED|SYNCHRONIZE)
        : TRUE;
}

//
// allocate memory for kernel object security descriptor
//
DWORD AllocPSD (LPSECURITY_ATTRIBUTES lpsa, LPName *psd)
{
    LPName sd = NULL;
    DWORD  cbSize, dwErr = 0;

    __try {
        if (sizeof (SECURITY_ATTRIBUTES) != lpsa->nLength) {
            dwErr = ERROR_INVALID_PARAMETER;
        } else if (lpsa->lpSecurityDescriptor) {
            if (!(cbSize = SDSize (lpsa->lpSecurityDescriptor))) {
                dwErr = ERROR_INVALID_PARAMETER;
            } else if (!(sd = AllocName (cbSize))) {
                dwErr = ERROR_OUTOFMEMORY;
            } else {
                memcpy (sd->name, lpsa->lpSecurityDescriptor, cbSize);
                if (!ValidateSD ((PCSECDESCHDR) sd->name)) {
                    dwErr = ERROR_INVALID_PARAMETER;
                }
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        dwErr = ERROR_INVALID_PARAMETER;
    }
    if(!dwErr) {
        *psd = sd;
    } else if (sd) {
        FreeName (sd);
    }
    return dwErr;
}

//
// perform Access check
//
BOOL SC_AccessCheck (HANDLE hTok, LPVOID pSecDesc, DWORD dwDesiredAccess)
{
    PCTOKENINFO pTok = HandleToToKen (hTok);

    DEBUGCHK (pTok);
    
    return DoAccessCheck (pTok, (PCSECDESCHDR) pSecDesc, dwDesiredAccess);
}
//
// privilege check
//
BOOL SC_PrivilegeCheck (HANDLE hTok, LPDWORD pPriv, int nPrivs)
{
    PCTOKENINFO pTok = HandleToToKen (hTok);

    return DoPrivilegeCheck (pTok, pPriv, nPrivs);
}


