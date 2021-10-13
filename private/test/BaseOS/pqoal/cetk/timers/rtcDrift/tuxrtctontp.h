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
/*
 * tuxRtcToNtp.h
 */

#ifndef __TUX_RTC_TO_NTP_H__
#define __TUX_RTC_TO_NTP_H__

#include <windows.h>
#include <tux.h>

TESTPROCAPI usage(
          UINT uMsg, 
          TPPARAM tpParam, 
          LPFUNCTION_TABLE_ENTRY lpFTE);

/*
 * Compare the rtc to a ntp server.
 */
TESTPROCAPI compareRTCToNTP(
                 UINT uMsg, 
                 TPPARAM tpParam, 
                 LPFUNCTION_TABLE_ENTRY lpFTE);

/*
 * Compare the gtc to a ntp server.
 */
TESTPROCAPI compareGTCToNTP(
                 UINT uMsg, 
                 TPPARAM tpParam, 
                 LPFUNCTION_TABLE_ENTRY lpFTE);


#endif // __TUX_RTC_TO_NTP_H__
