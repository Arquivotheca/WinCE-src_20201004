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


*/
#ifndef __USERINQ_HPP_INCLUDED__
#define __USERINQ_HPP_INCLUDED__

#include <cmsgque.h>
#include <tchddi.h>
#include <keybddr.h>

extern MsgQueue    *v_pmsgqOOMInput;
extern HWND        v_hwndOOM;
extern BOOL        v_fStartupAfterOOMIsDone;

/*
	This is a bit kludgy.  The keyboard event flags define
	KeyStateKeyEventFlag (0x80000000) which is used to identify keyboard
	events.  We use 0x40000000 to identify mouse events.  By default, no flags
	set means a touch event.  This allows saving device specific flags in one
	flags field in the input event structure.  This is fairly important since
	we have a fixed size message queue and so any size increase in the
	structure cause a big hit in memory usage.
	
	We use the MOUSEEVENTF_xxx flags to describe both mouse and touch events.
*/

union EventFlags_t
{
	KEY_STATE_FLAGS	m_KeyStateFlags;
	UINT32			m_MouseTouchFlags;
};



enum EventType_t
	{
	TOUCH_EVENT = 0,
    MOUSE_EVENT,
    KEYBD_EVENT
	};


EventType_t
EventTypeFromFlags(
	EventFlags_t	EventFlags
	);


//	Common header for every user input queue entry.
struct	BaseEventEntry_t
	{
	EventFlags_t	m_Flags;
	};


struct	TouchEventEntry_t : public BaseEventEntry_t
    {
	INT32						m_X;
	INT32						m_Y;
	INT32						m_W;
    DWORD                       m_dwTime;
    };


struct	KeybdEventEntry_t : public BaseEventEntry_t
	{
	UINT32			m_VirtualKey;
	UINT32			m_ScanCode;
	};



//	An entry in the user input queue.
union	UserInputQueueEntry_t
	{
	BaseEventEntry_t	m_BaseEvent;
	TouchEventEntry_t	m_TouchEvent;		//	Touch event entry.
	KeybdEventEntry_t	m_KeybdEvent;		//	Keyboard event entry.
	};

// the following is a gwes private value from the public 
// enumTouchPanelSampleFlags defined in the OAK public header: tchddi.h
// Note: the related MOUSEEVENTF_INTERNAL_PING flag is defined in cursor.hpp
const	DWORD	TouchSampleFilterFlag= 0x10000000;	// Identify this sample
													// should be considered 
													// for filtering

#define	TouchSampleFilter(Flags)	( (Flags) & TouchSampleFilterFlag )

const int CNT_USER_INPUT_QUEUE_ENTRIES = 512;


typedef void (*PFN_SYSTEM_MODAL)(
	UINT	msg,
	UINT	Param1,
	UINT	Param2
	);

extern PFN_SYSTEM_MODAL v_pSystemModalFunction;

//	Should only be used by journaling.
BOOL
UserInputQueue_PutTMEvent(
	TOUCH_PANEL_SAMPLE_FLAGS	Flags,
	INT32				X,
	INT32				Y,
	INT32				W
	);


BOOL
UserInputQueue_PutTouchEvent(
	TOUCH_PANEL_SAMPLE_FLAGS	Flags,
	INT32				X,
	INT32				Y
	);


BOOL
UserInputQueue_PutKeybdEvent(
	UINT32			VirtualKey,
	KEY_STATE_FLAGS	KeyEvent
	);

BOOL
UserInputQueue_PutKeybdEventEx(
	UINT32			VirtualKey,
	UINT32			ScanCode,
	KEY_STATE_FLAGS	KeyEvent
	);


void
UserInputQueue_Signal(
	void
	);


void
UserInput_SetOOMWindow(
	HWND	hwnd
	);

void
UserInput_OOMCycleDone(
	void
	);


void
UserInputQueue_ReplaceMouseWithTouch(
	int	UserInputQueueIndex,
	int	x,
	int	y
	);

void
UserInput_PingCursor(
	void
	);


#endif

