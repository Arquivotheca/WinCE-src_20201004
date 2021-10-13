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
////////////////////////////////////////////////////////////////////////////////

#ifndef MEDIA_H
#define MEDIA_H

#include <vector>
#include <windows.h>
#include <stdlib.h>

#define TEST_MAX_PATH (MAX_PATH+20)       // For invalid long file name test
#define MAX_PROFILE_NAME 256

class VideoInfo
{
public:
    VideoInfo();
    ~VideoInfo();

public:
    // description from AM_MEDIA_TYPE
    TCHAR    m_szType[256];
    // index file
    TCHAR m_szIndexFile[MAX_PATH];

    DWORD m_dwBitRate;
    // number of video frames 
    DWORD m_dwFrames;
    DWORD m_dwFrameSize;
    // video height
    INT32 m_nHeight;
    // video width
    INT32 m_nWidth;

    BOOL m_bCompressed;
    // Is the video interlaced or progress
    BOOL m_bInterlaced;

    // X aspect ratio
    FLOAT m_fXAspectRatio;
    // Y aspect ratio
    FLOAT m_fYAspectRatio;

    // For performance information from IQualProp interface
    INT32 m_nQualPropAvgFrameRate;
    INT32 m_nQualPropAvgSyncOffset;
    INT32 m_nQualPropDevSyncOffset;
    INT32 m_nQualPropFramesDrawn;
    INT32 m_nQualPropFramesDroppedInRenderer;
    INT32 m_nQualPropJitter;
};

class AudioInfo
{
public:
    AudioInfo();
    ~AudioInfo();

public:
    // description from AM_MEDIA_TYPE
    TCHAR    m_szType[256];
    // index file
    TCHAR m_szIndexFile[MAX_PATH];
    // number of samples
    DWORD m_dwSamples;
    // sample size, 8 or 16 bit
    DWORD m_dwSampleSize;
    
    DWORD    m_dwPlaytimeAllowance;    // in ms, default is 200
    DWORD    m_dwCaptimeAllowance;    // in ms, default is 200
    DWORD    m_dwSampleFreq;            // default is 11025
    DWORD    m_dwChannels;            // nubmer of channels, default is 1
};


enum {
    MEDIA_NAME_LENGTH = 64,
    PROTOCOL_NAME_LENGTH = 64
};

class Media;

typedef std::vector<Media*> MediaList;
typedef std::vector<TCHAR*> StringList;

#define MAX_PROTOCOLS 8

class BaseUrl
{
public:
    BaseUrl();
    TCHAR szProtocolName[PROTOCOL_NAME_LENGTH];
    TCHAR szBaseUrl[TEST_MAX_PATH];
};

// Media encapsualtes info about the media. Properties include
// - url
// - list of filters needed to handle the media
// - media properties such as dimension, bitrate etc
// - known metadata present in the media
class Media
{
public:
    // The file name for the media
    TCHAR m_szFileName[TEST_MAX_PATH];
    // The well known name
    TCHAR m_szName[64];
    // Description of the clip
    TCHAR m_szDescription[128];
	//FileID for updating MediaCube
	TCHAR m_szFileID[64];

    // Media file type: AVI, WAV, MP3, MPEG, WMV, WMA etc.
    TCHAR    m_szMediaType[256];
    bool    m_bCompressed;    
    DWORD    m_dwBitRate;

    // data members
    DWORD    m_dwDurations;        // in milliseconds
    DWORD    m_dwSize;            // the size of the media in bytes.

    // The possible base urls
    BaseUrl baseUrls[MAX_PROTOCOLS];

    // video/audio flag
    bool m_bIsVideo;
    // Video specific info
    VideoInfo videoInfo;
    // Audio specific info
    AudioInfo audioInfo;

protected:
    // The currently used url
    TCHAR m_szCurrentUrl[TEST_MAX_PATH];
    bool  m_bProtocolSet;
    // Was this downloaded?
    bool m_bDownloaded;

    // Given protocol find base url
    const TCHAR* FindBaseUrl( const TCHAR* szProtocol );
public:
    Media( const TCHAR* szUrl );
    Media();
    virtual ~Media();

    HRESULT SetProtocol( const TCHAR* szProtocol );    
    const TCHAR* GetUrl( const TCHAR* szProtocol = NULL );
    const TCHAR* GetName();
};

Media* FindMedia(MediaList* pMediaList, TCHAR* szMediaName, int length);
HRESULT GetProtocolName(TCHAR* szMediaProtocol, TCHAR* szProtocol, int length);
HRESULT GetMediaName(TCHAR* szMediaProtocol, TCHAR* szMedia, int length);

class CFormat
{
    public:
    CFormat();
    ~CFormat() {}

    GUID m_MediaSubtype;
    bool m_bPreRotated;
    INT32 m_nMinWidth;
    INT32 m_nDefaultWidth;
    INT32 m_nMaxWidth;
    INT32 m_nWidthStepping;

    INT32 m_nMinHeight;
    INT32 m_nDefaultHeight;
    INT32 m_nMaxHeight;
    INT32 m_nHeightStepping;

    INT32 m_nMinCropWidth;
    INT32 m_nMaxCropWidth;
    INT32 m_nCropWidthStepping;
    INT32 m_nCropWidthAlign;

    INT32 m_nMinCropHeight;
    INT32 m_nMaxCropHeight;
    INT32 m_nCropHeightStepping;
    INT32 m_nCropHeightAlign;

    INT32 m_nStretchTapsWidth;
    INT32 m_nStretchTapsHeight;
    INT32 m_nShrinkTapsWidth;
    INT32 m_nShrinkTapsHeight;

    INT32 m_nMinFrameRate;
    INT32 m_nDefaultFrameRate;
    INT32 m_nMaxFrameRate;
};

typedef std::vector<class CFormat *> Format;

class CProperty
{
public:
    CProperty();
    ~CProperty() {}
public:
    ULONG m_ulCapabilities;
    ULONG m_ulDefaultCapability;

    // a driver can support get without set and set without get
    bool m_bGetSupported;
    bool m_bSetSupported;

    // min, max, step are msdn defined for most properties, but
    // this gives us a way to override
    INT32 m_nMin;
    INT32 m_nDefault;
    INT32 m_nMax;
    INT32 m_nStep;

    // the delay to insert during the processing of the syncronous
    // command, and a delay to insert while processing the asyncronous
    // commen. Separate because the driver can support both sync and async
    // and the delay is not guaranteed to be the same.
    DWORD m_dwDelay;
    bool m_bFailRequests;

    DWORD m_dwASyncDelay;
    bool m_bFailASyncRequests;
};

class CCameraProperty
{
public:
    CCameraProperty();
    ~CCameraProperty();

public:
    TCHAR        m_ptszProfileName[MAX_PROFILE_NAME];

    // video properties
    CProperty    m_Brightness;
    CProperty    m_Contrast;
    CProperty    m_Hue;
    CProperty    m_Saturation;
    CProperty    m_Sharpness;
    CProperty    m_WhiteBalance;
    CProperty    m_Gamma;
    CProperty    m_ColorEnable;
    CProperty    m_BacklightCompensation;
    CProperty    m_Gain;

    // control properties
    CProperty    m_Pan;
    CProperty    m_Tilt;
    CProperty    m_Roll;
    CProperty    m_Zoom;
    CProperty    m_Iris;
    CProperty    m_Exposure;
    CProperty    m_Focus;
    CProperty    m_Flash;

    // supported formats
    Format        m_PreviewFormat;
    DWORD       m_dwPreviewMemoryModel;
    DWORD       m_dwPreviewMaxBuffers;
    DWORD       m_dwPreviewPossibleCount;
    DWORD       m_dwPreviewDeliveryDelay;

    Format        m_CaptureFormat;
    DWORD       m_dwCaptureMemoryModel;
    DWORD       m_dwCaptureMaxBuffers;
    DWORD       m_dwCapturePossibleCount;
    DWORD       m_dwCaptureDeliveryDelay;

    Format        m_StillFormat;
    DWORD       m_dwStillMemoryModel;
    DWORD       m_dwStillMaxBuffers;
    DWORD       m_dwStillPossibleCount;
    bool            m_bSampleScannedEventSupported;
    DWORD       m_dwStillDeliveryDelay;

    bool            m_bMetadataEnabled;

    // animation control
    bool            m_bImageConsistency;
    int              m_nAnimationLength;
};

#endif
