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
BOOL ADBInit();
BOOL ADBDeinit();

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



//
// FSINT_ADBCreateAccount - implementation of ADBCreateAccount
//
LONG 
FSINT_ADBCreateAccount(
    IN LPCWSTR pszName,
    IN DWORD dwAcctFlags,
    IN DWORD Reserved);
LONG 
FSEXT_ADBCreateAccount(
    IN LPCWSTR pszName,
    IN DWORD dwAcctFlags,
    IN DWORD Reserved);

//
// FSINT_ADBDeleteAccount - implementation of ADBDeleteAccount
//
LONG 
FSINT_ADBDeleteAccount(
    IN LPCWSTR pszName);

LONG 
FSEXT_ADBDeleteAccount(
    IN LPCWSTR pszName);

//
// FS_ADBAddAccountToGroup - implementation of ADBAddAccountToGroup
//
LONG 
FS_ADBAddAccountToGroup(
    IN PCWSTR pszName,
    IN PCWSTR pszGroupName);

//
// FS_ADBRemoveAccountFromGroup - implementation of ADBRemoveAccountFromGroup
//
LONG 
FS_ADBRemoveAccountFromGroup(
    IN PCWSTR pszName,
    IN PCWSTR pszGroupName);


//
// FSINT_ADBGetAccountProperty/FSEXT_ADBGetAccountProperty - implementation of ADBGetAccountProperty
//
LONG
FSINT_ADBGetAccountProperty(
    IN PCWSTR pszName,
    IN DWORD propertyId,
    IN OUT PDWORD pcbPropertyValue,
    OUT PVOID pValue);
LONG
FSEXT_ADBGetAccountProperty(
    IN PCWSTR pszName,
    IN DWORD propertyId,
    IN OUT PDWORD pcbPropertyValue,
    OUT PVOID pValue);


//
// FSINT_ADBSetAccountProperties/FSEXT_ADBSetAccountProperties - implementation of ADBGetAccountProperty
//
LONG
FSINT_ADBSetAccountProperties(
    IN PCWSTR pszName,
    IN DWORD cProperties,
    IN ADB_BLOB *rgProperties    
);
LONG
FSEXT_ADBSetAccountProperties(
    IN PCWSTR pszName,
    IN DWORD cProperties,
    IN ADB_BLOB *rgProperties    
);


//
// FSINT_ADBEnumAccounts/FSEXT_ADBEnumAccounts - implementation of ADBEnumAccounts
//
LONG
FSINT_ADBEnumAccounts(
    IN DWORD dwFlags,
    IN PCWSTR pszPrevName,
    IN OUT PDWORD pchNextName,
    OUT PWSTR pszNextName);
LONG
FSEXT_ADBEnumAccounts(
    IN DWORD dwFlags,
    IN PCWSTR pszPrevName,
    IN OUT PDWORD pchNextName,
    OUT PWSTR pszNextName);


//
// FSINT_ADBGetAccountSecurityInfo/FSEXT_ADBGetAccountSecurityInfo - implementation of ADBGetAccountSecurityInfo
//
LONG
FSINT_ADBGetAccountSecurityInfo(
    IN PCWSTR pszName,
    IN OUT PDWORD pcbBuf,
    OUT ADB_SECURITY_INFO *pSecurityInfo
    );
LONG
FSEXT_ADBGetAccountSecurityInfo(
    IN PCWSTR pszName,
    IN OUT PDWORD pcbBuf,
    OUT ADB_SECURITY_INFO *pSecurityInfo
    );



#ifdef __cplusplus
}
#endif

#endif // __ADBI_H__

