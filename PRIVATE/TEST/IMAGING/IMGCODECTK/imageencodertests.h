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

namespace ImageEncoderTests
{
    INT TestInitTerminateEncoder(IImagingFactory* pImagingFactory, IImageEncoder* pImageEncoder, ImageCodecInfo* pici);
    INT TestGetEncodeSink(IImagingFactory* pImagingFactory, IImageEncoder* pImageEncoder, ImageCodecInfo* pici);
    INT TestSetFrameDimension(IImagingFactory* pImagingFactory, IImageEncoder* pImageEncoder, ImageCodecInfo* pici);
    INT TestEncoderParameters(IImagingFactory* pImagingFactory, IImageEncoder* pImageEncoder, ImageCodecInfo* pici);
    
    typedef INT (*PFNIMAGEENCODERTEST)(IImagingFactory*, IImageEncoder*, ImageCodecInfo*);
}

