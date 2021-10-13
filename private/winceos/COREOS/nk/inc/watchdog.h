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
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 6.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

//
//    watchdog.h - internal kernel memory mapped file header
//

#ifndef _NK_WATCHDOG_H_
#define _NK_WATCHDOG_H_

#include "kerncmn.h"

//
// watchdog structure
//
struct _WatchDog {
    DLIST           link;           // list of watchdogs
    PEVENT          pEvt;           // back ptr to event
    WORD            wState;         // current state
    WORD            wDfltAction;    // default action
    DWORD           dwExpire;       // time expired
    DWORD           dwPeriod;       // watchdog period
    DWORD           dwWait;         // time to wait before default action taken
    DWORD           dwParam;        // parameter passed to IOCTL_HAL_REBOOT
    DWORD           dwProcId;       // process to be watched
};

// start a watchdog timer
BOOL WDStart (PEVENT pEvt);

// stop a watchdog timer
BOOL WDStop (PEVENT pEvt);

// refresh a watchdog timer
BOOL WDRefresh (PEVENT pEvt);

// destroy a watchdog timer
BOOL WDDelete (PEVENT pEvt);

// create or open a watchdog timer
HANDLE NKCreateWatchDog (LPSECURITY_ATTRIBUTES lpsa,    // security attributes
    LPCWSTR pszWatchDogName,                            // name of the watchdog
    DWORD dwDfltAction,                                 // default action if watchdog is signaled
    DWORD dwParam,                                      // reset parameter if default action is reset
    DWORD dwPeriod,                                     // watchdog period
    DWORD dwWait,                                       // wait period before taking default action
    BOOL  fCreate);                                     // create or open

// watchdog initialization
BOOL InitWatchDog (void);


#endif  // _NK_WATCHDOG_H_

