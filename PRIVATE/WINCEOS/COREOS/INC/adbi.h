//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __ADBI_H__
#define __ADBI_H__

#ifdef __cplusplus
extern "C" {
#endif

#define ADB_MAX_SID_SIZE    72
// well-known reserved account Ids for use in ACLs
#define ADB_CREATOR_OWNER_ID    2
#define ADB_CREATOR_GROUP_ID     3

// All functions return 0 on success and an error code on failure

typedef void *PCESID;
typedef const void *PCCESID;

typedef struct _ADBI_SECURITY_INFO {
    DWORD version;	// must be 1
    DWORD cbSize;  // size of this info
    DWORD accountFlags;	// account flags
    DWORD offsetSids;  // account id
    // the following are effective groups and privileges
    // obtained by expanding the groups that the account belongs to
    DWORD cImmediateGroups;
    DWORD cAllGroups;   // number of immediate & expanded groups
    PRIVILEGE allBasicPrivileges;
    DWORD cAllExtPrivileges;
    DWORD offsetAllExtPrivileges;
} ADBI_SECURITY_INFO, *PADBI_SECURITY_INFO;

typedef const ADBI_SECURITY_INFO *PCADBI_SECURITY_INFO;

LONG ADBIStartup();
LONG ADBICleanup();

// this is set to return the maximum size of NT SIDs but could
// potentially be a lot smaller
__inline unsigned int  ADBIMaxSidSize() {return ADB_MAX_SID_SIZE;}

__inline unsigned int ADBISidSize(PCCESID pSid) {return (* (PBYTE)pSid) == 1 ? 8 + (*((PBYTE) pSid + 1))*4 : sizeof (DWORD);}

__inline BOOL ADBIIsEqualSid(PCCESID pSid1, PCCESID pSid2)
{
    DWORD cbSize = ADBISidSize(pSid1);
    return (ADBISidSize(pSid2) == cbSize) && !memcmp(pSid1, pSid2, cbSize);
}


__inline BOOL ADBIIsCreatorOwnerSid(PCCESID pSid)
{
   return ( *(DWORD *)pSid == ADB_CREATOR_OWNER_ID);
}

// is this used??
__inline BOOL ADBIIsCreatorGroupSid(PCCESID pSid)
{
    return (*(DWORD *)pSid == ADB_CREATOR_GROUP_ID);
}

#define FirstSidOfPSI(psi) ((PCESID) ((PBYTE)(psi)+ (psi)->offsetSids))
#define NextSid(pSid) ((PCESID)((PBYTE)(pSid)+ ADBISidSize(pSid)))
#define ExPrivOfPSI(psi) ((PRIVILEGE *) ((PBYTE)(psi)+ (psi)->offsetAllExtPrivileges))


// this function can potentially be replaced by 
// ADBGetAccountProperty
/*
Parameters
pszName
[in] name of account.
pcbSize
[in/out] size in bytes of pSid buffer on input. Returns the size of the CESID.
pSid
[out] CESID for this account.
*/
LONG
ADBISidFromName(
    PCWSTR pszName,
    PDWORD pcbSize, 
    PCESID pSid);

/*
Parameters
pSid
[in] CESID of account.
pcchName
[in/out] length of pszName buffer on input. Returns the number of characters in the name (including trailing NULL).
pszName
	[out] returns name of account.
*/
LONG
ADBINameFromSid(
    IN PCCESID pSid, 
    IN OUT PDWORD pcchName, 
    OUT PWSTR pszName);

/*
Parameters
pszName
[in] name of account.
pcbSize
[in/out] size of pISecurityInfo buffer on input. Returns the size used (or required)
pISecurityInfo
[out] pointer to ADBI_SECURITY_INFO structure. Filled with expanded group and privilege information.
*/
LONG
ADBIGetSecurityInfo(
    IN PCWSTR pszName,
    IN OUT PDWORD pcbSize,
    OUT PADBI_SECURITY_INFO pISecurityInfo);

#ifdef __cplusplus
}
#endif

#endif // __ADBI_H__

