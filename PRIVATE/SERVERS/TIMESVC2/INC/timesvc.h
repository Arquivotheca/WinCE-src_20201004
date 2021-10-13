//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*--
Module Name: timesvc.h
Abstract: interface for timesvc components
--*/

//
//	Debug
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

int   InitializeDST (HINSTANCE hInst);
void  DestroyDST (void);
int   StartDST (void);
int   StopDST (void);
int   RefreshDST (void);
DWORD GetStateDST (void);
int   ServiceControlDST (PBYTE pBufIn, DWORD dwLenIn, PBYTE pBufOut, DWORD dwLenOut, PDWORD pdwActualOut, int *pfProcessed);
void  SetDaylightOrStandardTimeDST(SYSTEMTIME *pNewSystemTime);
