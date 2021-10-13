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
#ifndef SCF_HLP_H
#define SCF_HLP_H

#include <windows.h>
#include <sdcommon.h>
#include <sd_tst_cmn.h>

BOOL VerifySetCardFeatureCR(UINT indent, PTEST_PARAMS  pTestParams, SD_DEVICE_HANDLE hDevice, PSD_CARD_INTERFACE_EX pciSet, PSD_CARD_INTERFACE_EX pciExpect, LPCTSTR msg,...);

BOOL TimeWrites(UINT indent, SD_DEVICE_HANDLE hDevice, LPDWORD lpdwTimeArray, DWORD dwArrSize, PUCHAR pbuff, DWORD buffSize, PTEST_PARAMS pTestParams);

DWORD DWAvg(DWORD const *lpdwArray, DWORD dwSize);

BOOL CompareVals(DWORD dwFirst, DWORD dwLast, DWORD dwPlusMinus);

SD_API_STATUS GetBusWidthsSupported(UINT indent, SD_DEVICE_HANDLE hDevice, PTEST_PARAMS pTestParams, PSD_COMMAND_RESPONSE pResp, PUCHAR busWidths);

SD_INTERFACE_MODE HighestBusWidthSupportAtHost(UINT indent, SD_DEVICE_HANDLE hDevice, PTEST_PARAMS pTestParams, SD_API_STATUS *pRStat);

#endif //SCF_HLP_H

