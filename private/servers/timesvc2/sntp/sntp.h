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
/*--
Module Name: sntp.h
--*/

//
//  Debug
//
#if defined (DEBUG) || defined (_DEBUG)
#define ZONE_INIT               DEBUGZONE(0)        
#define ZONE_SERVER             DEBUGZONE(1)        
#define ZONE_CLIENT             DEBUGZONE(2)        
#define ZONE_PACKETS            DEBUGZONE(3)        
#define ZONE_TRACE              DEBUGZONE(4)        
#define ZONE_DST                DEBUGZONE(5)        

#define ZONE_MISC               DEBUGZONE(13)
#define ZONE_WARNING            DEBUGZONE(14)       
#define ZONE_ERROR              DEBUGZONE(15)       

#endif

int   InitializeSNTP (HINSTANCE hInst);
void  DestroySNTP (void);
int   StartSNTP (void);
int   StopSNTP (void);
int   RefreshSNTP (void);
DWORD GetStateSNTP (void);
int   ServiceControlSNTP (PBYTE pBufIn, DWORD dwLenIn, PBYTE pBufOut, DWORD dwLenOut, PDWORD pdwActualOut, int *pfProcessed);

void  SetDaylightOrStandardTimeDST(SYSTEMTIME *pNewSystemTime);
