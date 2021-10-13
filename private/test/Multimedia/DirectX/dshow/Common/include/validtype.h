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
#ifndef _VALID_TYPES_H
#define _VALID_TYPES_H


#include <windows.h>
#include <objbase.h>
#include <dshow.h>
#ifdef UNDER_CE
#include <mtype.h>
#endif
#include <uuids.h>
#include <dvdmedia.h>

typedef HRESULT (*CheckMediaFunction)(int index, const AM_MEDIA_TYPE* pMediaType);

// The Camera source pre-rotate flag if it is not already declared
#ifndef BI_SRCPREROTATE
// Flag value to specify the source DIB section has the same rotation angle
// as the destination.
#define BI_SRCPREROTATE    0x8000
#endif

// FOURCC definitions - if they don't already exist
#ifndef FOURCC_WMV1
#define FOURCC_WMV1        mmioFOURCC('W','M','V','1')
#endif

#ifndef FOURCC_wmv1
#define FOURCC_wmv1     mmioFOURCC('w','m','v','1')
#endif

#ifndef FOURCC_WMV2
#define FOURCC_WMV2     mmioFOURCC('W','M','V','2')
#endif

#ifndef FOURCC_wmv2
#define FOURCC_wmv2     mmioFOURCC('w','m','v','2')
#endif

#ifndef FOURCC_WMV3
#define FOURCC_WMV3     mmioFOURCC('W','M','V','3')
#endif

#ifndef FOURCC_wmv3
#define FOURCC_wmv3     mmioFOURCC('w','m','v','3')
#endif

#ifndef FOURCC_WVC1
#define FOURCC_WVC1     mmioFOURCC('W','V','C','1')
#endif

#ifndef FOURCC_wvc1
#define FOURCC_wvc1     mmioFOURCC('w','v','c','1')
#endif

#ifndef FOURCC_WMVP 
#define FOURCC_WMVP     mmioFOURCC('W','M','V','P')
#endif

#ifndef FOURCC_wmvp
#define FOURCC_wmvp     mmioFOURCC('w','m','v','p')
#endif

#ifndef FOURCC_WMVA
#define FOURCC_WMVA     mmioFOURCC('W','M','V','A')
#endif

#ifndef FOURCC_wmva
#define FOURCC_wmva     mmioFOURCC('w','m','v','a')
#endif

#ifndef FOURCC_WVP2     
#define FOURCC_WVP2     mmioFOURCC('W','V','P','2')
#endif

#ifndef FOURCC_wvp2
#define FOURCC_wvp2     mmioFOURCC('w','v','p','2')
#endif

#ifndef FOURCC_MPG4
#define FOURCC_MPG4     mmioFOURCC('M','P','G','4')
#endif

#ifndef FOURCC_mpg4
#define FOURCC_mpg4     mmioFOURCC('m','p','g','4')
#endif

#ifndef FOURCC_MP42
#define FOURCC_MP42     mmioFOURCC('M','P','4','2')
#endif

#ifndef FOURCC_mp42
#define FOURCC_mp42     mmioFOURCC('m','p','4','2')
#endif

#ifndef FOURCC_MP43
#define FOURCC_MP43     mmioFOURCC('M','P','4','3')
#endif

#ifndef FOURCC_mp43
#define FOURCC_mp43     mmioFOURCC('m','p','4','3')
#endif

#ifndef FOURCC_MP4S
#define FOURCC_MP4S     mmioFOURCC('M','P','4','S')
#endif

#ifndef FOURCC_mp4s
#define FOURCC_mp4s     mmioFOURCC('m','p','4','s')
#endif

#ifndef FOURCC_M4S2     
#define FOURCC_M4S2     mmioFOURCC('M','4','S','2')
#endif

#ifndef FOURCC_m4s2
#define FOURCC_m4s2     mmioFOURCC('m','4','s','2')
#endif

// these aren't used anywhere??
#ifndef FOURCC_MSS1
#define FOURCC_MSS1     mmioFOURCC('M','S','S','1')
#endif

#ifndef FOURCC_mss1
#define FOURCC_mss1     mmioFOURCC('m','s','s','1')
#endif

#ifndef FOURCC_MSS2
#define FOURCC_MSS2     mmioFOURCC('M','S','S','2')
#endif

//
// MEDIASUBTYPE GUIDs for Windows Media Codecs
//

EXTERN_GUID(MEDIASUBTYPE_WMV1,
FOURCC_WMV1, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);

EXTERN_GUID(MEDIASUBTYPE_wmv1,
FOURCC_wmv1, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);

EXTERN_GUID(MEDIASUBTYPE_WMV2,
FOURCC_WMV2, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);

EXTERN_GUID(MEDIASUBTYPE_wmv2,
FOURCC_wmv2, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);

EXTERN_GUID(MEDIASUBTYPE_wmv3,
FOURCC_wmv3, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);

EXTERN_GUID(MEDIASUBTYPE_WVC1,
FOURCC_WVC1, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);

EXTERN_GUID(MEDIASUBTYPE_wvc1,
FOURCC_wvc1, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);

// Look for the the definition for MEDIASUBTYPE_WMV3 below

#ifdef UNDER_CE

// The following two audio GUIDs seem to be missing from the common headers
EXTERN_GUID(MEDIASUBTYPE_MPEGLAYER3,  0x00000055, 0x0000, 0x0010, 0x80, 0x00, 
            0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71);

EXTERN_GUID(WMMEDIASUBTYPE_WMAudioV7, 
0x00000161, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71); 

// The WMV3 type needs to be exported by the product. Until then make a definition.
DEFINE_GUID(MEDIASUBTYPE_WMV3,
0x33564d57, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);

#else // #ifdef UNDER_CE


// These are pre-defined in CE - not apparently in the desktop
#include <wmsdk.h> // For WMV declarations
#include <dvdmedia.h>  // For VIDEOINFOHEADER2

#define MEDIASUBTYPE_WMV3 WMMEDIASUBTYPE_WMV3
#define MEDIASUBTYPE_MPEGLAYER3 WMMEDIASUBTYPE_MP3

// Surprisingly enough there is no definiton of VIDEOINFO2 on the desktop only on CE. Add to desktop.
typedef struct tagVIDEOINFO2 {

    RECT            rcSource;          // The bit we really want to use
    RECT            rcTarget;          // Where the video should go
    DWORD           dwBitRate;         // Approximate bit data rate
    DWORD           dwBitErrorRate;    // Bit error rate for this stream
    REFERENCE_TIME  AvgTimePerFrame;   // Average time per frame (100ns units)
    DWORD            dwInterlaceFlags;
    DWORD            dwCopyProtectFlags;
    DWORD            dwPictAspectRatioX;
    DWORD            dwPictAspectRatioY; 
    DWORD            dwReserved1;
    DWORD            dwReserved2;

    BITMAPINFOHEADER bmiHeader;

    union {
        RGBQUAD         bmiColors[iPALETTE_COLORS];     // Colour palette
        DWORD           dwBitMasks[iMASK_COLORS];       // True colour masks
        TRUECOLORINFO   TrueColorInfo;                  // Both of the above
    };

} VIDEOINFO2;

#endif // UNDER_CE


// Video types
#define YVYU_Name "YVYU"
#define YUY2_Name "YUY2"
#define YV12_Name "YV12"
#define YV16_Name "YV16"
#define RGB1_Name "RGB1"
#define RGB4_Name "RGB4"
#define RGB8_Name "RGB8"
#define RGB555_Name "RGB555"
#define RGB565_Name "RGB565"
#define RGB24_Name "RGB24"
#define RGB32_Name "RGB32"
#define WMAv7_Name "WMAv7"
#define WMV1_Name "WMV1"
#define wmv1_Name "WMV1"
#define WMV2_Name "WMV2"
#define wmv2_Name "WMV2"
#define WMV3_Name "WMV3"
#define wmv3_Name "WMV3"
#define WVC1_Name "WVC1"
#define wvc1_Name "WVC1"
#define MPEG1VideoPacket_Name "MPEG1VideoPacket"
#define MPEG1VideoPayload_Name "MPEG1VideoPayload"
#define MPEG1VideoStream_Name "MPEG1VideoStream"
#define MPEG2Video_Name "MPEG2Video"
#define MPEG2VideoPES_Name "MPEG2VideoPES"
#define MPEG2TransportStream_Name "MPEG2TransportStream"
#define MPEG2TransportStreamPES_Name "MPEG2TransportStreamPES"
#define MPEG2ProgramStream_Name "MPEG2ProgramStream"


// Audio types
#define PCM_Name "PCM"
#define AU_Name "AU"
#define AIFF_Name "AIFF"
#define WAVE_Name "WAVE"
#define MPEG1Layer3_Name "MPEG1Layer3"
#define MPEG1AudioPacket_Name "MPEG1AudioPacket"
#define MPEG1AudioPayload_Name "MPEG1AudioPayload"
#define MPEG1AudioStream_Name "MPEG1AudioStream"
#define MPEG2Audio_Name "MPEG2Audio"
#define MPEG2AudioPES_Name "MPEG2AudioPES"
#define DOLBYAC3_Name "DOLBYAC3"
#define DOLBYAC3PES_Name "DOLBYAC3PES"
#define DVDLPCMAudio_Name "DVDLPCMAudio"
#define DVDLPCMAudioPES_Name "DVDLPCMAudioPES"

// System types
#define MPEG1SystemStream_Name "MPEG1SystemStream"
#define MPEG1VideoCDSystemStream_Name "MPEG1VideoCDSystemStream"

struct SystemTypeDesc
{
    GUID majortype;
    GUID subtype;
    GUID formattype;

    CheckMediaFunction checkfn;
    const char* name;
};

struct VideoTypeDesc
{
    GUID majortype;
    GUID subtype;
    GUID formattype;

    BOOL bFixedSizeSamples;
    BOOL bTemporalCompression;
    WORD biBitCount;
    DWORD biCompression;
    DWORD colorTableSize;

    CheckMediaFunction checkfn;
    const char* name;
};

struct AudioTypeDesc
{
    GUID majortype;
    GUID subtype;
    GUID formattype;

    BOOL bFixedSizeSamples;
    BOOL bTemporalCompression;
    
    CheckMediaFunction checkfn;
    const char* name;
};

struct TypeName {
    GUID guid;
    const char* name;
};

// Given a videoinfo object determine the color table and its size
HRESULT GetColorTable(const VIDEOINFO* pVideoInfo, BYTE** ppColorTable, DWORD *pColorTableSize);
HRESULT GetColorTable(const VIDEOINFO2* pVideoInfo, BYTE** ppColorTable, DWORD *pColorTableSize);

// Given a videoinfo object, construct the corresponding media type object. 
// Only look at the biCompression, biBitsPerPixel, biWidth, biHeight and color table to construct the media type.
HRESULT ConstructUncompressedVideoType(AM_MEDIA_TYPE* pMediaType, VIDEOINFO* pVideoInfo);
HRESULT ConstructUncompressedVideoType(AM_MEDIA_TYPE* pMediaType, VIDEOINFO2* pVideoInfo);

// Specific type-checks
bool IsValidUncompressedVideoType(const AM_MEDIA_TYPE* pMediaType);
bool IsValidWaveFormat(const AM_MEDIA_TYPE* pMediaType);
bool IsValidWaveFormat(const WAVEFORMATEX* pWfex);
bool IsEqualWaveFormat(const WAVEFORMATEX* pWfex, const WAVEFORMATEX* pRefWfex);
bool IsValidVideoInfoHeader(const VIDEOINFOHEADER* pVih);
bool IsValidVideoInfoHeader2(const VIDEOINFOHEADER2* pVih);
bool IsValidVideoInfoHeader2(const VIDEOINFOHEADER2* pVih);
bool IsValidEncVideoInfoHeader2(const VIDEOINFOHEADER2* pVih);
bool IsValidEncVideoInfoHeader(const VIDEOINFOHEADER* pVih);
bool IsValidEncVideoInfoHeader2(const VIDEOINFOHEADER2* pVih);
bool IsEqualVideoInfoHeaderNonStrict(const VIDEOINFOHEADER* pVih, const VIDEOINFOHEADER* pRefVih);
bool IsEqualEncVideoInfoHeaderNonStrict(const VIDEOINFOHEADER* pVih, const VIDEOINFOHEADER* pRefVih);
bool IsEqualEncVideoInfoHeader2NonStrict(const VIDEOINFOHEADER2* pVih, const VIDEOINFOHEADER2* pRefVih);
bool IsValidUncompressedVideoType(const AM_MEDIA_TYPE* pMediaType);
bool IsValidWMV3Type(const AM_MEDIA_TYPE* pMediaType);
bool IsEqualMediaType(const AM_MEDIA_TYPE* pMediaType1, const AM_MEDIA_TYPE* pMediaType2);

HRESULT CheckUncompressedVideo(int index, const AM_MEDIA_TYPE* pMediaType);
HRESULT CheckUncompressedAudio(int index, const AM_MEDIA_TYPE* pMediaType);
bool IsVideoType(const AM_MEDIA_TYPE* pMediaType);
bool IsAudioType(const AM_MEDIA_TYPE* pMediaType);
bool IsSystemType(const AM_MEDIA_TYPE* pMediaType);
const char* GetVideoTypeName(const AM_MEDIA_TYPE* pMediaType);
const char* GetAudioTypeName(const AM_MEDIA_TYPE* pMediaType);
const char* GetSystemTypeName(const AM_MEDIA_TYPE* pMediaType);
const char* GetTypeName(const AM_MEDIA_TYPE* pMediaType);
VideoTypeDesc* GetVideoTypeDesc(DWORD biCompression, DWORD biBitCount);

HRESULT CheckVideoType(const AM_MEDIA_TYPE* pMediaType);
HRESULT CheckAudioType(const AM_MEDIA_TYPE* pMediaType);
// This is to be deprecated
HRESULT CheckMediaTypeValidity(const AM_MEDIA_TYPE* pMediaType);



// Plain-text to GUID types and vice-versa
HRESULT SubTypeToGUID(const char* szSubType, GUID* pGuid);
HRESULT MajorTypeToGUID(const char* szSubType, GUID* pGuid);
HRESULT FormatTypeToGUID(const char* szFormatType, GUID* pGuid);
HRESULT CompressionToValue(const char* szCompression, DWORD *pCompression);
HRESULT WaveFormatTagToValue(const char* szFormatTag, WORD *pFormatTag);

#endif
