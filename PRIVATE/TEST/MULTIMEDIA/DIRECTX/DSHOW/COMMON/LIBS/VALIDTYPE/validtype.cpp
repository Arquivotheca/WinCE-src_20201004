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
#include "Imaging.h"
#include "ValidType.h"

// To do
// Table for MajorTypeToGUID
// Support VIDEOINFO2
// CheckUncompressedAudio
// Handling biCompression

// Tables for converting GUIDs and 4CC codes to plain text representations and vice-versa
static TypeName MajorType[] = {
	{MEDIATYPE_Video, "MEDIATYPE_Video"},
	{MEDIATYPE_Audio, "MEDIATYPE_Audio"},
	{MEDIATYPE_Stream, "MEDIATYPE_Stream"}
};

static TypeName SubType[] = {
	{MEDIASUBTYPE_YVYU, "MEDIASUBTYPE_YVYU"},
	{MEDIASUBTYPE_YUY2, "MEDIASUBTYPE_YUY2"},
	{MEDIASUBTYPE_YV12, "MEDIASUBTYPE_YV12"},
	{MEDIASUBTYPE_YV16, "MEDIASUBTYPE_YV16"},
	{MEDIASUBTYPE_RGB1, "MEDIASUBTYPE_RGB1"},
	{MEDIASUBTYPE_RGB555, "MEDIASUBTYPE_RGB555"},
	{MEDIASUBTYPE_RGB565, "MEDIASUBTYPE_RGB565"},
	{MEDIASUBTYPE_RGB24, "MEDIASUBTYPE_RGB24"},
	{MEDIASUBTYPE_RGB32, "MEDIASUBTYPE_RGB32"},

	{MEDIASUBTYPE_WMV3, "MEDIASUBTYPE_WMV3"},
	
	{MEDIASUBTYPE_PCM, "MEDIASUBTYPE_PCM"}
};

static TypeName FormatType[] = {
	{FORMAT_VideoInfo, "FORMAT_VideoInfo"},
	{FORMAT_VideoInfo2, "FORMAT_VideoInfo2"},
	{FORMAT_WaveFormatEx, "FORMAT_WaveFormatEx"}
};

struct CompressionName {
	DWORD biCompression;
	const char* name;
};

struct WaveFormatTagName {
	WORD wFormatTag;
	const char* name;
};

static CompressionName Compression[] = {
	{BI_RGB, "BI_RGB"},
	{BI_BITFIELDS, "BI_BITFIELDS"},
	{FOURCC_YVYU, "FOURCC_YVYU"},
	{FOURCC_YUY2, "FOURCC_YUY2"},
	{FOURCC_YV12, "FOURCC_YV12"},
	{FOURCC_YV16, "FOURCC_YV16"},
	{FOURCC_WMV3, "FOURCC_WMV3"}
};

static WaveFormatTagName WaveFormatTag[] = {
	{WAVE_FORMAT_PCM, "WAVE_FORMAT_PCM"}
};


// Tables to type-check media types - the sub-type is used as the primary value to compare with
static VideoTypeDesc VideoTypes[] = {
	// Major, Minor, Format, bFixedSizeSamples, bTemporalCompression, formattype, biBitCount, biCompression, CheckFunction, TypeName

	// Uncompressed types
	{MEDIATYPE_Video, MEDIASUBTYPE_YVYU,	GUID_NULL, TRUE, FALSE, 16, FOURCC_YVYU, 0, CheckUncompressedVideo, YVYU_Name},
	{MEDIATYPE_Video, MEDIASUBTYPE_YUY2,	GUID_NULL, TRUE, FALSE, 16, FOURCC_YUY2, 0, CheckUncompressedVideo, YUY2_Name}, 
	{MEDIATYPE_Video, MEDIASUBTYPE_YV12,	GUID_NULL, TRUE, FALSE, 12, FOURCC_YV12, 0, CheckUncompressedVideo, YV12_Name},
	{MEDIATYPE_Video, MEDIASUBTYPE_YV16,	GUID_NULL, TRUE, FALSE, 16, FOURCC_YV16, 0, CheckUncompressedVideo, YV16_Name},
	{MEDIATYPE_Video, MEDIASUBTYPE_RGB1,	GUID_NULL, TRUE, FALSE, 1, BI_RGB, 8, CheckUncompressedVideo, RGB1_Name},
	{MEDIATYPE_Video, MEDIASUBTYPE_RGB4,	GUID_NULL, TRUE, FALSE, 4, BI_RGB, 64, CheckUncompressedVideo, RGB4_Name},
	{MEDIATYPE_Video, MEDIASUBTYPE_RGB8,	GUID_NULL, TRUE, FALSE, 8, BI_RGB, 1024, CheckUncompressedVideo, RGB8_Name},
	{MEDIATYPE_Video, MEDIASUBTYPE_RGB555,	GUID_NULL, TRUE, FALSE, 16, BI_RGB, 0, CheckUncompressedVideo, RGB555_Name},
	{MEDIATYPE_Video, MEDIASUBTYPE_RGB565,	GUID_NULL, TRUE, FALSE, 16, BI_BITFIELDS, 12, CheckUncompressedVideo, RGB565_Name},
	{MEDIATYPE_Video, MEDIASUBTYPE_RGB24,	GUID_NULL, TRUE, FALSE, 24, BI_RGB, 0, CheckUncompressedVideo, RGB24_Name},
	{MEDIATYPE_Video, MEDIASUBTYPE_RGB32,	GUID_NULL, TRUE, FALSE, 32, BI_RGB, 0, CheckUncompressedVideo, RGB32_Name},

	// WM types
	{MEDIATYPE_Video, MEDIASUBTYPE_WMV1,   GUID_NULL, FALSE, TRUE, 0, FOURCC_WMV1, 0, NULL, WMV1_Name},
	{MEDIATYPE_Video, MEDIASUBTYPE_WMV1,   GUID_NULL, FALSE, TRUE, 0, FOURCC_wmv1, 0, NULL, WMV1_Name},
	{MEDIATYPE_Video, MEDIASUBTYPE_WMV2,   GUID_NULL, FALSE, TRUE, 0, FOURCC_WMV2, 0, NULL, WMV2_Name},
	{MEDIATYPE_Video, MEDIASUBTYPE_WMV2,   GUID_NULL, FALSE, TRUE, 0, FOURCC_wmv2, 0, NULL, WMV2_Name},
	{MEDIATYPE_Video, MEDIASUBTYPE_WMV3,   GUID_NULL, FALSE, TRUE, 0, FOURCC_WMV3, 0, NULL, WMV3_Name},
	{MEDIATYPE_Video, MEDIASUBTYPE_WMV3,   GUID_NULL, FALSE, TRUE, 0, FOURCC_wmv3, 0, NULL, WMV3_Name},

	// MPEG-1 types
	{MEDIATYPE_Video, MEDIASUBTYPE_MPEG1Packet, FORMAT_MPEGVideo, FALSE, TRUE, 0, 0, 0, NULL, MPEG1VideoPacket_Name},
	{MEDIATYPE_Video, MEDIASUBTYPE_MPEG1Payload, FORMAT_MPEGVideo, FALSE, TRUE, 0, 0, 0, NULL, MPEG1VideoPayload_Name},
	{MEDIATYPE_Stream, MEDIASUBTYPE_MPEG1Video, GUID_NULL, FALSE, TRUE, 0, 0, 0, NULL, MPEG1VideoStream_Name},

	// MPEG-2 types
	{MEDIATYPE_Video, MEDIASUBTYPE_MPEG2_VIDEO, FORMAT_MPEG2Video, FALSE, TRUE, 0, 0, 0, NULL, MPEG2Video_Name},
	{MEDIATYPE_MPEG2_PES, MEDIASUBTYPE_MPEG2_VIDEO, FORMAT_MPEG2Video, FALSE, TRUE, 0, 0, 0, NULL, MPEG2VideoPES_Name}

};

static SystemTypeDesc SystemTypes[] = {
	// MPEG-1
	{MEDIATYPE_Stream, MEDIASUBTYPE_MPEG1System, GUID_NULL, 0, MPEG1SystemStream_Name},
	{MEDIATYPE_Stream, MEDIASUBTYPE_MPEG1VideoCD, GUID_NULL, 0, MPEG1VideoCDSystemStream_Name},

	// MPEG-2
	{MEDIATYPE_Stream, MEDIASUBTYPE_MPEG2_TRANSPORT, GUID_NULL, 0, MPEG2TransportStream_Name},
	{MEDIATYPE_Stream, MEDIASUBTYPE_MPEG2_PROGRAM, GUID_NULL, 0, MPEG2ProgramStream_Name}
#if 0
	{MEDIATYPE_Stream, MEDIASUBTYPE_MPEG2_TRANSPORT_STRIDE, GUID_NULL, 0, MPEG2TransportStridedStream_Name}
#endif
};

static AudioTypeDesc AudioTypes[] = {
	// Major, Minor, Format, bFixedSizeSamples, bTemporalCompression, CheckFunction, TypeName
	{MEDIATYPE_Audio, MEDIASUBTYPE_PCM,		FORMAT_WaveFormatEx, FALSE, FALSE,	CheckUncompressedAudio, PCM_Name},
	{MEDIATYPE_Stream, MEDIASUBTYPE_WAVE,   FORMAT_WaveFormatEx, FALSE, FALSE,	NULL,	WAVE_Name},
	{MEDIATYPE_Stream, MEDIASUBTYPE_AU,		FORMAT_WaveFormatEx, FALSE, FALSE,	NULL,	AU_Name},
	{MEDIATYPE_Stream, MEDIASUBTYPE_AIFF,   FORMAT_WaveFormatEx, FALSE, FALSE,	NULL,	AIFF_Name},
	// So for MPEG1 even if the format type is FORMAT_WaveFormatEx, the actual format is expected to be MPEG1WAVEFORMAT
	{MEDIATYPE_Audio, MEDIASUBTYPE_MPEGLAYER3,   FORMAT_WaveFormatEx, FALSE, TRUE,	NULL,	MPEG1Layer3_Name},
	{MEDIATYPE_Audio, MEDIASUBTYPE_MPEG1Packet,   FORMAT_WaveFormatEx, FALSE, TRUE,	NULL,	MPEG1AudioPacket_Name},
	{MEDIATYPE_Audio, MEDIASUBTYPE_MPEG1AudioPayload,   FORMAT_WaveFormatEx, FALSE, TRUE,	NULL,	MPEG1AudioPayload_Name},
	// And no format for this
	{MEDIATYPE_Audio, MEDIASUBTYPE_MPEG1Audio,   GUID_NULL, FALSE, TRUE,	NULL,	MPEG1AudioStream_Name},
	// WMA types
	{MEDIATYPE_Audio, WMMEDIASUBTYPE_WMAudioV7,   FORMAT_WaveFormatEx, FALSE, FALSE,	NULL,	WMAv7_Name},

	// MPEG-2 types
	{MEDIATYPE_Audio, MEDIASUBTYPE_DOLBY_AC3, FORMAT_WaveFormatEx, FALSE, TRUE, NULL, DOLBYAC3_Name},
	{MEDIATYPE_MPEG2_PES, MEDIASUBTYPE_DOLBY_AC3, FORMAT_WaveFormatEx, FALSE, TRUE, NULL, DOLBYAC3PES_Name},

	{MEDIATYPE_Audio, MEDIASUBTYPE_MPEG2_AUDIO, FORMAT_WaveFormatEx, FALSE, TRUE, NULL, MPEG2Audio_Name},
	{MEDIATYPE_MPEG2_PES, MEDIASUBTYPE_MPEG2_AUDIO, FORMAT_WaveFormatEx, FALSE, TRUE, NULL, MPEG2AudioPES_Name},

	{MEDIATYPE_Audio, MEDIASUBTYPE_DVD_LPCM_AUDIO, FORMAT_WaveFormatEx, FALSE, TRUE, NULL, DVDLPCMAudio_Name},
	{MEDIATYPE_MPEG2_PES, MEDIASUBTYPE_DVD_LPCM_AUDIO, FORMAT_WaveFormatEx, FALSE, TRUE, NULL, DVDLPCMAudioPES_Name}
};

static const int nSystemTypes = sizeof(SystemTypes)/sizeof(SystemTypes[0]);
static const int nVideoTypes = sizeof(VideoTypes)/sizeof(VideoTypes[0]);
static const int nAudioTypes = sizeof(AudioTypes)/sizeof(AudioTypes[0]);

bool IsValidUncompressedVideoType(const AM_MEDIA_TYPE* pMediaType)
{
	if ((pMediaType->majortype != MEDIATYPE_Video) ||
		((pMediaType->formattype != FORMAT_VideoInfo) && (pMediaType->formattype != FORMAT_VideoInfo2)))
		return false;

	if (pMediaType->cbFormat < sizeof(VIDEOINFOHEADER))
		return false;

	VIDEOINFOHEADER* pVih = (VIDEOINFOHEADER*) pMediaType->pbFormat;
	
	if (!pVih)
		return false;

	bool valid = false;
	for (int i = 0; i < nVideoTypes; i++)
	{
        if ((pMediaType->subtype == VideoTypes[i].subtype) &&
			// BUGBUG: biCompression has flags in it which prevent direct comparison - rely on subtype
			// (pVih->bmiHeader.biCompression == VideoTypes[i].compression) &&
			(pVih->bmiHeader.biBitCount == VideoTypes[i].biBitCount))
		{
			valid = true;
			break;
		}
	}

	if (!valid)
		return false;
	return IsValidVideoInfoHeader(pVih);
}

bool IsValidWMV3Type(const AM_MEDIA_TYPE* pMediaType)
{
	if ((pMediaType->majortype != MEDIATYPE_Video) ||
		(pMediaType->formattype != FORMAT_VideoInfo) || 
		(pMediaType->subtype != MEDIASUBTYPE_WMV3))
		return false;

	if (pMediaType->cbFormat < sizeof(VIDEOINFOHEADER))
		return false;

	VIDEOINFOHEADER* pVih = (VIDEOINFOHEADER*) pMediaType->pbFormat;
	
	if (!pVih)
		return false;
	
	if (!IsValidVideoInfoHeader(pVih))
		return false;
	return IsValidEncVideoInfoHeader(pVih);
}


bool IsValidWaveFormat(const AM_MEDIA_TYPE* pMediaType)
{
	if ((pMediaType->majortype != MEDIATYPE_Audio) ||
		((pMediaType->subtype != MEDIASUBTYPE_PCM) && (pMediaType->subtype != GUID_NULL)) ||
		(pMediaType->formattype != FORMAT_WaveFormatEx))
		return false;

	if (!pMediaType->cbFormat || !pMediaType->pbFormat)
		return false;

	if (pMediaType->cbFormat < sizeof (WAVEFORMATEX))
		return false;

	WAVEFORMATEX *pWfex = (WAVEFORMATEX*)pMediaType->pbFormat;

	if ((pWfex->nChannels <= 0) || (pWfex->nSamplesPerSec <= 0) || (pWfex->nAvgBytesPerSec <= 0) || !pWfex->nBlockAlign)
		return false;
	
	// Check for the relation between nSamplesPerSec, nChannels, nAvgBytesPerSec,nBlockAlign
	if (pWfex->nBlockAlign != pWfex->wBitsPerSample*pWfex->nChannels/8)
		return false;
	
	if (pWfex->nAvgBytesPerSec != pWfex->nBlockAlign*pWfex->nSamplesPerSec)
		return false;
	
	return true;
}

bool IsValidWaveFormat(const WAVEFORMATEX* pWfex)
{
	if (!pWfex || (pWfex->nChannels <= 0) || (pWfex->nSamplesPerSec <= 0) || (pWfex->nAvgBytesPerSec <= 0) || !pWfex->nBlockAlign)
		return false;
	// Check for the relation between nSamplesPerSec, nChannels, nAvgBytesPerSec,nBlockAlign
	if (pWfex->nBlockAlign != pWfex->wBitsPerSample*pWfex->nChannels/8)
		return false;
	if (pWfex->nAvgBytesPerSec != pWfex->nBlockAlign*pWfex->nSamplesPerSec)
		return false;
	return true;
}

bool IsEqualWaveFormat(const WAVEFORMATEX* pWfex, const WAVEFORMATEX* pRefWfex)
{
	if ((pWfex->wFormatTag != pRefWfex->wFormatTag) ||
		(pWfex->nChannels != pRefWfex->nChannels) ||
		(pWfex->nBlockAlign != pRefWfex->nBlockAlign) ||
		(pWfex->wBitsPerSample != pRefWfex->wBitsPerSample))
		return false;

	// Accept this format
	return true;
}

bool IsValidVideoInfoHeader(const VIDEOINFOHEADER* pVih)
{
    // a height less than 0 indicates a bottom up bitmap
    if (!pVih || pVih->bmiHeader.biWidth < 0)
    {
        return false;
    }

    return true;
}

bool IsValidEncVideoInfoHeader(const VIDEOINFOHEADER* pVih)
{
	// To do: check that the frame rate and bitrate are in an acceptable range?
	if (!pVih ||
        (!pVih->dwBitRate) ||
		(!pVih->AvgTimePerFrame))
    {
        return false;
    }

    return true;
}


bool IsEqualVideoInfoHeaderNonStrict(const VIDEOINFOHEADER* pVih, const VIDEOINFOHEADER* pRefVih)
{
	if ((pVih->bmiHeader.biPlanes != pRefVih->bmiHeader.biPlanes) ||
		(pVih->bmiHeader.biBitCount != pRefVih->bmiHeader.biBitCount))
		//BUGBUG: since equality of biCompression may not work because of the rotation flags
		//(pVih->bmiHeader.biCompression != pRefVih->bmiHeader.biCompression)

		return false;

	// Accept this format
	return true;
}

bool IsEqualEncVideoInfoHeaderNonStrict(const VIDEOINFOHEADER* pVih, const VIDEOINFOHEADER* pRefVih)
{
	// Only check the bitrate and frame rate
	if ((pVih->dwBitRate != pRefVih->dwBitRate) ||
		(pVih->AvgTimePerFrame != pRefVih->AvgTimePerFrame))
		return false;

	// Accept this format
	return true;
}

HRESULT CheckUncompressedAudio(int index, const AM_MEDIA_TYPE* pMediaType)
{
	return S_FALSE;
}

HRESULT CheckUncompressedVideo(int index, const AM_MEDIA_TYPE* pMediaType)
{
	VIDEOINFOHEADER* pVih = NULL;
	VIDEOINFOHEADER2* pVih2 = NULL;

	// If the format size is 0, there is nothing we can check for
	if (!pMediaType->cbFormat)
		return S_OK;

	// Check if the format is VIDEOINFO or VIDEOINFO2 - cant be anything else
	if (pMediaType->formattype == FORMAT_VideoInfo) {
		pVih = (VIDEOINFOHEADER*) pMediaType->pbFormat;
		// Check the minimum size which must be VIDEOINFOHEADER
		if (pMediaType->cbFormat < sizeof(VIDEOINFOHEADER))
			return S_FALSE;
	}
	else if (pMediaType->formattype == FORMAT_VideoInfo2) {
		pVih = (VIDEOINFOHEADER*) pMediaType->pbFormat;
		pVih2 = (VIDEOINFOHEADER2*) pMediaType->pbFormat;
		// Check the minimum size which must be VIDEOINFOHEADER2
		if (pMediaType->cbFormat < sizeof(VIDEOINFOHEADER2))
			return S_FALSE;
	}
	else {
		return S_FALSE;
	}

	// source and target rectangles - make sure that none of the values are -ve (even for top & bottom?)
	if ((pVih->rcSource.left < 0) || (pVih->rcSource.right < 0) || (pVih->rcSource.bottom < 0) || (pVih->rcSource.top < 0))
		return S_FALSE;

	if ((pVih->rcTarget.left < 0) || (pVih->rcTarget.right < 0) || (pVih->rcTarget.bottom < 0) || (pVih->rcTarget.top < 0))
		return S_FALSE;

#ifdef UNDER_CE 
	// Check the biCompression flag - mask away the source prerotate bit
	if ((pVih->bmiHeader.biCompression & (~BI_SRCPREROTATE)) != (VideoTypes[index].biCompression & (~BI_SRCPREROTATE)))
		return S_FALSE;
#endif

	// Check the bit count flag
	if (pVih->bmiHeader.biBitCount != VideoTypes[index].biBitCount)
		return S_FALSE;

	// Check the size of the BMI header
	if (pVih->bmiHeader.biSize < sizeof(BITMAPINFOHEADER))
		return S_FALSE;

	// Check the width
	if (pVih->bmiHeader.biWidth <= 0)
		return S_FALSE;

	// 0 Height is legal
	if (pVih->bmiHeader.biHeight == 0)
		return S_FALSE;

	// Check the planes - according to documentation this should be 1 - subject to change?
	// ms-help://MS.VSCC.2003/MS.MSDNQTR.2003FEB.1033/wceui40/htm/cerefBITMAPINFOHEADER.htm
	if (pVih->bmiHeader.biPlanes != 1)
		return S_FALSE;

	// Since there are fields that we dont check such as biSizeImage, the color table or palette return E_FAIL
	return E_FAIL;
}

// Currently decide if this is a video type or not based on the major and sub-type
bool IsVideoType(const AM_MEDIA_TYPE* pMediaType)
{
	for (int i = 0; i < nVideoTypes; i++)
	{
        if ((pMediaType->majortype == VideoTypes[i].majortype) &&
			(pMediaType->subtype == VideoTypes[i].subtype))
		{
			return true;
		}
	}
	return false;
}

bool IsAudioType(const AM_MEDIA_TYPE* pMediaType)
{
	for (int i = 0; i < nAudioTypes; i++)
	{
        if ((pMediaType->majortype == AudioTypes[i].majortype) &&
			(pMediaType->subtype == AudioTypes[i].subtype))
		{
			return true;
		}
	}
	return false;
}

// Currently decide if this is a system type or not based on the major and sub-type
bool IsSystemType(const AM_MEDIA_TYPE* pMediaType)
{
	for (int i = 0; i < nSystemTypes; i++)
	{
        if ((pMediaType->majortype == SystemTypes[i].majortype) &&
			(pMediaType->subtype == SystemTypes[i].subtype))
		{
			return true;
		}
	}
	return false;
}

const char* GetVideoTypeName(const AM_MEDIA_TYPE* pMediaType)
{
	for (int i = 0; i < nVideoTypes; i++)
	{
        if ((pMediaType->majortype == VideoTypes[i].majortype) &&
			(pMediaType->subtype == VideoTypes[i].subtype))
		{
			return VideoTypes[i].name;
		}
	}
	return NULL;
}

const char* GetAudioTypeName(const AM_MEDIA_TYPE* pMediaType)
{
	for (int i = 0; i < nAudioTypes; i++)
	{
        if ((pMediaType->majortype == AudioTypes[i].majortype) &&
			(pMediaType->subtype == AudioTypes[i].subtype))
		{
			return AudioTypes[i].name;
		}
	}
	return NULL;
}

// Currently decide if this is a system type or not based on the major and sub-type
const char* GetSystemTypeName(const AM_MEDIA_TYPE* pMediaType)
{
	for (int i = 0; i < nSystemTypes; i++)
	{
        if ((pMediaType->majortype == SystemTypes[i].majortype) &&
			(pMediaType->subtype == SystemTypes[i].subtype))
		{
			return SystemTypes[i].name;
		}
	}
	return NULL;
}

const char* GetTypeName(const AM_MEDIA_TYPE* pMediaType)
{
	if (IsVideoType(pMediaType))
	{
		return GetVideoTypeName(pMediaType);
	}
	else if (IsAudioType(pMediaType))
	{
		return GetAudioTypeName(pMediaType);
	}
	else if (IsSystemType(pMediaType)) {
		return GetSystemTypeName(pMediaType);
	}
	return NULL;
}

VideoTypeDesc* GetVideoTypeDesc(DWORD biCompression, DWORD biBitCount)
{
	for (int i = 0; i < nVideoTypes; i++)
	{
        if ((biCompression == VideoTypes[i].biCompression) &&
			(biBitCount == VideoTypes[i].biBitCount))
		{
			return &VideoTypes[i];
		}
	}
	return NULL;
}

HRESULT CheckVideoType(const AM_MEDIA_TYPE* pMediaType)
{
	HRESULT hr = E_FAIL;

	for (int i = 0; i < nVideoTypes; i++)
	{
        if (pMediaType->subtype == VideoTypes[i].subtype)
		{

			// If we have a match - make consistency checks for temporal compression and fixed size samples
			if ((pMediaType->bTemporalCompression != VideoTypes[i].bTemporalCompression) ||
				(pMediaType->bFixedSizeSamples != VideoTypes[i].bFixedSizeSamples))
				return S_FALSE;

			// Make consistency checks for formattype and cbformat
			if (pMediaType->cbFormat < 0)
				return S_FALSE;
			else if (pMediaType->cbFormat)
			{
				// If there is a positive format block then there has to be a corresponding format type
				if ((pMediaType->formattype == GUID_NULL) || (pMediaType->formattype == FORMAT_None))
					return S_FALSE;

				if (VideoTypes[i].checkfn)
					hr = VideoTypes[i].checkfn(i, pMediaType);
			}
			else {
				if ((pMediaType->formattype != GUID_NULL) &&  (pMediaType->formattype != FORMAT_None))
					return S_FALSE;
			}
		}
	}

	return hr;
}

HRESULT CheckAudioType(const AM_MEDIA_TYPE* pMediaType)
{
	HRESULT hr = E_FAIL;

	for (int i = 0; i < nAudioTypes; i++)
	{
        if (pMediaType->subtype == AudioTypes[i].subtype)
		{

			// Make consistency checks for formattype and cbformat
			if (pMediaType->cbFormat < 0)
				return S_FALSE;
			else if (pMediaType->cbFormat)
			{
				// If there is a positive format block then there has to be a corresponding format type
				if ((pMediaType->formattype == GUID_NULL) || (pMediaType->formattype == FORMAT_None))
					return S_FALSE;

				if (AudioTypes[i].checkfn)
					hr = AudioTypes[i].checkfn(i, pMediaType);
			}
			else {
				if ((pMediaType->formattype != GUID_NULL) &&  (pMediaType->formattype != FORMAT_None))
					return S_FALSE;
			}
		}
	}

	return hr;
}
// This is a method that will make some basic sanity tests and will return
// S_FALSE if it finds something amiss
// E_FAIL if it finds that it doesn't recognize the type or make a complete determination
// if it recognizes the type and thinks everything is OK
HRESULT CheckMediaTypeValidity(const AM_MEDIA_TYPE* pMediaType)
{
	if (!pMediaType)
		return E_INVALIDARG;

	if (pMediaType->majortype == MEDIATYPE_Video)
		return CheckVideoType(pMediaType);
	else if (pMediaType->majortype == MEDIATYPE_Audio)
		return CheckAudioType(pMediaType);

	return E_FAIL;
}

bool IsEqualMediaType(const AM_MEDIA_TYPE* pMediaType1, const AM_MEDIA_TYPE* pMediaType2)
{
	CMediaType mt1(*pMediaType1);
	CMediaType mt2(*pMediaType2);
	return ((mt1 == mt2) == TRUE);
}

HRESULT MajorTypeToGUID(const char* szMajorType, GUID* pGuid)
{
	HRESULT hr = S_OK;

	int nTypes = sizeof(MajorType)/sizeof(MajorType[0]);
	for(int i = 0; i < nTypes; i++)
	{
		if (!strcmp(szMajorType, MajorType[i].name))
		{
			*pGuid = MajorType[i].guid;
			break;
		}
	}
	
	if (i == nTypes)
		hr = E_FAIL;
	
	return hr;
}

HRESULT SubTypeToGUID(const char* szSubType, GUID* pGuid)
{
	HRESULT hr = S_OK;

	int nTypes = sizeof(SubType)/sizeof(SubType[0]);
	for(int i = 0; i < nTypes; i++)
	{
		if (!strcmp(szSubType, SubType[i].name))
		{
			*pGuid = SubType[i].guid;
			break;
		}
	}
	
	if (i == nTypes)
		hr = E_FAIL;
	
	return hr;
}


HRESULT FormatTypeToGUID(const char* szFormatType, GUID* pGuid)
{
	HRESULT hr = S_OK;

	int nTypes = sizeof(FormatType)/sizeof(FormatType[0]);
	for(int i = 0; i < nTypes; i++)
	{
		if (!strcmp(szFormatType, FormatType[i].name))
		{
			*pGuid = FormatType[i].guid;
			break;
		}
	}
	
	if (i == nTypes)
		hr = E_FAIL;
	
	return hr;
}

HRESULT CompressionToValue(const char* szCompression, DWORD *pCompression)
{
	HRESULT hr = S_OK;

	int nTypes = sizeof(Compression)/sizeof(Compression[0]);
	for(int i = 0; i < nTypes; i++)
	{
		if (!strcmp(szCompression, Compression[i].name))
		{
			*pCompression = Compression[i].biCompression;
			break;
		}
	}
	
	if (i == nTypes)
		hr = E_FAIL;
	
	return hr;
}

HRESULT WaveFormatTagToValue(const char* szFormatTag, WORD *pFormatTag)
{
	HRESULT hr = S_OK;

	int nTypes = sizeof(WaveFormatTag)/sizeof(WaveFormatTag[0]);
	for(int i = 0; i < nTypes; i++)
	{
		if (!strcmp(szFormatTag, WaveFormatTag[i].name))
		{
			*pFormatTag = WaveFormatTag[i].wFormatTag;
			break;
		}
	}
	
	if (i == nTypes)
		hr = E_FAIL;
	
	return hr;
}

HRESULT GetColorTable(const VIDEOINFO* pVideoInfo, BYTE** ppColorTable, DWORD *pColorTableSize)
{
	// BUGBUG: this is not implemented yet
	return 0;
}

HRESULT GetColorTable(const VIDEOINFO2* pVideoInfo2, BYTE** ppColorTable, DWORD *pColorTableSize)
{
	// BUGBUG: this is not implemented yet
	return 0;
}

HRESULT ConstructUncompressedVideoType(AM_MEDIA_TYPE* pMediaType, VIDEOINFO* pVideoInfo)
{
	HRESULT hr = S_OK;

	// Get the type descriptor corresponding to videoinfo object
	VideoTypeDesc* pVideoTypeDesc = GetVideoTypeDesc(pVideoInfo->bmiHeader.biCompression, pVideoInfo->bmiHeader.biBitCount);
	if (!pVideoTypeDesc)
		return E_FAIL;

	// Set the values in the media type
	pMediaType->majortype = pVideoTypeDesc->majortype;
	pMediaType->subtype = pVideoTypeDesc->subtype;
	pMediaType->bFixedSizeSamples = pVideoTypeDesc->bFixedSizeSamples;
    pMediaType->bTemporalCompression = FALSE;
	// Calculate the size of the sample
    pMediaType->lSampleSize = AllocateBitmapBuffer(pVideoInfo->bmiHeader.biWidth, pVideoInfo->bmiHeader.biHeight, pVideoInfo->bmiHeader.biBitCount, pVideoInfo->bmiHeader.biCompression, NULL);
    pMediaType->formattype = pVideoTypeDesc->formattype;
    pMediaType->pUnk = NULL;

	// Calculate the color table size
	DWORD colorTableSize = pVideoTypeDesc->colorTableSize;

	// Allocate the format block and set the values
	DWORD cbFormat = sizeof(VIDEOINFOHEADER) + colorTableSize;
    VIDEOINFO* pVI = (VIDEOINFO*)new BYTE[cbFormat];
	if (!pVI)
		return E_OUTOFMEMORY;

	pVI->rcSource = pVideoInfo->rcSource;
	pVI->rcTarget = pVideoInfo->rcTarget;
	pVI->dwBitRate = pVideoInfo->dwBitRate;
	pVI->dwBitErrorRate = pVideoInfo->dwBitErrorRate;
	pVI->AvgTimePerFrame = pVideoInfo->AvgTimePerFrame;
	pVI->bmiHeader = pVideoInfo->bmiHeader;
	if (colorTableSize)
		memcpy(&pVI->bmiColors[0], &pVideoInfo->bmiColors[0], colorTableSize);

	// Set the format block
	pMediaType->pbFormat = (BYTE*)pVI;
	pMediaType->formattype = FORMAT_VideoInfo;
	pMediaType->cbFormat = cbFormat;

	return hr;
}

HRESULT ConstructUncompressedVideoType(AM_MEDIA_TYPE* pMediaType, VIDEOINFO2* pVideoInfo)
{
	HRESULT hr = S_OK;

	// Get the type descriptor corresponding to videoinfo object
	VideoTypeDesc* pVideoTypeDesc = GetVideoTypeDesc(pVideoInfo->bmiHeader.biCompression, pVideoInfo->bmiHeader.biBitCount);
	if (!pVideoTypeDesc)
		return E_FAIL;

	// Set the values in the media type
	pMediaType->majortype = pVideoTypeDesc->majortype;
	pMediaType->subtype = pVideoTypeDesc->subtype;
	pMediaType->bFixedSizeSamples = pVideoTypeDesc->bFixedSizeSamples;
    pMediaType->bTemporalCompression = FALSE;
	// Calculate the size of the sample
    pMediaType->lSampleSize = AllocateBitmapBuffer(pVideoInfo->bmiHeader.biWidth, pVideoInfo->bmiHeader.biHeight, pVideoInfo->bmiHeader.biBitCount, pVideoInfo->bmiHeader.biCompression, NULL);
    pMediaType->formattype = pVideoTypeDesc->formattype;
    pMediaType->pUnk = NULL;

	// Calculate the color table size
	DWORD colorTableSize = pVideoTypeDesc->colorTableSize;

	// Allocate the format block and set the values
	DWORD cbFormat = sizeof(VIDEOINFOHEADER) + colorTableSize;
    VIDEOINFO* pVI = (VIDEOINFO*)new BYTE[cbFormat];
	if (!pVI)
		return E_OUTOFMEMORY;

	pVI->rcSource = pVideoInfo->rcSource;
	pVI->rcTarget = pVideoInfo->rcTarget;
	pVI->dwBitRate = pVideoInfo->dwBitRate;
	pVI->dwBitErrorRate = pVideoInfo->dwBitErrorRate;
	pVI->AvgTimePerFrame = pVideoInfo->AvgTimePerFrame;
	pVI->bmiHeader = pVideoInfo->bmiHeader;
	if (colorTableSize)
		memcpy(&pVI->bmiColors[0], &pVideoInfo->bmiColors[0], colorTableSize);

	// Set the format block
	pMediaType->pbFormat = (BYTE*)pVI;
	pMediaType->formattype = FORMAT_VideoInfo;
	pMediaType->cbFormat = cbFormat;

	return hr;
}

#ifdef UNDER_CE
// Make GUID definitions
#include <initguid.h>
// The WMV3 type needs to be exported by the product. Until then make a definition.
DEFINE_GUID(MEDIASUBTYPE_WMV3,
0x33564d57, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
#endif
