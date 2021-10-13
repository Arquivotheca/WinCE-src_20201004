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
#include "ImageProvider.h"

namespace ImageDecoderTests
{
    INT TestInitTerminateDecoder(IImagingFactory * pImagingFactory, IImageDecoder * pImageDecoder, const CImageDescriptor & id);
    INT TestBeginEndDecode(IImagingFactory * pImagingFactory, IImageDecoder * pImageDecoder, const CImageDescriptor & id);
    INT TestFrames(IImagingFactory * pImagingFactory, IImageDecoder * pImageDecoder, const CImageDescriptor & id);
    INT TestGetImageInfo(IImagingFactory * pImagingFactory, IImageDecoder * pImageDecoder, const CImageDescriptor & id);
    INT TestGetThumbnail(IImagingFactory * pImagingFactory, IImageDecoder * pImageDecoder, const CImageDescriptor & id);
    INT TestDecoderParams(IImagingFactory * pImagingFactory, IImageDecoder * pImageDecoder, const CImageDescriptor & id);
    INT TestGetProperties(IImagingFactory * pImagingFactory, IImageDecoder * pImageDecoder, const CImageDescriptor & id);

    typedef INT (*PFNIMAGEDECODERTEST)(IImagingFactory *, IImageDecoder *, const CImageDescriptor &);
}

