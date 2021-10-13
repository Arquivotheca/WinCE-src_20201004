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

// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.

/**************************************************************************

Module Name:

   tuxmain.h

Abstract:

   Contains Tux function table entry.

***************************************************************************/
#ifndef __TUXMAIN_H_INCLUDED__
#define __TUXMAIN_H_INCLUDED__

enum infoType {
   FAIL,
   ECHO,
   DETAIL,
   ABORT,
   SKIP,
};

#define __EXCEPTION__          		0x00
#define __FAIL__					0x02
#define __ABORT__              		0x04
#define __SKIP__               		0x06
#define __NOT_IMPLEMENTED__   	0x08
#define __PASS__              		0x0A
#define  NO_MESSAGES 			if (uMsg != TPM_EXECUTE) return TPR_NOT_HANDLED
#define	BUFFER_SIZE			0x400
#define	SEPARATOR				L": "
#define	countof(a)				(sizeof(a)/sizeof(*(a)))
void			 info(infoType iType, LPCTSTR szFormat, ...);
TESTPROCAPI getCode(void);




TESTPROCAPI TestCeRunAppAtEvent(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI TestCeRunAppAtTime(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI TestCeSetUserNotification(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI TestCeClearUserNotification(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI TestCeGetUserNotificationPreferences(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI TestCeSetUserNotificationEx(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI TestCeHandleAppNotifications(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI TestCeGetUserNotificationHandles(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI TestCeGetUserNotification(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI TestNotificationsTime(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);
TESTPROCAPI TestNotificationsEvent(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);



/*----------------------------------------------*
| Test Registration Function Table
*-----------------------------------------------*/
static FUNCTION_TABLE_ENTRY g_lpFTE[] = {
TEXT("Notification Tests"),							0, 0, 0,    NULL,
    TEXT("Notifications API and use"),				1, 0, 0,    NULL,
    TEXT("TestCeRunAppAtEvent"),					2, 0, 10,   TestCeRunAppAtEvent,
    TEXT("TestCeRunAppAtTime"),					    2, 0, 11,   TestCeRunAppAtTime,
    TEXT("TestCeSetUserNotification"),				2, 0, 12,   TestCeSetUserNotification,
    TEXT("TestCeGetUserNotificationPreferences"),	2, 0, 13,   TestCeGetUserNotificationPreferences,
    TEXT("TestCeSetUserNotificationEx"),			2, 0, 14,   TestCeSetUserNotificationEx,
    TEXT("TestCeHandleAppNotifications"),			2, 0, 15,   TestCeHandleAppNotifications,
    TEXT("TestCeGetUserNotification"),				2, 0, 16,   TestCeGetUserNotification,
    TEXT("TestNotificationsTime"),					2, 0, 17,   TestNotificationsTime,
    TEXT("TestNotificationsEvent"),                 2, 0, 18,   TestNotificationsEvent,
    NULL,									        0, 0, 0,    NULL
};

#endif
