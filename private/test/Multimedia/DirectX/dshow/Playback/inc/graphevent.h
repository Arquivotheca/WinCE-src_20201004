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
////////////////////////////////////////////////////////////////////////////////

#ifndef GRAPH_EVENT_H
#define GRAPH_EVENT_H

#include <dshow.h>

enum GraphEvent
{
	UNKNOWN_GRAPH_EVENT = 0,
	BEGIN_FLUSH = 1,
	END_FLUSH = 2,
	EOS = 4,
	NEW_SEG = 8,
	SAMPLE = 16,
	QUERY_ACCEPT = 32,
	STATE_CHANGE_RUN = 64,
	STATE_CHANGE_PAUSE = 128,
	STATE_CHANGE_STOP = 256,
	VIDEO_TRANSFORM_CALL = 1024
};

typedef enum {
    SET_BUFFER_PARAMETERS = 0,
    GET_BUFFER_PARAMETERS,
    SET_SCALING_MODE,
    GET_SCALING_MODE,
    GET_ROTATION_CAPS,
    GET_SCALING_CAPS,
    GET_SCALING_MODE_CAPS
} FunctionName;

inline TCHAR* GraphEventToName(GraphEvent graphEvent)
{
	switch(graphEvent)
	{
	case BEGIN_FLUSH:
		return TEXT("BEGIN_FLUSH");
		break;

	case END_FLUSH:
		return TEXT("END_FLUSH");
		break;

	case EOS:
		return TEXT("EOS");
		break;

	case NEW_SEG:
		return TEXT("NEW_SEG");
		break;

	case SAMPLE:
		return TEXT("SAMPLE");
		break;

	case QUERY_ACCEPT:
		return TEXT("QUERY_ACCEPT");
		break;

	case STATE_CHANGE_RUN:
		return TEXT("RUN");
		break;

	case STATE_CHANGE_PAUSE:
		return TEXT("PAUSE");
		break;

	case STATE_CHANGE_STOP:
		return TEXT("STOP");
		break;
		
	case VIDEO_TRANSFORM_CALL:
        	return TEXT("VIDEO_TRANSFORM_CALL");
        	break;
	}
	return  TEXT("");
}

struct GraphSample
{
public:
	GraphEvent event;
	LONGLONG ats;
	IMediaSample* pMediaSample;
	bool bPreroll;
	bool bDiscontinuity;
	bool bSyncPoint;
	bool bTypeChange;
	LONGLONG start;
	LONGLONG stop;
	LONGLONG datalen;

public:
	HRESULT Serialize(BYTE* buffer, int size, DWORD* pBytesWritten);
	HRESULT Deserialize(BYTE* buffer, int size, DWORD* pBytesRead);
};

struct GraphBeginFlush
{
public:
	GraphEvent event;
	LONGLONG ats;
public:
	HRESULT Serialize(BYTE* buffer, int size, DWORD* pBytesWritten);
	HRESULT Deserialize(BYTE* buffer, int size, DWORD* pBytesRead);
};

struct GraphEndFlush
{
public:
	GraphEvent event;
	LONGLONG ats;
public:
	HRESULT Serialize(BYTE* buffer, int size, DWORD* pBytesWritten);
	HRESULT Deserialize(BYTE* buffer, int size, DWORD* pBytesRead);
};

struct GraphEndOfStream
{
public:
	GraphEvent event;
	LONGLONG ats;

public:
	HRESULT Serialize(BYTE* buffer, int size, DWORD* pBytesWritten);
	HRESULT Deserialize(BYTE* buffer, int size, DWORD* pBytesRead);
};

struct GraphNewSegment
{
public:
	GraphEvent event;
	LONGLONG ats;
	LONGLONG start;
	LONGLONG stop;
	double rate;

public:
	HRESULT Serialize(BYTE* buffer, int size, DWORD* pBytesWritten);
	HRESULT Deserialize(BYTE* buffer, int size, DWORD* pBytesRead);
};

struct GraphQueryAccept
{
public:
	GraphEvent event;
	LONGLONG ats;
	const AM_MEDIA_TYPE* pMediaType;
	bool bAccept;

public:
	HRESULT Serialize(BYTE* buffer, int size, DWORD* pBytesWritten);
	HRESULT Deserialize(BYTE* buffer, int size, DWORD* pBytesRead);
};

struct GraphRun
{
public:
	GraphEvent event;
	LONGLONG ats;
public:
	HRESULT Serialize(BYTE* buffer, int size, DWORD* pBytesWritten);
	HRESULT Deserialize(BYTE* buffer, int size, DWORD* pBytesRead);
};

struct GraphPause
{
public:
	GraphEvent event;
	LONGLONG ats;

public:
	HRESULT Serialize(BYTE* buffer, int size, DWORD* pBytesWritten);
	HRESULT Deserialize(BYTE* buffer, int size, DWORD* pBytesRead);
};

struct GraphStop
{
public:
	GraphEvent event;
	LONGLONG ats;
public:
	HRESULT Serialize(BYTE* buffer, int size, DWORD* pBytesWritten);
	HRESULT Deserialize(BYTE* buffer, int size, DWORD* pBytesRead);
};

#ifdef UNDER_CE
struct GraphVideoTransformCall
{
public:
	GraphEvent event;
	FunctionName Name;
	AM_ROTATION_ANGLE angle;
	AM_TRANSFORM_SCALING_MODE ScalingMode;
	int XPitch;
	int YPitch;
	DWORD RotationCapsMask;
	DWORD ScalingCapsMask;
	ULONG RatioCount;
	AMScalingRatio ScalingRatio;
	const AM_MEDIA_TYPE* pMediaType;

public:
	HRESULT Serialize(BYTE* buffer, int size, DWORD* pBytesWritten);
	HRESULT Deserialize(BYTE* buffer, int size, DWORD* pBytesRead);
};
#endif

#endif
