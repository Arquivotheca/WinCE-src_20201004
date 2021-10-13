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

// Headers for the interface tests
#include "ImageSinkTests.h"
#include "ImageEncoderTests.h"
#include "UnknownTests.h"
#include "ImageDecoderTests.h"
#include "PerfTests.h"

// Headers for the iterator test functions
#include "ImgEncoders.h"
#include "ImgDecoders.h"
#include "Perf_Imaging.h"


FUNCTION_TABLE_ENTRY g_lpFTE[] = {
    TEXT("ImgEncoders Tests"), 0, 0, 0, NULL,
    
    TEXT("IUnknown Interface"), 1, 0, 0, NULL,
    TEXT("AddRef/Release"),                             2, (DWORD) UnknownTests::TestAddRefRelease,                 1100, ImageEncoderTests::RunTestUnknown,
    TEXT("QueryInterface"),                             2, (DWORD) UnknownTests::TestQueryInterface,                1101, ImageEncoderTests::RunTestUnknownGuids,

    TEXT("IImageEncoder Interface"), 1, 0, 0, NULL,
    TEXT("Init/TerminateEncoder"),                      2, (DWORD) ImageEncoderTests::TestInitTerminateEncoder,     1200, ImageEncoderTests::RunTestOnImageEncoders,
    TEXT("GetEncodeSink"),                              2, (DWORD) ImageEncoderTests::TestGetEncodeSink,            1201, ImageEncoderTests::RunTestOnImageEncoders,
    TEXT("SetFrameDimension"),                          2, (DWORD) ImageEncoderTests::TestSetFrameDimension,        1202, ImageEncoderTests::RunTestOnImageEncoders,
    TEXT("Encoder Parameters"),                         2, (DWORD) ImageEncoderTests::TestEncoderParameters,        1203, ImageEncoderTests::RunTestOnImageEncoders,

    TEXT("IImageSink Interface"), 1, 0, 0, NULL,
    TEXT("BeginSink/EndSink"),                          2, (DWORD) ImageSinkTests::TestBeginEndSink,                1300, ImageEncoderTests::RunTestOnImageSinks,
    TEXT("SetPalette"),                                 2, (DWORD) ImageSinkTests::TestSetPalette,                  1301, ImageEncoderTests::RunTestOnImageSinks,
    TEXT("PushPixelData"),                              2, (DWORD) ImageSinkTests::TestPushPixelData,               1302, ImageEncoderTests::RunTestOnImageSinks,
    TEXT("Get/ReleasePixelDataBuffer"),                 2, (DWORD) ImageSinkTests::TestGetReleasePixelDataBuffer,   1303, ImageEncoderTests::RunTestOnImageSinks,
    TEXT("Get/PushPropertyX"),                          2, (DWORD) ImageSinkTests::TestGetPushProperty,             1304, ImageEncoderTests::RunTestOnImageSinks,

    TEXT("ImgDecoders Tests"), 0, 0, 0, NULL,
    
    TEXT("IUnknown Interface"), 1, 0, 0, NULL,
    TEXT("AddRef/Release"),                             2, (DWORD) UnknownTests::TestAddRefRelease,                 2100, ImageDecoderTests::RunTestUnknown,
    TEXT("QueryInterface"),                             2, (DWORD) UnknownTests::TestQueryInterface,                2101, ImageDecoderTests::RunTestUnknownGuids,
    
    TEXT("IImageDecoder Interface"), 1, 0, 0, NULL,
    TEXT("Init/TerminateDecoder"),                      2, (DWORD) ImageDecoderTests::TestInitTerminateDecoder,     2200, ImageDecoderTests::RunTestOnImageDecoders,
    TEXT("Begin/EndDecode"),                            2, (DWORD) ImageDecoderTests::TestBeginEndDecode,           2201, ImageDecoderTests::RunTestOnImageDecoders,
    TEXT("Frames"),                                     2, (DWORD) ImageDecoderTests::TestFrames,                   2202, ImageDecoderTests::RunTestOnImageDecoders,
    TEXT("GetImageInfo"),                               2, (DWORD) ImageDecoderTests::TestGetImageInfo,             2203, ImageDecoderTests::RunTestOnImageDecoders,
    TEXT("GetThumbnail"),                               2, (DWORD) ImageDecoderTests::TestGetThumbnail,             2204, ImageDecoderTests::RunTestOnImageDecoders,
    TEXT("DecoderParams"),                              2, (DWORD) ImageDecoderTests::TestDecoderParams,            2205, ImageDecoderTests::RunTestOnImageDecoders,
    TEXT("GetProperties"),                              2, (DWORD) ImageDecoderTests::TestGetProperties,            2206, ImageDecoderTests::RunTestOnImageDecoders,

    TEXT("Imaging Performace Tests"), 0, 0, 0, NULL,
    
    TEXT("Decoding"), 1, 0, 0, NULL,
    TEXT("CreateBitmapFromImage Without Caching"),     2, (DWORD) PerfTests::TestCreateBitmapFromImageInMemory,     3100, PerfTests::RunPerfTestWithImage,
    TEXT("CreateBitmapFromImage Including Load Time"), 2, (DWORD) PerfTests::TestCreateBitmapFromImageWithLoad,     3101, PerfTests::RunPerfTestWithImage,
    TEXT("CreateBitmapFromImage Directly From File"),  2, (DWORD) PerfTests::TestCreateBitmapFromImageFromFile,     3102, PerfTests::RunPerfTestWithImage,
    TEXT("CreateBitmapFromImage Caching"),             2, (DWORD) PerfTests::TestCreateBitmapFromImageCache,        3103, PerfTests::RunPerfTestWithImage,
    TEXT("GetThumbnail"),                              2, (DWORD) PerfTests::TestGetThumbnail,                      3104, PerfTests::RunPerfTestWithImage,

    TEXT("Encoding"), 1, 0, 0, NULL,
    TEXT("Encode Bitmap to Memory Stream"),            2, (DWORD) PerfTests::TestEncodeBitmapToMemory,              3200, PerfTests::RunPerfTestWithEncoder,
    TEXT("Encode Bitmap to Memory and Save"),          2, (DWORD) PerfTests::TestEncodeBitmapWithSave,              3201, PerfTests::RunPerfTestWithEncoder,
    TEXT("Encode Bitmap to File"),                     2, (DWORD) PerfTests::TestEncodeBitmapToFile,                3202, PerfTests::RunPerfTestWithEncoder,
    
    NULL, 0, 0, 0, NULL,
};

