//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#pragma once

#include <windows.h>
#include <imaging.h>
#include <tux.h>

namespace ImageSinkTests
{
    enum SinkParamFlags
    {
        spfImageCodecInfo = 1,
        spfSize = 2
    };
    typedef struct _SinkParam
    {
        UINT uiFlags;
        union
        {
            ImageCodecInfo* pici;
            SIZE* pSize;
        };
    } SinkParam;

    typedef INT (*PFNRESETIMAGESINK)(IImageSink*);
    INT TestBeginEndSink(IImageSink* pImageSink, SinkParam* psp, PFNRESETIMAGESINK pfnResetImageSink);
    INT TestSetPalette(IImageSink* pImageSink, SinkParam* psp, PFNRESETIMAGESINK pfnResetImageSink);
    INT TestPushPixelData(IImageSink* pImageSink, SinkParam* psp, PFNRESETIMAGESINK pfnResetImageSink);
    INT TestGetReleasePixelDataBuffer(IImageSink* pImageSink, SinkParam* psp, PFNRESETIMAGESINK pfnResetImageSink);
    INT TestGetPushProperty(IImageSink* pImageSink, SinkParam* psp, PFNRESETIMAGESINK pfnResetImageSink);

    typedef INT (*PFNIMAGESINKTEST)(IImageSink*, SinkParam*, PFNRESETIMAGESINK);
}

