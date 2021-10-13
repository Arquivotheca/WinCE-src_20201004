//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++


Module Name:

    dbg.h

Abstract:

    dbg module header file

Environment:

    WinCE

--*/

#ifndef _DBGH_
#define _DBGH_


#include "kernel.h"


extern PCALLSTACK pStk;
extern PPROCESS pLastProc;
extern PTHREAD pWalkThread;
extern BOOL g_fTopFrame;

extern DWORD g_dwExceptionCode;

extern PROCESS *g_pFocusProcOverride;


//
// Counted String
//

typedef struct _STRING {
    USHORT Length;
    USHORT MaximumLength;
#ifdef MIDL_PASS
    [size_is(MaximumLength), length_is(Length) ]
#endif // MIDL_PASS
    PCHAR Buffer;
} STRING;
typedef STRING *PSTRING;

typedef STRING ANSI_STRING;
typedef PSTRING PANSI_STRING;

typedef STRING OEM_STRING;
typedef PSTRING POEM_STRING;

//
// CONSTCounted String
//

typedef STRING CANSI_STRING;
typedef PSTRING PCANSI_STRING;


void kdbgWtoA(LPWSTR pWstr, LPCHAR pAstr);
DWORD kdbgwcslen(LPWSTR pWstr);

//
// Define NK function prototypes.
//


BOOL
KdpRemapVirtualMemory(
    LPVOID lpvAddrNewVirtual,
    LPVOID lpvAddrOrigVirtual,
    DWORD* pdwData
    );


typedef struct _KD_MODULE_INFO
{
    PCHAR   szName;
    ULONG   ImageBase;
    ULONG   ImageSize;
    ULONG   dwDllRwStart;
    ULONG   dwDllRwEnd;
    ULONG   dwTimeStamp;
    HMODULE hDll;
    DWORD   dwInUse;
    WORD    wFlags;
    BYTE    bTrustLevel;
} KD_MODULE_INFO;


BOOL
GetModuleInfo(
    PROCESS *pProc,
    DWORD dwStructAddr,
    KD_MODULE_INFO *pkmodi,
    BOOL fRedundant,
    BOOL fUnloadSymbols
    );


BOOL
GetProcessAndThreadInfo(
    IN PSTRING pParamBuf
    );


PVOID
SafeDbgVerify(
    PVOID  pvAddr,
    BOOL fProbeOnly
    );


void *
MapToDebuggeeCtxKernEquivIfAcc(
    BYTE * pbAddr,
    BOOL fProbeOnly
    );


BOOL
TranslateRA(
    DWORD *pRA,
    DWORD *pSP,
    DWORD dwStackFrameAddr,
    DWORD *pdwProcHandle
    );


BOOL
TranslateAddress(
    PULONG lpAddr,
    PCONTEXT pCtx
    );


ULONG
Kdstrtoul(
    LPBYTE lpStr,
    ULONG radix
    );


void
SetKernLoadAddress(
    void
    );


#endif // _DBGH_

