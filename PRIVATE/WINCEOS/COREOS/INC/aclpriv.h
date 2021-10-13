//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef _ACL_PRIV_H_
#define _ACL_PRIV_H_

#include <adb.h>
#include <adbi.h>
#include <cesddl.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _CeACE {
    UCHAR       AceType;
    UCHAR       AceFlags;
    USHORT      AceSize;
    ACCESS_MASK Mask;
} CeACE, *PCeACE;
typedef const CeACE *PCCeACE;

#define PsidOfAce(pAce)     ((PCCESID) (pAce+1))

typedef struct _CeACL {
    UCHAR       AclRevision;
    UCHAR       Sbz1;
    USHORT      AclSize;
    USHORT      AceCount;
    USHORT      Sbz2;
} CeACL, *PCeACL;
typedef const CeACL *PCCeACL;

__inline GetAclSize (PCCeACL pAcl)
{
    return pAcl? pAcl->AclSize : 0;
}


typedef struct _SECDESCHDR {
    BYTE        Revision;
    BYTE        fFlags;
    WORD        cbSize;
    // variable length struct follows
    // CeSID    owner;
    // CeSID    group;
    // CeACL    dacl;
    // CeACL    sacl;   // not supported for CE
} SECDESHDR, *PSECDESCHDR;
typedef const SECDESHDR *PCSECDESCHDR;

// bit fields in fFlags
#define SD_OWNER_EXIST          ((BYTE) OWNER_SECURITY_INFORMATION)
#define SD_GROUP_EXIST          ((BYTE) GROUP_SECURITY_INFORMATION)
#define SD_DACL_EXIST           ((BYTE) DACL_SECURITY_INFORMATION)

__inline DWORD SDSize (LPCVOID psd)
{
    return ((PCSECDESCHDR) psd)->cbSize;
}

__inline PCESID SDNextSID (PCCESID pSid)
{
    return (PCESID) ((LPBYTE) pSid + ADBISidSize (pSid));
}

typedef BOOL (* PFN_ENUMACL) (PCCeACE pAce, LPVOID pUserData);

//
// function to enumerate ACL, inlined because it can be used in multiple places
//
__inline BOOL EnumerateACL (PCCeACL pAcl, PFN_ENUMACL pfnEnum, LPVOID pUserData)
{
    PCCeACE pAce = (PCCeACE) (pAcl+1);
    int     i;

    for (i = 0; i < pAcl->AceCount; i ++, pAce = (PCCeACE) ((DWORD) pAce + pAce->AceSize)) {
        if (!pfnEnum (pAce, pUserData))
            return FALSE;
    }
    return TRUE;
}

#define CE_ACL_REVISION CE_SD_REVISION


#ifdef __cplusplus
}

#endif

#endif  // _ACL_PRIV_H_
