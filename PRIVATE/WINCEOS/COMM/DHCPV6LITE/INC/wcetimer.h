//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*

 THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Module Name:  
    

Abstract:  

	CE specific functions..


Functions:	
	


Revision history:

	01/29/2001  :: Original version.

    
--*/


#ifndef __WCETIMER_H__

#define __WCETIMER_H__


///////////////////////////////////////////////////////////////////////////////
//  CE function prototypes for timer routines..
///////////////////////////////////////////////////////////////////////////////

#define NETCON_STATUS                   DWORD
#define SERVICE_STATUS_HANDLE           VOID *
#define HDEVNOTIFY                      VOID *
#define JET_SESID                       VOID *
#define JET_DBID                        VOID *
#define JET_TABLEID                     VOID *
#define SESSION_CONTAINER               VOID *
#define NCS_MEDIA_DISCONNECTED          0x00
#define NCS_CONNECTED                   0x00
#define NCS_CONNECTING                  0x00
#define DBLOG_SZFMT_BUFFS               1
#define DBLOG_SZFMT_SIZE                1



typedef VOID (NTAPI * WAITORTIMERCALLBACKFUNC) (PVOID, BOOLEAN );   // winnt

typedef WAITORTIMERCALLBACKFUNC WAITORTIMERCALLBACK ;



#endif  //  __WCETIMER_H__