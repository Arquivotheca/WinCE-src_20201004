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

#pragma once


#define DBP_HANDLE DWORD

#define MAX_SW_DATA_BREAKPOINTS 8
#define MAX_HW_DATA_BREAKPOINTS 8
#define MAX_DATA_BREAKPOINTS (MAX_SW_DATA_BREAKPOINTS + MAX_HW_DATA_BREAKPOINTS)

#define INDEX_TO_HANDLE(X) (X+1)
#define HANDLE_TO_INDEX(X) (X-1)

typedef struct _DATA_BREAKPOINT_PAGE {
    PPROCESS pVM; // VM Associated with breakpoint.
    DWORD dwAddress; // Page Address
    PTENTRY PTEOriginal; //original page table entry.
    WORD wWriteRefCount; //number of write bp's on page
    WORD wReadRefCount;    //number of read bp's on page.
    WORD wRefCount; 
} DATA_BREAKPOINT_PAGE, *PDATA_BREAKPOINT_PAGE;


typedef struct _DATA_BREAKPOINT_ENTRY {
    PPROCESS pVM; // VM Associated with breakpoint.
    DWORD dwAddress; // Address that the user specified for bp
    DWORD dwBytes;
    union
    {
        DWORD dwCookie;  //Handle returned for hardware bp's
        PDATA_BREAKPOINT_PAGE pDBPage; //protected page for software bp's
    };
    BYTE fHardware : 1;
    BYTE fReadDBP : 1;
    BYTE fWriteDBP : 1;
    BYTE fEnabled : 1;  
    BYTE fInUse : 1;
    BYTE fCommitted : 1;
} DATA_BREAKPOINT_ENTRY, *PDATA_BREAKPOINT_ENTRY;


void DataBreakpointsInit();
void DataBreakpointsDeInit();

HRESULT AddDataBreakpoint(PPROCESS pVM, DWORD dwAddress, DWORD dwBytes, BOOL fHardware, BOOL fReadDBP, BOOL fWriteDBP, DWORD *dwBPHandle);
HRESULT DeleteDataBreakpoint(DWORD dwHandle);
DWORD FindDataBreakpoint(PPROCESS pVM, DWORD dwAddress, BOOL fWrite);
DWORD EnableDataBreakpointsOnPage(PPROCESS pVM, DWORD dwAddress);
DWORD DisableDataBreakpointsOnPage(PPROCESS pVM, DWORD dwAddress);
HRESULT EnableDataBreakpoint(DWORD dwHandle);
HRESULT DisableDataBreakpoint(DWORD dwHandle);
DWORD EnableAllDataBreakpoints();
DWORD DisableAllDataBreakpoints();

BOOL DataBreakpointTrap(EXCEPTION_RECORD *pExceptionRecord, CONTEXT *pContext, BOOL SecondChance, BOOL *pfIgnore);
BOOL DataBreakpointVMPageOut(DWORD dwPageAddr, DWORD dwNumPages);
BOOL HdstubIsDataBreakpoint(DWORD dwAddress, BOOL fWrite);
void HdstubCommitHWDataBreakpoints();

