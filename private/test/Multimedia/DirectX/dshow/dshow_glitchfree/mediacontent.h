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
// --------------------------------------------------------------------
//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
//                                                                     
// --------------------------------------------------------------------
#pragma once

#pragma warning(disable:4786)
#include <windows.h>
#include <string>

using namespace std ;

class CLocations
{
public:
	CLocations();

	string m_Local;
	string m_HTTP;
	string m_MMS;
	string m_MMSU;
	string m_MMST;
};

class CVideoStats
{
public:
	CVideoStats();

	int m_iHeight;
	int m_iWidth;
	long m_lBitRate;
	bool m_bContainsAudio;
};

class CAudioStats
{
public:
	CAudioStats();

	enum Channel {Stereo, Mono};
	long m_lBitRate;
	int m_iHz;
	Channel m_Channels;
};

class CQualityControl
{
public:
	CQualityControl();

	long m_lBitRate;
	double m_dDuration;
	int m_iAvgFrameRate;
	int m_iAvgSyncOffset;
	int m_iDevSyncOffset;
	long m_lFramesDrawn;
	long m_lFramesDroppedInRenderer;
	long m_lFramesDroppedInDecoder;
	int m_iJitter;

	double m_dDurationMarginOfError;
	int m_iAvgFrameRateMarginOfError;
	int m_iAvgSyncOffsetMarginOfError;
	int m_iDevSyncOffsetMarginOfError;
	long m_lFramesDrawnMarginOfError;
	long m_lFramesDroppedInRendererMarginOfError;
	long m_lFramesDroppedInDecoderMarginOfError;
	int m_iJitterMarginOfError;
};

class CMediaContent
{
public:
	CMediaContent();
	~CMediaContent();
	string m_id;
	string m_Descripton;
	string m_ClipName;
	string m_DRMClipName;
	string m_Codec;
	bool m_bDRM;

	CLocations m_Locations;
	CVideoStats m_VideoStats;
	CAudioStats m_AudioStats;
	CQualityControl m_QualityControl;
};

