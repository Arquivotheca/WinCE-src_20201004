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
#ifndef __FSDMGRP_H__
#define __FSDMGRP_H__

#include <windows.h>
#include <types.h>
#include <extfile.h>
#include <errorrep.h>
#include <diskio.h>
#include <storemgr.h>
#include <fsnotify_api.h>
#include <fsioctl.h>
#include <pfsioctl.h>

#include "fsddbg.h"
#include "fsdhelper.hpp"

#ifndef UNDER_CE
#define STRSAFE_NO_DEPRECATE
#include <ntincludes.hpp>
#endif

#define HDSK    PDSK
#define HVOL    HANDLE
#define PVOLUME PVOL
#define PFILE   PHDL
#define PSEARCH PHDL

typedef class LogicalDisk_t DSK, *PDSK;
typedef class MountedVolume_t VOL, *PVOL;
typedef class FileSystemHandle_t HDL, *PHDL;
typedef class StoreDisk_t STORE, *PSTORE;
typedef class PartitionDisk_t PARTITION, *PPARTITION;

#include <fsdmgr.h> // This inclusiong has to be deferred until HDSK, HVOL, etc, are defined
 
typedef struct _FSDLOADLIST
{
    TCHAR       szPath[MAX_PATH];
    TCHAR       szName[MAX_PATH];
    DWORD       dwOrder;
    struct _FSDLOADLIST *pNext;
} FSDLOADLIST, *PFSDLOADLIST;

#define LOAD_FLAG_SYNC      0x00000001
#define LOAD_FLAG_ASYNC     0x00000002

PFSDLOADLIST LoadFSDList( HKEY hKey, DWORD dwFlags, const WCHAR *szPath = NULL, PFSDLOADLIST pExisting = NULL, BOOL bReverse = FALSE);

#endif  // __FSDMGRP_H__
