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
#include "kernel.h"

#ifndef __TZ_INIT_H__
#define __TZ_INIT_H__

//
// soft RTC related registries
//

// registry key definition
#define REGKEY_PLATFORM         L"Platform"

// registry value names
#define REGVAL_SOFTRTC          L"SoftRTC"

//
// time zone bias registry info
//
#define REGKEY_BOOTVARS     L"init\\BootVars"

#define REGVAL_KTZBIAS      L"KTzBias"

// for storing biases/putting bias information in registry
typedef struct _BIAS_STRUCT {
    DWORD InDaylight;
    DWORD NormalBias;
    DWORD DaylightBias;
} BIAS_STRUCT, *PBIAS_STRUCT;

// for use in testing DST status on init
#define DST_GTE(date1, date2)               \
    ((date1.wMonth > date2.wMonth) ||       \
    (date1.wDay > date2.wDay &&             \
        date1.wMonth == date2.wMonth)  ||   \
    (date1.wHour >= date2.wHour &&          \
        date1.wDay == date2.wDay &&         \
        date1.wMonth == date2.wMonth))

#define DST_LT(date1, date2) !DST_GTE(date1, date2)

// from time.c for use in tzinit/notzinit
BOOL SetTimeHelper (ULONGLONG ui64Ticks, DWORD dwType, LONGLONG *pi64Delta);
ULONGLONG GetTimeInTicks (DWORD dwType);
void SignalKernelAlarm (void);

// change kernel tz bias and set registry bias
// if necessary
BOOL BiasChangeHelper(PBIAS_STRUCT pBias, BOOL fMaintainUTC, BOOL fSignalAlarm);

// sysgen-dependent function to init kernel tz bias if direcy query fails
void InitKTzBiasFromTzInfo(PBIAS_STRUCT pBias);

// variables
BIAS_STRUCT g_tzBias;

extern CRITICAL_SECTION rtccs;

#endif // __TZ_INIT_H__
