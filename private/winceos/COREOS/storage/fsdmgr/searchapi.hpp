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
#ifndef __ENUMAPI_HPP__
#define __ENUMAPI_HPP__

// API set handle for these APIs.
extern HANDLE hSearchAPI;
LRESULT InitializeSearchPI ();

// Search Handle-based APIs.
//
// These APIs take a search handle. These APIs parallel the HT_FIND API set
// type and are registered as such during in itialization of the storage 
// manager.
//
EXTERN_C BOOL FSDMGR_FindClose (FileSystemHandle_t* pHandle);
EXTERN_C BOOL FSDMGR_FindNextFileW (FileSystemHandle_t* pHandle, WIN32_FIND_DATA* pFindData, DWORD SizeOfFindData);

#endif // __ENUMAPI_HPP__

