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
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2002 BSQUARE Corporation. All rights reserved.
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
// --------------------------------------------------------------------
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// --------------------------------------------------------------------
//
#ifndef MEM_HLP_H
#define MEM_HLP_H

#include <windows.h>
#include <sdmem.h>
#include <sdcommon.h>
#include <sd_tst_cmn.h>

typedef struct
{
    UINT uiFree;
    UINT uiTotal;
}MemData;

//void GetMemStat(UINT indent, MemData *md, SD_MEMORY_LIST_HANDLE hMem);

SD_MEMORY_LIST_HANDLE CreateAndGetStat(UINT indent,  PTEST_PARAMS  pTestParams, MemData *md, DWORD dwNumBlks, DWORD dwBlkSize);

BOOL AllocWriteAndGetStat(UINT indent,  PTEST_PARAMS  pTestParams, MemData *md, SD_MEMORY_LIST_HANDLE hMem, PVOID *ppvMem, DWORD dwBlkSize);

BOOL FreeAndGetStat(UINT indent,  PTEST_PARAMS  pTestParams, MemData *md, SD_MEMORY_LIST_HANDLE hMem, PVOID *pvMem);

#endif //MEM_HLP_H

