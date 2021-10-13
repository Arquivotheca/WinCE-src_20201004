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
////////////////////////////////////////////////////////////////////////////////
//
//  Event logger objects
//
//
////////////////////////////////////////////////////////////////////////////////

#include <dshow.h>
#include "EventLogger.h"
#include "TestGraph.h"

EventLoggerFactoryTableEntry g_EventLoggerFactoryTable[] = 
{
	{SampleProperties, CreateSampleLogger},
	{VideoFrameData, NULL},	
	{VideoTimeStamps, NULL},
	{AudioFrameData, NULL},
	{AudioTimeStamps, NULL},
	{InsertFrameNumbers, CreateFrameNumberInstrumenter},
	{LogFrameNumbers, CreateFrameNumberLogger},
	{StreamData, CreateStreamLogger},
	{GraphEvents, CreateGraphEventLogger},
	{RemoteEvents, CreateRemoteEventLogger},
};

int g_EventLoggerFactoryTableSize = sizeof(g_EventLoggerFactoryTable)/sizeof(EventLoggerFactoryTableEntry);

const TCHAR* GetEventLoggerString(EventLoggerType type)
{
	return NULL;
}

bool IsLoggable(EventLoggerType type)
{
	for(int i = 0; i < g_EventLoggerFactoryTableSize; i++)
	{
		if ((g_EventLoggerFactoryTable[i].type == type) &&
			g_EventLoggerFactoryTable[i].fn)
		{
			return true;
		}
	}
	return false;
}

HRESULT CreateEventLogger(EventLoggerType type, void* pEventLoggerData, TestGraph* pTestGraph, IEventLogger** ppEventLogger)
{
	HRESULT hr = E_NOTIMPL;
	for(int i = 0; i < g_EventLoggerFactoryTableSize; i++)
	{
		if ((g_EventLoggerFactoryTable[i].type == type) &&
			g_EventLoggerFactoryTable[i].fn)
		{
			return g_EventLoggerFactoryTable[i].fn(type, pEventLoggerData, pTestGraph, ppEventLogger);
		}
	}
	return hr;
}

// This is the call back method from the tap filter. This call needs to be relayed to the event logger
HRESULT GenericLoggerCallback(GraphEvent event, void* pEventData, void* pTapCallbackData, void* pObject)
{
	return ((IEventLogger *)(pObject))->ProcessEvent(event, pEventData, pTapCallbackData);
}