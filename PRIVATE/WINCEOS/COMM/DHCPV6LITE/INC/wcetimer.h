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