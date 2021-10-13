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
extern PROCESS *g_pFocusProcOverride;
extern ACCESSKEY g_ulOldKey;


//
// Counted String
//

DBGPARAM dpCurSettings;

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


typedef struct _NameandBase
{
    PCHAR   szName;
    ULONG   ImageBase;
    ULONG   ImageSize;
    ULONG   dwDllRwStart;
    ULONG   dwDllRwEnd;
} NAMEANDBASE, *PNAMEANDBASE;


void kdbgWtoA(LPWSTR pWstr, LPCHAR pAstr);
DWORD kdbgwcslen(LPWSTR pWstr);

//
// Define NK function prototypes.
//
BOOL 
GetNameandImageBase(
    PPROCESS pProc, 
    DWORD dwAddress, 
    PNAMEANDBASE pnb, 
    BOOL fRedundant,
    DWORD BreakCode
    );


BOOL 
GetProcessAndThreadInfo(
    IN PSTRING pParamBuf
    );


PVOID
SafeDbgVerify(
    PVOID  pvAddr,
    int option
    );


ULONG 
VerifyAddress(
    PVOID Addr
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

