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
#include <streams.h>
#include "logging.h"
#include "globals.h"
#include "GraphEvent.h"
#include "RemoteEvent.h"

HRESULT GraphSample::Serialize(BYTE* pBuffer, int size, DWORD* pBytesWritten)
{
	BYTE* outbuf = pBuffer;

#if 0
	// Write out the graph event
	*(DWORD*)outbuf = (DWORD)SAMPLE;
	outbuf += sizeof(DWORD);
	
	// Placeholder for the size
	DWORD* pSize = (DWORD*)outbuf;
	outbuf += sizeof(DWORD);
#endif

	// Arrival time
	*(LONGLONG*)outbuf = ats;
	outbuf += sizeof(LONGLONG);

	// Serialize the media samle
	// SerializeMediaSample();

	// Preroll
	*(bool*)outbuf = bPreroll;
	outbuf += sizeof(bool);

	// Discontinuity
	*(bool*)outbuf = bDiscontinuity;
	outbuf += sizeof(bool);
	
	// SyncPoint
	*(bool*)outbuf = bSyncPoint;
	outbuf += sizeof(bool);
	
	// TypeChange
	*(bool*)outbuf = bTypeChange;
	outbuf += sizeof(bool);

	// start and stop times and datalen
	*(LONGLONG*)outbuf = start;
	outbuf += sizeof(LONGLONG);

	*(LONGLONG*)outbuf = stop;
	outbuf += sizeof(LONGLONG);

	*(LONGLONG*)outbuf = datalen;
	outbuf += sizeof(LONGLONG);

#if 0
	// Write the size of the event
	*pSize = outbuf - pBuffer;
#endif

	if (pBytesWritten)
		*pBytesWritten = outbuf - pBuffer;
	
	return S_OK;
}

HRESULT GraphSample::Deserialize(BYTE* buffer, int size, DWORD* pBytesRead)
{
	return S_OK;
}

HRESULT GraphBeginFlush::Serialize(BYTE* pBuffer, int size, DWORD* pBytesWritten)
{
	BYTE* outbuf = pBuffer;

	// Arrival time
	*(LONGLONG*)outbuf = ats;
	outbuf += sizeof(LONGLONG);

	if (pBytesWritten)
		*pBytesWritten = outbuf - pBuffer;
	
	return S_OK;
}

HRESULT GraphBeginFlush::Deserialize(BYTE* buffer, int size, DWORD* pBytesRead)
{
	return S_OK;
}

HRESULT GraphEndFlush::Serialize(BYTE* pBuffer, int size, DWORD* pBytesWritten)
{
	BYTE* outbuf = pBuffer;

	// Arrival time
	*(LONGLONG*)outbuf = ats;
	outbuf += sizeof(LONGLONG);

	if (pBytesWritten)
		*pBytesWritten = outbuf - pBuffer;
	
	return S_OK;
}

HRESULT GraphEndFlush::Deserialize(BYTE* buffer, int size, DWORD* pBytesRead)
{
	return S_OK;
}

HRESULT GraphEndOfStream::Serialize(BYTE* pBuffer, int size, DWORD* pBytesWritten)
{
	BYTE* outbuf = pBuffer;

	// Arrival time
	*(LONGLONG*)outbuf = ats;
	outbuf += sizeof(LONGLONG);

	if (pBytesWritten)
		*pBytesWritten = outbuf - pBuffer;
	
	return S_OK;
}

HRESULT GraphEndOfStream::Deserialize(BYTE* buffer, int size, DWORD* pBytesRead)
{
	return S_OK;
}

HRESULT GraphNewSegment::Serialize(BYTE* pBuffer, int size, DWORD* pBytesWritten)
{
	BYTE* outbuf = pBuffer;

	// Arrival time
	*(LONGLONG*)outbuf = ats;
	outbuf += sizeof(LONGLONG);

	// start and stop times and rate
	*(LONGLONG*)outbuf = start;
	outbuf += sizeof(LONGLONG);

	*(LONGLONG*)outbuf = stop;
	outbuf += sizeof(LONGLONG);
	
	*(double*)outbuf = rate;
	outbuf += sizeof(double);

	if (pBytesWritten)
		*pBytesWritten = outbuf - pBuffer;
	
	return S_OK;
}

HRESULT GraphNewSegment::Deserialize(BYTE* buffer, int size, DWORD* pBytesRead)
{
	return S_OK;
}

HRESULT GraphQueryAccept::Serialize(BYTE* pBuffer, int size, DWORD* pBytesWritten)
{
	BYTE* outbuf = pBuffer;

	// Arrival time
	*(LONGLONG*)outbuf = ats;
	outbuf += sizeof(LONGLONG);

	// BUGBUG: serialize the media type
	// SerializeMediaType();

	// Accepted?
	*(DWORD*)outbuf = (DWORD) (bAccept ? 1 : 0);
	outbuf += sizeof(DWORD);

	if (pBytesWritten)
		*pBytesWritten = outbuf - pBuffer;
	
	return S_OK;
}

HRESULT GraphQueryAccept::Deserialize(BYTE* buffer, int size, DWORD* pBytesRead)
{
	return S_OK;
}


HRESULT GraphRun::Serialize(BYTE* pBuffer, int size, DWORD* pBytesWritten)
{
	LOG(TEXT(""));
	BYTE* outbuf = pBuffer;

	// Arrival time
	*(LONGLONG*)outbuf = ats;
	outbuf += sizeof(LONGLONG);

	if (pBytesWritten)
		*pBytesWritten = outbuf - pBuffer;
	
	return S_OK;
}

HRESULT GraphRun::Deserialize(BYTE* buffer, int size, DWORD* pBytesRead)
{
	return S_OK;
}

HRESULT GraphPause::Serialize(BYTE* pBuffer, int size, DWORD* pBytesWritten)
{
	BYTE* outbuf = pBuffer;

	// Arrival time
	*(LONGLONG*)outbuf = ats;
	outbuf += sizeof(LONGLONG);

	if (pBytesWritten)
		*pBytesWritten = outbuf - pBuffer;
	
	return S_OK;
}

HRESULT GraphPause::Deserialize(BYTE* buffer, int size, DWORD* pBytesRead)
{
	return S_OK;
}

HRESULT GraphStop::Serialize(BYTE* pBuffer, int size, DWORD* pBytesWritten)
{
	BYTE* outbuf = pBuffer;

	// Arrival time
	*(LONGLONG*)outbuf = ats;
	outbuf += sizeof(LONGLONG);

	if (pBytesWritten)
		*pBytesWritten = outbuf - pBuffer;
	
	return S_OK;
}

HRESULT GraphStop::Deserialize(BYTE* buffer, int size, DWORD* pBytesRead)
{
	return S_OK;
}

#ifdef UNDER_CE
HRESULT GraphVideoTransformCall::Deserialize(BYTE* buffer, int size, DWORD* pBytesRead)
{
	return S_OK;
}

HRESULT GraphVideoTransformCall::Serialize(BYTE* pBuffer, int size, DWORD* pBytesWritten)
{
	BYTE* outbuf = pBuffer;

#if 0
	// Write out the graph event
	*(DWORD*)outbuf = (DWORD)SAMPLE;
	outbuf += sizeof(DWORD);
	
	// Placeholder for the size
	DWORD* pSize = (DWORD*)outbuf;
	outbuf += sizeof(DWORD);
#endif

	// Surface Angle
	*(AM_ROTATION_ANGLE*)outbuf = angle;
	outbuf += sizeof(AM_ROTATION_ANGLE);

	// Serialize the media samle
	// SerializeMediaSample();

	// Function Name
	*(UINT*)outbuf = ScalingMode;
	outbuf += sizeof(UINT);

	// Scaling Mode
	*(AM_TRANSFORM_SCALING_MODE*)outbuf = ScalingMode;
	outbuf += sizeof(AM_TRANSFORM_SCALING_MODE);

	// XPitch
	*(int*)outbuf = XPitch;
	outbuf += sizeof(int);
	
	// YPitch
	*(int*)outbuf = YPitch;
	outbuf += sizeof(int);
	
	// Rotation Caps
	*(DWORD*)outbuf = RotationCapsMask;
	outbuf += sizeof(DWORD);

	// Scaling Caps
	*(DWORD*)outbuf = ScalingCapsMask;
	outbuf += sizeof(DWORD);
       // Ration counts
	*(ULONG*)outbuf = RatioCount;
	outbuf += sizeof(ULONG);
       // Scaling ratios
	*(AMScalingRatio*)outbuf = ScalingRatio;
	outbuf += sizeof(AMScalingRatio);

#if 0
	// Write the size of the event
	*pSize = outbuf - pBuffer;
#endif

	if (pBytesWritten)
		*pBytesWritten = outbuf - pBuffer;
	
	return S_OK;
}
#endif

HRESULT RemoteGraphEvent::Serialize(BYTE* buffer, int bufsize, 
									TCHAR* szTapName, int szTapNameLength,
									GraphEvent graphEvent, BYTE* pGraphEventData, int eventDataSize, 
									DWORD* pBytesWritten)
{
	BYTE* outbuf = buffer;

	if (!buffer || !szTapName)
		return E_POINTER;

	if (szTapNameLength == 0)
		return E_INVALIDARG;

	if (eventDataSize && !pGraphEventData)
		return E_INVALIDARG;

	// BUGBUG: no check if bufsize will be exceeded

	// Output the type
	*(RemoteEventType*)outbuf = REMOTE_GRAPH_EVENT;
	outbuf += sizeof(RemoteEventType);

	// Placeholder for the size of the event in the buffer
	DWORD* pSize = (DWORD*)outbuf;
	outbuf += sizeof(DWORD);

	// Output the tap name
	// BUGBUG: hacking the name of the string
	memcpy(outbuf, szTapName, szTapNameLength*sizeof(TCHAR));
	outbuf += szTapNameLength;

	// Output the graph event
	*(GraphEvent*)outbuf = graphEvent;
	outbuf += sizeof(GraphEvent);

	// Output the size
	*(int*)outbuf = eventDataSize;
	outbuf += sizeof(int);

	// Output the graph event data
	if (eventDataSize > 0)
	{
		memcpy(outbuf, pGraphEventData, eventDataSize);
		outbuf += eventDataSize;
	}

	// Write the size of the remote event
	*pSize = outbuf - buffer;

	if (pBytesWritten)
		*pBytesWritten = *pSize;

	return S_OK;
}

