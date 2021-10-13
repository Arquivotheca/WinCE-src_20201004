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
/***
*initsect.cpp - RTC support
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*
*Revision History:
*       11-03-98  KBF   Module incorporated into CRTs
*       05-11-99  KBF   Error if RTC support define not enabled
*       08-10-99  RMS   Use external symbols for BBT support
*	03-13-04  MSL   Removed __try that ate errors unnecessarily - prefast
*
****/

#ifndef _RTC
#error  RunTime Check support not enabled!
#endif

#include "internal.h"
#include "rtcpriv.h"
#include "sect_attribs.h"

#pragma const_seg(".rtc$IAA")
extern "C" const _CRTALLOC(".rtc$IAA") _PVFV __rtc_iaa[] = { 0 };


#pragma const_seg(".rtc$IZZ")
extern "C" const _CRTALLOC(".rtc$IZZ") _PVFV __rtc_izz[] = { 0 };


#pragma const_seg(".rtc$TAA")
extern "C" const _CRTALLOC(".rtc$TAA") _PVFV __rtc_taa[] = { 0 };


#pragma const_seg(".rtc$TZZ")
extern "C" const _CRTALLOC(".rtc$TZZ") _PVFV __rtc_tzz[] = { 0 };

#pragma const_seg()


#pragma comment(linker, "/MERGE:.rtc=.rdata")

#ifndef _RTC_DEBUG
#pragma optimize("g", on)
#endif

// Run the RTC initializers
extern "C" void __declspec(nothrow) __cdecl _RTC_Initialize()
{
    // Just step thru every item
    const _PVFV *f;
    for (f = __rtc_iaa + 1; f < __rtc_izz; f++)
    {
        if (*f)
		{
            (**f)();
		}
    }
}

// Run the RTC terminators
extern "C" void __declspec(nothrow) __cdecl _RTC_Terminate()
{
    // Just step thru every item
    const _PVFV *f;
    for (f = __rtc_taa + 1; f < __rtc_tzz; f++)
    {
        if (*f)
		{
            (**f)();
		}
    }
}
