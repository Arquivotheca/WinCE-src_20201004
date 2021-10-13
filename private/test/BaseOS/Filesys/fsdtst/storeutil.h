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
#include <windows.h>
#include <diskio.h>
#include <storemgr.h>
#include "tuxmain.h"
#include "mfs.h"

#ifndef __STOREUTIL_H__
#define __STOREUTIL_H__

#define SZ_PART_FMT             _T("PART%02X")

#define STORAGE_PROFILES_KEY    _T("System\\StorageManager\\Profiles")

typedef struct _MOUNTED_FILESYSTEM_LIST
{
    CMtdPart *pMtdPart;
    _MOUNTED_FILESYSTEM_LIST *pNextFS;

} MOUNTED_FILESYSTEM_LIST, *PMOUNTED_FILESYSTEM_LIST;

CMtdPart *GetPartition(DWORD index);
DWORD BuildFSList(VOID);
VOID DestroyFSList(VOID);
DWORD GetPartitionCount(VOID);
#endif //__STORE_H__
