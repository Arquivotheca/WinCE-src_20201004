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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
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
//
// FT.h
//
//
#if (!defined(__FT_H__) || defined(__GLOBALS_CPP__))
#ifndef __FT_H__
#define __FT_H__
#endif

////////////////////////////////////////////////////////////////////////////////
// Local macros

#ifdef __DEFINE_FTE__
#undef BEGIN_FTE
#undef FTE
#undef FTH
#undef END_FTE
#define BEGIN_FTE FUNCTION_TABLE_ENTRY g_lpFTE[] = {
#define FTH(a, b) { TEXT(b), a, 0, 0, NULL },
#define FTE(a, b, c, d, e) { TEXT(b), a, d, c, e },
#define END_FTE { NULL, 0, 0, 0, NULL } };
#else // __DEFINE_FTE__
#ifdef __GLOBALS_CPP__
#define BEGIN_FTE
#else // __GLOBALS_CPP__
#define BEGIN_FTE extern FUNCTION_TABLE_ENTRY g_lpFTE[];
#endif // __GLOBALS_CPP__
#define FTH(a, b)
#define FTE(a, b, c, d, e) TESTPROCAPI e(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
#define END_FTE
#endif // __DEFINE_FTE__

#define DUMMY_INDEX             0
#define BUS_REQ_INDEX           1000
#define INFO_QUERY_INDEX        2000
#define CARD_FEATURE_INDEX      3000
#define MISC_FUNC_INDEX         4000
#define MEM_LIST_INDEX          5000
#define FEATURE_FULLNESS_INDEX  6000
#define INVALID_MODIFIER        500
#define COMPLEX_MODIFIER        200

////////////////////////////////////////////////////////////////////////////////
// TUX Function table
//  To create the function table and function prototypes, two macros are
//  available:
//
//   FTH(level, description)
//       (Function Table Header) Used for entries that don't have functions,
//       entered only as headers (or comments) into the function table.
//
//   FTE(level, description, code, param, function)
//       (Function Table Entry) Used for all functions. DON'T use this macro
//       if the "function" field is NULL. In that case, use the FTH macro.
//
//  You must not use the TEXT or _T macros here. This is done by the FTH and FTE
//  macros.
//
//  In addition, the table must be enclosed by the BEGIN_FTE and END_FTE macros.

BEGIN_FTE

// dwUserData indicated the expect return value
/*     lpDescription;   uDepth;  dwUserData;  dwUniqueID; lpTestProc; */

    FTH(0, "Simple Bus Request Tests")
    FTE(1,   "Simple Synchronous Test of CMD7",               BUS_REQ_INDEX + 9,  0, TestCMD7)
    FTE(1,   "Simple Asynchronous Test of CMD7",              BUS_REQ_INDEX + 10, 1, TestCMD7)
    FTE(1,   "Simple Synchronous Test of CMD9",               BUS_REQ_INDEX + 11, 0, TestCMD9)
    FTE(1,   "Simple Asynchronous Test of CMD9",              BUS_REQ_INDEX + 12, 1, TestCMD9)
    FTE(1,   "Simple Synchronous Test of CMD10",              BUS_REQ_INDEX + 13, 0, TestCMD10)
    FTE(1,   "Simple Asynchronous Test of CMD10",             BUS_REQ_INDEX + 14, 1, TestCMD10)
    FTE(1,   "Simple Synchronous Test of CMD12 after write",  BUS_REQ_INDEX + 15, 0, TestCMD12)
    FTE(1,   "Simple Asynchronous Test of CMD12 after write", BUS_REQ_INDEX + 16, 1, TestCMD12)
    FTE(1,   "Simple Synchronous Test of CMD12 after read",   BUS_REQ_INDEX + 17, 0, TestCMD12_2)
    FTE(1,   "Simple Asynchronous Test of CMD12 after read",  BUS_REQ_INDEX + 18, 1, TestCMD12_2)
    FTE(1,   "Simple Synchronous Test of CMD13",              BUS_REQ_INDEX + 19, 0, TestCMD13)
    FTE(1,   "Simple Asynchronous Test of CMD13",             BUS_REQ_INDEX + 20, 1, TestCMD13)
    FTE(1,   "Simple Synchronous Test of CMD16",              BUS_REQ_INDEX + 23, 0, TestCMD16)
    FTE(1,   "Simple Asynchronous Test of CMD16",             BUS_REQ_INDEX + 24, 1, TestCMD16)
    FTE(1,   "Simple Synchronous Test of CMD17",              BUS_REQ_INDEX + 25, 0, TestCMD17)
    FTE(1,   "Simple Asynchronous Test of CMD17",             BUS_REQ_INDEX + 26, 1, TestCMD17)
    FTE(1,   "Simple Synchronous Test of CMD18",              BUS_REQ_INDEX + 27, 0, TestCMD18)
    FTE(1,   "Simple Asynchronous Test of CMD18",             BUS_REQ_INDEX + 28, 1, TestCMD18)
    FTE(1,   "Simple Synchronous Test of CMD24",              BUS_REQ_INDEX + 29, 0, TestCMD24)
    FTE(1,   "Simple Asynchronous Test of CMD24",             BUS_REQ_INDEX + 30, 1, TestCMD24)
    FTE(1,   "Simple Synchronous Test of CMD25",              BUS_REQ_INDEX + 31, 0, TestCMD25)
    FTE(1,   "Simple Asynchronous Test of CMD25",             BUS_REQ_INDEX + 32, 1, TestCMD25)
    FTE(1,   "Simple Synchronous Test of CMD32",              BUS_REQ_INDEX + 41, 0, TestCMD32)
    FTE(1,   "Simple Asynchronous Test of CMD32",             BUS_REQ_INDEX + 42, 1, TestCMD32)
    FTE(1,   "Simple Synchronous Test of CMD33",              BUS_REQ_INDEX + 43, 0, TestCMD33)
    FTE(1,   "Simple Asynchronous Test of CMD33",             BUS_REQ_INDEX + 44, 1, TestCMD33)
    FTE(1,   "Simple Synchronous Test of CMD38",              BUS_REQ_INDEX + 45, 0, TestCMD38)
    FTE(1,   "Simple Asynchronous Test of CMD38",             BUS_REQ_INDEX + 46, 1, TestCMD38)
    FTE(1,   "Simple Synchronous Test of ACMD13",             BUS_REQ_INDEX + 55, 0, TestACMD13)
    FTE(1,   "Simple Asynchronous Test of ACMD13",            BUS_REQ_INDEX + 56, 1, TestACMD13)
    FTE(1,   "Simple Synchronous Test of ACMD51",             BUS_REQ_INDEX + 66, 1, TestACMD51)
    FTE(1,   "Simple Asynchronous Test of ACMD51",            BUS_REQ_INDEX + 67, 0, TestACMD51)

    FTH(0, "Cancel Bus Request Tests")
    FTE(1,   "Test of SDCancelBusRequest",  BUS_REQ_INDEX + 68, 0, TestCancelBusRequest)
    FTE(1,   "Read Block Partial Test",     BUS_REQ_INDEX + COMPLEX_MODIFIER+1, 0, TestReadBlockPartial)
    FTE(1,   "Write Block Partial Test",    BUS_REQ_INDEX + COMPLEX_MODIFIER+2, 0, TestWriteBlockPartial)
    FTE(1,   "Read Block Misalign Test",    BUS_REQ_INDEX + COMPLEX_MODIFIER+3, 0, TestReadBlockMisalign)
    FTE(1,   "Write Block Misalign Test",   BUS_REQ_INDEX + COMPLEX_MODIFIER+4, 0, TestWriteBlockMisalign)

    FTH(0, "Simple SDCardInfoQuery Tests")
    FTE(1,   "Simple SDCardInfoQuery test of getting the CID ",                INFO_QUERY_INDEX + 2, 0, Test_CIQ_CID)
    FTE(1,   "Simple SDCardInfoQuery test of getting the CSD ",                INFO_QUERY_INDEX + 3, 0, Test_CIQ_CSD)
    FTE(1,   "Simple SDCardInfoQuery test of getting the RCA ",                INFO_QUERY_INDEX + 4, 0, Test_CIQ_RCA)
    FTE(1,   "Simple SDCardInfoQuery test of getting the Card Interface ",     INFO_QUERY_INDEX + 5, 0, Test_CIQ_CrdInfce)
    FTE(1,   "Simple SDCardInfoQuery test of getting the Status Register ",    INFO_QUERY_INDEX + 6, 0, Test_CIQ_Status)
    FTE(1,   "Simple SDCardInfoQuery test of getting the Host Interface Caps", INFO_QUERY_INDEX + 7, 0, Test_CIQ_HIF_Caps)
    FTE(1,   "Simple SDCardInfoQuery test of getting the Host Block Caps ",    INFO_QUERY_INDEX + 8, 0, Test_CIQ_HBlk_Caps)

    FTH(0, "Simple SDSetCardFeature Tests")
    FTE(1,   "Simple SDSetCardFeature test of adjusting the clock rate", CARD_FEATURE_INDEX + 1, 0, Test_SCF_ClockRate)
    FTE(1,   "Simple SDSetCardFeature test of adjusting the bus width",  CARD_FEATURE_INDEX + 2, 0, Test_SCF_BusWidth)
    FTE(1,   "SDSetCardFeature SD_CARD_FORCE_RESET",                     CARD_FEATURE_INDEX + 4, 0, TestSetCardFeaturesForceReset)
    FTE(1,   "SDSetCardFeature SD_CARD_DESELECT",                        CARD_FEATURE_INDEX + 5, 0, TestSetCardFeaturesDeselect)
    FTE(1,   "SDSetCardFeature SD_CARD_SELECT",                          CARD_FEATURE_INDEX + 6, 0, TestSetCardFeaturesSelect)

    FTH(0, "Misc Tests")
    FTE(1,   "GetBitSlice Test",             MISC_FUNC_INDEX + 2, 0, TestGetBitSlice)
    FTE(1,   "Testing unload of sdbus.dll",  MISC_FUNC_INDEX + 4, 0, TestUnloadBusDriverTest)
    FTE(1,   "SDIO Version 1.1 Bus IOCTLS ", MISC_FUNC_INDEX + 6, 0, TestSDIO1_1_BusReq)

    FTH(0, "Memory List Tests")
    FTE(1,   "Basic Memory List Test",  MEM_LIST_INDEX + 1, 0, TestMemoryList)

    FTH(0, "Feature Fullness Tests")
    FTE(1,   "Host Interface Feature Fullness Test", FEATURE_FULLNESS_INDEX + 1, 0, TestHostInterfaceFF)
    FTE(1,   "SD/MMC Card Feature Fullness Test",    FEATURE_FULLNESS_INDEX + 2, 0, TestCardInterfaceFF)
END_FTE

////////////////////////////////////////////////////////////////////////////////

#endif // !__FT_H__ || __GLOBALS_CPP__

