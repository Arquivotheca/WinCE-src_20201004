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
#include <stdafx.h>
#include "MPEGPinUtils.h"

using namespace MSDvr;

CMPEGPSMediaTypeDescription::CMPEGPSMediaTypeDescription(CMediaType &rcMediaType)
	: CMediaTypeDescription(rcMediaType)
{
	m_fIsAVStream = true;
	// TODO:  figure out what this should use for frequency and size
	m_uMinimumMediaSampleBufferSize = 0;
	m_uMaximumSamplesPerSecond = 10;  // estimated based on 6 Mbs maximum supported bitrate
}

CMPEGPSMediaTypeDescription::~CMPEGPSMediaTypeDescription()
{
}

CMPEGVideoMediaTypeDescription::CMPEGVideoMediaTypeDescription(CMediaType &rcMediaType)
	: CMediaTypeDescription(rcMediaType)
{
	m_fIsVideo = true;
	// TODO:  figure out what this should use for frequency and size
	m_uMinimumMediaSampleBufferSize = 0;
	m_uMaximumSamplesPerSecond = 70;
}

CMPEGVideoMediaTypeDescription::~CMPEGVideoMediaTypeDescription()
{
}

CDVDAC3MediaTypeDescription::CDVDAC3MediaTypeDescription(CMediaType &rcMediaType)
	: CMediaTypeDescription(rcMediaType)
{
	m_fIsAudio = true;
	// TODO:  figure out what this should use for frequency and size
	m_uMinimumMediaSampleBufferSize = 0;
	m_uMaximumSamplesPerSecond = 70;
}

CDVDAC3MediaTypeDescription::~CDVDAC3MediaTypeDescription()
{
}

CMediaTypeDescription *CMPEGInputMediaTypeAnalyzer::AnalyzeMediaType(CMediaType &rcMediaType) const
{
	CMediaTypeDescription *pcMediaTypeDescription = NULL;

	if (rcMediaType.Type() && IsEqualGUID(*rcMediaType.Type(), MEDIATYPE_Stream) &&
		rcMediaType.Subtype() && IsEqualGUID(*rcMediaType.Subtype(), MEDIASUBTYPE_MPEG2_PROGRAM))
		pcMediaTypeDescription = new CMPEGPSMediaTypeDescription(rcMediaType);
	else if (rcMediaType.Type() && IsEqualGUID(*rcMediaType.Type(), MEDIATYPE_AUXLine21Data) &&
		rcMediaType.Subtype() && IsEqualGUID(*rcMediaType.Subtype(), MEDIASUBTYPE_Line21_BytePair))
		pcMediaTypeDescription = new CMPEGVBIMediaTypeDescription(rcMediaType);

	ASSERT(pcMediaTypeDescription);
	return pcMediaTypeDescription;
}

CMediaTypeDescription *CMPEGOutputMediaTypeAnalyzer::AnalyzeMediaType(CMediaType &rcMediaType) const
{
	CMediaTypeDescription *pcMediaTypeDescription = NULL;

	if (rcMediaType.Type() && IsEqualGUID(*rcMediaType.Type(), MEDIATYPE_Stream) &&
		rcMediaType.Subtype() && IsEqualGUID(*rcMediaType.Subtype(), MEDIASUBTYPE_MPEG2_PROGRAM))
		pcMediaTypeDescription = new CMPEGPSMediaTypeDescription(rcMediaType);
	else if (rcMediaType.Type() && IsEqualGUID(*rcMediaType.Type(), MEDIATYPE_AUXLine21Data) &&
		rcMediaType.Subtype() && IsEqualGUID(*rcMediaType.Subtype(), MEDIASUBTYPE_Line21_BytePair))
		pcMediaTypeDescription = new CMPEGVBIMediaTypeDescription(rcMediaType);
	else if (rcMediaType.Type() && IsEqualGUID(*rcMediaType.Type(), MEDIATYPE_DVD_ENCRYPTED_PACK) &&
		rcMediaType.Subtype() && IsEqualGUID(*rcMediaType.Subtype(), MEDIASUBTYPE_MPEG2_VIDEO))
		pcMediaTypeDescription = new CMPEGVideoMediaTypeDescription(rcMediaType);
	else if (rcMediaType.Type() && IsEqualGUID(*rcMediaType.Type(), MEDIATYPE_DVD_ENCRYPTED_PACK) &&
		rcMediaType.Subtype() && IsEqualGUID(*rcMediaType.Subtype(), MEDIASUBTYPE_DOLBY_AC3))
		pcMediaTypeDescription = new CDVDAC3MediaTypeDescription(rcMediaType);

	ASSERT(pcMediaTypeDescription);
	return pcMediaTypeDescription;
}


CMPEGVBIMediaTypeDescription::CMPEGVBIMediaTypeDescription(CMediaType &rcMediaType)
	: CMediaTypeDescription(rcMediaType)
{
	// TODO:  figure out what this should use for frequency and size
	m_uMinimumMediaSampleBufferSize = 0;
	m_uMaximumSamplesPerSecond = 30;
}
