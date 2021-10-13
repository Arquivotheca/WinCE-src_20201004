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
////////////////////////////////////////////////////////////////////////////////

#ifndef TEST_GRAPH_H
#define TEST_GRAPH_H

#include <dshow.h>
#include <dvdmedia.h>
#include <mmreg.h>
#include "FilterDesc.h"
#include "GraphDesc.h"
#include "Media.h"
#include "GraphEvent.h"
#include "Verification.h"
#include "EventLogger.h"
#include "TapFilter.h"
#include "VideoRenderer.h"
#include "Timer.h"

using namespace std;

typedef basic_string<_TCHAR> tstring;


#define MAX_STREAMS 4
#define MAX_AUDIO 2
#define MAX_VIDEO 2

#define WSTR_SourceFilter L"SourceFilter"

typedef map<int, IGraphVerifier*> VerifierList;
typedef pair <int, IGraphVerifier*> VerifierPair;

// Utility methods and classes

// Get the string correspondign to a graph state
extern const TCHAR* GetStateName(FILTER_STATE state);

// TestGraph
// A facade that encapsulates common functions that are required by all tests
class TestGraph
{
public:
	enum Flags 
	{
		SYNCHRONOUS = 32
#if 0
		START_SOURCE = 1,
		START_SINK = 2,
		DEPTH_FIRST = 4,
		BREADTH_FIRST = 8,
		ASYNCHRONOUS = 16,
		START_RENDER = 64
#endif
	};

	enum {
		STATE_CHANGE_TIMEOUT_MS = 10000
	};

private:
	// The Graph Builder interface
	IGraphBuilder* m_pGraphBuilder;

	// Interfaces
	IMediaControl* m_pMediaControl;
	IMediaSeeking* m_pMediaSeeking;
	IMediaPosition* m_pMediaPosition;
	IMediaEvent* m_pMediaEvent;
	IBasicVideo* m_pBasicVideo;
	IVideoWindow* m_pVideoWindow;
	IBasicAudio* m_pBasicAudio;
	IDirectDrawVideo* m_pDirectDrawVideo;

	// The pointers to important filters
	IBaseFilter* m_pSourceFilter;
	IBaseFilter* m_pAudioRenderer;
	IBaseFilter* m_pVideoRenderer;
	IBaseFilter* m_pAudioDecoder;
	IBaseFilter* m_pVideoDecoder;
	IBaseFilter* m_pColorConverter;

#ifdef UNDER_CE
	// This is only available on CE
	// The IAMVideoRendererMode interface
	IAMVideoRendererMode* m_pVRMode;
#endif

	// Currently set media
	Media* m_pMedia;

	// Has the graph been built yet?
	bool m_bGraphBuilt;

	// The current list of filters in the graph - all not necessarily connected.
	FilterDescList m_FilterList;

	// The list of connections
	vector<ConnectionDesc*> m_ConnectionList;

	// The list of video stream info

	// Duration
	LONGLONG m_Duration;

	// Seeking caps
	DWORD m_SeekCaps;

	// Preroll (in ms)
	DWORD m_Preroll;

	// Does this media contain multiple language streams?
	bool m_bMultiLanguageAudio;

	// Does this media contain multiple bitrate video streams? 
	bool m_bMultiBitRateVideo;

	// Is this stream protected?
	bool m_bDRM;

	// The number of video streams
	int m_nVideoStreams;

	// The number of video streams
	int m_nAudioStreams;

	// The number of text streams
	int m_nTextStreams;

	// The number of undetermined streams
	int m_nUndeterminedStreams;

	// Verifier
	VerifierList m_VerifierList;

	// Logger list
	EventLoggerList m_EventLoggerList;

	// Latency timer
	Timer m_LatencyTimer;

	// The positions set
	LONGLONG m_StartPosSet;
	LONGLONG m_StopPosSet;

	// The rate set
	double m_RateSet;

	// The video window parameters set
	LONG m_VideoWindowWidthSet;
	LONG m_VideoWindowHeightSet;

private:
	// Helper methods
	// Create an empty graph with no filters
	HRESULT CreateEmptyGraph();
	
	// Identify the filters in the graph by their names
	HRESULT IdentifyFilters();

	// Determine the filters in the graph
	HRESULT DetermineFilterList();

	// Determine the number of streams in the graph
	HRESULT DetermineStreams();

	// Look for the primary source filter, renderers, decoders etc
	HRESULT DeterminePrimaryFilters();

	// Try to determine the system stream characteristics from the filter
	HRESULT DetermineSystemStreamFromSplitter(FilterDesc* pSplitterDesc, Media* pMedia);

	// Acquire the IAMVideoRendererMode interface
	HRESULT AcquireVRModeIface();

	//Load the URL into the source filter using the IFileSourceFilter interface
	HRESULT LoadURL(FilterDesc* pFromFilter);

	// Helper methods to get properties from media types
	HRESULT GetVideoFormat(VideoInfo* pVideo, AM_MEDIA_TYPE* pMediaType);
	HRESULT GetVideoInfoFormat(VideoInfo* pVideo, VIDEOINFOHEADER* pVideoInfoHeader);
	HRESULT GetVideoInfoFormat2(VideoInfo* pVideo, VIDEOINFOHEADER2* pVideoInfoHeader);
	HRESULT GetBitmapInfoHeader(VideoInfo* pVideo, BITMAPINFOHEADER* pBitmapInfoHeader);
	HRESULT GetMPEG1VideoInfo(VideoInfo* pVideo, MPEG1VIDEOINFO* pMPEGVideoInfo);
	HRESULT GetMPEG2VideoInfo(VideoInfo* pVideo, MPEG2VIDEOINFO* pMPEGVideoInfo);
	HRESULT GetMPEG1WaveFormat(AudioInfo* pAudio, MPEG1WAVEFORMAT* pMPEGWfex);
	HRESULT GetWaveFormatEx(AudioInfo* pAudio, WAVEFORMATEX* pWfex);
	HRESULT GetAudioFormat(AudioInfo* pAudio, AM_MEDIA_TYPE* pMediaType);

public:
	TestGraph();
	~TestGraph();

	// Methods to query the test graph

	// Has a source filter been added yet?
	bool IsAddedSourceFilter();

	// Has the graph been completely built?
	bool IsGraphBuilt();

	// Have the renderer filters been added yet?
	bool IsAddedVideoRendererFilters();
	bool IsAddedAudioRendererFilters();

	// The number of video streams
	int GetNumVideoStreams();

	// The number of audio streams
	int GetNumAudioStreams();

	// The number of text streams
	int GetNumTextStreams();

	// Get the width of the video stream
	HRESULT GetVideoWidth(long *pWidth);

	// Get the height of the video stream
	HRESULT GetVideoHeight(long *pHeight);

	// Set the source of the video
	HRESULT SetSourceRect(long left, long top, long width, long height);

	// Set the destination of the video
	HRESULT SetDestinationRect(long left, long top, long width, long height);

	// The number of unknown streams
	int GetNumUndeterminedStreams();

	// Query if filter(s) are in the graph and connected
	HRESULT AreFiltersInGraph(StringList filterNameList);
	
	// The following methods are used by the test functions to build composite tests.
	// These change the internal state of the test graph
	
	// Resets the testgraph object. Releases all resources and puts it in the initial state.
	void Reset();

	// Release filter list
	void ReleaseFilterList();


	// Set the media for the graph
	HRESULT SetMedia(Media* pMedia);

	// Get the currently set media object
	Media* GetCurrentMedia();

	// Build an empty graph
	HRESULT BuildEmptyGraph();

	// Get the Graph Builder interface
	IGraphBuilder* GetGraph();

	// Intelligently add a source filter corresponding to the media specified
	HRESULT AddSourceFilter();

	// Add a specific source filter to the graph
	HRESULT AddSourceFilter(const TCHAR* szFilterName, TCHAR* szNewFilterName, int length);

	// Add the default renderer filters to the graph for the media specified. This method will add the 
	HRESULT AddDefaultRendererFilters();

	// Add the default video renderer filter to the graph for the media specified.
	HRESULT AddDefaultVideoRenderer();

	// Add the default audio renderer filter to the graph for the media specified.
	HRESULT AddDefaultAudioRenderer();

	// Add the filter by id. Get back the new filter name if not NULL.
	HRESULT AddFilter(FilterId id, TCHAR* szNewFilterName = NULL, int length = 0);

	// Build the complete playback graph for the specified media
	HRESULT BuildGraph();
	HRESULT BuildGraph(Media* pMedia);


	// If building manually, set as built
	HRESULT SetBuilt( bool built = true );

	// Is the graph built?
	bool IsBuilt();

	// Search for an interface implemented by the filters.
	HRESULT FindInterfaceOnFilter(REFIID riid, void **ppInterface);

	// Add a "tap" filter to the filter graph between two pins
	HRESULT AddTapFilter(ITapFilter* pTapFilter, IPin* pFromPin, IPin* pToPin);

	// Get the interfaces that are needed. If some interface is not essential then no error is returned. Error is returned for an essential interface.
	HRESULT AcquireInterfaces();
	
	// Release acquired interfaces
	void ReleaseInterfaces();

	//Render the unconnected output pins of the graph - there should be a source filter already inserted
	HRESULT RenderUnconnectedPins();

	// Connect 2 pins on 2 filters using intelligent connect
	HRESULT Connect(const TCHAR* szFromFilter, int iFromPin, const TCHAR* szToFilter, int iToPin, AM_MEDIA_TYPE * pMediaType = NULL);

	// Connect 2 pins on 2 filters directly - no intelligent connect
	HRESULT ConnectDirect(const TCHAR* szFromFilter, const TCHAR* szToFilter, int iFromPin, int iToPin, AM_MEDIA_TYPE * pMediaType = NULL);

	// Set the state (synchronously/asynchronously) and measure time taken to do so. If unable to measure, then the testgraph will return 0.
	HRESULT SetState(FILTER_STATE state, DWORD flags = 0, DWORD *pTime = NULL);

	// StopWhenReady delegation to IMediaControl
	HRESULT StopWhenReady();

	// Get the state. 
	// If in the middle of a transition, then the current state returned is Transitioning. 
	// If Transitioning is returned, the prev and next state are set appropriately
	// Otherwise one of the 3 valid states is returned. The prev state is set appropriately.
	HRESULT GetState(int *pCurrState, int *pPrevState, int* pNextState);
	
	// Run the graph to completion
	HRESULT RunGraphToCompletion();

	// Run the graph until EC_COMPLETE is received, error or timeout occurs. Specify timeout in msec.
	HRESULT WaitForCompletion(DWORD timeout, LONG *evCode = 0 );

	// Poll the event queue until the 
	HRESULT WaitForEvent(long event, DWORD timeout);

	// Get the wait event handle
	HRESULT GetEventHandle(HANDLE* pEvent);

	// Is the graph description equivalent to the one that we have built?
	HRESULT IsGraphEquivalent(GraphDesc* pGraphDesc);

	// Are the specified connections in the graph
	HRESULT AreConnectionsInGraph(ConnectionDescList* pConnectionList);

	// Set the absolute start and stop positions
	HRESULT SetAbsolutePositions(LONGLONG start, LONGLONG stop);

	// Set the start/stop position
	HRESULT SetPosition(LONGLONG start, bool bStartPos = true );

	// Set the rate
	HRESULT SetRate(double rate);

	// Get the currently set start and stop positions (not the current position)
	void GetPositionSet(LONGLONG *pStart, LONGLONG* pStop);

	// Get the current rate
	HRESULT GetRateSet(double* rate);

	// Get the rate that was set
	HRESULT GetRate(double* rate);

	// Get the current position
	HRESULT GetCurrentPosition(LONGLONG* pCurrent);

	// Get the current start and stop positions
	HRESULT GetPositions(LONGLONG* pCurrent, LONGLONG *pStop);

	// Get the duration of playback
	HRESULT GetDuration(LONGLONG* pDuration);

	// Gets the video window position and dimensions
	HRESULT GetVideoWindowPosition(long* pLeft, long* pTop, long* pWidth, long* pHeight);

	// Set the position of the video window and dimensions
	HRESULT SetVideoWindowPosition(long left, long top, long width, long height);

	// Enable/disable full screen mode
	HRESULT SetFullScreenMode(bool bFullScreen);

	// Set min/max image size
	HRESULT SetVRImageSize(VRImageSize imageSize);

	// Set the size of the video window
	HRESULT SetVideoWindowSize(long width, long height);

	// Get the set size of the video window
	HRESULT GetVideoWindowSizeSet(long* pWidth, long* pHeight);

	// Get the size of the video window
	HRESULT GetVideoWindowSize(long* pWidth, long* pHeight);

	// Set the video renderer mode
	HRESULT SetVideoRendererMode(VideoRendererMode mode);

	// Set the video renderer surface types
	HRESULT SetVideoRendererSurfaceType(DWORD surfaceMode);

	// Get the video renderer surface type
	HRESULT GetVideoRendererSurfaceType(DWORD* pSurfaceType);

	// Get the video renderer mode
	HRESULT GetVideoRendererMode(VideoRendererMode* pVRMode);

	// Set the maximum back-buffers registry
	HRESULT SetMaxVRBackBuffers(DWORD dwMaxBackBuffers);

	// Enable/disable primary flipping
	HRESULT SetVRPrimaryFlipping(bool bAllowPrimaryFlipping);

	// Enable/disable the scan line usage regkey of the VR
	HRESULT SetVRScanLineUsage(bool bUseScanLine);

	// Are we capable of this verification?
	bool IsCapable(VerificationType type);

	// Add a custom verifier
	HRESULT AddVerifier(VerificationType type, IGraphVerifier* pVerifier);

	// Enable verification
	HRESULT EnableVerification(VerificationType type, void* pVerificationData, IGraphVerifier** ppVerifier = NULL);

	// Release the verifiers
	void ReleaseVerifiers();

	// Start the verification
	HRESULT StartVerification();

	// Stop the verification
	void StopVerification();

	// Reset the verification and make it ready for another time
	void ResetVerification();

	// Enable a specific logger.
	HRESULT EnableLogging(EventLoggerType type, void* pEventLoggerData);

	// Start all the loggers.
	HRESULT StartLogging();

	// Stop all the loggers.
	void StopLogging();

	// Reset the loggers for the next run.
	void ResetLogging();

	// Release the logger resources
	void ReleaseLoggers();

	// Get the result of the verification
	HRESULT GetVerificationResult(VerificationType type, void* pVerificationResult, TCHAR* pResultStr);

	// Get a cumulative result from all the verifiers
	HRESULT GetVerificationResults();

	// Get a pointer to the filter using the id
	IBaseFilter* GetFilter(FilterId id);

	// Get the filter desc given filter class and sub-class 
	FilterDesc* GetFilterDesc(DWORD filterClass, DWORD filterSubClass);
	FilterDesc* GetFilterDesc(DWORD filterType);

	// Get the filter desc object given name
	FilterDesc* GetFilter(const TCHAR* szFromFilter);
	FilterDesc* GetFilter(IBaseFilter* pBaseFilter);

	// Get the list of filter object
	FilterDescList* GetFilterDescList();


	// Insert a tap filter
	HRESULT InsertTapFilter(IPin* pOutPin, ITapFilter** ppTapFilter);
	HRESULT InsertTapFilter(DWORD filter, PIN_DIRECTION pindir, GUID majortype, ITapFilter** ppTapFilter);
	HRESULT DisconnectTapFilter(DWORD filter, GUID majortype);
	
	// Utility methods
	FILTER_STATE GetRandomState();

	// Get the media properties
	HRESULT GetMediaInfo(Media* pMedia);

	// Get the system stream properties
	HRESULT GetSystemInfo(Media* pMedia);

	// Get the video stream properties
	HRESULT GetVideoInfo(Media* pMedia);

	// Get the audio stream properties 
	HRESULT GetAudioInfo(Media* pMedia);
	

	// Get interface methods
	HRESULT GetMediaControl(IMediaControl** ppMediaControl);
	HRESULT GetMediaSeeking( IMediaSeeking **ppMediaSeeking );
	HRESULT GetMediaPosition( IMediaPosition **ppMediaPosition );
	HRESULT GetMediaEvent( IMediaEvent **ppMediaEvent );
	HRESULT GetDirectDrawVideo( IDirectDrawVideo** pDirectDrawVideo );
	HRESULT GetBasicVideo( IBasicVideo **pBasicVideo );
	HRESULT GetVideoWindow( IVideoWindow **pVideoWindow );

};
#endif