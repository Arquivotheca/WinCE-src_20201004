//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
