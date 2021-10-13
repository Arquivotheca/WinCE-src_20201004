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
void GweNotifyCallback (DWORD cause, HPROCESS hprc, HTHREAD hthd);


void
MsgQue_Initialize(
	void
	);

void
Timer_Initialize(
	void
	);


void
TouchCalibrateRestoreFromRegistry(
	void
	);


extern "C"
BOOL
WINAPI
TouchCalibrate(
	void
    );


VOID AudioDriverPowerHandler(BOOL power_down);
VOID TouchPowerHandler(BOOL power_down);
VOID KeybdPowerHandler(BOOL power_down);


void
SystemIdleTimerUpdateMax(
	void
	);


void
KeybdAutoRepeat_Update(
	void
	);


VOID
MenuProcessCleanup (
    HPROCESS hprc
    );

VOID
Initialize_OutOfMemoryHandler (
	void
	);




void
UpdateIdleTimeoutFromRegistry(
	void
	);


extern "C"
BOOL
TouchPanelSetMode (
	INT index,
	LPVOID pv
	);



