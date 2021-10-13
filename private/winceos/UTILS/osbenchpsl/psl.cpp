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
//------------------------------------------------------------------------------
//
//  THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
//  ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
//  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
//  PARTICULAR PURPOSE.
//
//------------------------------------------------------------------------------
#include <windows.h>
#include <tchar.h>
#include <stdarg.h>
#include <pkfuncs.h>
#include <nkintr.h>
#include <ceddk.h>
#include <osbench.hpp>
#include "psl.hpp"

#ifdef _PREFAST_
#pragma warning(push)
#pragma warning(disable:34005 "Turn off PREFast warnings about calling deprecated APIs we need them for <CE6.0")
#endif 

#define PSL_ITERATIONS 1000

#define IMPLICIT_CALL PRIV_IMPLICIT_CALL

//
// The actual calls to the API's are macros that trap into the kernel.
// Here we defined a wrapper macro for each of the exported functions.
//

// minimal no parameter call
#define mApi1   IMPLICIT_DECL(DWORD, g_nApiSetId, 2, (void))
// 6-DWORD args
#define mApi2   IMPLICIT_DECL(DWORD, g_nApiSetId, 3, (DWORD,DWORD,DWORD,DWORD,DWORD,DWORD))
// 6-PTR to DWORD args
#define mApi3   IMPLICIT_DECL(DWORD, g_nApiSetId, 4, (PDWORD,PDWORD,PDWORD,PDWORD,PDWORD,PDWORD))
// 6-I_PTR args
#define mApi4   IMPLICIT_DECL(DWORD, g_nApiSetId, 5, \
    (PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD ) )
// 6-O_PTR args
#define mApi5   IMPLICIT_DECL(DWORD, g_nApiSetId, 6, \
    (PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD ) )
// 6-IO_PTR args
#define mApi6   IMPLICIT_DECL(DWORD, g_nApiSetId, 7, \
    (PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD ) )
// 6-String (wide) args
#define mApi7   IMPLICIT_DECL(DWORD, g_nApiSetId, 8, (LPWSTR, LPWSTR, LPWSTR, LPWSTR, LPWSTR, LPWSTR ) )
// 6-I_PTR args
#define mApi8   IMPLICIT_DECL(DWORD, g_nApiSetId, 9, (PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD ) )
// 6-O_PTR args
#define mApi9   IMPLICIT_DECL(DWORD, g_nApiSetId, 10, (PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD ) )
// 6-IO_PTR args
#define mApi10  IMPLICIT_DECL(DWORD, g_nApiSetId, 11, (PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD ) )
// 6-String (wide) args
#define mApi11  IMPLICIT_DECL(DWORD, g_nApiSetId, 12, (LPWSTR, LPWSTR, LPWSTR, LPWSTR, LPWSTR, LPWSTR ) )
// 6-I_PTR args
#define mApi12   IMPLICIT_DECL(DWORD, g_nApiSetId, 13, (PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD ) )
// 6-O_PTR args
#define mApi13   IMPLICIT_DECL(DWORD, g_nApiSetId, 14, (PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD ) )
// 6-IO_PTR args
#define mApi14  IMPLICIT_DECL(DWORD, g_nApiSetId, 15, (PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD ) )
// 6-String (wide) args
#define mApi15  IMPLICIT_DECL(DWORD, g_nApiSetId, 16, (LPWSTR, LPWSTR, LPWSTR, LPWSTR, LPWSTR, LPWSTR ) )
// helper to set expected buf size
#define mApiSetBufSize  IMPLICIT_DECL(DWORD, g_nApiSetId, 17, ( DWORD ) )

// define these against _BOWMORE macros so that the calls will still work on
// Bowmore where FIRST_METHOD has been redefined.
// minimal no parameter call
#define bmApi1   IMPLICIT_DECL_BOWMORE(DWORD, g_nApiSetId, 2, (void))
// 6-DWORD args
#define bmApi2   IMPLICIT_DECL_BOWMORE(DWORD, g_nApiSetId, 3, (DWORD,DWORD,DWORD,DWORD,DWORD,DWORD))
// 6-PTR to DWORD args
#define bmApi3   IMPLICIT_DECL_BOWMORE(DWORD, g_nApiSetId, 4, (PDWORD,PDWORD,PDWORD,PDWORD,PDWORD,PDWORD))
// 6-I_PTR args
#define bmApi4   IMPLICIT_DECL_BOWMORE(DWORD, g_nApiSetId, 5, \
    (PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD ) )
// 6-O_PTR args
#define bmApi5   IMPLICIT_DECL_BOWMORE(DWORD, g_nApiSetId, 6, \
    (PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD ) )
// 6-IO_PTR args
#define bmApi6   IMPLICIT_DECL_BOWMORE(DWORD, g_nApiSetId, 7, \
    (PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD ) )
// 6-String (wide) args
#define bmApi7   IMPLICIT_DECL_BOWMORE(DWORD, g_nApiSetId, 8, (LPWSTR, LPWSTR, LPWSTR, LPWSTR, LPWSTR, LPWSTR ) )
// 6-I_PTR args
#define bmApi8   IMPLICIT_DECL_BOWMORE(DWORD, g_nApiSetId, 9, (PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD ) )
// 6-O_PTR args
#define bmApi9   IMPLICIT_DECL_BOWMORE(DWORD, g_nApiSetId, 10, (PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD ) )
// 6-IO_PTR args
#define bmApi10  IMPLICIT_DECL_BOWMORE(DWORD, g_nApiSetId, 11, (PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD ) )
// 6-String (wide) args
#define bmApi11  IMPLICIT_DECL_BOWMORE(DWORD, g_nApiSetId, 12, (LPWSTR, LPWSTR, LPWSTR, LPWSTR, LPWSTR, LPWSTR ) )
// 6-I_PTR args
#define bmApi12   IMPLICIT_DECL_BOWMORE(DWORD, g_nApiSetId, 13, (PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD ) )
// 6-O_PTR args
#define bmApi13   IMPLICIT_DECL_BOWMORE(DWORD, g_nApiSetId, 14, (PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD ) )
// 6-IO_PTR args
#define bmApi14  IMPLICIT_DECL_BOWMORE(DWORD, g_nApiSetId, 15, (PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD ) )
// 6-String (wide) args
#define bmApi15  IMPLICIT_DECL_BOWMORE(DWORD, g_nApiSetId, 16, (LPWSTR, LPWSTR, LPWSTR, LPWSTR, LPWSTR, LPWSTR ) )
// helper to set expected buf size
#define bmApiSetBufSize IMPLICIT_DECL_BOWMORE(DWORD, g_nApiSetId, 17, ( DWORD ) )


static DWORD g_dwBase;
int     g_nApiSetId;
HANDLE  g_hApi;
DWORD   g_dwBufSize = 0;

DWORD OsBenchAPICall1(void);
DWORD OsBenchAPICall2(DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
DWORD OsBenchAPICall3(PDWORD, PDWORD, PDWORD, PDWORD, PDWORD, PDWORD);
DWORD OsBenchAPI6PTR(PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD, PVOID, DWORD);
DWORD OsBenchSetBufSize( DWORD );
DWORD OsBenchAPIWSTR( LPWSTR, LPWSTR, LPWSTR, LPWSTR, LPWSTR, LPWSTR );
void  OsBenchNotifyCallback(DWORD, HPROCESS, HTHREAD);

PFNVOID PfnTable[] = {
    (PFNVOID)OsBenchNotifyCallback,                 // 0  - NotifyCallBack
    NULL,                                           // 1  - reserved
    (PFNVOID)OsBenchAPICall1,                       // 2  - mApi1: No params
    (PFNVOID)OsBenchAPICall2,                       // 3  - mApi2: 6xDW
    (PFNVOID)OsBenchAPICall3,                       // 4  - mApi3: 6xPDW
    (PFNVOID)OsBenchAPI6PTR,                        // 5  - mApi4: 6xIPTR-SM
    (PFNVOID)OsBenchAPI6PTR,                        // 6  - mApi5: 6xOPTR-SM
    (PFNVOID)OsBenchAPI6PTR,                        // 7  - mApi6: 6xIOPTR-SM
    (PFNVOID)OsBenchAPIWSTR,                        // 8  - mApi7: 6xWSTR-SM
    (PFNVOID)OsBenchAPI6PTR,                        // 9  - mApi8: 6xIPTR-LG
    (PFNVOID)OsBenchAPI6PTR,                        // 10 - mApi9: 6xOPTR-LG
    (PFNVOID)OsBenchAPI6PTR,                        // 11 - mApi10: 6xIOPTR-LG
    (PFNVOID)OsBenchAPIWSTR,                        // 12 - mApi11: 6xWSTR-LG
    (PFNVOID)OsBenchAPI6PTR,                        // 13 - mApi8: 6xIPTR-UD
    (PFNVOID)OsBenchAPI6PTR,                        // 14 - mApi9: 6xOPTR-UD
    (PFNVOID)OsBenchAPI6PTR,                        // 15 - mApi10: 6xIOPTR-UD
    (PFNVOID)OsBenchAPIWSTR,                        // 16 - mApi11: 6xWSTR-UD
    (PFNVOID)OsBenchSetBufSize,                     // 17 - mApiSetBufSize
};

#define NUM_APIS    (sizeof(PfnTable) / sizeof(PfnTable[0]))

//
// The signature table defines the number of parameters and their
// type (DWORD or Pointer)
//
const ULONGLONG SigTable[NUM_APIS] = {
    FNSIG3(DW,DW,DW),                                       // 0  - Notify
    FNSIG0(),                                               // 1  - reserved
    FNSIG0(),                                               // 2  - mApi1
    FNSIG6(DW,DW,DW,DW,DW,DW),                              // 3  - mApi2
    FNSIG6(IO_PDW, IO_PDW, IO_PDW, IO_PDW, IO_PDW, IO_PDW), // 4  - mApi3
    FNSIG12(I_PTR, DW, I_PTR, DW, I_PTR, DW, I_PTR, DW, I_PTR, DW, I_PTR, DW),
                                                            // 5  - mApi4
    FNSIG12(O_PTR, DW, O_PTR, DW, O_PTR, DW, O_PTR, DW, O_PTR, DW, O_PTR, DW),
                                                            // 6  - mApi5
    FNSIG12(IO_PTR, DW, IO_PTR, DW, IO_PTR, DW, IO_PTR, DW, IO_PTR, DW, IO_PTR, DW),                                                        // 7  - mApi6
    FNSIG6(I_WSTR,  I_WSTR, I_WSTR, I_WSTR, I_WSTR, I_WSTR),// 8  - mApi7
    FNSIG12(I_PTR, DW, I_PTR, DW, I_PTR, DW, I_PTR, DW, I_PTR, DW, I_PTR, DW),
                                                            // 9  - mApi8
    FNSIG12(O_PTR, DW, O_PTR, DW, O_PTR, DW, O_PTR, DW, O_PTR, DW, O_PTR, DW),
                                                            // 10 - mApi9
    FNSIG12(IO_PTR, DW, IO_PTR, DW, IO_PTR, DW, IO_PTR, DW, IO_PTR, DW, IO_PTR, DW),                                                        // 11 - mApi10
    FNSIG6(I_WSTR, I_WSTR, I_WSTR, I_WSTR, I_WSTR, I_WSTR), // 12 - mApi11
    FNSIG12(I_PTR, DW, I_PTR, DW, I_PTR, DW, I_PTR, DW, I_PTR, DW, I_PTR, DW),
                                                            // 13  - mApi12
    FNSIG12(O_PTR, DW, O_PTR, DW, O_PTR, DW, O_PTR, DW, O_PTR, DW, O_PTR, DW),
                                                            // 14 - mApi13
    FNSIG12(IO_PTR, DW, IO_PTR, DW, IO_PTR, DW, IO_PTR, DW, IO_PTR, DW, IO_PTR, DW),                                                        // 15 - mApi14
    FNSIG6(I_WSTR, I_WSTR, I_WSTR, I_WSTR, I_WSTR, I_WSTR), // 16 - mApi15
    FNSIG1( DW )                                            // 17 - mApiSetBufSize

};

#pragma warning(push)
#pragma warning(disable:4293)

const DWORD BMSigTable[NUM_APIS] = {
    BOWMORE_FNSIG3( DW, DW, DW ),                           // 0  - Notify
    BOWMORE_FNSIG0(),                                       // 1  - reserved
    BOWMORE_FNSIG0(),                                       // 2  - mApi1
    BOWMORE_FNSIG6(DW,DW,DW,DW,DW,DW),                      // 3  - mApi2
    BOWMORE_FNSIG12(PTR, DW, PTR, DW, PTR, DW, PTR, DW, PTR, DW, PTR, DW ),
                                                            // 4  - mApi3
    BOWMORE_FNSIG12(PTR, DW, PTR, DW, PTR, DW, PTR, DW, PTR, DW, PTR, DW ),
                                                            // 5  - mApi4
    BOWMORE_FNSIG12(PTR, DW, PTR, DW, PTR, DW, PTR, DW, PTR, DW, PTR, DW ),
                                                            // 6  - mApi5
    BOWMORE_FNSIG12(PTR, DW, PTR, DW, PTR, DW, PTR, DW, PTR, DW, PTR, DW ),
                                                            // 7  - mApi6
    BOWMORE_FNSIG6(PTR, PTR, PTR, PTR, PTR, PTR),
                                                            // 8  - mApi7
    BOWMORE_FNSIG12(PTR, DW, PTR, DW, PTR, DW, PTR, DW, PTR, DW, PTR, DW ),
                                                            // 9  - mApi8
    BOWMORE_FNSIG12(PTR, DW, PTR, DW, PTR, DW, PTR, DW, PTR, DW, PTR, DW ),
                                                            // 10 - mApi9
    BOWMORE_FNSIG12(PTR, DW, PTR, DW, PTR, DW, PTR, DW, PTR, DW, PTR, DW ),
                                                            // 11 - mApi10
    BOWMORE_FNSIG6(PTR, PTR, PTR, PTR, PTR, PTR ),
                                                            // 12 - mApi11
    BOWMORE_FNSIG12(PTR, DW, PTR, DW, PTR, DW, PTR, DW, PTR, DW, PTR, DW ),
                                                            // 13 - mApi12
    BOWMORE_FNSIG12(PTR, DW, PTR, DW, PTR, DW, PTR, DW, PTR, DW, PTR, DW ),
                                                            // 14 - mApi13
    BOWMORE_FNSIG12(PTR, DW, PTR, DW, PTR, DW, PTR, DW, PTR, DW, PTR, DW ),
                                                            // 15 - mApi14
    BOWMORE_FNSIG6(PTR, PTR, PTR, PTR, PTR, PTR ),
                                                            // 16 - mApi15
    BOWMORE_FNSIG1( DW )                                    // 17 - mApiSetBufSize
};

#pragma warning(pop)


enum {
    /* Kernel Call */
    SUBID_NK = 0,                                // 0

    // Non pointer calls
    SUBID_NO_PARAMS,                             // 1
    SUBID_DWORD_PARAMS,                          // 2
    SUBID_PDW_PARAMS,                            // 3

    /* Small pointer calls */
    SUBID_IPTR_SMALL_PARAMS,                     // 3
    SUBID_OPTR_SMALL_PARAMS,                     // 4
    SUBID_IOPTR_SMALL_PARAMS,                    // 5
    SUBID_IWSTR_SMALL_PARAMS,                    // 6

    /* Large pointer calls */
    SUBID_IPTR_LARGE_PARAMS,                     // 7
    SUBID_OPTR_LARGE_PARAMS,                     // 8
    SUBID_IOPTR_LARGE_PARAMS,                    // 9
    SUBID_IWSTR_LARGE_PARAMS,                    // 10

    /* User defined size ointer calls (if -s param) */
    SUBID_IPTR_USERDEF_PARAMS,                   // 11
    SUBID_OPTR_USERDEF_PARAMS,                   // 12
    SUBID_IOPTR_USERDEF_PARAMS,                  // 13
    SUBID_IWSTR_USERDEF_PARAMS,                  // 14

    NUM_SUBIDS                                   // count=15

};

#define isSmallCall( x ) \
    ( \
     (x) >= SUBID_IPTR_SMALL_PARAMS && \
     (x) <= SUBID_IWSTR_SMALL_PARAMS \
    )

#define isLargeCall( x ) \
    ( \
     (x) >= SUBID_IPTR_LARGE_PARAMS && \
     (x) <= SUBID_IWSTR_LARGE_PARAMS \
    )

#define isUserDefCall( x ) \
    ( \
     (x) >= SUBID_IPTR_USERDEF_PARAMS && \
     (x) <= SUBID_IWSTR_USERDEF_PARAMS \
    )


// Pointer tables needs to be initialized after *g_nApiSetId* is set
PAPIFN *g_pfnmApi;

const LPTSTR szDescription[2][3][NUM_SUBIDS] = {
    {
    {
        TEXT("Kernel API call (from Usermode) (roundtrip) to NK.EXE\r\n") \
        TEXT("Calls an API function (GetOwnerProcess) in NK.EXE which returns immediately."),
        TEXT("UserMode PSL API self-call (roundtrip) intra-process\r\n") \
        TEXT("Calls an API function with no parameters and returns immediately."),
        TEXT("UserMode PSL API self-call (roundtrip) intra-process\r\n") \
        TEXT("Calls an API function with 6 DWORD parameters and returns immediately."),
        TEXT("UserMode PSL API self-call (roundtrip) intra-process\r\n") \
        TEXT("Calls an API function with 6 Pointer to DWORD parameters and returns immediately."),
        TEXT("UserMode PSL API self-call (roundtrip) intra-process\r\n") \
        TEXT("Calls an API function with 6 Input 128-byte pointer parameters and returns immediately."),
        TEXT("UserMode PSL API self-call (roundtrip) intra-process\r\n") \
        TEXT("Calls an API function with 6 Output 128-byte pointer parameters and returns immediately."),
        TEXT("UserMode PSL API self-call (roundtrip) intra-process\r\n") \
        TEXT("Calls an API function with 6 Input/Output 128-byte pointer parameters and returns immediately."),
        TEXT("UserMode PSL API self-call (roundtrip) intra-process\r\n") \
        TEXT("Calls an API function with 6 Input 128-byte string parameters and returns immediately."),
        TEXT("UserMode PSL API self-call (roundtrip) intra-process\r\n") \
        TEXT("Calls an API function with 6 Input 4096-byte pointer parameters and returns immediately."),
        TEXT("UserMode PSL API self-call (roundtrip) intra-process\r\n") \
        TEXT("Calls an API function with 6 Output 4096-byte pointer parameters and returns immediately."),
        TEXT("UserMode PSL API self-call (roundtrip) intra-process\r\n") \
        TEXT("Calls an API function with 6 Input/Output 4096-byte pointer parameters and returns immediately."),
        TEXT("UserMode PSL API self-call (roundtrip) intra-process\r\n") \
        TEXT("Calls an API function with 6 Input 4096-byte string parameters and returns immediately."),
        TEXT("UserMode PSL API self-call (roundtrip) intra-process\r\n") \
        TEXT("Calls an API function with 6 Input user defined size pointer parameters and returns immediately."),
        TEXT("UserMode PSL API self-call (roundtrip) intra-process\r\n") \
        TEXT("Calls an API function with 6 Output user defined size pointer parameters and returns immediately."),
        TEXT("UserMode PSL API self-call (roundtrip) intra-process\r\n") \
        TEXT("Calls an API function with 6 Input/Output user defined size pointer parameters and returns immediately."),
        TEXT("UserMode PSL API self-call (roundtrip) intra-process\r\n") \
        TEXT("Calls an API function with 6 Input user defined size string parameters and returns immediately."),

    },
    {
        TEXT("Kernel API call (from Usermode) (roundtrip) to NK.EXE\r\n") \
        TEXT("Calls an API function (GetOwnerProcess) in NK.EXE which returns immediately."),
        TEXT("UserMode to UserMode PSL API call (roundtrip) inter-process\r\n") \
        TEXT("Calls an API function with no parameters and returns immediately."),
        TEXT("UserMode to UserMode PSL API call (roundtrip) inter-process\r\n") \
        TEXT("Calls an API function with 6 DWORD parameters and returns immediately."),
        TEXT("UserMode to UserMode PSL API call (roundtrip) inter-process\r\n") \
        TEXT("Calls an API function with 6 Pointer to DWORD parameters and returns immediately."),
        TEXT("UserMode to UserMode PSL API call (roundtrip) inter-process\r\n") \
        TEXT("Calls an API function with 6 Input 128-byte pointer parameters and returns immediately."),
        TEXT("UserMode to UserMode PSL API call (roundtrip) inter-process\r\n") \
        TEXT("Calls an API function with 6 Output 128-byte pointer parameters and returns immediately."),
        TEXT("UserMode to UserMode PSL API call (roundtrip) inter-process\r\n") \
        TEXT("Calls an API function with 6 Input/Output 128-byte pointer parameters and returns immediately."),
        TEXT("UserMode to UserMode PSL API call (roundtrip) inter-process\r\n") \
        TEXT("Calls an API function with 6 Input 128-byte string parameters and returns immediately."),
        TEXT("UserMode to UserMode PSL API call (roundtrip) inter-process\r\n") \
        TEXT("Calls an API function with 6 Input 4096-byte pointer parameters and returns immediately."),
        TEXT("UserMode to UserMode PSL API call (roundtrip) inter-process\r\n") \
        TEXT("Calls an API function with 6 Output 4096-byte pointer parameters and returns immediately."),
        TEXT("UserMode to UserMode PSL API call (roundtrip) inter-process\r\n") \
        TEXT("Calls an API function with 6 Input/Output 4096-byte pointer parameters and returns immediately."),
        TEXT("UserMode to UserMode PSL API call (roundtrip) inter-process\r\n") \
        TEXT("Calls an API function with 6 Input 4096-byte string parameters and returns immediately."),
        TEXT("UserMode to UserMode PSL API call (roundtrip) inter-process\r\n") \
        TEXT("Calls an API function with 6 Input user defined size pointer parameters and returns immediately."),
        TEXT("UserMode to UserMode PSL API call (roundtrip) inter-process\r\n") \
        TEXT("Calls an API function with 6 Output user defined size pointer parameters and returns immediately."),
        TEXT("UserMode to UserMode PSL API call (roundtrip) inter-process\r\n") \
        TEXT("Calls an API function with 6 Input/Output user defined size pointer parameters and returns immediately."),
        TEXT("UserMode to UserMode PSL API call (roundtrip) inter-process\r\n") \
        TEXT("Calls an API function with 6 Input user defined size string parameters and returns immediately."),

    },
    {
        TEXT("Kernel API call (from Usermode) (roundtrip) to NK.EXE\r\n") \
        TEXT("Calls an API function (GetOwnerProcess) in NK.EXE which returns immediately."),
        TEXT("KernelMode to UserMode PSL API call (roundtrip) inter-process\r\n") \
        TEXT("Calls an API function with no parameters and returns immediately."),
        TEXT("KernelMode to UserMode PSL API call (roundtrip) inter-process\r\n") \
        TEXT("Calls an API function with 6 DWORD parameters and returns immediately."),
        TEXT("KernelMode to UserMode PSL API call (roundtrip) inter-process\r\n") \
        TEXT("Calls an API function with 6 Pointer to DWORD parameters and returns immediately."),
        TEXT("KernelMode to UserMode PSL API call (roundtrip) inter-process\r\n") \
        TEXT("Calls an API function with 6 Input 128-byte pointer parameters and returns immediately."),
        TEXT("KernelMode to UserMode PSL API call (roundtrip) inter-process\r\n") \
        TEXT("Calls an API function with 6 Output 128-byte pointer parameters and returns immediately."),
        TEXT("KernelMode to UserMode PSL API call (roundtrip) inter-process\r\n") \
        TEXT("Calls an API function with 6 Input/Output 128-byte pointer parameters and returns immediately."),
        TEXT("KernelMode to UserMode PSL API call (roundtrip) inter-process\r\n") \
        TEXT("Calls an API function with 6 Input 128-byte string parameters and returns immediately."),
        TEXT("KernelMode to UserMode PSL API call (roundtrip) inter-process\r\n") \
        TEXT("Calls an API function with 6 Input 4096-byte pointer parameters and returns immediately."),
        TEXT("KernelMode to UserMode PSL API call (roundtrip) inter-process\r\n") \
        TEXT("Calls an API function with 6 Output 4096-byte pointer parameters and returns immediately."),
        TEXT("KernelMode to UserMode PSL API call (roundtrip) inter-process\r\n") \
        TEXT("Calls an API function with 6 Input/Output 4096-byte pointer parameters and returns immediately."),
        TEXT("KernelMode to UserMode PSL API call (roundtrip) inter-process\r\n") \
        TEXT("Calls an API function with 6 Input 4096-byte string parameters and returns immediately."),
        TEXT("KernelMode to UserMode PSL API call (roundtrip) inter-process\r\n") \
        TEXT("Calls an API function with 6 Input user defined size pointer parameters and returns immediately."),
        TEXT("KernelMode to UserMode PSL API call (roundtrip) inter-process\r\n") \
        TEXT("Calls an API function with 6 Output user defined size pointer parameters and returns immediately."),
        TEXT("KernelMode to UserMode PSL API call (roundtrip) inter-process\r\n") \
        TEXT("Calls an API function with 6 Input/Output user defined size pointer parameters and returns immediately."),
        TEXT("KernelMode to UserMode PSL API call (roundtrip) inter-process\r\n") \
        TEXT("Calls an API function with 6 Input user defined size string parameters and returns immediately."),

    }

    },

    {
    {
        TEXT("Kernel API self-call (from Kernelmode) (roundtrip) to NK.EXE\r\n") \
        TEXT("Calls an API function (GetOwnerProcess) in NK.EXE which returns immediately."),
        TEXT("KernelMode PSL API self-call (roundtrip) intra-process\r\n") \
        TEXT("Calls an API function with no parameters and returns immediately."),
        TEXT("KernelMode PSL API self-call (roundtrip) intra-process\r\n") \
        TEXT("Calls an API function with 6 DWORD parameters and returns immediately."),
        TEXT("KernelMode PSL API self-call (roundtrip) intra-process\r\n") \
        TEXT("Calls an API function with 6 Pointer to DWORD parameters and returns immediately."),
        TEXT("KernelMode PSL API self-call (roundtrip) intra-process\r\n") \
        TEXT("Calls an API function with 6 Input 128-byte pointer parameters and returns immediately."),
        TEXT("KernelMode PSL API self-call (roundtrip) intra-process\r\n") \
        TEXT("Calls an API function with 6 Output 128-byte pointer parameters and returns immediately."),
        TEXT("KernelMode PSL API self-call (roundtrip) intra-process\r\n") \
        TEXT("Calls an API function with 6 Input/Output 128-byte pointer parameters and returns immediately."),
        TEXT("KernelMode PSL API self-call (roundtrip) intra-process\r\n") \
        TEXT("Calls an API function with 6 Input 128-byte string parameters and returns immediately."),
        TEXT("KernelMode PSL API self-call (roundtrip) intra-process\r\n") \
        TEXT("Calls an API function with 6 Input 4096-byte pointer parameters and returns immediately."),
        TEXT("KernelMode PSL API self-call (roundtrip) intra-process\r\n") \
        TEXT("Calls an API function with 6 Output 4096-byte pointer parameters and returns immediately."),
        TEXT("KernelMode PSL API self-call (roundtrip) intra-process\r\n") \
        TEXT("Calls an API function with 6 Input/Output 4096-byte pointer parameters and returns immediately."),
        TEXT("KernelMode PSL API self-call (roundtrip) intra-process\r\n") \
        TEXT("Calls an API function with 6 Input 4096-byte string parameters and returns immediately."),
        TEXT("KernelMode PSL API self-call (roundtrip) intra-process\r\n") \
        TEXT("Calls an API function with 6 Input user defined size pointer parameters and returns immediately."),
        TEXT("KernelMode PSL API self-call (roundtrip) intra-process\r\n") \
        TEXT("Calls an API function with 6 Output user defined size pointer parameters and returns immediately."),
        TEXT("KernelMode PSL API self-call (roundtrip) intra-process\r\n") \
        TEXT("Calls an API function with 6 Input/Output user defined pointer parameters and returns immediately."),
        TEXT("KernelMode PSL API self-call (roundtrip) intra-process\r\n") \
        TEXT("Calls an API function with 6 Input user defined size string parameters and returns immediately."),

    },
    {
        TEXT("Kernel API call (from Usermode) (roundtrip) to NK.EXE\r\n") \
        TEXT("Calls an API function (GetOwnerProcess) in NK.EXE which returns immediately."),
        TEXT("UserMode to KernelMode PSL API call (roundtrip) inter-process\r\n") \
        TEXT("Calls an API function with no parameters and returns immediately."),
        TEXT("UserMode to KernelMode PSL API call (roundtrip) inter-process\r\n") \
        TEXT("Calls an API function with 6 DWORD parameters and returns immediately."),
        TEXT("UserMode to KernelMode PSL API call (roundtrip) inter-process\r\n") \
        TEXT("Calls an API function with 6 Pointer to DWORD parameters and returns immediately."),
        TEXT("UserMode to KernelMode PSL API call (roundtrip) inter-process\r\n") \
        TEXT("Calls an API function with 6 Input 128-byte pointer parameters and returns immediately."),
        TEXT("UserMode to KernelMode PSL API call (roundtrip) inter-process\r\n") \
        TEXT("Calls an API function with 6 Output 128-byte pointer parameters and returns immediately."),
        TEXT("UserMode to KernelMode PSL API call (roundtrip) inter-process\r\n") \
        TEXT("Calls an API function with 6 Input/Output 128-byte pointer parameters and returns immediately."),
        TEXT("UserMode to KernelMode PSL API call (roundtrip) inter-process\r\n") \
        TEXT("Calls an API function with 6 Input 128-byte string parameters and returns immediately."),
        TEXT("UserMode to KernelMode PSL API call (roundtrip) inter-process\r\n") \
        TEXT("Calls an API function with 6 Input 4096-byte pointer parameters and returns immediately."),
        TEXT("UserMode to KernelMode PSL API call (roundtrip) inter-process\r\n") \
        TEXT("Calls an API function with 6 Output 4096-byte pointer parameters and returns immediately."),
        TEXT("UserMode to KernelMode PSL API call (roundtrip) inter-process\r\n") \
        TEXT("Calls an API function with 6 Input/Output 4096-byte pointer parameters and returns immediately."),
        TEXT("UserMode to KernelMode PSL API call (roundtrip) inter-process\r\n") \
        TEXT("Calls an API function with 6 Input 4096-byte string parameters and returns immediately."),
        TEXT("UserMode to KernelMode PSL API call (roundtrip) inter-process\r\n") \
        TEXT("Calls an API function with 6 Input user defined size pointer parameters and returns immediately."),
        TEXT("UserMode to KernelMode PSL API call (roundtrip) inter-process\r\n") \
        TEXT("Calls an API function with 6 Output user defined size pointer parameters and returns immediately."),
        TEXT("UserMode to KernelMode PSL API call (roundtrip) inter-process\r\n") \
        TEXT("Calls an API function with 6 Input/Output user defined size pointer parameters and returns immediately."),
        TEXT("UserMode to KernelMode PSL API call (roundtrip) inter-process\r\n") \
        TEXT("Calls an API function with 6 Input user defined size string parameters and returns immediately."),
    },
    {
        TEXT("Kernel API call (from Kernelmode) (roundtrip) to NK.EXE\r\n") \
        TEXT("Calls an API function (GetOwnerProcess) in NK.EXE which returns immediately."),
        TEXT("KernelMode to KernelMode PSL API call (roundtrip) inter-thread\r\n") \
        TEXT("Calls an API function with no parameters and returns immediately."),
        TEXT("KernelMode to KernelMode PSL API call (roundtrip) inter-thread\r\n") \
        TEXT("Calls an API function with 6 DWORD parameters and returns immediately."),
        TEXT("KernelMode to KernelMode PSL API call (roundtrip) inter-thread\r\n") \
        TEXT("Calls an API function with 6 Pointer to DWORD parameters and returns immediately."),
        TEXT("KernelMode to KernelMode PSL API call (roundtrip) inter-thread\r\n") \
        TEXT("Calls an API function with 6 Input 128-byte pointer parameters and returns immediately."),
        TEXT("KernelMode to KernelMode PSL API call (roundtrip) inter-thread\r\n") \
        TEXT("Calls an API function with 6 Output 128-byte pointer parameters and returns immediately."),
        TEXT("KernelMode to KernelMode PSL API call (roundtrip) inter-thread\r\n") \
        TEXT("Calls an API function with 6 Input/Output 128-byte pointer parameters and returns immediately."),
        TEXT("KernelMode to KernelMode PSL API call (roundtrip) inter-thread\r\n") \
        TEXT("Calls an API function with 6 Input 128-byte string parameters and returns immediately."),
        TEXT("KernelMode to KernelMode PSL API call (roundtrip) inter-thread\r\n") \
        TEXT("Calls an API function with 6 Input 4096-byte pointer parameters and returns immediately."),
        TEXT("KernelMode to KernelMode PSL API call (roundtrip) inter-thread\r\n") \
        TEXT("Calls an API function with 6 Output 4096-byte pointer parameters and returns immediately."),
        TEXT("KernelMode to KernelMode PSL API call (roundtrip) inter-thread\r\n") \
        TEXT("Calls an API function with 6 Input/Output 4096-byte pointer parameters and returns immediately."),
        TEXT("KernelMode to KernelMode PSL API call (roundtrip) inter-thread\r\n") \
        TEXT("Calls an API function with 6 Input 4096-byte string parameters and returns immediately."),
        TEXT("KernelMode to KernelMode PSL API call (roundtrip) inter-thread\r\n") \
        TEXT("Calls an API function with 6 Input user defined size pointer parameters and returns immediately."),
        TEXT("KernelMode to KernelMode PSL API call (roundtrip) inter-thread\r\n") \
        TEXT("Calls an API function with 6 Output user defined size pointer parameters and returns immediately."),
        TEXT("KernelMode to KernelMode PSL API call (roundtrip) inter-thread\r\n") \
        TEXT("Calls an API function with 6 Input/Output user defined size pointer parameters and returns immediately."),
        TEXT("KernelMode to KernelMode PSL API call (roundtrip) inter-thread\r\n") \
        TEXT("Calls an API function with 6 Input user defined size string parameters and returns immediately."),
    }

    }
};

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
OsBenchNotifyCallback(
    DWORD cause,
    HPROCESS hprc,
    HTHREAD hthd
    )
{
    if (cause == DLL_PROCESS_DETACH) {
        RETAILMSG(1, (TEXT("NotifyCallback\r\n")));
    }
}



//------------------------------------------------------------------------------
//
// OsBenchAPICall1:
//  This function tests the minimal API call (no parameters)
//
//------------------------------------------------------------------------------
DWORD
OsBenchAPICall1(void)
{
    return 0;
}



//------------------------------------------------------------------------------
//
// OsBenchAPICall2:
//  This function tests many parameters but none require mapping (no pointers).
//  Comparing with OsBenchAPICall3 shows the cost of pointer mapping.
//
//------------------------------------------------------------------------------
DWORD
OsBenchAPICall2(
    DWORD dwParam1,
    DWORD dwParam2,
    DWORD dwParam3,
    DWORD dwParam4,
    DWORD dwParam5,
    DWORD dwParam6
    )
{
    return 0;
}



//------------------------------------------------------------------------------
//
// OsBenchAPICall3: 6 Input/Output Pointers to DWORDs
//
//------------------------------------------------------------------------------
DWORD
OsBenchAPICall3(
    PDWORD pdwParam1,
    PDWORD pdwParam2,
    PDWORD pdwParam3,
    PDWORD pdwParam4,
    PDWORD pdwParam5,
    PDWORD pdwParam6
    )
{
    if (g_fMapPTRs) {
        pdwParam1 = (PDWORD)MapCallerPtr( pdwParam1, sizeof(DWORD) );
        pdwParam2 = (PDWORD)MapCallerPtr( pdwParam2, sizeof(DWORD) );
        pdwParam3 = (PDWORD)MapCallerPtr( pdwParam3, sizeof(DWORD) );
        pdwParam4 = (PDWORD)MapCallerPtr( pdwParam4, sizeof(DWORD) );
        pdwParam5 = (PDWORD)MapCallerPtr( pdwParam5, sizeof(DWORD) );
        pdwParam6 = (PDWORD)MapCallerPtr( pdwParam6, sizeof(DWORD) );
    }

    return 0;
}

//------------------------------------------------------------------------------
//
// OsBenchAPI6PTR
//
//------------------------------------------------------------------------------
DWORD
OsBenchAPI6PTR(
    PVOID pvParam1, DWORD dwParam1Size,
    PVOID pvParam2, DWORD dwParam2Size,
    PVOID pvParam3, DWORD dwParam3Size,
    PVOID pvParam4, DWORD dwParam4Size,
    PVOID pvParam5, DWORD dwParam5Size,
    PVOID pvParam6, DWORD dwParam6Size
    )
{
    if (g_fMapPTRs) {
        pvParam1 = (PDWORD)MapCallerPtr( pvParam1, dwParam1Size );
        pvParam2 = (PDWORD)MapCallerPtr( pvParam2, dwParam2Size );
        pvParam3 = (PDWORD)MapCallerPtr( pvParam3, dwParam3Size );
        pvParam4 = (PDWORD)MapCallerPtr( pvParam4, dwParam4Size );
        pvParam5 = (PDWORD)MapCallerPtr( pvParam5, dwParam5Size );
        pvParam6 = (PDWORD)MapCallerPtr( pvParam6, dwParam6Size );
    }

    return 0;
}

DWORD
OsBenchSetBufSize( DWORD bufSize ) {
    return ( g_dwBufSize = bufSize );
}

DWORD
OsBenchAPIWSTR(
    LPWSTR lpwStr1,
    LPWSTR lpwStr2,
    LPWSTR lpwStr3,
    LPWSTR lpwStr4,
    LPWSTR lpwStr5,
    LPWSTR lpwStr6
    )
{

    if (g_fMapPTRs && g_dwBufSize) {
        lpwStr1 = (LPWSTR)MapCallerPtr( lpwStr1, g_dwBufSize );
        lpwStr2 = (LPWSTR)MapCallerPtr( lpwStr2, g_dwBufSize );
        lpwStr3 = (LPWSTR)MapCallerPtr( lpwStr3, g_dwBufSize );
        lpwStr4 = (LPWSTR)MapCallerPtr( lpwStr4, g_dwBufSize );
        lpwStr5 = (LPWSTR)MapCallerPtr( lpwStr5, g_dwBufSize );
        lpwStr6 = (LPWSTR)MapCallerPtr( lpwStr6, g_dwBufSize );
    }

    return 0;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL
InitializeFunctionTable()
{
    if(g_pfnmApi) {
        //NKDbgPrintfW(TEXT("g_pfnmApi has already been initialized to 0x%x\r\n"), g_pfnmApi );
        return TRUE;
    }

    g_pfnmApi = (PAPIFN*)HeapAlloc(     GetProcessHeap(),
                                        HEAP_ZERO_MEMORY,
                                        NUM_SUBIDS*sizeof(PAPIFN)
                                  );
    if(!g_pfnmApi) {
        RETAILMSG(1,(TEXT("!!!!! ERROR - Heap Allocation failed on function table !!!!!\r\n")));
        return FALSE;
    }

    //
    if(g_nApiSetId > 0 && g_nApiSetId < NUM_API_SETS ){
        if(g_fPreYamazaki) {
            // Bowmore: different PSL trap area, signatures
            g_pfnmApi[SUBID_NO_PARAMS] = (PAPIFN)bmApi1;
            g_pfnmApi[SUBID_DWORD_PARAMS] = (PAPIFN)bmApi2;
            g_pfnmApi[SUBID_PDW_PARAMS] = (PAPIFN)bmApi3;
            g_pfnmApi[SUBID_NK] = (PAPIFN)GetOwnerProcess;
            g_pfnmApi[SUBID_IPTR_SMALL_PARAMS] = (PAPIFN)bmApi4;
            g_pfnmApi[SUBID_OPTR_SMALL_PARAMS] = (PAPIFN)bmApi5;
            g_pfnmApi[SUBID_IOPTR_SMALL_PARAMS] = (PAPIFN)bmApi6;
            g_pfnmApi[SUBID_IWSTR_SMALL_PARAMS] = (PAPIFN)bmApi7;
            g_pfnmApi[SUBID_IPTR_LARGE_PARAMS] = (PAPIFN)bmApi8;
            g_pfnmApi[SUBID_OPTR_LARGE_PARAMS] = (PAPIFN)bmApi9;
            g_pfnmApi[SUBID_IOPTR_LARGE_PARAMS] = (PAPIFN)bmApi10;
            g_pfnmApi[SUBID_IWSTR_LARGE_PARAMS] = (PAPIFN)bmApi11;
            g_pfnmApi[SUBID_IPTR_USERDEF_PARAMS] = (PAPIFN)bmApi12;
            g_pfnmApi[SUBID_OPTR_USERDEF_PARAMS] = (PAPIFN)bmApi13;
            g_pfnmApi[SUBID_IOPTR_USERDEF_PARAMS] = (PAPIFN)bmApi14;
            g_pfnmApi[SUBID_IWSTR_USERDEF_PARAMS] = (PAPIFN)bmApi15;
        }
        else {
            g_pfnmApi[SUBID_NO_PARAMS] = (PAPIFN)mApi1;
            g_pfnmApi[SUBID_DWORD_PARAMS] = (PAPIFN)mApi2;
            g_pfnmApi[SUBID_PDW_PARAMS] = (PAPIFN)mApi3;
            g_pfnmApi[SUBID_NK] = (PAPIFN)GetOwnerProcess;
            g_pfnmApi[SUBID_IPTR_SMALL_PARAMS] = (PAPIFN)mApi4;
            g_pfnmApi[SUBID_OPTR_SMALL_PARAMS] = (PAPIFN)mApi5;
            g_pfnmApi[SUBID_IOPTR_SMALL_PARAMS] = (PAPIFN)mApi6;
            g_pfnmApi[SUBID_IWSTR_SMALL_PARAMS] = (PAPIFN)mApi7;
            g_pfnmApi[SUBID_IPTR_LARGE_PARAMS] = (PAPIFN)mApi8;
            g_pfnmApi[SUBID_OPTR_LARGE_PARAMS] = (PAPIFN)mApi9;
            g_pfnmApi[SUBID_IOPTR_LARGE_PARAMS] = (PAPIFN)mApi10;
            g_pfnmApi[SUBID_IWSTR_LARGE_PARAMS] = (PAPIFN)mApi11;
            g_pfnmApi[SUBID_IPTR_USERDEF_PARAMS] = (PAPIFN)mApi12;
            g_pfnmApi[SUBID_OPTR_USERDEF_PARAMS] = (PAPIFN)mApi13;
            g_pfnmApi[SUBID_IOPTR_USERDEF_PARAMS] = (PAPIFN)mApi14;
            g_pfnmApi[SUBID_IWSTR_USERDEF_PARAMS] = (PAPIFN)mApi15;
        }
        return TRUE;
    }
    else {
        RETAILMSG(1,(TEXT("!!!!! ERROR - Can't initialize Function table with g_nApiSetId <= 0\r\n !!!!!")));
        return FALSE;
    }

}


//------------------------------------------------------------------------------
//
// This creates and registers the PSL we will test. Note that it uses a
// different signature table for Yamazaki and pre-Yamazaki OSs
//
//------------------------------------------------------------------------------
BOOL
CreateOsBenchAPISet()
{
    BOOL bResult;
    char *apiName = g_fInKMode? "OSBK":"OSBU";
    HMODULE hCoreDLL = GetModuleHandle( TEXT("coredll.dll") );

    DEBUGMSG(ZONE_FUNCTION, (TEXT("+CreateOsBenchAPISet\r\n")));

    // CreateAPISet - Ordinal 559 in Bowmore and 2539 in Yamazaki+
    PFN_CREATEAPISET pfnCreateAPISet = (PFN_CREATEAPISET)GetProcAddress( hCoreDLL, TEXT("CreateAPISet") );
    // RegisterDirectMethods == coredll export 2555 only in Yamazaki+
    PFN_REGISTER_DIRECT pfnRegDir = (PFN_REGISTER_DIRECT)GetProcAddress( hCoreDLL, TEXT("RegisterDirectMethods") );

    if(!pfnCreateAPISet || (!g_fPreYamazaki && !pfnRegDir) ) {
        RETAILMSG(1, (TEXT("Can't find expected coredll function: pfnCreateAPISet = 0x%08x, pfnRegDir = 0x%08x\r\n"), pfnCreateAPISet, pfnRegDir) );
    }

    if(g_fPreYamazaki) {
        g_hApi = pfnCreateAPISet( apiName, NUM_APIS, PfnTable, (const ULONGLONG*)BMSigTable );
    }
    else {
        g_hApi = pfnCreateAPISet( apiName, NUM_APIS, PfnTable, SigTable);
    }
    if (g_hApi == NULL) {
        RETAILMSG(1, (TEXT("CreateAPISet failed\r\n")));
        return FALSE;
    }
    bResult = RegisterAPISet(g_hApi, 0);
    if (bResult == FALSE) {
        RETAILMSG(1, (TEXT("RegisterAPISet failed\r\n")));
        return FALSE;
    }

    if( g_fInKMode ) {
        if( (pfnRegDir && !pfnRegDir( g_hApi, PfnTable ))
            ||
            (!g_fPreYamazaki && !pfnRegDir) ) {
            RETAILMSG(1, (TEXT("RegisterDirectMethods failed\r\n")));
            return FALSE;
        }
    }

    DEBUGMSG(ZONE_FUNCTION, (TEXT("-CreateOsBenchAPISet (0x%08X)\r\n"), g_hApi));
    return TRUE;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

DWORD
OB_PSLThread(
    LPVOID pParam
    )
{
    DWORD i, j;
    LARGE_INTEGER liRef1, liRef2;
    DWORD dwParam = (DWORD) pParam;

    DEBUGMSG(ZONE_THREAD, (TEXT("+OB_PSLThread\r\n")));

    //----------------------------------------------------------
    // Reference
    //
    QueryPerformanceCounter(&liRef1);
    for (j = 0; j < PSL_ITERATIONS; j++) {

        //
        // Just measuring the loop overhead and the overhead of
        // calling the QueryPerformanceCounter() function.
        //
        SUBMARKER1();
        SUBMARKER2();
    }
    QueryPerformanceCounter(&liRef2);
    g_dwBase = (DWORD) (liRef2.QuadPart - liRef1.QuadPart);


    switch (dwParam) {

        case SUBID_NO_PARAMS : {
            for (i=0; i< g_nIterations; i++) {
                QPC1(i);
                for (j = 0; j < PSL_ITERATIONS; j++) {
                    //
                    // Round-trip fast API call.
                    //
                    SUBMARKER1();
                    g_pfnmApi[dwParam]();
                    SUBMARKER2();
                }
                QPC2(i);
                POST1(i);
            }
            break;
        }

        case SUBID_DWORD_PARAMS : {
            for (i=0; i< g_nIterations; i++) {
                QPC1(i);
                for (j = 0; j < PSL_ITERATIONS; j++) {
                    //
                    // Round-trip DWORD param API call.
                    //
                    SUBMARKER1();
                    g_pfnmApi[dwParam](0, 0, 0, 0, 0, 0);
                    SUBMARKER2();
                }
                QPC2(i);
                POST1(i);
            }
            break;
        }

        case SUBID_PDW_PARAMS : {

            for (i=0; i< g_nIterations; i++) {
                QPC1(i);
                for (j = 0; j < PSL_ITERATIONS; j++) {
                    //
                    // Round-trip PVOID param API call.
                    //
                    SUBMARKER1();
                    g_pfnmApi[dwParam](&i, &j, &i, &j, &i, &j);
                    SUBMARKER2();
                }
                QPC2(i);
                POST1(i);
            }
            break;

        }

        case SUBID_IPTR_SMALL_PARAMS:
        case SUBID_OPTR_SMALL_PARAMS:
        case SUBID_IOPTR_SMALL_PARAMS:
        case SUBID_IPTR_LARGE_PARAMS:
        case SUBID_OPTR_LARGE_PARAMS:
        case SUBID_IOPTR_LARGE_PARAMS:
        case SUBID_IPTR_USERDEF_PARAMS:
        case SUBID_OPTR_USERDEF_PARAMS:
        case SUBID_IOPTR_USERDEF_PARAMS:
        {
            DWORD dwSize = 0;

            if( isSmallCall(dwParam) ) {
                dwSize = SMALL_BUF_SIZE;
            }
            else if (isLargeCall(dwParam) ) {
                dwSize = LARGE_BUF_SIZE;
            }
            else if (isUserDefCall(dwParam) ) {
                if(!g_dwUserDefBufSize) {
                    DEBUGMSG(1, (TEXT("Notice: user defined buffer size is 0\r\n")));
                }
                dwSize = g_dwUserDefBufSize;
            }
            else {
                ASSERT(FALSE);
            }

            PBYTE p1 = (PBYTE)HeapAlloc(    GetProcessHeap(),
                                            0,
                                            dwSize);
            PBYTE p2 = (PBYTE)HeapAlloc(    GetProcessHeap(),
                                            0,
                                            dwSize);
            PBYTE p3 = (PBYTE)HeapAlloc(    GetProcessHeap(),
                                            0,
                                            dwSize);
            PBYTE p4 = (PBYTE)HeapAlloc(    GetProcessHeap(),
                                            0,
                                            dwSize);
            PBYTE p5 = (PBYTE)HeapAlloc(    GetProcessHeap(),
                                            0,
                                            dwSize);
            PBYTE p6 = (PBYTE)HeapAlloc(    GetProcessHeap(),
                                            0,
                                            dwSize);

            if(!p1 || !p2 || !p3 || !p4 || !p5 || !p6) {
                RETAILMSG(1, (TEXT("!!!!! ERROR - HeapAlloc failed in PSL server!!!!!\r\n")));
                // in case some of them worked, cleanup
                HeapFree( GetProcessHeap(), 0, p1);
                HeapFree( GetProcessHeap(), 0, p2);
                HeapFree( GetProcessHeap(), 0, p3);
                HeapFree( GetProcessHeap(), 0, p4);
                HeapFree( GetProcessHeap(), 0, p5);
                HeapFree( GetProcessHeap(), 0, p6);
                return (DWORD)-1;
            }


            memset( p1, 1, dwSize);
            memset( p2, 2, dwSize);
            memset( p3, 3, dwSize);
            memset( p4, 4, dwSize);
            memset( p5, 5, dwSize);
            memset( p6, 6, dwSize);

            for (i=0; i< g_nIterations; i++) {
                QPC1(i);
                for (j = 0; j < PSL_ITERATIONS; j++) {
                    //
                    // Round-trip PTR param API call.
                    //
                    SUBMARKER1();
                    g_pfnmApi[dwParam](     p1, dwSize,
                                            p2, dwSize,
                                            p3, dwSize,
                                            p4, dwSize,
                                            p5, dwSize,
                                            p6, dwSize
                                      );
                    SUBMARKER2();
                }
                QPC2(i);
                POST1(i);
            }

            // cleanup
            HeapFree( GetProcessHeap(), 0, p1);
            HeapFree( GetProcessHeap(), 0, p2);
            HeapFree( GetProcessHeap(), 0, p3);
            HeapFree( GetProcessHeap(), 0, p4);
            HeapFree( GetProcessHeap(), 0, p5);
            HeapFree( GetProcessHeap(), 0, p6);

            break;
        }

        case SUBID_IWSTR_SMALL_PARAMS:
        case SUBID_IWSTR_LARGE_PARAMS:
        case SUBID_IWSTR_USERDEF_PARAMS:
        {
            DWORD dwSize = 0;

            if (isSmallCall(dwParam)) {
                dwSize = SMALL_BUF_SIZE;
            }
            else if(isLargeCall(dwParam)) {
                dwSize = LARGE_BUF_SIZE;
            }
            else if(isUserDefCall(dwParam)) {

                if(!g_dwUserDefBufSize) {
                    DEBUGMSG(1, (TEXT("Notice: user defined buffer size is 0\r\n")));
                }

                // This ASSERT shouldn't go off; we wchar align during parsing
                ASSERT(!(g_dwUserDefBufSize%sizeof(WCHAR)));

                dwSize = g_dwUserDefBufSize?
                            g_dwUserDefBufSize
                            :
                            sizeof(WCHAR); // need at least space to null-term
            }
            else {
                // shouldn't get here
                ASSERT(FALSE);
            }

            // Register the size with the psl, so it knows what to expect
            if(g_fPreYamazaki) {
                bmApiSetBufSize( dwSize );
            }
            else {
                mApiSetBufSize( dwSize );
            }

            LPWSTR p1 = (LPWSTR)HeapAlloc(  GetProcessHeap(),
                                            0,
                                            dwSize);

            LPWSTR p2 = (LPWSTR)HeapAlloc(  GetProcessHeap(),
                                            0,
                                            dwSize);

            LPWSTR p3 = (LPWSTR)HeapAlloc(  GetProcessHeap(),
                                            0,
                                            dwSize);

            LPWSTR p4 = (LPWSTR)HeapAlloc(  GetProcessHeap(),
                                            0,
                                            dwSize);

            LPWSTR p5 = (LPWSTR)HeapAlloc(  GetProcessHeap(),
                                            0,
                                            dwSize);

            LPWSTR p6 = (LPWSTR)HeapAlloc(  GetProcessHeap(),
                                            0,
                                            dwSize);

            if(!p1 || !p2 || !p3 || !p4 || !p5 || !p6) {
                RETAILMSG(1, (TEXT("!!!!! ERROR - HeapAlloc failed in PSL server!!!!!\r\n")));
                // in case some of them worked, cleanup
                HeapFree( GetProcessHeap(), 0, p1);
                HeapFree( GetProcessHeap(), 0, p2);
                HeapFree( GetProcessHeap(), 0, p3);
                HeapFree( GetProcessHeap(), 0, p4);
                HeapFree( GetProcessHeap(), 0, p5);
                HeapFree( GetProcessHeap(), 0, p6);
                return (DWORD)-1;
            }

            memset( p1, '1', dwSize );
            memset( p2, '2', dwSize );
            memset( p3, '3', dwSize );
            memset( p4, '4', dwSize );
            memset( p5, '5', dwSize );
            memset( p6, '6', dwSize );

            // null terminate
            p1[ dwSize/sizeof(WCHAR) - 1 ] = 0;
            p2[ dwSize/sizeof(WCHAR) - 1 ] = 0;
            p3[ dwSize/sizeof(WCHAR) - 1 ] = 0;
            p4[ dwSize/sizeof(WCHAR) - 1 ] = 0;
            p5[ dwSize/sizeof(WCHAR) - 1 ] = 0;
            p6[ dwSize/sizeof(WCHAR) - 1 ] = 0;

            for (i=0; i< g_nIterations; i++) {
                QPC1(i);
                for (j = 0; j < PSL_ITERATIONS; j++) {
                    //
                    // Round-trip PTR WSTR param API call.
                    //
                    SUBMARKER1();
                    g_pfnmApi[dwParam](p1, p2, p3, p4, p5, p6);
                    SUBMARKER2();
                }
                QPC2(i);
                POST1(i);
            }


            HeapFree( GetProcessHeap(), 0, p1);
            HeapFree( GetProcessHeap(), 0, p2);
            HeapFree( GetProcessHeap(), 0, p3);
            HeapFree( GetProcessHeap(), 0, p4);
            HeapFree( GetProcessHeap(), 0, p5);
            HeapFree( GetProcessHeap(), 0, p6);

            break;
        }

        /* Kernel Call GetOwnerProcess() */
        case SUBID_NK : {
            for (i=0; i< g_nIterations; i++) {
                QPC1(i);
                for (j = 0; j < PSL_ITERATIONS; j++) {
                    //
                    // Round-trip NK API call.
                    //
                    SUBMARKER1();
                    g_pfnmApi[dwParam]();
                    SUBMARKER2();
                }
                QPC2(i);
                POST1(i);
            }
            break;
        }

    }

    DEBUGMSG(ZONE_THREAD, (TEXT("-OB_PSLThread\r\n")));
    return 0;
}




//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void
TestPSL(
    DWORD dwTestId,
    DWORD dwType,
    DWORD dwParam
    )
{
    int subid;
    HANDLE hEvent;
    HANDLE hPerfSession = INVALID_HANDLE_VALUE;
    HRESULT hr = E_FAIL;
    const DWORD cTests = 16;

    DEBUGMSG(ZONE_FUNCTION, (TEXT("TestPSL (%d, %d)\r\n"), dwType, dwParam));

    hEvent = CreateEvent(NULL, FALSE, FALSE, TEXT("OSBENCH_TEST_PSL"));

    switch (dwType) {

        case TYPE_PRIMARY : {

            if (fCacheSync) {
                //
                // All the PSL call tests must run without the CacheSync option
                // because they are looped.
                //
                break;
            }

            if(!CreateOsBenchAPISet()) {
                RETAILMSG(1, (TEXT("ERROR: Creating APISet. Quiting.\r\n")));
                break;
            }
            
            g_nApiSetId = QueryAPISetID ( (g_fInKMode? "OSBK":"OSBU") );
            if (g_nApiSetId == -1) {
                RETAILMSG(1, (TEXT("ERROR : API Set not found. Quiting.\r\n")));
                break;
            }

            if (!InitializeFunctionTable()) {
                RETAILMSG(1,(TEXT("ERROR: initializing function table. Quiting\r\n")));

                if(g_pfnmApi) {
                    VERIFY( HeapFree( GetProcessHeap(), 0, g_pfnmApi ));
                    g_pfnmApi=0;
                }

                break;
            }

// bts
#if !defined(MINRT)
            if ( g_logBTS == TRUE )
            {
                hr = CePerfOpenStandardSession(&hPerfSession, L"PSL");
                ASSERT(SUCCEEDED(hr));
                if (FAILED(hr)) return;

                for (DWORD i = 0; i < g_PSLCount; i++)
                {
                    g_PSL_PerfItems[i].hTrackedItem = INVALID_HANDLE_VALUE;
                    g_PSL_PerfItems[i].type = CEPERF_TYPE_STATISTIC;
                    g_PSL_PerfItems[i].dwRecordingFlags = CEPERF_STATISTIC_RECORD_SHORT;
                    g_PSL_PerfItems[i].lpszItemName = g_fRunKMode ? const_cast<LPWSTR>(g_kernel_PSL[i]) : const_cast<LPWSTR>(g_user_PSL[i]);
                }

                hr = CePerfRegisterBulk(hPerfSession, g_PSL_PerfItems, g_PSLCount, 0);
                ASSERT(SUCCEEDED(hr));
                if (FAILED(hr)) return;
#if defined(_DUMP_)
                DumpHandles (g_PSL_PerfItems, g_PSLCount);
#endif
            }
#endif // MINRT
// bts


            //
            // First, within this process
            //
            for (subid = 0; subid < NUM_SUBIDS; subid++) {

                if ( (  g_dwUserDefBufSize &&  isUserDefCall(subid) ) ||
                     ( !g_dwUserDefBufSize && !isUserDefCall(subid) ) ) {

                    if(!OB_SpinThreadAndBlock(
                                (PVOID) OB_PSLThread,
                                subid,
                                RT_PRIO_0)
                        ) {
                        // test failed: most likely memory alloc failed
                        RETAILMSG(1, (TEXT("Error running PSL subtest #%d, not printing results\r\n"), subid));
                    }
                    else {
                        OB_PrintResults(
                                dwTestId,
                                subid,
                                OB_INTRAPROCESS,
                                PSL_ITERATIONS,
                                g_dwBase,
                                cTests,
                                szDescription[g_fInKMode][OB_INTRAPROCESS][subid]
                                );
                    }
                }
            }

            //
            // Now, interprocess (usermode caller)
            //


            // subid = 1; skip call to kernel:GetOwnerProcess here.
            for (subid = 1; subid < NUM_SUBIDS; subid++) {

                if ( (  g_dwUserDefBufSize &&  isUserDefCall(subid) ) ||
                     ( !g_dwUserDefBufSize && !isUserDefCall(subid) ) ) {

                    OB_SendCommand (dwTestId, TYPE_SECONDARY, subid);
                    WaitForSingleObject(hEvent, INFINITE);

                    if(! GetEventData( hEvent ) ) {
                        // Test failed: most likely memory alloc
                        RETAILMSG(1, (TEXT("Error running PSL subtest #%d, not printing results\r\n"), subid));
                    }
                    else {
                        OB_PrintResults(
                                dwTestId,
                                subid,
                                OB_UINTERPROCESS,
                                PSL_ITERATIONS,
                                g_dwBase,
                                cTests,
                                szDescription[g_fInKMode][OB_UINTERPROCESS][subid]
                                );
                    }
                }
            }

            //
            // Now, with kernelmode caller (same process space). Yamazaki only.
            //
            if(!g_fPreYamazaki) {

            // subid = 1; skip call to kernel:GetOwnerProcess here.
            for (subid = 1; subid < NUM_SUBIDS; subid++) {

                if ( (  g_dwUserDefBufSize &&  isUserDefCall(subid) ) ||
                     ( !g_dwUserDefBufSize && !isUserDefCall(subid) ) ) {

                    OB_SendCommand (dwTestId, TYPE_TERTIARY, subid);
                    WaitForSingleObject(hEvent, INFINITE);

                    if(! GetEventData( hEvent ) ) {
                        // Test failed: most likely memory alloc
                        RETAILMSG(1, (TEXT("Error running PSL subtest #%d, not printing results\r\n"), subid));
                    }
                    else {
                        OB_PrintResults(
                                dwTestId,
                                subid,
                                OB_KINTERPROCESS,
                                PSL_ITERATIONS,
                                g_dwBase,
                                cTests,
                                szDescription[g_fInKMode][OB_KINTERPROCESS][subid]
                                );
                    }
                }
            }

            }

            //
            // Clean-up
            //

// bts
#if !defined(MINRT)
            if ( g_logBTS == TRUE )
            {
                CePerfDeregisterBulk(g_PSL_PerfItems, g_PSLCount);
                CePerfCloseSession(hPerfSession);
            }
#endif // MINRT
// bts

            CloseHandle(g_hApi);
            if(g_pfnmApi) {
                VERIFY( HeapFree( GetProcessHeap(), 0, g_pfnmApi ));
                g_pfnmApi=0;
            }

            break;
        }

        case TYPE_SECONDARY:
        case TYPE_TERTIARY : {
            //
            // This is the second process
            //

            DWORD dwRet = 0;
            if (fCacheSync) {
                //
                // All the PSL call tests must run without the CacheSync option because
                // they are looped.
                //
                break;
            }
            
            // Since the kernel mode server may get left around, always try
            // the usermode server 1st (it should be cleaned up)
            if  ( ( (g_nApiSetId = QueryAPISetID ( "OSBU" ))==-1) &&
                  ( (g_nApiSetId = QueryAPISetID ( "OSBK" ))==-1) ){
                RETAILMSG(1, (TEXT("ERROR : API Set not found\r\n")));
                break;
            }

            if (!InitializeFunctionTable()) {
                RETAILMSG(1,(TEXT("Osbench(2): Error initializing function table. Bailing\r\n")));

                if(g_pfnmApi) {
                    VERIFY( HeapFree( GetProcessHeap(), 0, g_pfnmApi ));
                    g_pfnmApi=0;
                }

                break;
            }

            dwRet = OB_SpinThreadAndBlock((PVOID) OB_PSLThread, dwParam, RT_PRIO_0);
            SetEventData(hEvent, dwRet);
            SetEvent(hEvent);
            VERIFY( HeapFree( GetProcessHeap(), 0, g_pfnmApi ));
            g_pfnmApi=0;
            break;
        }

        case TYPE_PRINT_HELP : {
           RETAILMSG(1, (TEXT("TestId %2d : PSL API call overhead\r\n"), dwTestId));
           break;
        }

        default : {
           RETAILMSG(1, (TEXT("TestId %2d : Unknown command type %d\r\n"), dwTestId, dwType));
           break;
        }
    }

    // cleanup
    CloseHandle(hEvent);
}

#ifdef _PREFAST_
#pragma warning(pop)
#endif


