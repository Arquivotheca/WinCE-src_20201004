//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//

//
//
// !! NOTE: This file is used by both kernel and coredll. Kernel includes this file directly in kacl.c. !!
//
//

#include <windows.h>
#include <tokpriv.h>
#include <aclpriv.h>

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

// 
// access check 
//
BOOL DoAccessCheck (PCTOKENINFO pTok, PCSECDESCHDR psd, DWORD dwDesiredAccess)
{
    TEnumTokenStruct ets = { NULL, dwDesiredAccess, NULL, NULL, NULL, TRUE };
    LPBYTE p = (LPBYTE) (psd+1);
    BOOL   fRet = TRUE; // default TRUE, when NULL DACL (desktop spec)

    DEBUGCHK (pTok);

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
// check a single privilege against token
//
BOOL ChkOnePriv (PCTOKENINFO pTok, DWORD dwPriv)
{
    if (dwPriv & 0x80000000) {
        DWORD i;
        LPDWORD pPrivs = (LPDWORD) ((LPBYTE) pTok + pTok->offsetAllExtPrivileges);
        for (i = 0; (i < pTok->cAllExtPrivileges); i ++) {
            if (dwPriv == pPrivs[i]) {
                return TRUE;
            }
        }
    } else {
        return (dwPriv & pTok->allBasicPrivileges) == dwPriv;
    }
    return FALSE;
}

//
// privilege check
//
BOOL DoPrivilegeCheck (PCTOKENINFO pTok, LPDWORD pPriv, int nPrivs)
{
    BOOL  fRet = TRUE;

    DEBUGCHK (pTok);
    __try {
        while (nPrivs -- > 0) {
            if (!ChkOnePriv (pTok, pPriv[nPrivs])) {
                fRet = FALSE;
                break;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        SetLastError (ERROR_INVALID_PARAMETER);
        fRet = FALSE;
    }

    return fRet;
}



