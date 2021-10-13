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
// --------------------------------------------------------------------
//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
// Copyright (c) 1998-2000 Microsoft Corporation.  All rights reserved.
//                                                                     

#ifndef __HEAPTEST_H__
#define __HEAPTEST_H__

#ifdef __cplusplus
extern "C" {
#endif

// --------------------------------------------------------------------
//***** Constants and Macros
//******************************************************************************

// Heap Test Flags
#define HTF_BREAK_ON_CORRUPTION  0x00000001
#define HTF_CALL_HEAPVALIDATE    0x00000002
#define HTF_NO_OUTPUT            0x00000004

// Heap Test Messages for callback
#define HTM_HEAPVALIDATE_FAILED  1
#define HTM_BLOCK_CORRUPTED      2
#define HTM_HEAPFREE_FAILED      3


//******************************************************************************
//***** Types and Structures
//******************************************************************************

typedef BOOL (CALLBACK *HEAPTESTPROC)(HANDLE hSession, HANDLE hHeap, DWORD dwMessage,
                                      LPARAM lParam, DWORD dwUserData);

typedef struct _HTS_BLOCK_CORRUPTED {
   DWORD dwStartAddress;
   DWORD dwSize;
} HTS_BLOCK_CORRUPTED, *PHTS_BLOCK_CORRUPTED;

typedef struct _HTS_HEAPFREE_FAILED {
   DWORD dwStartAddress;
   DWORD dwSize;
} HTS_HEAPFREE_FAILED, *PHTS_HEAPFREE_FAILED;


//******************************************************************************
//***** Prototypes
//******************************************************************************

HANDLE WINAPI HeapTestStartSession(HANDLE hHeap, DWORD dwInterval, DWORD dwThreadPriority,
                                   HEAPTESTPROC heaptestproc, DWORD dwUserData, DWORD dwFlags);

BOOL   WINAPI HeapTestStopSession(HANDLE hSession);

BOOL   WINAPI HeapTestHexDump(DWORD dwAddress, DWORD dwSize, DWORD dwFlags);


#ifdef __cplusplus
} // extern "C"
#endif

#endif // __HEAPTEST_H__
