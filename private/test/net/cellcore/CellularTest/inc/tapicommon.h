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

//////////////////////////////////////////////////////////
// TAPICommon - Commonly used TAPI functions and macros //
// David Anson (DavidAns@Microsoft.com)                 //
//////////////////////////////////////////////////////////


#ifndef TAPICOMMON_HEADER
#define TAPICOMMON_HEADER

#define TAPICOMMON_BUFFER_SIZE (256)

struct WaitForTAPILineData
{
    HANDLE hLineMessagesSemaphore;
    GrowableThingCollection<LINEMESSAGE>* gtcLineMessages;
    DWORD dwLastCALLSTATE;
    DWORD dwLastDISCONNECTMODE;
    DWORD dwTAPITimeoutMilliseconds;
};

#define WaitForTAPI_DEFAULT_TIMEOUT (INFINITE-1)
#define WaitForLINE_REPLY(id, wftldData) WaitForTAPILine(LINE_REPLY, id, 0, WaitForTAPI_DEFAULT_TIMEOUT, wftldData)
#define WaitForLINE_CALLSTATE(s, wftldData) WaitForTAPILine(LINE_CALLSTATE, 0, s, WaitForTAPI_DEFAULT_TIMEOUT, wftldData)
BOOL WaitForTAPILine(const DWORD dwWaitFor, const DWORD dwRequestID, const DWORD dwWaitForCALLSTATEMask, DWORD dwTimeout, WaitForTAPILineData& rwftldData);

DWORD GetTSPLineDeviceID(const HLINEAPP hLineApp, const DWORD dwNumberDevices, const DWORD dwAPIVersionLow, const DWORD dwAPIVersionHigh, const TCHAR* const psTSPLineName);
void OutputLINECALLSTATUSInformation(const TCHAR* const psDescription, const HCALL hCall, const LINECALLSTATUS& rlcsCallStatus);

#endif // TAPICOMMON_HEADER
