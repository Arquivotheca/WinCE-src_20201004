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

namespace PerfTests
{
    INT TestCreateBitmapFromImageInMemory(IImagingFactory* pImagingFactory, const CImageDescriptor& id);
    INT TestCreateBitmapFromImageWithLoad(IImagingFactory* pImagingFactory, const CImageDescriptor& id);
    INT TestCreateBitmapFromImageFromFile(IImagingFactory* pImagingFactory, const CImageDescriptor& id);
    INT TestCreateBitmapFromImageCache(IImagingFactory* pImagingFactory, const CImageDescriptor& id);
    INT TestGetThumbnail(IImagingFactory* pImagingFactory, const CImageDescriptor& id);

    INT TestEncodeBitmapToMemory(IImagingFactory* pImagingFactory, const CImageDescriptor & id, const ImageCodecInfo * pici);
    INT TestEncodeBitmapWithSave(IImagingFactory* pImagingFactory, const CImageDescriptor & id, const ImageCodecInfo * pici);
    INT TestEncodeBitmapToFile(IImagingFactory* pImagingFactory, const CImageDescriptor & id, const ImageCodecInfo * pici);

    typedef INT (*PFNPERFTEST)(IImagingFactory*, const CImageDescriptor&);
    typedef INT (*PFNPERFTESTENCODER)(IImagingFactory*, const CImageDescriptor&, const ImageCodecInfo *);
}

