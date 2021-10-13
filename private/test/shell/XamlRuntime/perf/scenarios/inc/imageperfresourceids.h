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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#pragma once

#define RT_JPEG 0   //  L"JPEG"
#define RT_PNG  1   //  L"PNG"

//Xaml picture resources - 10X
#define ID_GRAPH_800x600_PNG        101
#define ID_PHOTO_800x600_PNG        102
#define ID_GRAPH_800x480_PNG        103
#define ID_PHOTO_800x480_PNG        104
#define ID_GRAPH_640x480_PNG        105
#define ID_PHOTO_640x480_PNG        106
#define ID_GRAPH_480x640_PNG        107
#define ID_PHOTO_480x640_PNG        108
#define ID_GRAPH_1024x600_PNG       109
#define ID_PHOTO_1024x600_PNG       110
#define ID_GRAPH_LARGE_PNG          111
#define ID_PHOTO_LARGE_PNG          112
#define ID_GRAPHIC_PIX02_PNG        113

//Baml picture resources - 50x
#define ID_GRAPH_800x600_PNG_BAML   501
#define ID_PHOTO_800x600_PNG_BAML   502
#define ID_GRAPH_800x480_PNG_BAML   503
#define ID_PHOTO_800x480_PNG_BAML   504
#define ID_GRAPH_640x480_PNG_BAML   505
#define ID_PHOTO_640x480_PNG_BAML   506
#define ID_GRAPH_480x640_PNG_BAML   507
#define ID_PHOTO_480x640_PNG_BAML   508
#define ID_GRAPH_1024x600_PNG_BAML  509
#define ID_PHOTO_1024x600_PNG_BAML  510
#define ID_GRAPH_LARGE_PNG_BAML     511
#define ID_PHOTO_LARGE_PNG_BAML     512
#define ID_GRAPHIC_PIX02_PNG_BAML   513

//Xaml resources
#define IDX_LARGE_GRAPH                 100
#define IDX_LARGE_PHOTO                 101
#define IDX_FULL_SIZE_GRAPH             102
#define IDX_FULL_SIZE_PHOTO             103
#define IDX_PINCH_STRETCH_UC_GRAPH      104
#define IDX_PINCH_STRETCH_UC_PHOTO      105
#define IDX_PINCH_STRETCH               106

#define IDX_LARGE_GRAPH_BAML            1100
#define IDX_LARGE_PHOTO_BAML            1101
#define IDX_FULL_SIZE_GRAPH_BAML        1102
#define IDX_FULL_SIZE_PHOTO_BAML        1103
#define IDX_PINCH_STRETCH_UC_GRAPH_BAML 1104
#define IDX_PINCH_STRETCH_UC_PHOTO_BAML 1105
#define IDX_PINCH_STRETCH_BAML          1106
#define IDX_SMALL_GRAPH_BAML            5000

#define IDX_LARGE_GRAPH_JPG             200
#define IDX_LARGE_PHOTO_JPG             201
#define IDX_FULL_SIZE_GRAPH_JPG         202
#define IDX_FULL_SIZE_PHOTO_JPG         203

#define KNOWN_DEVICE_TYPES              5

#define FULL_SCREEN_GRAPH_JPG_XAML_TEMPLATE     XRPERF_RESOURCE_DIR L"FullScreenGraphic_jpg%s.xaml"
#define FULL_SCREEN_PHOTO_JPG_XAML_TEMPLATE     XRPERF_RESOURCE_DIR L"FullScreenPhoto_jpg%s.xaml"

#define XAML_RESOURCE_GRAPH_NAME                L"XRImageManipulationPerfResourceLGraph"
#define BAML_RESOURCE_GRAPH_NAME                L"XRImageManipulationPerfResourceLGraph_Baml"
#define XAML_RESOURCE_PHOTO_NAME                L"XRImageManipulationPerfResourceLPhoto"
#define BAML_RESOURCE_PHOTO_NAME                L"XRImageManipulationPerfResourceLPhoto_Baml"


WCHAR* ScreenSizeStrings[KNOWN_DEVICE_TYPES] = {L"800x600", L"800x480", L"640x480", L"480x640", L"1024x600"};
