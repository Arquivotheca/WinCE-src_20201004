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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
/*---------------------------------------------------------------------------*\
 *  module: dst.h
 *  purpose: header file for dst.cpp
 *
\*---------------------------------------------------------------------------*/

#ifndef _DST_H_
#define _DST_H_

static DWORD WINAPI DST_Init(void);
static BOOL         DST_Auto(void);
static BOOL         DST_ShowMessage(void);
static HANDLE       DST_SetDSTEvent(void);
static HANDLE       DST_SetTimezoneChangeEvent(void);
static HANDLE       DST_SetTimeChangeEvent(void);
static int          DST_WaitForEvents(LPHANDLE DST_handles);
static BOOL         DST_DetermineChangeDate(LPSYSTEMTIME pst, BOOL fThisYear = FALSE);


// helper functions
static int DowFromDate(SYSTEMTIME *pst);
static int GetStartDowForMonth(int yr, int mo);
static BOOL LocaleSupportsDST(TIME_ZONE_INFORMATION *ptzi);


#endif // _DST_H_
