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

