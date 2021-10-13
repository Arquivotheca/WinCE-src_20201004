//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//+-------------------------------------------------------------------------
//  Microsoft Windows CE
//
//
//  File:       ntlmi.h
//
//  Contents:   Internal NTLM User Management Routines
//
//
//----------------------------------------------------------------------------

#ifndef __NTLMI_H__
#define __NTLMI_H__

#ifdef __cplusplus
extern "C" {
#endif

#define USERDB_DATA_TYPE_LM_HASH 1
#define USERDB_DATA_TYPE_NT_HASH 2
#define USERDB_DATA_TYPE_PASSWORD 3

// ------------------------------------------------
// COMMON ERROR CODES
//
//	ERROR_SUCCESS - operation success
// 	ERROR_INTERNAL_ERROR - non-specific error
//	ERROR_INVALID_PARAMETER - arguments are invalid
// 	ERROR_OUTOFMEMORY - no more memory
//	TRUE - operation success
//	FALSE - operation failed

// ------------------------------------------------
// DESCRIPTION
// 	Get the stored user info
// ARGS
// 	IN pszUser - username in unicode format
//	IN dwDataType - type of information to retrieve
//			USERDB_DATA_TYPE_LM_HASH - lm hash
//			USERDB_DATA_TYPE_NT_HASH - nt hash
//			USERDB_DATA_TYPE_PASSWORD - password
//	IN OUT pData - buffer to hold information
//			can be NULL to figure out size of buffer required
//	IN out pcbData - size of buffer in bytes
//			on output contains actual size of information
	
BOOL NTLMGetUserInfo(
    IN LPTSTR pszUser,
    IN DWORD dwDataType,
    OUT BYTE*  pData,
    IN OUT DWORD* pcbData);

// ------------------------------------------------
// DESCRIPTION
// 	Should the password of a local user be stored in registry or not
// ARGS
// 	IN bSavePassword - set to TRUE if password can be saved

BOOL NTLMSavePassword(BOOL bSavePassword);

#ifdef __cplusplus
}
#endif

#endif


