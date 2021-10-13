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
/*---------------------------------------------------------------------------*\
 *
 *  module: sysutils.h
 *
 *  purpose:    public header for system utilities module 
 *
\*---------------------------------------------------------------------------*/
#ifndef _SYSUTILS_H_
#define _SYSUTILS_H_

#include <windows.h>
#include <tlhelp32.h>
#include <qautils.h>

//@doc SYSUTIL

#ifdef __cplusplus
	extern "C" {
#endif

		

typedef void ( cdecl *PRINTPROC )( LPTSTR lpszFormat, ... );



//
// memory management
//

void *QA_Alloc( UINT nSize );
void *QA_ReAlloc( void *pvMem, UINT nSize );
BOOL QA_Free( void *pvMem );



//
// waiting
//
#define WAITSLEEP		100   // sleep time (in ms) between attempts.
#define SYSUT_TIMEOUT	20    // 20 sec wait time for windows


BOOL RWaitForSystemIdleEx( DWORD dwMaxWaitMilliSecs );
BOOL RWaitForSystemIdle( void );
#define _WAIT   RWaitForSystemIdle()
#define DWaitForSystemIdle()    _WAIT


/*
    @struct HEAPINFO | Structure to store heap information obtained
    by the <f GetProcessHeapInfo> function. Note this information is for
    all heaps owned by the process.
*/
typedef struct _HEAPINFO {
    DWORD   dwProcID;           // @field ProcessID
    DWORD   dwNumBlocks;        // @field Number of blocks
    DWORD   dwSizeBlocks;       // @field Total size blocks
    DWORD   dwNumFreeBlocks;    // @field Number of free blocks
    DWORD   dwSizeFreeBlocks;   // @field Total size free blocks
    DWORD   dwNumFragmentedBlocks;  // @field Number of fragmented blocks
    DWORD   dwSizeFragmentedBlocks; // @field Size fragmented blocks
    DWORD   dwNumBytes;         // @field Number of bytes not freed or decommitted
    DWORD   dwNumUsedBlocks;    // @field Number of blocks not freed or decommitted
}   HEAPINFO, *LPHEAPINFO;

BOOL InitMemStatus( MEMORYSTATUS *ms );
BOOL LogMemStatus( MEMORYSTATUS *prevms, PRINTPROC pPrint );

BOOL SetProgramMemoryDivision( DWORD *dwStorePages, DWORD dwKBytes );
BOOL ResetProgramMemoryDivision( DWORD *dwStorePages );

#ifdef UNDER_CE
BOOL EnumerateProcessInfo( LPPROCESSENTRY32 pProcInfo, UINT cbSize, BOOL fInit, PRINTPROC pPrint );
BOOL GetProcessHeapInfo( DWORD dwProcID, LPHEAPINFO phi, PRINTPROC pPrint );
#endif
DWORD GetProcessIdByName( LPCTSTR ptszTargetAppName );

BOOL GetFreeProgramMemory( DWORD *pdwFree );

BOOL StartLowMemMonitor( LPVOID *pData, BOOL fHibernate, BOOL fMemMonitor, PRINTPROC pPrint );
BOOL CloseLowMemMonitor( LPVOID *pData );
BOOL ChewToHibernate( LPVOID *pData, BOOL fChew, DWORD *pdwBytesEaten );


#ifdef __cplusplus
	}
#endif

#endif /* _SYSUTILS_H_ */
