//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
/*


*/
#ifndef __GWE_USERIN_SERVER_TOUCH_H__
#define __GWE_USERIN_SERVER_TOUCH_H__

#include <windows.h>
#include <gwe_s.h>
#include <tchddi.h>
#include <cmsgque.h>

void
TouchCalibrate(
	BOOL	fForceCalibration
	);

void
TouchCalibrationAbort(
	void
	);

void
TouchEventSend(
	TOUCH_PANEL_SAMPLE_FLAGS	Flags,
	INT32						X,
	INT32						Y,
	INT32						W,
    DWORD                       dwTime,
	int							UserInputQueueIndex
	);

void TouchGotEscapeKey(
	void
	);

MsgQueue*
Touch_pmsgqCurrentStroke(
	void
	);


HANDLE GetStartupScreenThreadHandle();

BOOL
SetInputLock(
	BOOL bActivate
	);

LRESULT
HookTouch(
    UINT32  Msg,
    int	    X,
    int	    Y,
    int	    W
    );


LRESULT
TouchHookCallbackHandler(void);

HHOOK
WINAPI
SetWindowsTouchHook_I(
    HOOKPROC    lpfn,
    HINSTANCE   hmod,
    DWORD       dwThreadId
    );


BOOL
WINAPI
UnhookWindowsTouchHook_I(
    HHOOK hhk
    );



void
TouchMessageQueueTerminated(
    __in MsgQueue* pmsgq
    );


#endif

