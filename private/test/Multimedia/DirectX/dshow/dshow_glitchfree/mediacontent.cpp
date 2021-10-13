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
#include "MediaContent.h"
#include "utility.h"

CLocations::CLocations()
{
	LOGZONES(TEXT("%S: ENTER %d@%S"), __FUNCTION__, __LINE__, __FILE__);
	
	m_Local = "";
	m_HTTP = "";
	m_MMS = "";
	m_MMSU = "";
	m_MMST = "";

	LOGZONES(TEXT("%S: EXIT %d@%S"), __FUNCTION__, __LINE__, __FILE__);
}

CVideoStats::CVideoStats()
{
	LOGZONES(TEXT("%S: ENTER %d@%S"), __FUNCTION__, __LINE__, __FILE__);
	
	m_iHeight = 0;
	m_iWidth = 0;
	m_lBitRate = 0;
	m_bContainsAudio = false;

	LOGZONES(TEXT("%S: EXIT %d@%S"), __FUNCTION__, __LINE__, __FILE__);
}

CAudioStats::CAudioStats()
{
	LOGZONES(TEXT("%S: ENTER %d@%S"), __FUNCTION__, __LINE__, __FILE__);
	
	m_lBitRate = 0;
	m_iHz = 0;
	m_Channels = Mono;

	LOGZONES(TEXT("%S: EXIT %d@%S"), __FUNCTION__, __LINE__, __FILE__);
}

CMediaContent::CMediaContent()
{
	LOGZONES(TEXT("%S: ENTER %d@%S"), __FUNCTION__, __LINE__, __FILE__);
	
	m_id = "";
	string m_Descripton = "";
	string m_ClipName = "";
	string m_DRMClipName = "";
	string m_Codec = "";
	m_bDRM = false;

	LOGZONES(TEXT("%S: EXIT %d@%S"), __FUNCTION__, __LINE__, __FILE__);	
}

CQualityControl::CQualityControl()
{
	LOGZONES(TEXT("%S: ENTER %d@%S"), __FUNCTION__, __LINE__, __FILE__);
	
	m_lBitRate = 0;
	m_dDuration = 0.0;
	m_iAvgFrameRate = 0;
	m_iAvgSyncOffset = 0;
	m_iDevSyncOffset = 0;
	m_lFramesDrawn = 0;
	m_lFramesDroppedInRenderer = 0;
	m_lFramesDroppedInDecoder = 0;
	m_iJitter = 0;

	m_dDurationMarginOfError = -1.0;
	m_iAvgFrameRateMarginOfError = -1;
	m_iAvgSyncOffsetMarginOfError = -1;
	m_iDevSyncOffsetMarginOfError = -1;
	m_lFramesDrawnMarginOfError = -1;
	m_lFramesDroppedInRendererMarginOfError = -1;
	m_lFramesDroppedInDecoderMarginOfError = -1;
	m_iJitterMarginOfError = -1;

	LOGZONES(TEXT("%S: EXIT %d@%S"), __FUNCTION__, __LINE__, __FILE__);
}

CMediaContent::~CMediaContent()
{
}
