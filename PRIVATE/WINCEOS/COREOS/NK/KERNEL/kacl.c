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
#include <kernel.h>
#include <adb.h>
#include <adbi.h>
#include <aclpriv.h>


struct _TOKENINFO {
    const ADBI_SECURITY_INFO *psi;          // ptr to ADB infomation
    PHDATA phdTok;
};

#define HEAP_TOKENINFO      HEAP_CLEANEVENT
ERRFALSE (sizeof(TOKENINFO) <= sizeof (CLEANEVENT));

typedef BOOL (* PFN_ENUMTOKEN) (PCCESID pSid, LPVOID pUserData);

BOOL TOKDelete (PTOKENINFO pTok);
BOOL TOKImpersonate (PTOKENINFO pTok);
PCCESID TOKGetGroupSID (PTOKENINFO pTok, DWORD idx);
PCCESID TOKGetOwnerSID (PTOKENINFO pTok);

const PFNVOID TokenMthds[] = {
    (PFNVOID)TOKDelete,
    (PFNVOID)0,
    (PFNVOID)TOKImpersonate,    // impersonate a token
    (PFNVOID)TOKGetOwnerSID,    // get owner SID
    (PFNVOID)TOKGetGroupSID,    // get group sid
};

static const ULONGLONG tokenSigs[] = {
    0,
    0,
    FNSIG1 (DW),                // Impersonate
    FNSIG1 (DW),                // get owner sid
    FNSIG2 (DW, DW)             // get group sid
};

const CINFO cinfToken = {
    "TOKN",
    DISPATCH_KERNEL_PSL,
    SH_CURTOKEN,
    sizeof(TokenMthds)/sizeof(TokenMthds[0]),
    TokenMthds,
    TokenMthds,
    tokenSigs,
    0,
};

ERRFALSE ((sizeof(TokenMthds) / sizeof(TokenMthds[0])) == (sizeof(tokenSigs) / sizeof(tokenSigs[0])));


_inline PTOKENLIST GetTokenList (PTHREAD pth)
{
    // we uses the last 2 bits to determine if it's a list or just the token itself
    // handle always have bit 1 set, and list will not (it's dword aligned)
    return (PTOKENLIST) (IsDwordAligned(pth->hTok)? pth->hTok : NULL);
}


//
// function to enumerate Token SIDS
//
static BOOL EnumerateToken (PCTOKENINFO pTok, PFN_ENUMTOKEN pfnEnum, LPVOID pUserData)
{
    PCCESID pSid = FirstSidOfPSI (pTok->psi);
    DWORD i;
    // totoal number of sids is pTok->psi->cAllGroups + 1
    for (i = 0; i <= pTok->psi->cAllGroups; i ++, pSid = NextSid (pSid)) {
        if (!pfnEnum (pSid, pUserData)) {
            return FALSE;
        }
    }
    return TRUE;
}

PHDATA LockThrdToken (PTHREAD pth)
{
    PHDATA phd;
    PTOKENLIST ptl = GetTokenList (pth);
    
    if (!ptl) {
        // not impersonated (original thread token)
        phd = (TOKEN_SYSTEM == pth->hTok)
                ? NULL
                : LockHandleData (pth->hTok, g_pprcNK);
    } else {
        // impersonated
        PTOKENINFO pTok = ptl->pTok;
        if (phd = (pTok? pTok->phdTok : NULL)) {
            LockHDATA (phd);
        }
    }
    return phd;
}

//
// Create a token, should only be called from filesys
//
HANDLE NKCreateTokenHandle (LPVOID psi, DWORD dwFlags)
{
    PTOKENINFO pTok = (PTOKENINFO) AllocMem (HEAP_TOKENINFO);
    HANDLE   hRet = NULL;

    DEBUGCHK (pActvProc == g_pprcNK);
    if (pTok) {
        if (hRet = HNDLCreateHandle (&cinfToken, pTok, g_pprcNK)) {
            pTok->phdTok = LockHandleData (hRet, g_pprcNK);
            pTok->psi    = psi;
            UnlockHandleData (pTok->phdTok);
        } else {
            FreeMem (pTok, HEAP_TOKENINFO);
        }
    }
    return hRet;
}

//
// revert to self
//
BOOL NKRevertToSelf (void)
{
    PTHREAD    pth = pCurThread;
    PTOKENLIST ptl = GetTokenList (pth);
    
    if (ptl) {
        PCALLSTACK pcstk = pth->pcstkTop;

        DEBUGCHK (ptl);

        if (pActvProc != g_pprcNK) {
            // called from user mode, pcstkTop is for the "RevertToSelf call", we need
            // to get to the nexe.
            DEBUGCHK (pcstk);
            pcstk = pcstk->pcstkNext;
        }

        // if pcstk doesn't match, it means that we're trying a revert accross PSL boundary. e.g.
        //
        //      CeImpersonateCurrentProcess ();
        //      MakeAnotherPSLCall ();
        //      CeRevertToSelf ();
        //      
        // pcstk won't match if the PSL Call calls CeRevertToSelf. 
        if (pcstk == ptl->pcstk) {
        
            pth->hTok = (HANDLE) ptl->pNext;
            if (ptl->pTok) {
                UnlockHandleData (ptl->pTok->phdTok);
            }
            FreeMem (ptl, HEAP_TOKLIST);
            return TRUE;
        }
    }
    return FALSE;
}

//
// CleanupTokenList: cleaning up leaked tokens up to PSL boundary (pcstk != NULL),
//                   or all of the tokens on thread exit.
// 
void CleanupTokenList (PCALLSTACK pcstk)
{
    PTHREAD    pth = pCurThread;
    PTOKENLIST ptl = GetTokenList (pth);

    while (ptl) {

        // if pcstk is NULL, we're removing all token lists, otherwise, we're removing all token
        // list associated with the callstack.
        if (pcstk && (pcstk != ptl->pcstk)) {
            break;
        }

        // if you hit this debugchk, the thread returned from PSLs with Impersonation not reverted
        // - privilege leak.
        DEBUGCHK (0);
        
        pth->hTok = (HANDLE) ptl->pNext;
        if (ptl->pTok) {
            UnlockHandleData (ptl->pTok->phdTok);
        }
        FreeMem (ptl, HEAP_TOKLIST);
        ptl = GetTokenList (pth);
    }
}

//
// check if we have impersonate privilege 
//
static BOOL HasImpersonatePriv (PTHREAD pth)
{
    BOOL   fRet = TRUE;
    PHDATA phdTok = LockThrdToken (pth);    // phdTok == NULL iff TOKEN_SYSTEM

    if (phdTok) {
        PTOKENINFO pTok = GetTokenPtr (phdTok);

        DEBUGCHK (pTok && (pTok->phdTok == phdTok));

        fRet = (CEPRI_IMPERSONATE & pTok->psi->allBasicPrivileges);
        UnlockHandleData (phdTok);
    }

    return fRet;
}


BOOL TOKDelete (PTOKENINFO pTok)
{
    FSFreePSI ((LPVOID) pTok->psi);
    FreeMem (pTok, HEAP_TOKENINFO);
    return TRUE;
}

//
// impersonate a token
//
BOOL DoImpersonateToken (PTOKENINFO pTok, PCALLSTACK pcstk)
{
    PTOKENLIST ptl = (PTOKENLIST) AllocMem (HEAP_TOKLIST);
    if (ptl) {
        PTHREAD pth = pCurThread;
        if (ptl->pTok = pTok) {
            DEBUGCHK (pTok->phdTok);
            LockHDATA (pTok->phdTok);
        }
        ptl->pNext = (PTOKENLIST) pth->hTok;
        ptl->pcstk = pcstk;
        pth->hTok  = (HANDLE) ptl;
    } else {
        SetLastError (ERROR_OUTOFMEMORY);
    }
    
    return NULL != ptl;
}

//
// Impersonate current process
//
BOOL NKImpersonateCurrProc (void)
{
    PHDATA     phdTok = LockHandleData (pActvProc->hTok, g_pprcNK);
    PTOKENINFO pTok   = GetTokenPtr (phdTok);   // pTok == NULL iff TOKEN_SYSTEM
    PCALLSTACK pcstk  = pCurThread->pcstkTop;
    BOOL       fRet;

    // if called from kernel, it's a direct call, and there is no callstack created. If caller from
    // user, there is a callstack for this API and we need to skip it.
    if (pActvProc != g_pprcNK) {
        DEBUGCHK (pcstk);
        pcstk = pcstk->pcstkNext;
    }

    fRet = DoImpersonateToken (pTok, pcstk);
    UnlockHandleData (phdTok);

    return fRet;
}

//
// impersonate a token
//
BOOL TOKImpersonate (PTOKENINFO pTok)
{
    BOOL fRet = FALSE;
    
    if (HasImpersonatePriv (pCurThread)) {
        PCALLSTACK pcstk = pCurThread->pcstkTop;

        // if called from kernel, it's a direct call, and there is no callstack created. If caller from
        // user, there is a callstack for this API and we need to skip it.
        if (pActvProc != g_pprcNK) {
            DEBUGCHK (pcstk);
            pcstk = pcstk->pcstkNext;
        }
        
        fRet = DoImpersonateToken (pTok, pcstk);
    } else {
        KSetLastError (pCurThread, ERROR_ACCESS_DENIED);
    }
    return fRet;
}


typedef struct _EnumTokenStruct {
    PCCeACL     pAcl;
    DWORD       dwDesiredAccess;
    PCCESID     pOwnerSid;
    PCCESID     pGroupSid;
    PCCESID     pSidToCheck;
    BOOL        fContinue;
} TEnumTokenStruct, *PEnumTokenStruct;


// checking ACE against SID
BOOL CheckAceAgainstSid (PCCeACE pAce, LPVOID pUserData)
{
    PEnumTokenStruct pets = (PEnumTokenStruct) pUserData;
    PCCESID pSid = PsidOfAce (pAce);

    if (ADBIIsCreatorOwnerSid (pSid)) {
        pSid = pets->pOwnerSid;
    } else if (ADBIIsCreatorGroupSid (pSid)) {
        pSid = pets->pGroupSid;
    }

    if (ADBIIsEqualSid (pSid, pets->pSidToCheck)) {
        if (ACCESS_ALLOWED_ACE_TYPE == pAce->AceType) {
            pets->dwDesiredAccess &= ~pAce->Mask;
            pets->fContinue = pets->dwDesiredAccess;
        } else if (pets->dwDesiredAccess & pAce->Mask) {
            // access denied. Stop
            pets->fContinue = FALSE;
        }
    }
    return pets->fContinue;
}

// checking SID against ACL
BOOL CheckSidAgainstAcl (PCCESID pSid, LPVOID pUserData)
{
    PEnumTokenStruct pets = (PEnumTokenStruct) pUserData;

    pets->pSidToCheck = pSid;

    EnumerateACL (pets->pAcl, CheckAceAgainstSid, pets);

    return pets->fContinue;
}

static BOOL DoAccessCheck (PTOKENINFO pTok, PCSECDESCHDR psd, DWORD dwDesiredAccess)
{
    BOOL fRet = TRUE;
    TEnumTokenStruct ets = { NULL, dwDesiredAccess, NULL, NULL, NULL, TRUE };
    LPBYTE p = (LPBYTE) (psd+1);

    DEBUGCHK (pTok && psd);
    __try {
        // owner exist?
        if (SD_OWNER_EXIST & psd->fFlags) {
            p += ADBISidSize (ets.pOwnerSid = (PCCESID) p);
        }
    
        // group exist?
        if (SD_GROUP_EXIST & psd->fFlags) {
            p += ADBISidSize (ets.pGroupSid = (PCCESID) p);
        }
    
        // DACL exist?
        if (SD_DACL_EXIST & psd->fFlags) {
            ets.pAcl = (PCCeACL) p;
            EnumerateToken (pTok, CheckSidAgainstAcl, &ets);
            fRet = (0 == ets.dwDesiredAccess);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        SetLastError (ERROR_INVALID_PARAMETER);
        fRet = FALSE;
    }

    return fRet;
}


// 
// access check 
//
BOOL NKAccessCheck (PCSECDESCHDR psd, HANDLE hTok, DWORD dwDesiredAccess)
{
    BOOL   fRet = TRUE; // default TRUE, when NULL DACL (desktop spec)

    if (psd && (TOKEN_SYSTEM != hTok)) {
        
        PHDATA phdTok = LockHandleParam (hTok, pActvProc);
        PTOKENINFO pTok = GetTokenPtr (phdTok);

        if (pTok) {
            fRet = DoAccessCheck (pTok, psd, dwDesiredAccess);
            
        } else if (!(fRet = (GetCurrentToken () == hTok))) {
            SetLastError (ERROR_INVALID_HANDLE);
        }

        UnlockHandleData (phdTok);
    }
    return fRet;

}

//
// enumerate function to count ACL size
//
BOOL CountSize (PCCeACE pAce, LPVOID pUserData)
{
    *(LPDWORD)pUserData += pAce->AceSize;
    return TRUE;
}

//
// validate if an SD is valid, return size of the SD if succeed, 0 if failed
//
DWORD ValidateSD (PCSECDESCHDR psd)
{
    DWORD cbTotal = psd->cbSize, cbSize = sizeof (SECDESHDR);
    PCCESID pSid = (PCCESID) (psd+1);
    PCCeACL pAcl;

    if (CE_ACL_REVISION != psd->Revision) {
        // invalid version
        return 0;
    }

    // owner present?
    if (psd->fFlags & SD_OWNER_EXIST) {
        cbSize += ADBISidSize (pSid);
        pSid = SDNextSID (pSid);
    }

    // group present?
    if (psd->fFlags & SD_GROUP_EXIST) {
        cbSize += ADBISidSize (pSid);
        pSid = SDNextSID (pSid);
    }

    // ACL starts after SIDs
    pAcl = (PCCeACL) pSid;

    cbSize += (psd->fFlags & SD_DACL_EXIST)? pAcl->AclSize : 0;

    if (cbSize != cbTotal)
        return 0;

    // DACL present, validate ACL
    if (psd->fFlags & SD_DACL_EXIST) {
        cbSize = 0;
        EnumerateACL (pAcl, CountSize, &cbSize);
        if (cbSize + sizeof (CeACL) != pAcl->AclSize)
            return 0;
    }
    return cbTotal;
}


//
// perform ACL check on kernel objects
//
BOOL ACLCheck (PCSECDESCHDR psd)
{
    PHDATA phdTok;
    BOOL fRet = !psd || !(phdTok = LockThrdToken (pCurThread));

    if (!fRet) {
        PTOKENINFO pTok = GetTokenPtr (phdTok);

        DEBUGCHK (pTok);
        fRet = DoAccessCheck (pTok, psd, STANDARD_RIGHTS_REQUIRED|SYNCHRONIZE);
        UnlockHandleData (phdTok);
    }
    return fRet;
}

//
// allocate memory for kernel object security descriptor
//
DWORD AllocPSD (LPSECURITY_ATTRIBUTES lpsa, PNAME *ppn)
{
    DWORD       cbSize, dwErr = 0;
    PSECDESCHDR psd;
    PNAME       pn = NULL;

    __try {
        if (sizeof (SECURITY_ATTRIBUTES) != lpsa->nLength) {
            dwErr = ERROR_INVALID_PARAMETER;
        } else if (psd = lpsa->lpSecurityDescriptor) {
            if (!IsValidUsrPtr (psd, sizeof (SECDESHDR), FALSE)
                || ((int) (cbSize = SDSize (lpsa->lpSecurityDescriptor)) < sizeof (SECDESHDR))) {
                dwErr = ERROR_INVALID_PARAMETER;
                
            // +2 to make sure DWORD alignment
            } else if (!(pn = AllocName (cbSize+2))) {
                dwErr = ERROR_OUTOFMEMORY;
                
            } else {
                memcpy (PSDFromLPName(pn), psd, cbSize);
                if (!ValidateSD (PSDFromLPName(pn))) {
                    dwErr = ERROR_INVALID_PARAMETER;
                }
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        dwErr = ERROR_INVALID_PARAMETER;
    }
    if(!dwErr) {
        *ppn = pn;
    } else if (pn) {
        FreeName (pn);
    }
    return dwErr;

}

//
// check a single privilege against token
//
BOOL ChkOnePriv (PCTOKENINFO pTok, DWORD dwPriv)
{
    if (dwPriv & 0x80000000) {
        DWORD i;
        PRIVILEGE *pPrivs = ExPrivOfPSI (pTok->psi);
        for (i = 0; (i < pTok->psi->cAllExtPrivileges); i ++) {
            if (dwPriv == pPrivs[i]) {
                return TRUE;
            }
        }
    } else {
        return (dwPriv & pTok->psi->allBasicPrivileges) == dwPriv;
    }
    return FALSE;
}

//
// privilege check
//
BOOL NKPrivilegeCheck (HANDLE hTok, LPDWORD pPriv, int nPrivs)
{
    BOOL fRet = (TOKEN_SYSTEM == hTok);

    if (!fRet) {
        PHDATA phdTok = LockHandleParam (hTok, pActvProc);
        PTOKENINFO pTok = GetTokenPtr (phdTok);

        if (pTok) {
            int i;
            __try {
                // must access pPriv from low to high so it'll hit the 64K protection 
                // before getting into kernel area.
                for (i = 0; i < nPrivs; i ++) {
                    if (!(fRet = ChkOnePriv (pTok, pPriv[i]))) {
                        break;
                    }
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                SetLastError (ERROR_INVALID_PARAMETER);
                fRet = FALSE;
            }
        } else if (!(fRet = (GetCurrentToken () == hTok))) {
            SetLastError (ERROR_INVALID_HANDLE);
        }
        UnlockHandleData (phdTok);
    }
    return fRet;
}

//
// SimplePrivilegeCheck - check privilege against the token of current thread
//
BOOL SimplePrivilegeCheck (DWORD dwPriv)
{
    return (TOKEN_SYSTEM == pCurThread->hTok) || NKPrivilegeCheck (GetCurrentToken (), &dwPriv, 1);
}


//
// Get Owner SID
//
PCCESID TOKGetOwnerSID (PTOKENINFO pTok)
{
    return FirstSidOfPSI (pTok->psi);

}

//
// Get Owner SID
//
PCCESID TOKGetGroupSID (PTOKENINFO pTok, DWORD idx)
{
    PCCESID psid = NULL;

    if (idx < pTok->psi->cAllGroups) {
        psid = FirstSidOfPSI (pTok->psi);
        
        do {
            psid = NextSid (psid);
        } while (idx --);
    }
    
    return psid;
}
