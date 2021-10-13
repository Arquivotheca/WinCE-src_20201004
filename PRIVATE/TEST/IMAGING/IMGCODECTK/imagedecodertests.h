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

