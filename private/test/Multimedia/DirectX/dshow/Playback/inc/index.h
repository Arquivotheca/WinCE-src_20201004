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
//
//  Index objects
//
//
////////////////////////////////////////////////////////////////////////////////

#ifndef INDEX_H
#define INDEX_H

#include <vector>
#include <dshow.h>
#include "PlaybackMedia.h"

struct TimeStampIndexEntry
{
	int framenum;
	LONGLONG start;
	LONGLONG stop;
	bool bSyncPoint;
	bool bPreroll;
	bool bDiscontinuity;
};

typedef std::vector<TimeStampIndexEntry> TimeStampIndexList;

class TimeStampIndex
{
public:
	TimeStampIndex(TCHAR* szIndexFile);
	HRESULT GetNumFrames(int* pNumFrames);
	HRESULT GetTimeStamps(int frameno, LONGLONG* pStart, LONGLONG* pStop);
	HRESULT GetNearestKeyFrames(int frameno, 
								LONGLONG* pPrevStart, LONGLONG* pPrevStop, 
								LONGLONG* pNextStart, LONGLONG* pNextStop);
	HRESULT GetNearestKeyFrames(LONGLONG start, int* pPrevKeyFrame, int* pNextKeyFrame);

private:
	HRESULT ParseIndexFile();

private:
	TCHAR m_szIndexFile[MAX_PATH];
	HANDLE m_hIndexFile;
	TimeStampIndexList m_IndexList;
	int m_nFrames;
};

class FrameIndex
{
public:
	FrameIndex(TCHAR* szIndexFile);
	HRESULT VerifyFrame(int frameno, IMediaSample* pMediaSample);
	HRESULT GetFrame(int frameno, IMediaSample** ppMediaSample);
};

#endif