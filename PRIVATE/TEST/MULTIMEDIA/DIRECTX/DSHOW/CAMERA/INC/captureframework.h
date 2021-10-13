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
#include "logging.h"
#include "CPropertyBag.h"

// for ccom pointers
#include <atlbase.h>
// for various filters and streams
#include <streams.h>
// for WMHeaderInfo
#include <wmsdkidl.h>
// for the encoder DMO
#include <dmodshow.h>
#include <dmoreg.h>

#include "CamDriverLibrary.h"
#include "asfparse.h"
#include <camera.h>

#include <wmcodecids.h>
#include <wmcodecstrs.h>

#include "grabber.h"
#include "eventsink.h"

#ifndef CAPTUREFRAMEWORK_H
#define CAPTUREFRAMEWORK_H

#define TEST_FRAMEWORK_VERSION 42

typedef  HRESULT ( __stdcall *pFuncpfv)();

static const GUID CLSID_CameraDriver = DEVCLASS_CAMERA_GUID;

// Filters that can be specified
#define VIDEO_CAPTURE_FILTER                     0x1
#define STILL_IMAGE_SINK                         0x2
#define VIDEO_RENDERER                           0x4
#define VIDEO_RENDERER_INTELI_CONNECT            0x8
#define FILE_WRITER                                  0x10
#define AUDIO_CAPTURE_FILTER                     0x20
#define VIDEO_ENCODER                              0x40
#define AUDIO_ENCODER                              0x80

// ALL_COMPONENTS specifies all the components
#define COMPONENTS_MASK                          0xff
#define ALL_COMPONENTS                           0xf7
#define ALL_COMPONENTS_INTELI_CONNECT_VIDEO      0xfb

// Options that can be specified
#define OPTIONS_MASK                       0x80000000
#define OPTION_SIMULT_CONTROL              0x80000000
#define ALL_OPTIONS                        0x80000000

enum QUALITY {
    QUALITY_LOW = 0,
    QUALITY_MEDIUM = 1,
    QUALITY_HIGH = 2,
    QUALITY_NONE = 255,
    QUALITY_DWORD = 0xffffffff,
};

enum PropertiesFlags{
    Flags_Auto = 0x1,
    Flags_Manual = 0x2,
};

enum Properties {
    PROPERTY_CAMERACONTROL,
    PROPERTY_VIDPROCAMP
};

enum Streams {
    STREAM_PREVIEW,
    STREAM_STILL,
    STREAM_CAPTURE,
    STREAM_AUDIO
};

#define STILL_IMAGE_STATICFILE_NAME TEXT("\\release\\test.jpg")
#define STILL_IMAGE_FILE_NAME TEXT("\\release\\test%d.jpg")
#define VIDEO_CAPTURE_STATICFILE_NAME TEXT("\\release\\test.asf")
#define VIDEO_CAPTURE_FILE_NAME TEXT("\\release\\test%d.asf")

#define VIDEOBUFFER 1
#define AUDIOBUFFER 2

// allow a 4 second timeout because sometimes audio hardware takes a long time to initialize
#define MEDIAEVENT_TIMEOUT 4000
// allow a 3 minute maximum timeout.  If the event doesn't come within 3 minutes it's never going to.
#define MAXIMUM_MEDIAEVENT_TIMEOUT 3*60*1000
#define LOCATIONVARIANCE 5

#define countof(x)  (sizeof(x)/sizeof(*(x)))

#define NULLCAM_PINKEY TEXT("PinCount")
#define NULLCAM_MODEKEYKEY TEXT("MemoryModel")

#define NULLCAM_STREAMKEY0 TEXT("StreamGuid0")
#define NULLCAM_STREAMKEY1 TEXT("StreamGuid1")
#define NULLCAM_STREAMKEY2 TEXT("StreamGuid2")
#define NULLCAM_STREAMKEY3 TEXT("StreamGuid3")
#define NULLCAM_STREAMKEY4 TEXT("StreamGuid4")
#define NULLCAM_STREAMKEY5 TEXT("StreamGuid5")

#define S_MULTIPLE_EVENTS  ((HRESULT)0x00000200)
#define E_OOM_EVENT  ((HRESULT)0x80000201)
#define E_WRITE_ERROR_EVENT  ((HRESULT)0x80000202)
#define E_GRAPH_UNSUPPORTED  ((HRESULT)0x80000203)
#define E_NO_FRAMES_PROCESSED  ((HRESULT)0x80000204)
#define E_BUFFER_DELIVERY_FAILED  ((HRESULT)0x80000205)

typedef struct _VIDEORENDERER_STATS
{
    double dFrameRate;
    double dActualRate;
    long   lFramesDropped;
    long lSourceWidth;
    long lSourceHeight;
    long lDestWidth;
    long lDestHeight;
} VIDEORENDERER_STATS;

enum
{
    VERIFICATION_FORCEAUTO,
    VERIFICATION_FORCEMANUAL,
    VERIFICATION_OFF
};


enum DIAGNOSTICS_FILTER_INDEX
{
    UNUSED = 0,
    DIAGNOSTICSFILTER_STILLIMAGE,
    DIAGNOSTICSFILTER_VIDEORENDERER,
    DIAGNOSTICSFILTER_VIDEOCAPTURE,
    DIAGNOSTICSFILTER_VIDEOTOBEMUXED,
    DIAGNOSTICSFILTER_AUDIOCAPTURE,
    DIAGNOSTICSFILTER_AUDIOTOBEMUXED,

};
enum DIAGNOSTICS_INDEX
{
    FILTER_NAME = 0,
    FILTER_LOCATION,
};

static TCHAR *DiagnosticsData[][2] = {
   { TEXT("UNKNOWN DIAGNOSTICS LOCATION"), TEXT("UNKNOWN FILTER LOCATION") },
   { TEXT("Still image grabber filter"), TEXT("between the video capture filter and still image sink") },
   { TEXT("Video renderer grabber filter"), TEXT("just before the video renderer filter") },
   { TEXT("Video capture grabber filter"), TEXT("at the beginning of the video capture pipeline") },
   { TEXT("Video muxed grabber filter"), TEXT("at the video input of the multiplexer") },
   { TEXT("Audio capture grabber filter"), TEXT("at the output of the audio capture filter") },
   { TEXT("Audio muxed grabber filter"), TEXT("at the audio input of the multiplexer") },
};

typedef struct _LIBRARY_LOAD_ACCELERATOR
{
    TCHAR *tszLibraryName;
    HINSTANCE hLibrary;
} LIBRARY_LOAD_ACCELERATOR;

// to speed up the loading and unloading of the tests, we'll pre load all of the
// libraries the test suite depends on, so we don't have to wait for them to load and
// unload over and over while the test is running. Once they're loaded they stay loaded
// until we exit, then we'll free them in the destructor.
// If any new libraries are loaded between tests, just add them to the list here
// so we can run faster and not wait for them to load and unload.
// Functionally this is a noop, if it's not there it'll get loaded automatically,
// if it is here then it's already loaded and will just save us the load and unload.
static LIBRARY_LOAD_ACCELERATOR g_LibrariesToAccelerateLoad[] = {
    // these dll's we're directly using
   { TEXT("quartz.dll"), NULL},
   { TEXT("ddraw.dll"), NULL},
   { TEXT("mmtimer.dll"), NULL},
   { TEXT("wmvdmoe.dll"), NULL},
   { TEXT("msdmo.dll"), NULL},
   { TEXT("asfparse.dll"), NULL},
   { TEXT("imaging.dll"), NULL},
   { TEXT("zlib.dll"), NULL},

    // these just get loaded and unloaded as a side effect, so we'll just leave
    // them loaded
   { TEXT("urlmon.dll"), NULL},
   { TEXT("shlwapi.dll"), NULL},
   { TEXT("wininet.dll"), NULL},
   { TEXT("ws2.dll"), NULL},
   { TEXT("iphlpapi.dll"), NULL},
   { TEXT("wmvdmod.dll"), NULL},
   { TEXT("wmsdmod.dll"), NULL},
   { TEXT("wmadmod.dll"), NULL},
   { TEXT("icm.dll"), NULL},
   { TEXT("msrle32.dll"), NULL},
};

typedef class CCaptureFramework
{
    public:
        CCaptureFramework();
        ~CCaptureFramework();

        void OutputRuntimeParameters();
        BOOL ParseCommandLine(LPCTSTR  tszCommandLine);
        HRESULT SetStillCaptureFileName(TCHAR *tszFileName);
        HRESULT GetStillCaptureFileName(TCHAR *tszFileName, int *nStringLength);
        HRESULT SetVideoCaptureFileName(TCHAR *tszFileName);
        HRESULT GetVideoCaptureFileName(TCHAR *tszFileName, int *nStringLength);
        HRESULT PropogateFilenameToCaptureFilter();
        HRESULT GetFrameStatistics(long *plDropped, long *plNotDropped, long *plAvgFrameSize);
        HRESULT GetRange(int nProperty, long lProperty, long *lpMin, long *lpMax, long *lpStepping, long *lpDefault, long *lpFlags);
        HRESULT Get(int nProperty, long lProperty, long *lpValue, long *lpFlags);
        HRESULT Set(int nProperty, long lProperty, long lValue, long lFlags);
        HRESULT GetStreamCaps(int nStream, int nIndex, AM_MEDIA_TYPE **pFormat, BYTE *pSCC);
        HRESULT SetFormat(int nStream, AM_MEDIA_TYPE *pFormat);
        HRESULT GetFormat(int nStream, AM_MEDIA_TYPE **pFormat);
        HRESULT GetNumberOfCapabilities(int nStream, int *piCount, int *piSize);
        HRESULT GetScreenOrientation(DWORD *dwOrientation);
        HRESULT SetScreenOrientation(DWORD dwOrientation);
        HRESULT GetCurrentPosition(REFERENCE_TIME *rt);
        HRESULT GetBufferingDepth(int nStream, REFERENCE_TIME *rt);
        HRESULT SetBufferingDepth(int nStream, REFERENCE_TIME *rt);

        HRESULT InitCameraDriver();
        HRESULT SelectCameraDevice(TCHAR *tszCamDeviceName);
        HRESULT SelectCameraDevice(int nCameraDevice);
        HRESULT GetDriverList(TCHAR ***tszCamDeviceName, int *nEntryCount);

        HRESULT Init(HWND OwnerWnd, RECT *rcCoordinates, DWORD dwComponentsToAdd);
        HRESULT InitCore(HWND OwnerWnd, RECT *rcCoordinates, DWORD dwComponentsToAdd);
        HRESULT FinalizeGraph();

        HRESULT RunGraph();
        HRESULT PauseGraph();
        HRESULT StopGraph();
        HRESULT TriggerStillImage();
        HRESULT CaptureStillImage();
        HRESULT CaptureStillImageBurst(int nBurstCount);
        HRESULT StartStreamCapture();
        HRESULT StopStreamCapture(DWORD dwAdditionalDelay = 0);
        HRESULT TimedStreamCapture(int nCaptureLength);

        HRESULT Cleanup();

        HRESULT SetVideoWindowPosition(RECT *rc);
        HRESULT SetVideoWindowSize(long Width, long Height);
        HRESULT SetVideoWindowVisible(BOOL bVisible);
        VOID SetForceColorConversion(BOOL bValue) { m_bForceColorConversion = bValue; }
        BOOL GetForceColorConversion() { return m_bForceColorConversion; }

        HRESULT SwitchVideoRenderMode();
        HRESULT GetVideoRenderMode(DWORD *dwMode);
        HRESULT SetVideoRenderMode(DWORD dwMode);
        int GetDriverPinCount() {return m_uiPinCount; }

        HRESULT GetVideoRenderStats(VIDEORENDERER_STATS *Statistics);
        HRESULT SetVideoEncoderProperty(LPCOLESTR ptzPropName,  VARIANT *pVar);
        HRESULT SetAudioEncoderProperty(LPCOLESTR ptzPropName,  VARIANT *pVar);
        HRESULT GetAudioEncoderInfo(LPCOLESTR tszEntryName, VARIANT *varg);
        HRESULT GetVideoEncoderInfo(LPCOLESTR tszEntryName, VARIANT *varg);


        HRESULT SetStillImageQuality(int n);

        HRESULT VerifyPreviewWindow();
        HRESULT VerifyStillImageCapture();
        HRESULT VerifyStillImageLocation(TCHAR *tszImageName, int nTickCountBefore, int nTickCountAfter);
        HRESULT VerifyVideoFileCaptured(TCHAR *tszVideoFileName);

        static void ProcessMediaEvent(LONG lEventCode, LONG lParam1, LONG lParam2, LPVOID lpParameter);
        // Wait for the next matching event to trigger for dwMilliseconds
        HRESULT WaitOnEvent(const DShowEvent * event, DWORD dwMilliseconds) {return m_EventSink.WaitOnEvent(event, dwMilliseconds);}
        // Wait for one or all matching events to trigger for dwMilliseconds
        HRESULT WaitOnEvents(DWORD nCount, const DShowEvent* pEventFilters, BOOL fWaitAll, DWORD dwMilliseconds)
                                                 {return m_EventSink.WaitOnEvents(nCount, pEventFilters, fWaitAll, dwMilliseconds);}
        // search the event queue for an event.
        DShowEvent *FindFirstEvent(DWORD nCount, const DShowEvent* pEventFilters) {return m_EventSink.FindFirstEvent(nCount, pEventFilters);}
        DShowEvent *FindNextEvent() {return m_EventSink.FindNextEvent();}
        HRESULT GetEvent(LONG *lEventCode, LONG *lParam1, LONG *lParam2, DWORD Milliseconds) {return m_EventSink.GetEvent(lEventCode, lParam1, lParam2, Milliseconds);}
        HRESULT SetEventFilters(DWORD nCount, DShowEvent* pEventFilters) { return m_EventSink.SetEventFilters(nCount, pEventFilters); }

        HRESULT SetEventCallback(EVENTCALLBACK * pEventCallback, DWORD nCount, const DShowEvent *pEventFilters, LPVOID lParam) { return m_EventSink.SetEventCallback(pEventCallback, nCount, pEventFilters, lParam); }
        HRESULT ClearEventCallbacks() { return m_EventSink.ClearEventCallbacks(); }
        HRESULT PurgeEvents() { return m_EventSink.Purge(); }

        HRESULT GetFramesProcessed(LONG *lVideoFramesProcessed, LONG *lAudioFramesProcessed);
        HRESULT GetFramesDropped(LONG *lVideoFramesDropped, LONG *lAudioFramesDropped);
        HRESULT GetPinCount(UINT *uiPinCount);

        HRESULT GetBuilderInterface(LPVOID* ppint, REFIID riid);
        HRESULT GetBaseFilter(LPVOID* ppint, DWORD component);
        void DisplayPinsOfFilter(IBaseFilter *pFilter);
        HRESULT DisplayFilters();

    private:
        HRESULT CoCreateComponents();
        HRESULT AddInterfaces();
        HRESULT InitFilters();
        HRESULT AddFilters();
        HRESULT FindInterfaces();
        HRESULT InitializeStreamFormats();
        virtual HRESULT RenderGraph();

        HRESULT ConfigureASF();
        HRESULT SetupQualityRegistryKeys();
        HRESULT EnsureQualityKeyCreated(QUALITY Quality);
    
        int FindCameraDevice(TCHAR *tszCamDeviceName);
        HRESULT GetAllCameraData();
        HRESULT SetupNULLCameraDriver();
        HRESULT VerifyDriverImage(HBITMAP hbmp, int nStartTime, int nStopTime, DWORD dwOrientation);

        void OutputEventInformation(LONG lEventCode, LONG lParam1, LONG lParam2);
        void OutputMissingEventsInformation(DWORD dwEvent);
        HRESULT InsertFilter(IBaseFilter *pFilterBase, PIN_DIRECTION pindir, const GUID *pCategory, const GUID *pType, int index, IBaseFilter *pFilterToInsert);
        HRESULT  RegisterDll(TCHAR * tszDllName);
        HRESULT InsertDiagnosticsFilters();
        HRESULT InsertGrabberFilter(IBaseFilter *pFilterBase, PIN_DIRECTION pindir, const GUID *pCategory, const GUID *pType, int index, DIAGNOSTICS_FILTER_INDEX FilterIndex);
        static HRESULT GrabberCallback(IMediaSample *pSample, REFERENCE_TIME *StartTime, REFERENCE_TIME *StopTime, BOOL TypeChanged, AM_MEDIA_TYPE *mtype, LPVOID lpUser);

        TCHAR m_tszStillCaptureName[MAX_PATH];
        TCHAR m_tszVideoCaptureName[MAX_PATH];

        HWND m_hwndOwner;
        RECT m_rcLocation;
        LONG m_lDefaultWidth;
        LONG m_lDefaultHeight;
        DWORD m_dwComponentsInUse;
        DWORD m_dwOptionsInUse;
        LONG m_lPictureNumber;
        LONG m_lCaptureNumber;

        CAMDRIVERLIBRARY m_CamDriverUtils;
        int m_nSelectedCameraDevice;
        BOOL m_bUsingNULLDriver;

        int  m_nPreviewVerificationMode;
        int  m_nStillVerificationMode;

        TCHAR m_tszVideoFileLibraryPath[MAX_PATH];
        HMODULE m_hVerifyVideoFile;
        PFNVERIFYVIDEOFILE m_pfnVerifyVideoFile;

        BOOL m_bCoInitialized;
        BOOL m_bInitialized;

        CComPtr<ICaptureGraphBuilder2> m_pCaptureGraphBuilder;
        CComPtr<IGraphBuilder> m_pGraph;
        CComPtr<IBaseFilter> m_pColorConverter;
        CComPtr<IBaseFilter> m_pSmartTeeFilter;
        CComPtr<IBaseFilter> m_pVideoRenderer;
        CComPtr<IAMVideoRendererMode> m_pVideoRendererMode;
        CComPtr<IBasicVideo> m_pVideoRendererBasicVideo;
        CComPtr<IVideoWindow> m_pVideoWindow;
        CComPtr<IBaseFilter> m_pVideoCapture;
        CComPtr<IBaseFilter> m_pAudioCapture;
        CComPtr<IAMVideoControl> m_pVideoControl;
        CComPtr<IAMCameraControl> m_pCameraControl;
        CComPtr<IAMDroppedFrames> m_pDroppedFrames;
        CComPtr<IAMVideoProcAmp> m_pVideoProcAmp;
        CComPtr<IKsPropertySet> m_pKsPropertySet;
        CComPtr<IAMStreamConfig> m_pStillStreamConfig;
        CComPtr<IAMStreamConfig> m_pCaptureStreamConfig;
        CComPtr<IAMStreamConfig> m_pPreviewStreamConfig;
        CComPtr<IAMStreamConfig> m_pAudioStreamConfig;
        CComPtr<IFileSinkFilter2> m_pStillFileSink;
        CComPtr<IFileSinkFilter> m_pVideoFileSink;
        CComPtr<IPersistPropertyBag> m_pVideoPropertyBag;
        CComPtr<IPersistPropertyBag> m_pAudioPropertyBag;

        CComPtr<IMediaControl> m_pMediaControl;
        CComPtr<IMediaEvent> m_pMediaEvent;
        CComPtr<IBaseFilter> m_pStillSink;
        CComPtr<IPin> m_pStillPin;

        CComPtr<IImageSinkFilter> m_pImageSinkFilter;
        CComPtr<IWMHeaderInfo3> m_pHeaderInfo;
        CComPtr<IBaseFilter> m_pVideoDMOWrapperFilter;
        CComPtr<IBaseFilter> m_pMuxFilter;
        CComPtr<IDMOWrapperFilter> m_pVideoEncoderDMO;
        CComPtr<IBaseFilter> m_pAudioDMOWrapperFilter;
        CComPtr<IDMOWrapperFilter> m_pAudioEncoderDMO;

        CComPtr<IBuffering> m_pAudioBuffering;
        CComPtr<IBuffering> m_pVideoBuffering;

        BOOL m_bForceColorConversion;

        CDShowEventSink m_EventSink;

        CCritSec m_Lock;

        // track the number of frames processed across the media event recieving functions.
        LONG m_lVideoFramesProcessed;
        LONG m_lAudioFramesProcessed;
        BOOL m_bDiskWriteErrorOccured;
        long m_lVideoFramesDropped;
        long m_lAudioFramesDropped;
        BOOL m_bBufferDeliveryFailed;

        UINT m_uiPinCount;

        BOOL m_bOutputKeyEvents;
        BOOL m_bOutputAllEvents;
        BOOL m_bOutputFilterInformation;
        BOOL m_bInsertPreviewDiagnostics;
        BOOL m_bInsertStillDiagnostics;
        BOOL m_bInsertCaptureDiagnostics;

        int m_nQuality;
        int m_nKeyFrameDistance;
        int m_nComplexity;

        // this copy allows us to know what the last successfully set stream format was, and also gives us a place to 
        // store the properties before initialization for setting the property before the stream is rendered.
        AM_MEDIA_TYPE *m_pamtPreviewStream;
        AM_MEDIA_TYPE *m_pamtStillStream;
        AM_MEDIA_TYPE *m_pamtCaptureStream;
        AM_MEDIA_TYPE *m_pamtAudioStream;

        BOOL m_bUseGSM;

        CLSID m_clsidVideoEncoder;
        CLSID m_clsidAudioEncoder;
        CLSID m_clsidMux;
}CAPTUREFRAMEWORK, *PCAPTUREFRAMEWORK;

#endif
