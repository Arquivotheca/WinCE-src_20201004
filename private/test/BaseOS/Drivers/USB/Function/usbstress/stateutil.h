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
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright  1998-2000  Microsoft Corporation.  All Rights Reserved.

Module Name: stateutil.h 
*/

#ifndef __STATEUTIL_H_
#define __STATEUTIL_H_

#include <windows.h>
#include <pwrtstutil.h>		// for power state simulation
#include <windows.h>
#include <usbdi.h>
#include "usbstress.h"
#include "usbserv.h"
#include "UsbClass.h"
#include "resource.h"
#include "pipetest.h"

 //GLOBAL VARIABLE - related with RTC wakeup
extern HANDLE g_RTCEvent;
extern BOOL g_fWakeupSet;
extern DWORD dwWakeCount;

// Set wakeup for n seconds.  Must be at least 30
BOOL SetWakeup(WORD wSeconds);
// Clear wakeup source
VOID ClearWakeup();

DWORD SleepDevice(UsbClientDrv * pDriverPtr);

DWORD ResetDevice(UsbClientDrv * pDriverPtr, BOOL bReEnum);
DWORD SuspendDevice(UsbClientDrv * pDriverPtr);
DWORD ResumeDevice(UsbClientDrv * pDriverPtr);
#endif
