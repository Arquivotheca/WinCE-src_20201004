//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
