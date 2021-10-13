//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
////////////////////////////////////////////////////////////////////////////////
//
//  WHPU Test Help Suite
//
//  Module: hqamem.h
//          Memory management functions
//
//  Revision history:
//      10/09/1997  lblanco     Created.
//
////////////////////////////////////////////////////////////////////////////////

#ifndef __HQAMEM_H__
#define __HQAMEM_H__

#include <tlhelp32.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

////////////////////////////////////////////////////////////////////////////////
// Macros

#define TEST_MAXPROCESSES   32

////////////////////////////////////////////////////////////////////////////////
// Types

typedef struct
{
    DWORD   m_dwPID;
    TCHAR   m_szProcessName[MAX_PATH];
    ULONG   m_nHeaps,
            m_nUsedBytes,
            m_nFreeBytes,
            m_nAllocs,
            m_nFreeBlocks;
} TEST_PROCESSDATA;

typedef struct
{
    TEST_PROCESSDATA    m_rgpi[TEST_MAXPROCESSES];
    int                 m_nProcesses;
} TEST_MEMORYDATA;

typedef void(*PFNHEAPPROCESS)(PROCESSENTRY32 *pProcessEntry, HEAPLIST32 *pHeapList, HEAPENTRY32 *pHeapEntry, LPVOID pvData);

////////////////////////////////////////////////////////////////////////////////
// Prototypes

void    DisableOutOfMemoryManager(void);
void    OutputTestMemoryData();
void    OutputProcessHeap(TCHAR *szProcName);
void    GetTestMemoryData(TEST_MEMORYDATA *pmd, PFNHEAPPROCESS pfnProc = NULL, LPVOID pvData = NULL);
void    EstimateAvailableMemory(DWORD *pdwFree, DWORD *pdwFragments);
BOOL    SetMemorySliderPosition(DWORD *pdwStorePages, UINT nPercentProgMem);
BOOL    RestoreMemorySliderPosition(DWORD dwStorePages);
ULONG   GetAllFreeBytes();

////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
};
#endif /* __cplusplus */

#endif // __HQAMEM_H__
