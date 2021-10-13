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
#include <imaging.h>

#define countof(x) (sizeof(x)/sizeof(x[0]))

struct TypeName {
	GUID guid;
	const TCHAR* name;
};

static TypeName MajorType[] = {
	{MEDIATYPE_Video, TEXT("MEDIATYPE_Video")},
	{MEDIATYPE_VideoBuffered, TEXT("MEDIATYPE_VideoBuffered")},
	{MEDIATYPE_Audio, TEXT("MEDIATYPE_Audio")},
	{MEDIATYPE_AudioBuffered, TEXT("MEDIATYPE_AudioBuffered")},
	{MEDIATYPE_Stream, TEXT("MEDIATYPE_Stream")}
};

static TypeName SubType[] = {
	{MEDIASUBTYPE_YVYU, TEXT("MEDIASUBTYPE_YVYU")},
	{MEDIASUBTYPE_YUY2, TEXT("MEDIASUBTYPE_YUY2")},
	{MEDIASUBTYPE_YV12, TEXT("MEDIASUBTYPE_YV12")},
	{MEDIASUBTYPE_YV16, TEXT("MEDIASUBTYPE_YV16")},
	{MEDIASUBTYPE_UYVY, TEXT("MEDIASUBTYPE_UYVY")},
	{MEDIASUBTYPE_RGB1, TEXT("MEDIASUBTYPE_RGB1")},
	{MEDIASUBTYPE_RGB555, TEXT("MEDIASUBTYPE_RGB555")},
	{MEDIASUBTYPE_RGB565, TEXT("MEDIASUBTYPE_RGB565")},
	{MEDIASUBTYPE_RGB24, TEXT("MEDIASUBTYPE_RGB24")},
	{MEDIASUBTYPE_RGB32, TEXT("MEDIASUBTYPE_RGB32")},
	{MEDIASUBTYPE_PCM, TEXT("MEDIASUBTYPE_PCM")}
};

static TypeName FormatType[] = {
	{FORMAT_VideoInfo, TEXT("FORMAT_VideoInfo")},
	{FORMAT_VideoInfo2, TEXT("FORMAT_VideoInfo2")},
	{FORMAT_WaveFormatEx, TEXT("FORMAT_WaveFormatEx")}
};

HRESULT GetFormatInfo(TCHAR *tszFormatInfo, UINT MaxStringLength, AM_MEDIA_TYPE *pMediaType)
{
    const TCHAR *tszUnknown = TEXT("Unknown");
    const TCHAR *tszMediaType = tszUnknown;
    const TCHAR *tszMediaSubType = tszUnknown;
    const TCHAR *tszFormatType = tszUnknown;
    int index;
    HRESULT hr = E_FAIL;

    if(tszFormatInfo)
    {
        if(pMediaType)
        {
            for(index = 0; index < countof(MajorType); index++)
            {
                if(MajorType[index].guid == pMediaType->majortype)
                    tszMediaType = MajorType[index].name;
            }

            for(index = 0; index < countof(SubType); index++)
            {
                if(SubType[index].guid == pMediaType->subtype)
                    tszMediaSubType = SubType[index].name;
            }

            for(index = 0; index < countof(FormatType); index++)
            {
                if(FormatType[index].guid == pMediaType->formattype)
                    tszFormatType = FormatType[index].name;
            }

            hr = S_OK;
        }

        _sntprintf(tszFormatInfo, MaxStringLength, TEXT("MajorType:%s; SubType:%s; FormatType:%s"), tszMediaType, tszMediaSubType, tszFormatType);
    }

    return hr;
}

