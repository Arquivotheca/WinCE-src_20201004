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
#include "..\\PipelineInterfaces.h"
#include "DvrInterfaces.h"
#include "..\\writer\\recordingset.h"
#include "..\\plumbing\\PipelineManagers.h"
#include "..\\ComponentWorkerThread.h"
#include "..\\Plumbing\\Source.h"

namespace MSDvr
{

class CReader;

class CReaderThread : public CComponentWorkerThread
{
public:
	CReaderThread(CReader &rReader) : m_rReader(rReader) {}
	bool WasSignaledToStop() { return m_fShutdownSignal; }
private:
	// @cmember Thin member which calls through to CReader::StreamingThreadProc.
	DWORD ThreadMain();

	CReader &m_rReader;
};

class CReaderAutoPosition
{
public:
	CReaderAutoPosition(CReader &rReader, LONGLONG tCur) : m_rReader(rReader), m_tCur(tCur) {}
	~CReaderAutoPosition();

private:
	CReaderAutoPosition();
	CReaderAutoPosition(const CReaderAutoPosition&);
	
private:
	CReader &m_rReader;
	LONGLONG m_tCur;
};

class CReader :	public IReader,
				public IPauseBufferCallback,
 				public IPipelineComponentIdleAction,
				public TRegisterableCOMInterface<IFileSourceFilter>,
				public TRegisterableCOMInterface<ISourceVOBUInfo>
{
	friend class CReaderThread;
	friend class CReaderAutoPosition;

public:
	// CReader
	CReader();
	~CReader();
	void Cleanup();
	static DWORD WINAPI StreamingThreadProc(void *p);

	// IPipelineComponent
	void RemoveFromPipeline();
	ROUTE GetPrivateInterface(REFIID riid, void *&rpInterface);
	ROUTE NotifyFilterStateChange(FILTER_STATE eState);

	// IPlaybackPipelineComponent
	unsigned char AddToPipeline(IPlaybackPipelineManager& rcManager);
	ROUTE DecideBufferSize(	IMemAllocator&	riAllocator,
							ALLOCATOR_PROPERTIES& rProperties,
							CDVROutputPin&	rcOutputPin);

	// IPauseBufferCallback
	void PauseBufferUpdated(CPauseBufferData *pData);

	// IReader
	bool CanReadFile(LPCOLESTR wszFullPathName);
	LONGLONG GetMinSeekPosition();
	LONGLONG GetMaxSeekPosition();
	LONGLONG GetCurrentPosition();
	DWORD SetCurrentPosition(LONGLONG newPos, bool fSeekToKeyframe);
	FRAME_SKIP_MODE GetFrameSkipMode();
	DWORD GetFrameSkipModeSeconds();
	DWORD SetFrameSkipMode(FRAME_SKIP_MODE skipMode, DWORD skipModeSeconds);
	bool SetIsStreaming(bool fEnabled);
	bool GetIsStreaming() {return m_fIsStreaming;}
	BYTE *GetCustomData() { return m_cRecSet.GetCustomData(); }
	DWORD GetCustomDataSize() { return m_cRecSet.GetCustomDataSize(); }
	void SetBackgroundPriorityMode(BOOL fUseBackgroundPriority);

	// IFileSourceFilter
	STDMETHODIMP Load(LPCOLESTR wszFileName, const AM_MEDIA_TYPE *);
	STDMETHODIMP GetCurFile(LPOLESTR *ppszFileName,	AM_MEDIA_TYPE *pmt);

	// ISourceVOBUInfo
	STDMETHODIMP GetVOBUOffsets(LONGLONG tPresentation,
								LONGLONG tSearchStartBound,
								LONGLONG tSearchEndBound,
								VOBU_SRI_TABLE *pVobuTable);
	STDMETHODIMP GetRecordingSize(	LONGLONG tStart,
									LONGLONG tEnd,
									ULONGLONG* pcbSize);
	STDMETHODIMP GetRecordingDurationMax(	LONGLONG tStart,
									        ULONGLONG cbSizeMax,
									        LONGLONG* ptDurationMax);
	STDMETHODIMP GetKeyFrameTime(	LONGLONG tTime,
									LONG nSkipCount,
									LONGLONG* ptKeyFrame);
	STDMETHODIMP GetRecordingData(	LONGLONG tStart,
									BYTE* rgbBuffer,
									ULONG cbBuffer,
									ULONG* pcbRead,
									LONGLONG* ptEnd);
	STDMETHODIMP MiniLoad(WCHAR *wszFileName) { return S_OK; }

	/* IPipelineComponentIdleAction */
	virtual void DoAction();

private:

	// @cmember Method that is the guts of the streaming thread.  The reason this was
	// implemented under CReader rather than CReaderThread is that this proc greatly
	// utilizes the contents of the reader class.  So implementing the code here,
	// rather than in CReaderThread, removes a level of indirection when accessing
	// the reader members.
	DWORD StreamingThreadProc(CReaderThread &rThread);
	void ShutdownStreamingThread();

	void PauseBufferUpdatedInternal(CPauseBufferData *pData, bool fOptimizeOpen);

	long GetNavPackOffset(LONGLONG tStart, LONGLONG tSearchStartBound, LONGLONG tSearchEndBound, LONGLONG llInitialOffset, bool fFullSearch);

	CPipelineRouter m_router;						// @cmember The pipeline router
	IPauseBufferMgr *m_pPBMgr;						// @cmember The pausebuffer mgr
	IPlaybackPipelineManager *m_pPipelineMgr;		// @cmember The playbackpipeline manager
	CRecordingSetRead m_cRecSet;					// @cmember Recording set we read from
	IFileSourceFilter *m_pNextFileSource;			// @cmember Next IFileSourceFilter in pipeline
	CReaderThread *m_pThread;						// @cmember Streaming thread
	CCritSec m_csLockReader;						// @cmember Leaf lock for protecting reader
													// data structures across multiple threads
	FILTER_STATE m_filterState;						// @cmember Current state of the filter.
	bool m_fBoundToLive;							// @cmember True iff currently bound to live
	bool m_fRecordingIsComplete;					// @cmember True iff not bound to live and the recording is known to be complete
													// in its recording set.
	bool m_fIsStreaming;							// @cmember True iff streaming thread is in action
	HANDLE m_hIsStreamingEvent;						// @cmember Event to dis/enable streaming thread
	bool m_fForceStreamingThreadLoop;				// @cmember True iff the streaming thread should
													// reset its loop due to a seek
	bool m_fForceStreamingModeChange;				// @cemember True iff the streaming should
													// reset its loop due to change in mode
	SAMPLE_HEADER *m_pCurSampHeader;				// @cmember Sample currently being delivered by
													// the streaming thread - may be NULL depending
													// on where the streaming thread is in its loop.
	FRAME_SKIP_MODE m_skipMode;						// @cmember Current frame skip mode.
	DWORD m_skipModeSeconds;						// @cmember Current frame skip mode interval in seconds.
	bool m_fChoseBufferSize;						// @cmember True once the reader has returned
													// the necessary buffer size.
	IDecoderDriver *m_piDecoderDriver;				// @cmember The decoder driver component -- maintains the
													// idle action registry
	unsigned m_uIncompleteOpenCountdown;			// @cmember The reader may have partially (lazy) opened files
													// in its recording set.
	BOOL m_fEnableBackgroundPriority;				// @cmember Remembers whether threads should use background priorities

	static const m_llSeekInsteadOfScanAtHyperFast = 40000000;	// @cmember When reading at hyper fast modes, if skipping at least this
													// amount between I-frames then we should be seeking instead of scanning.  Much faster
													// that way.

};

} // namespace MSDvr
