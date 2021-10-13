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
#include "recordingset.h"
#include "..\\PauseBuffer.h"

namespace MSDvr
{

class CWriter :	public IWriter,
				public CPauseBufferMgrImpl,
				public TRegisterableCOMInterface<IFileSinkFilter>,
				public TRegisterableCOMInterface<IFileSinkFilter2>,
				public TRegisterableCOMInterface<IStreamBufferCapture>
{
public:
	// CWriter
	CWriter();
	~CWriter();
	void Cleanup();
	CDVRSinkFilter& GetFilter() { return m_pPipelineMgr->GetSinkFilter(); }
	STRMBUF_CAPTURE_MODE GetCaptureMode() { return m_capMode; }

	// @cmember Function that converts the specified recording file pair
	// to temporary.  The file may or may not be part of the current
	// recording set.
	HRESULT ConvertRecordingFileToTempOnDisk(const SPauseBufferEntry &entry);

	// IPipelineComponent
	void RemoveFromPipeline();
	ROUTE GetPrivateInterface(REFIID riid, void *&rpInterface);
	ROUTE ConfigurePipeline(UCHAR iNumPins,
							CMediaType cMediaTypes[],
							UINT iSizeCustom,
							BYTE pbCustom[]);
	virtual ROUTE	NotifyFilterStateChange(
						// State being put into effect
						FILTER_STATE	eState);
	
	// ICapturePipelineComponent
	unsigned char AddToPipeline(ICapturePipelineManager& rcManager);
	ROUTE ProcessInputSample(	IMediaSample& riSample,
								CDVRInputPin& rcInputPin);
	ROUTE NotifyAllocator(	IMemAllocator& riAllocator,
							bool fReadOnly,
							CDVRInputPin &rcInputPin);

	// IWriter
	bool CanWriteFile(LPCTSTR sFullPathName);
	LONGLONG GetMaxSampleLatency();
	LONGLONG GetLatestSampleTime();

	// IFileSinkFilter
	STDMETHODIMP GetCurFile(LPOLESTR *ppszFileName, AM_MEDIA_TYPE *pmt);
	STDMETHODIMP SetFileName(LPCOLESTR wszFileName, const AM_MEDIA_TYPE *pmt);

	// IFileSinkFilter2
	STDMETHODIMP GetMode(DWORD *) {return E_NOTIMPL;}
	STDMETHODIMP SetMode(DWORD) {return E_NOTIMPL;}

	// IStreamBufferCapture
	STDMETHODIMP GetCaptureMode(STRMBUF_CAPTURE_MODE *pMode,
								LONGLONG *pllMaxBufMillisecs);
	STDMETHODIMP BeginTemporaryRecording(LONGLONG llBufferSizeInMillisecs);
	STDMETHODIMP BeginPermanentRecording(	LONGLONG llRetainedSizeInMillisecs,
											LONGLONG *pllActualRetainedSizeInMillisecs);
	STDMETHODIMP ConvertToTemporaryRecording(LPCOLESTR pszFileName);
	STDMETHODIMP SetRecordingPath(LPCOLESTR pszPath);
	STDMETHODIMP GetRecordingPath(LPOLESTR *ppszPath);
	STDMETHODIMP GetBoundToLiveToken(LPOLESTR *ppszToken);
	STDMETHODIMP GetCurrentPosition(/* [out] */ LONGLONG *phyCurrentPosition);

	// IPauseBufferMgr
	CPauseBufferData* GetPauseBuffer();

private:
	void FirePauseBufferUpdated();
	std::wstring GenerateRandomFilename();
	bool IsMPEGNavPack(IMediaSample &rSample);
	void RetryPendingFileDeletions();
	void EnsureSubDirectories(const std::wstring strPath);
	std::wstring GenerateFullFileName(const std::wstring strRecOnly);

	// Same as BeginTemporaryRecording above but doesn't forward.
	HRESULT BeginTemporaryRecordingInternal(LONGLONG llBufferSizeInMillisecs);

	ICapturePipelineManager *m_pPipelineMgr;		// The capture pipeline manager
	CRecordingSetWrite m_cRecSet;					// Recording set we write to
	CRecordingFile* m_pcRecPrev;					// @cmember The last recording file of the
													// previous recording being created.  This
													// enables recording transitions to be made
													// on key frame boundaries.
	std::wstring m_strRecPath;						// Path recordings are made in
	IFileSinkFilter2 * m_pNextFileSink2;			// Next IFileSinkFilter2 in pipeline
	IStreamBufferCapture * m_pNextSBC;				// Next IStreamBufferCapture in pipeline
	CMediaType *m_rgMediaTypes;						// Array of cached media types
	DWORD m_dwNumMediaTypes;						// Number of media types
	BYTE *m_pbCustomPipelineData;					// Custom buffer passed to ConfigurePipeline
	DWORD m_dwCustomPipelineDataLen;				// Length of custom pipeline buffer
	DWORD m_dwNegotiatedBufferSize;					// @cmember Size of the media sample buffers
													// chosen during allocator negotiation.
	CPauseBufferData m_cPB;							// Current pause buffer info.  The writer
													// is cheating a bit by bypassing the
													// AddRef/Release mechanism of the pause
													// buffer.  That's ok though since we'll
													// duplicate this before passing it out.
	bool m_fJustStartedRecording;					// Set in BeginFooRecording and cleared
													// in ProcessInputSample
	std::vector<SPauseBufferEntry> m_vecPBPermFiles;// @cmember We want don't want permanent
													// recordings to fall out one file at a time,
													// but rather all-or-nothing.  So as perm
													// recording files are aged from m_cPB, they
													// are added to this member until the whole
													// permanent recording is aged.
	STRMBUF_CAPTURE_MODE m_capMode;					// @cmember Current capture mode.
	CCritSec m_csRecSetLock;						// @cmember Leaf lock for protecting recording
													// set between app and streaming threads.
	ULONGLONG m_cbMaxPermanentFileSize;				// Maximum size of a permanent recording file
	ULONGLONG m_cbMaxTemporaryFileSize;				// Maximum size of a temporary recording file
	LONGLONG m_dwBytesWritten;						// The total number of media sample actual data
													// bytes that have been written.  This is
													// never reset during the life of the CWriter.
	REFERENCE_TIME m_tLastDiscontinuity;			// The media time of the last discontinuity the
													// writer has written to disk.

	LONGLONG m_hyGetTimeValueOfLastSampleWritten;
	std::vector<std::wstring> m_vecPendingFileDeletes;
	CComPtr<IMediaEventSink> m_piMediaEventSink;	// Used for issuing advisories about long sample writes
	DWORD m_dwLastHaveAVEvent;
};

} // namespace MSDvr