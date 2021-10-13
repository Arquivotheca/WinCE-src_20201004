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
#include "filtergraph.h"
#include "dvrinterfaces.h"
#include "DvdMedia.h"

typedef enum
{
    INPUT_TYPE_SVIDEO,
    INPUT_TYPE_COMPOSITE_VIDEO,
    INPUT_TYPE_TUNER
} INPUT_TYPE;

typedef enum
{
    TUNE_UP,
    TUNE_DOWN,
    TUNE_TO
} TUNE_MODE;

class CSinkFilterGraph :
    public CFilterGraph
{
public:
    CSinkFilterGraph();
    ~CSinkFilterGraph(void);

    virtual HRESULT Initialize();
    virtual void UnInitialize();
    virtual BOOL SetCommandLine(const TCHAR * CommandLine);
    void OutputCurrentCommandLineConfiguration();

    virtual HRESULT SetupFilters(LPCOLESTR pszSourceFileName);
    virtual HRESULT SetupFilters(BOOL bUseGrabber);
    virtual HRESULT SetupFilters();
    virtual HRESULT SetupFilters(LPCOLESTR pszSourceFileName, BOOL bUseGrabber);

    // IAMCrossbar
    long GetCrossbarPinIndex(PhysicalConnectorType physicalConnectorType, BOOL bInputPin);
    HRESULT CanRoute(long OutputPinIndex, long InputPinIndex);
    HRESULT get_CrossbarPinInfo(BOOL bIsInputPin, long pinIndex, long *pPinIndexRelated, long *pPhysicalType);
    HRESULT get_IsRoutedTo(long outputPinIndex, long *pInputPinIndex);
    HRESULT get_PinCounts(long *pOutputPinCount, long *pInputPinCount);
    HRESULT Route(long outputPinIndex, long inputPinIndex);

    HRESULT SelectInput(PhysicalConnectorType physicalVideoConnectorType, PhysicalConnectorType physicalAudioConnectorType);
    HRESULT SelectInput(INPUT_TYPE inputType);

    //IAMVideoCompression
    HRESULT GetVideoCompressionInfo(WCHAR *pszVersion, int *pcbVersion, LPWSTR pszDescription, int *pcbDescription, long *pDefaultKeyFrameRate, long *pDefaultPFramesPerKey, double *pDefaultQuality, long *pCapabilities);
    HRESULT put_KeyFrameRate(long keyFrameRate);
    HRESULT get_KeyFrameRate(long *pKeyFrameRate);
    HRESULT put_PFramesPerKeyFrame(long lPFramesPerKeyFrame);
    HRESULT get_PFramesPerKeyFrame(long *pPFramesPerKeyFrame);
    HRESULT put_Quality(double Quality);
    HRESULT get_Quality(double *pQuality);
    HRESULT put_WindowSize(DWORDLONG WindowSize);
    HRESULT get_WindowSize(DWORDLONG *pWindowSize);
    HRESULT OverrideKeyFrame(long FrameNumber);
    HRESULT OverrideFrameSize(long FrameNumber, long Size);

#ifndef _WIN32_WCE
    //IAMTVAudio
    HRESULT get_TVAudioMode(long *plModes);
    HRESULT GetAvailableTVAudioModes(long *plModes);
    HRESULT GetHardwareSupportedTVAudioModes(long *plModes);
    HRESULT put_TVAudioMode(long plModes);
#endif

    //tuning
    HRESULT Tune(TUNE_MODE tuneMode, long lTargetChannel, long lVideoSubChannel, long lAudioSubChannel);
    HRESULT TuneUp();
    HRESULT TuneDown();
    HRESULT TuneTo(long lTargetChannel, long lVideoSubChannel, long lAudioSubChannel);

    //IStreamBufferCapture
    HRESULT GetCaptureMode(STRMBUF_CAPTURE_MODE *pMode, LONGLONG * pllMaxBufferMilliseconds);
    HRESULT BeginTemporaryRecording(LONGLONG llBufferSizeInMilliSeconds);
    HRESULT BeginPermanentRecording(LONGLONG llBufferSizeInMilliSeconds);
    HRESULT ConvertToTemporaryRecording(LPCOLESTR pszFileName);
    HRESULT SetRecordingPath(LPCOLESTR pszPath);
    HRESULT SetRecordingPath();
    HRESULT GetRecordingPath(LPOLESTR *ppszPath);
    HRESULT GetDefaultRecordingPath(TCHAR *ppszPath);
    HRESULT GetBoundToLiveToken(LPOLESTR *ppszToken);
    HRESULT GetCurrentPosition(LONGLONG  *phyCurrentPosition);

    //IFileSinkFilter
    HRESULT SetFileName(LPCOLESTR pszFileName, const AM_MEDIA_TYPE *pmt);
    HRESULT GetCurFile(LPOLESTR *ppszFileName, AM_MEDIA_TYPE *pmt);

    //IDVREngineHelper
    HRESULT DeleteRecording(LPCOLESTR pszFileName);
    HRESULT CleanupOrphanedRecordings(LPCOLESTR pszDirName);

    // Macrovision
    HRESULT SetMacrovisionLevel(AM_COPY_MACROVISION_LEVEL macrovisionLevel);

    // IContentRestrictionCapture
    
    // This interface is used to communicate policy related to
    // restrictions on the content's usage, including copy protection
    // and (later) parental control. 

    // Method SetEncryptionEnabled() turns encryption of media samples
    // on or off. If the argument is TRUE, encryption is on. If false,
    // encryption is off. The result should normally be S_OK. If this
    // method is not supported, E_NOTIMPL will be returned.
    HRESULT SetEncryptionEnabled(BOOL fEncrypt);

    // Method SetEncryptionEnabled() reports whether encryption has
    // been turned on by setting the argument. The result S_OK is
    // returned when *pfEncrypt was set. E_POINTER is returned if
    // the argument is NULL. E_NOTIMPL is returned if this method
    // is not supported.
    HRESULT GetEncryptionEnabled(BOOL *pfEncrypt);

    // Method ExpireVBICopyProtection() tells the DVR engine that
    // the copy protection policy received earllier as VBI data has
    // expired. The DVR engine should revert to (VBI-based) unprotected content
    // until further instructions are received via VBI. If new information
    // has been received such that the given policy is no longer what
    // was reported by VBI, the error XACT_E_WRONGSTATE will be returned instead.
    HRESULT ExpireVBICopyProtection(ULONG uExpiredPolicy);

    private:
        GUID m_UserSpecifiedSourceCLSID;
        TCHAR m_pszSourceVideoFile[MAX_PATH];
        TCHAR m_pszRecordingPath[MAX_PATH];
        ULONG m_VideoStreamPID;
        ULONG m_AudioStreamPID;
        GUID m_AudioSubType;

};

