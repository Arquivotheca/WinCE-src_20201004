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
#include "stdafx.h"
#include "reader.h"
#include "..\\PauseBuffer.h"
#include "..\\SampleProducer\\ProducerConsumerUtilities.h"
#include "..\\DVREngine.h"
#include "AVGlitchEvents.h"

using std::vector;

#define DELAY_UNTIL_FINISH_OPEN		10
#define SERIOUSLY_LONG_READ_TIME	500		/* 500 ms */

namespace MSDvr
{
	// In order to support UI on the system info page with 'a/v status codes' for
	// shipping builds, create a helper class to monitor for long reads and do
	// notification:

	class CReaderReadTimeNotifier
	{
	public:
		CReaderReadTimeNotifier(CReader &rcReader, IDecoderDriver *piDecoderDriver)
			: m_rcReader(rcReader)
			, m_piDecoderDriverToNotify(piDecoderDriver)
			, m_dwReadStartTime(GetTickCount())
		{
		}

		~CReaderReadTimeNotifier()
		{
			if (m_piDecoderDriverToNotify)
			{
				DWORD dwTimeNow = GetTickCount();
				DWORD dwInterval = 0;
				if (dwTimeNow > m_dwReadStartTime)
					dwInterval = dwTimeNow - m_dwReadStartTime;
				else
					dwInterval = 1 + dwTimeNow + (0xffffffff - m_dwReadStartTime);
				if (dwInterval >= SERIOUSLY_LONG_READ_TIME)
				{
					try {
						if (m_piDecoderDriverToNotify)
						{
							m_piDecoderDriverToNotify->IssueNotification(&m_rcReader, AV_GLITCH_LONG_DVR_READ, dwTimeNow, dwInterval, true, false);
						}
					} catch (const std::exception &) {};
				}
			}
		}

	private:
		CReader & m_rcReader;
		IDecoderDriver *m_piDecoderDriverToNotify;
		DWORD m_dwReadStartTime;
	};

	class CReaderLoadFailureNotifier
	{
	public:
		CReaderLoadFailureNotifier(CReader &rcReader, IPlaybackPipelineManager *piPipelineMgr)
			: m_rcReader(rcReader)
			, m_pPipelineMgr(piPipelineMgr)
			, m_fLoadSucceeded(false)
		{
		}

		~CReaderLoadFailureNotifier()
		{
			if (!m_fLoadSucceeded && m_pPipelineMgr)
			{
				DWORD dwTimeNow = GetTickCount();
				CBaseFilter &rcBaseFilter = m_pPipelineMgr->GetFilter();
				IFilterGraph *piFilterGraph = rcBaseFilter.GetFilterGraph();
				CComPtr<IMediaEventSink> piMediaEventSink;
				if (piFilterGraph)
				{
					HRESULT hrQI = piFilterGraph->QueryInterface(IID_IMediaEventSink, (void **)&piMediaEventSink.p);
					if (FAILED(hrQI))
					{
						piMediaEventSink.p = NULL;
					}
				}

				if (piMediaEventSink)
				{
					piMediaEventSink->Notify(AV_GLITCH_DVR_LOAD_FAILED, dwTimeNow, 0);
				}
			}
		}

		void SetLoadSuccess() { m_fLoadSucceeded = true; }

	private:
		CReader & m_rcReader;
		IPlaybackPipelineManager *m_pPipelineMgr;
		bool m_fLoadSucceeded;
	};

	CReader::CReader()
	:	m_pPipelineMgr(NULL),
    	m_pThread(NULL),
		m_filterState(State_Stopped),
		m_fBoundToLive(false),
		m_pPBMgr(NULL),
		m_fIsStreaming(true),
		m_hIsStreamingEvent(CreateEvent(NULL, TRUE, TRUE, NULL)),
		m_pCurSampHeader(NULL),
		m_skipMode(SKIP_MODE_NORMAL),
		m_fChoseBufferSize(false),
		m_fForceStreamingModeChange(false),
		m_fForceStreamingThreadLoop(false),
		m_skipModeSeconds(0),
		m_piDecoderDriver(NULL),
		m_uIncompleteOpenCountdown(0),
		m_fRecordingIsComplete(false),
		m_fEnableBackgroundPriority(FALSE)

{
	g_cDVREngineHelpersMgr.RegisterReader(this);

	// Make sure our IsStreaming event was created
	if (!m_hIsStreamingEvent)
		throw std::bad_alloc("Error creating event(m_hIsStreamingEvent)");
}

CReader::~CReader()
{
	Cleanup();
}

void CReader::RemoveFromPipeline()
{
	Cleanup();
}

void CReader::Cleanup()
{
	g_cDVREngineHelpersMgr.UnregisterReader(this);

	if (m_pPBMgr)
	{
		m_pPBMgr->CancelNotifications(this);
		m_pPBMgr = NULL;
	}
	if (m_piDecoderDriver)
	{
		m_piDecoderDriver->UnregisterIdleAction(this);
		m_piDecoderDriver = NULL;
	}

	m_pPipelineMgr = NULL;
	m_cRecSet.Close();
	m_filterState = State_Stopped;
	ShutdownStreamingThread();
	if (m_hIsStreamingEvent)
	{
		CloseHandle(m_hIsStreamingEvent);
		m_hIsStreamingEvent = NULL;
	}
	m_fIsStreaming = false;
	DbgLog((LOG_ENGINE_OTHER, 3, _T("CReader::Cleanup():  freed the streaming event, set to not-streaming\n")));
}

unsigned char CReader::AddToPipeline(IPlaybackPipelineManager& rcManager)
{
	m_fIsStreaming = true;
	if (!m_hIsStreamingEvent)
		m_hIsStreamingEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
	else
		SetEvent(m_hIsStreamingEvent);
	DbgLog((LOG_ENGINE_OTHER, 3, _T("CReader::AddToPipeline():  ensured have event in set streaming, m_fIsStreaming true\n")));

	// Keep the pipeline mgr
	m_pPipelineMgr = &rcManager;

	// Grab a synchronous pipeline router.  We'll get an asynchronous one when
	// CReader::Load is called because some components may not have been added
	// to the pipeline yet.
	m_router = m_pPipelineMgr->GetRouter(NULL, false, false);

	// If I call m_pPipelineMgr->IPipelineManager::RegisterCOMInterface without
	// going throug this temporary variable I get an unresolved external symbol.
	IPipelineManager *pBasePipelineMgr = m_pPipelineMgr;

	// Register our com interfaces
	m_pNextFileSource = (IFileSourceFilter *)
						pBasePipelineMgr->RegisterCOMInterface(
						*((TRegisterableCOMInterface<IFileSourceFilter> *) this));

	pBasePipelineMgr->RegisterCOMInterface(
						*((TRegisterableCOMInterface<ISourceVOBUInfo> *) this));

	// Get the pause buffer mgr and register for update notifications
	ASSERT(!m_pPBMgr);
	void *pPBMgr = NULL;
	ROUTE eRoute = m_router.GetPrivateInterface(IID_IPauseBufferMgr, pPBMgr);
	if ((eRoute == HANDLED_STOP) || (eRoute == HANDLED_CONTINUE))
		m_pPBMgr = static_cast<IPauseBufferMgr*>(pPBMgr);

	if (m_pPBMgr)
		m_pPBMgr->RegisterForNotifications(this);

	return 1;
}

ROUTE CReader::GetPrivateInterface(REFIID riid, void *&rpInterface)
{
    if (riid == IID_IReader)
    {
        rpInterface = ((IReader *) this);
        return HANDLED_STOP;
    }
    return UNHANDLED_CONTINUE;
}

ROUTE CReader::DecideBufferSize(IMemAllocator&	riAllocator,
								ALLOCATOR_PROPERTIES& rProperties,
								CDVROutputPin&	rcOutputPin)
{
	// Determine the properties that we want
	ALLOCATOR_PROPERTIES propReq;
    // TODO: a-angusg: updated the buffer count from 2 to 10 to allow the DVRNav filter to work
    //       This number needs to be optimized (i.e. reduced) at some point in the future
	propReq.cBuffers = max(rProperties.cBuffers, 6);
	propReq.cbBuffer = max(rProperties.cbBuffer, (long) m_cRecSet.GetMaxSampleSize());
	propReq.cbAlign = max(rProperties.cbAlign, 1);
	propReq.cbPrefix = max(rProperties.cbPrefix, 0);

	// Set the properties on
	if (FAILED(riAllocator.SetProperties(&propReq, &rProperties)))
	{
		// Unexpected problem
		throw std::runtime_error("Error setting allocator properties.");
	}

	m_fChoseBufferSize = true;

	return HANDLED_CONTINUE;
}

void CReader::ShutdownStreamingThread()
{
	if (m_pThread)
	{
		// Shutdown streaming thread
		m_pThread->SignalThreadToStop();
		SetEvent(m_hIsStreamingEvent);
		m_pThread->SleepUntilStopped();
		m_pThread->Release();
		m_pThread = NULL;
	}
	{
		DbgLog((LOG_SOURCE_STATE, 3, _T("CReader::ShutdownStreamingThread() ... ensuring consistent state for starting up later\n")));
		CAutoLock al(&m_csLockReader);

		if (m_fIsStreaming)
			SetEvent(m_hIsStreamingEvent);
		else
			ResetEvent(m_hIsStreamingEvent);
	}
}

STDMETHODIMP CReader::GetCurFile(LPOLESTR *ppszFileName, AM_MEDIA_TYPE *pmt)
{
	// Don't chain this method.

	if (!m_cRecSet.IsOpen())
		return E_FAIL;
		
	if(pmt)
	{
		memset(pmt, 0, sizeof(*pmt));
		pmt->majortype = MEDIATYPE_NULL;
		pmt->subtype = MEDIASUBTYPE_NULL;
	}
	DbgLog((LOG_FILE_OTHER, 3, TEXT("CReader::GetCurFile() returning %s\n"),
			m_cRecSet.GetFileName().c_str() ));
	return WStringToOleString(m_cRecSet.GetFileName(), ppszFileName);
}

ROUTE CReader::NotifyFilterStateChange(FILTER_STATE eState)
{
	DbgLog((LOG_SOURCE_STATE, 3, _T("CReader::NotifyFilterStateChange(%d):  prior state %d\n"), eState, m_filterState));

	// Make sure out input is valid
	if ((eState != State_Paused) && (eState != State_Stopped) && (eState != State_Running))
		return UNHANDLED_STOP;

	// Make sure we're not making an invalid transition
	if ((m_filterState == eState) ||
		((m_filterState == State_Stopped) && (eState == State_Running)))
		return UNHANDLED_STOP;

	// If the graph is going from paused to running or vice versa
	if ((m_filterState != State_Stopped) && (eState != State_Stopped))
	{
		// No work necessary here.  When the renderer is paused, buffers from the
		// allocator won't free up so we'll naturally get paused.  When the graph
		// is unpaused, buffers will start freeing up again.

		// Save the new state and bail
		m_filterState = eState;
		return HANDLED_CONTINUE;
	}

	// If we're stopping the graph
	if (eState == State_Stopped)
	{
		ShutdownStreamingThread();

		// We're intentionally not closing the rec set here.  This let's the graph
		// get re-run and the other IReader methods still work on the existing
		// rec set.

		// Save the new state and bail
		m_filterState = eState;
		return HANDLED_CONTINUE;
	}

	// The only transition left is stopped->running.
	// So start up the streaming thread.

	ASSERT(eState == State_Paused);
	ASSERT(m_filterState == State_Stopped);

	// Save the new state
	m_filterState = eState;

	// Create a new thread

	// Heads up:  parental control needs to be able to run a sequence of
	// actions in a fashion that is effectively atomic with respect to
	// a/v reaching the renderers. Right now, parental control is doing
	// so by raising the priority of the thread in PC to a registry-supplied
	// value (PAL priority 1 = CE priority 249). If you change the code
	// here to raise the priority of the DVR engine playback graph
	// streaming threads, be sure to check that either these threads or
	// the DVR splitter output pin threads are still running at a priority
	// less urgent than parental control.

	ASSERT(!m_pThread);
	m_pThread = new CReaderThread(*this);
	m_pThread->StartThread();
	return HANDLED_CONTINUE;
}

DWORD CReaderThread::ThreadMain()
{
#ifdef _WIN32_WCE
	// It may help tuning and other trick modes to make this normally-sleepy thread
	// have a little higher priority so that it can wake up and respond quickly
	// if there is a need to flush, change rate, or seek.
	CeSetThreadPriority(GetCurrentThread(), m_rReader.m_fEnableBackgroundPriority ? g_dwReaderPriority + 1 : g_dwReaderPriority);
#endif // _WIN32_WCE

	// Retrieve the reader from the cookie
	return m_rReader.StreamingThreadProc(*this);
}

HRESULT CReader::Load(LPCOLESTR wszFileName, const AM_MEDIA_TYPE *pmt)
{
	CReaderLoadFailureNotifier cReaderLoadFailureNotifier(*this, m_pPipelineMgr);

	RETAILMSG(1, (L"*** DVREngine::Reader attempting to load %s\n", wszFileName));

	// bugbug todo : Here's the problem.  Once we've loaded a file
	// into the DVR source, the buffer size gets chosen.  When we then try
	// to load a new recording, m_cRecSet is closed.  That clears
	// m_cRecSet.m_dwMaxSampleSize.  Then when AddFile is called, we pass
	// in m_fChoseBufferSize(true) which causes a failure in the recording
	// set since the necessary buffer size is > than the stored buffer size (0).
	// So reset this to false for the time being.  The real solution would
	// be to store the max buffer size in the reader and pass that in to
	// AddFile.
	m_fChoseBufferSize = false;
	m_fRecordingIsComplete = false;

	if (!wszFileName)
		return E_INVALIDARG;

	// Make sure we're in a valid state
	if ((m_filterState != State_Stopped) || m_pThread ||!m_pPipelineMgr)
		return XACT_E_WRONGSTATE;

	if (!m_hIsStreamingEvent)
	{
		m_hIsStreamingEvent = CreateEvent(NULL, TRUE, m_fIsStreaming ? TRUE : FALSE, NULL);
		DbgLog((LOG_SOURCE_STATE, 3, _T("CReader::Load():  ensured have event that matches m_fIsStreaming (%u)\n"),
			(unsigned) m_fIsStreaming));
	}

	// Make sure the existing recording set is closed
	m_cRecSet.Close();

	// If wszFileName is null or empty, then we don't want to load anything
	if (!wszFileName || !*wszFileName)
 	{
		cReaderLoadFailureNotifier.SetLoadSuccess();

		return S_OK;
	}

	// See if we're binding to live or the recording.
	m_fBoundToLive = CSampleProducerLocator::IsLiveTvToken(wszFileName);
	if (!m_fBoundToLive)
	{
		// If we binding to a recording, verify that it is not a deleted recording:

		if (IsRecordingDeleted(wszFileName))
		{
			DbgLog((LOG_FILE_OTHER, 3, TEXT("CReader::Load() -- rejecting deleted file %s\n"), wszFileName ));
			return E_FAIL;
		}
	}

	// Open the recording set
	if (ERROR_SUCCESS != m_cRecSet.Open(wszFileName, m_fBoundToLive, !m_fBoundToLive))
	{
		_CHR(E_FAIL);
	}

	// Now forward the call on.  When bound-to-live, the guts of our
	// load have NOT been done.  Where as when bound-to-recording, the
	// bulk of the work IS done.
	if (m_pNextFileSource)
	{
		_CHR(m_pNextFileSource->Load(wszFileName, pmt));
	}

	// If we're hooked up to the pause buffer, then add files to the recording set
	if (m_fBoundToLive)
	{
		// Make sure we got a pause buffer mgr.
		if (!m_pPBMgr)
		{
			_CHR(E_FAIL);
		}

		// Get a pause buffer from the PB manager.  GetPauseBuffer() returns a pointer
		// to a CPauseBufferData whose reference count is already been incremented.
		// So we need to attach rather than construct or assign to pPB.
		CSmartRefPtr<CPauseBufferData> pPB;
		pPB.Attach(m_pPBMgr->GetPauseBuffer());
		if (!pPB)
		{
			_CHR(E_FAIL);
		}

		// We can't handle a totally empty pause buffer since we need at least
		// 1 file for getting the media type, max sample size, etc.
		if (pPB->GetCount() <= 0)
		{
			_CHR(E_FAIL);
		}

		// Add the contents of the pause buffer to the rec set.
		try { PauseBufferUpdatedInternal(pPB, true); }
		catch (const std::exception&)
		{
			_CHR(E_FAIL);
		}

		// If we successfully loaded but don't have any files in our
		// recording set.  That means that we have a single empty
		// recording file in our pause buffer.  Handle this case.
		if (m_cRecSet.m_vecFiles.size() == 0)
		{
			ASSERT(pPB->GetCount() == 1);
			ASSERT((*pPB)[0].tStart == -1);

			// Add our file
			if (m_cRecSet.AddFile((*pPB)[0].strFilename.c_str(), !m_fChoseBufferSize))
			{
				_CHR(E_FAIL);
			}
			// The AddFile() above was not a lazy open so no need to claim that we might've
			// done a lazy/optimized open.
		}
	}
	else
	{
		// Seek the reader to the beginning of the recording.  This might
		// not be the beginning of the file if the permanent recording has
		// some 'bonus' or rather, unwanted temporary, content.
		LONGLONG tMin = GetMinSeekPosition();
		if (SetCurrentPosition(tMin, true))
		{
			_CHR(E_FAIL);
		}

		// Make sure we know whether the recording is now complete:
		size_t uNumFiles = m_cRecSet.m_vecFiles.size();
		if ((uNumFiles > 0) && !m_cRecSet.m_vecFiles[uNumFiles-1]->GetHeader().fInProgress)
		{
			DbgLog((LOG_PAUSE_BUFFER, 3, TEXT("CReader::Load():  recording is complete\n") ));
			m_fRecordingIsComplete = true;
		}
	}

	// Configure the pipeline.
	m_router.ConfigurePipeline(	(UCHAR) m_cRecSet.GetNumStreams(),
								m_cRecSet.GetStreamTypes(),
								m_cRecSet.GetCustomDataSize(),
								m_cRecSet.GetCustomData());

	// Now grab an asychronous pipeline router which will be used subsequently
	m_router = m_pPipelineMgr->GetRouter(NULL, false, false);

	if (m_piDecoderDriver)
	{
		m_piDecoderDriver->UnregisterIdleAction(this);
		m_piDecoderDriver = NULL;
	}
	if (!m_piDecoderDriver && m_pPipelineMgr)
	{
		CPipelineRouter cPipelineRouter = m_pPipelineMgr->GetRouter(NULL, false, false);
		void *pvDecoderDriver = NULL;
		cPipelineRouter.GetPrivateInterface(IID_IDecoderDriver, pvDecoderDriver);
		m_piDecoderDriver = static_cast<IDecoderDriver*>(pvDecoderDriver);
		if (m_piDecoderDriver)
			m_piDecoderDriver->RegisterIdleAction(this);
	}
	m_uIncompleteOpenCountdown = DELAY_UNTIL_FINISH_OPEN;

	RETAILMSG(1, (L"*** DVREngine::Reader successfully loaded %s\n", wszFileName));

	cReaderLoadFailureNotifier.SetLoadSuccess();

	return S_OK;
}

void CReader::PauseBufferUpdated(CPauseBufferData *pData)
{
	PauseBufferUpdatedInternal(pData, true);
}

void CReader::PauseBufferUpdatedInternal(CPauseBufferData *pData, bool fOptimizeOpen)
{
	DbgLog((LOG_PAUSE_BUFFER, 3, TEXT("CReader::PauseBufferUpdatedInternal(%s) -- entry\n"), fOptimizeOpen ? TEXT("optimized-open") : TEXT("normal-operation") ));

	// Protect the recording set
	CAutoLock al(&m_csLockReader);

	bool fNoFilesAtEntry = m_cRecSet.m_vecFiles.empty();
	// Handle the easy case - simply playing back a recording
	if (!m_fBoundToLive)
	{
		// Add any new files
		//
		// File I/O is a serious bottleneck. Rather than spend a ton of time
		// and create disk contention by always searching for files in the
		// recording [reading the header of each file matching the file spec],
		// do two optimizations:
		//   a) if the recording is already known to be complete, there is no
		//      need to update.
		//   b) don't search for new files on disk -- search for them in the
		//      supplied pData.

		if (m_fRecordingIsComplete)
		{
			// Nothing needs to be done
			DbgLog((LOG_PAUSE_BUFFER, 3, TEXT("CReader::PauseBufferUpdated() -- no-op because recording is complete\n") ));
		}
		else if (!pData)
		{
			// We are no longer attached to a capture graph, so we must be complete:

			m_cRecSet.OpenNewFiles(false);
			size_t uNumFiles = m_cRecSet.m_vecFiles.size();
			if ((uNumFiles > 0) && !m_cRecSet.m_vecFiles[uNumFiles-1]->GetHeader().fInProgress)
			{
				DbgLog((LOG_PAUSE_BUFFER, 3, TEXT("CReader::PauseBufferUpdated():  recording is complete [1]\n") ));
				m_fRecordingIsComplete = true;
			}
			m_uIncompleteOpenCountdown = DELAY_UNTIL_FINISH_OPEN;
 		}
		else
		{
			// The last file (the one that was in progress) may have had its header
			// updated. Re-read the header to make sure we have the latest & greatest:

			size_t uNumFiles = m_cRecSet.m_vecFiles.size();
			if (uNumFiles > 0)
			{
				m_cRecSet.m_vecFiles[uNumFiles-1]->ReadHeader();
			}

			// Scan the provided pause buffer for entries related to our permanent
			// recording:

			std::wstring &strRecordingName = m_cRecSet.GetFileName();
			for (size_t i = 0; i < pData->GetCount(); i++)
			{
				const SPauseBufferEntry &sPauseBufferEntry = (*pData)[i];

				if (!sPauseBufferEntry.fPermanent ||
					(strRecordingName != sPauseBufferEntry.strRecordingName))
				{
					// This file is either of a different recording or is part of a
					// temporary recording that was later partly converted to a
					// permanent recording.  We're not interested in it.

					continue;
				}

				// This entry is part of our permanent recording. Walk through the 
				// recording set to see if we already know about the file. If not,
				// add it:

				bool fFound = false;
				for (std::vector<CRecordingFile *>::iterator it = m_cRecSet.m_vecFiles.begin();
					(it != m_cRecSet.m_vecFiles.end()) && !fFound;
					it++)
				{
					// Compare the filenames
					if (!_wcsicmp(	(*it)->GetFileName().c_str(),
									sPauseBufferEntry.strFilename.c_str()))
						fFound = true;
				}
				if (!fFound)
				{
					// Add the file -- we know it is part of our permanent recording:
					if (m_cRecSet.AddFile(sPauseBufferEntry.strFilename.c_str(), !m_fChoseBufferSize, true))
					{
						// Fatal error opening the file
						throw std::runtime_error("Error opening file.");
					}
					m_uIncompleteOpenCountdown = DELAY_UNTIL_FINISH_OPEN;
				}
			}

			if (fOptimizeOpen && fNoFilesAtEntry)
			{
				DWORD dwFinishOpen = m_cRecSet.FinishOptimizedOpen();
				if (dwFinishOpen != ERROR_SUCCESS)
				{
					DbgLog((LOG_ERROR, 1, TEXT("CReader::PauseBufferUpdatedInternal():  error 0x%x from FinishOptimizedOpen()\n"), dwFinishOpen ));

					throw std::runtime_error("Error finishing optimized open-file-set.");
				}
			}

			// Find out if this recording has been completed:

			uNumFiles = m_cRecSet.m_vecFiles.size();
			if ((uNumFiles > 0) && !m_cRecSet.m_vecFiles[uNumFiles-1]->GetHeader().fInProgress)
			{
				DbgLog((LOG_PAUSE_BUFFER, 3, TEXT("CReader::PauseBufferUpdated():  recording is complete [2]\n") ));
				m_fRecordingIsComplete = true;
			}
		}
#ifdef DEBUG
		m_cRecSet.Dump();
#endif // DEBUG
		return;
	}

	if (!pData)
	{
		// Don't close m_cRecSet because that will get done in Load or
		// component cleanup.  Just gracefully return for simplicity.

		m_uIncompleteOpenCountdown = DELAY_UNTIL_FINISH_OPEN;
		return;
	}

	// If there are any files in the recording set
	// that aren't in the pause buffer, remove them.
	// Do this by making a duplicate of m_cRecSet.m_vecFiles
	// and then iterating over each item
	vector<CRecordingFile *> dupVec = m_cRecSet.m_vecFiles;
	for (	vector<CRecordingFile *>::iterator it = dupVec.begin();
			it != dupVec.end();
			it++)
	{
		// Get the first file
		CRecordingFile *pFile = *it;
		ASSERT(pFile);

		// Walk through the pause buffer searching for the file
		bool fFound = false;
		for (size_t i = 0; (i < pData->GetCount()) && !fFound; i++)
		{
			// Compare the filenames
			if (!_wcsicmp(	pFile->GetFileName().c_str(),
							(*pData)[i].strFilename.c_str()))
			{
				fFound = true;
				if ((pFile->GetHeader().fTemporary && (*pData)[i].fPermanent) ||
					(!pFile->GetHeader().fTemporary && !(*pData)[i].fPermanent))
				{
					// The recording has changed between temporary and permanent.
					// Since we are bound to live tv, the only thing that matters
					// is the temporary vs permanent status [of those things
					// updated when a file is converted between permanent and temporary].
					// So to be easy-on-disk-io, we'll just change our state:
					pFile->GetHeader().fTemporary = !(*pData)[i].fPermanent;
				}
			}
		}

		if (!fFound)
		{
			// pFile isn't in the pause buffer, remove it.
			m_cRecSet.RemoveSubFile(pFile->GetFileName());
		}
	}

	// Add any files which are in the pause buffer but not already in the
	// recording set.  Once we've hit a file which isn't part of the recording
	// set then we can safely assume that the rest of the files aren't part
	// of the recording set either.
	bool fAlreadyAddedOne = false;
	for (size_t i = 0; i < pData->GetCount(); i++)
	{
		bool fFound = false;

		// Ignore any entries that don't have any sample yet.  They are indicated
		// by a start time of -1.
		if ((*pData)[i].tStart == -1)
			continue;

		if (!fAlreadyAddedOne)
		{
			// Walk through the recording set searching for the file
			for (std::vector<CRecordingFile *>::iterator it = m_cRecSet.m_vecFiles.begin();
				(it != m_cRecSet.m_vecFiles.end()) && !fFound;
				it++)
			{
				// Compare the filenames
				if (!_wcsicmp(	(*it)->GetFileName().c_str(),
								(*pData)[i].strFilename.c_str()))
					fFound = true;
			}
		}

		if (fAlreadyAddedOne || !fFound)
		{
			// Add the file
			if (m_cRecSet.AddFile((*pData)[i].strFilename.c_str(), !m_fChoseBufferSize, fOptimizeOpen))
			{
				// Fatal error opening the file
				throw std::runtime_error("Error opening file.");
			}
			fAlreadyAddedOne = true;
		}
	}
	if (fOptimizeOpen && fNoFilesAtEntry)
	{
		DWORD dwFinishOpen = m_cRecSet.FinishOptimizedOpen();
		if (dwFinishOpen != ERROR_SUCCESS)
		{
			DbgLog((LOG_ERROR, 1, TEXT("CReader::PauseBufferUpdatedInternal():  error 0x%x from FinishOptimizedOpen() [2]\n"), dwFinishOpen ));

			throw std::runtime_error("Error finishing optimized open-file-set.");
		}
	}
	m_uIncompleteOpenCountdown = DELAY_UNTIL_FINISH_OPEN;
#ifdef DEBUG
	m_cRecSet.Dump();
#endif // !DEBUG
}

DWORD CReader::StreamingThreadProc(CReaderThread &rThread)
{
	DbgLog((LOG_SOURCE_STATE, 3, _T("CReader::StreamingThreadProc():  entry\n")));
	bool fSendEOSOnExit = false;

	m_pCurSampHeader = NULL;

	CComPtr<IMediaSample> pSample;
	SAMPLE_HEADER sampHeader;
	memset(&sampHeader, 0, sizeof(sampHeader));

	// I know these idental nested while loop conditions look funny.
	// The reason for them is so that we can break out of the first loop,
	// do some special processing, and then start looping again for the
	// EOF case.  Alternatively we could use goto's instead of break's
	// to eliminate the outer while loop, or we could use some serious
	// nested if-then-else's, but I don't think either of those yield
	// any more-readable code than this.  Sue me.
	while (!rThread.WasSignaledToStop())
	{
		// Keep streaming until we're told to stop.  Though we'll
		// break out of this on EOS
		while (!rThread.WasSignaledToStop())
		{
ReadLoopStart:

			pSample.Release();

			// Make sure that we're enabled
			ASSERT(m_hIsStreamingEvent);
			HANDLE rgHandles[2] = {m_pPipelineMgr->GetFlushEvent(), m_hIsStreamingEvent};
			DWORD dw = WaitForMultipleObjects(2, rgHandles, FALSE, INFINITE);

			// Wait for the end flush if appropriate
			if (dw == WAIT_OBJECT_0)
			{
				DbgLog((LOG_SOURCE_STATE, 3, _T("CReader::StreamingThreadProc() awoke due to flush event, calling EndWaitFlush()\n")));
				if (!m_pPipelineMgr->WaitEndFlush())
				{
					rThread.SignalThreadToStop();
					break;
				}
				continue;  // We can't fall through the loop because it might
						   // be that we are not supposed to be streaming.
			}

			if (rThread.WasSignaledToStop())
				break;

			// Get the pin number of the upcoming sample
			DWORD dwPinNum = 0;
			{
				CReaderReadTimeNotifier cReaderReadTimeNotifier(*this, m_piDecoderDriver);
				CAutoLock al(&m_csLockReader);
				DWORD dwError = 0;

				// If the skip mode is normal OR if this thread was instructed to loop
				// (via a seek or frame skip mode change), then don't use a hyper fast
				// mode.
				if ((m_fForceStreamingThreadLoop || m_fForceStreamingModeChange) || (m_skipMode == SKIP_MODE_NORMAL))
				{
					dwError = m_cRecSet.ReadSampleHeader(sampHeader,	// by ref
										!m_fBoundToLive,				// Check for new files?
										false,							// Reset file pos?
										m_skipMode != SKIP_MODE_NORMAL,	// Skip to index?
										0 != (m_skipMode & SKIP_MODE_FORWARD_FLAG)); // Forward?
				}
				else
				{
					// Determine how many seconds of keyframes we want to skip
					int iSkipTime = 0;	// Default for FAST and REVERSE_FAST
					switch (m_skipMode)
					{
						case SKIP_MODE_FAST_NTH:
						case SKIP_MODE_REVERSE_FAST_NTH:
							iSkipTime = m_skipModeSeconds * UNITS;
							break;
					};
					if (m_skipMode & SKIP_MODE_REVERSE_FLAG)
						iSkipTime *= -1;

					// Determine when the next sample should after (or before depending
					// on play direction).  We can use sampHeader.tStart since the first
					// time through our loop at hyper-fast - the above block executed
					// filling in sampHeader.
					LONGLONG llBoundary = sampHeader.tStart + iSkipTime;
					LONGLONG llPriorStart = sampHeader.tStart;
					bool fTryingForFinalKeyFrame = false;

					if (!m_fBoundToLive && (iSkipTime > 0))
					{
						// Reading every Nth sample -- be sure to read the
						// last key frame:

						LONGLONG llMaxSeekPosition = GetMaxSeekPosition();
						if (llBoundary > llMaxSeekPosition)
						{
							fTryingForFinalKeyFrame = true;
							DbgLog((LOG_SOURCE_STATE, 3,
								TEXT("READER:  capping at max seek position %I64d [was %I64d]\n"),
								llMaxSeekPosition, llBoundary));
							llBoundary = llMaxSeekPosition;
						}
					}
					else if (iSkipTime < 0)
					{
						// Reading every Nth sample going backwards -- again, be
						// sure to read the last key frame:

						LONGLONG llMinSeekPosition = GetMinSeekPosition();
						if (llBoundary < llMinSeekPosition)
						{
							fTryingForFinalKeyFrame = true;
							DbgLog((LOG_SOURCE_STATE, 3,
								TEXT("READER:  capping at min seek position %I64d [was %I64d]\n"),
								llMinSeekPosition, llBoundary));
							llBoundary = llMinSeekPosition;
						}
					}

					if ((iSkipTime > m_llSeekInsteadOfScanAtHyperFast) || (iSkipTime < (-1 * m_llSeekInsteadOfScanAtHyperFast)))
					{
						dwError = m_cRecSet.SeekToSampleTime(llBoundary, true);
					}

					if (dwError == ERROR_SUCCESS)
					{
						dwError = m_cRecSet.ReadSampleHeaderHyperFast(sampHeader,	// by ref
											!m_fBoundToLive,			// Check for new files?
											llBoundary,					// Skip boundary
											0 != (m_skipMode & SKIP_MODE_FORWARD_FLAG)); // Forward?
					}

					if ((dwError == ERROR_SUCCESS) &&
						fTryingForFinalKeyFrame &&
						(sampHeader.tStart == llPriorStart))
					{
						// We tried to get to the last key frame but it seems we were
						// already there. We're at EOF
						DbgLog((LOG_SOURCE_STATE, 3, TEXT("ReadSampleHeaderHyperFast() returned same old sample\n") ));
						dwError = ERROR_HANDLE_EOF;
					}
					else if ((dwError == ERROR_HANDLE_EOF) && (iSkipTime != 0))
					{
						DbgLog((LOG_SOURCE_STATE, 3, TEXT("ReadSampleHeaderHyperFast() returned EOF, seeing if 2nd try is warranted\n") ));
						bool fTryAgain = false;
						LONGLONG newPos = 0;
						if (iSkipTime > 0)
						{
							newPos = GetMaxSeekPosition() - 6000000LL;  // 600 ms before the end should include an i-frame
							if (newPos > llPriorStart)
								fTryAgain = true;
						}
						else
						{
							newPos = GetMinSeekPosition() + 6000000LL;
							if (newPos < llPriorStart)
								fTryAgain = true;
						}
						if (fTryAgain)
						{
							DbgLog((LOG_SOURCE_STATE, 3, TEXT("Reader:  makes sense to look for an i-frame\n") ));
							dwError = m_cRecSet.SeekToSampleTime(newPos, TRUE);
							if (dwError == ERROR_SUCCESS)
							{
								dwError = m_cRecSet.ReadSampleHeader(sampHeader,	// by ref
										!m_fBoundToLive,				// Check for new files?
										false,							// Reset file pos?
										m_skipMode != SKIP_MODE_NORMAL,	// Skip to index?
										0 != (m_skipMode & SKIP_MODE_FORWARD_FLAG)); // Forward?
							}
							if ((dwError != ERROR_SUCCESS) || (sampHeader.tStart == llPriorStart))
							{
								dwError = ERROR_HANDLE_EOF;
								DbgLog((LOG_SOURCE_STATE, 3, TEXT("Reader:  didn't find another\n") ));
							}
							else
							{
								DbgLog((LOG_SOURCE_STATE, 3, TEXT("Reader:  found one last i-frame\n") ));
							}
						}
					}
				}

				// Clear our loop state
				m_fForceStreamingThreadLoop = false;
				m_fForceStreamingModeChange = false;
	
				// See if the read worked
				if (dwError != ERROR_SUCCESS)
				{
					// EOF
					fSendEOSOnExit = true;
					break;
				}

				// Store the sample header.  This lets the app thread know where we
				// are in calls to GetCurSeekPos while this thread is blocked in
				// GetDeliveryBuffer.
				m_pCurSampHeader = &sampHeader;
			}

			// Look up that pin
			CComPtr<CDVROutputPin> pcPin = (CDVROutputPin *)
												(m_pPipelineMgr->GetFilter().GetPin(dwPinNum));
			if (!pcPin)
			{
				// Unexpected error.  Send EOF.
				fSendEOSOnExit = true;
				break;
			}

			// Get a buffer - this call will block until one becomes available
			bool fShutdown = false;
			while (!fShutdown && FAILED(pcPin->GetDeliveryBuffer(&pSample, NULL, NULL, 0)))
			{
				// See if we're flushing
				bool f = WaitForSingleObject(m_pPipelineMgr->GetFlushEvent(), 0) == WAIT_OBJECT_0;
				if (f)
				{
					DbgLog((LOG_SOURCE_DISPATCH, 3, _T("CReader::StreamingThreadProc() GetDeliveryBuffer() failed due to flush event, calling EndWaitFlush()\n")));
					// Wait for the end of the flush
					if (!m_pPipelineMgr->WaitEndFlush())
					{
						// We are stopping - so bail.
						fShutdown = true;
					}
					// else we were just in the middle of a flush so it makes sense
					// to try for a buffer now that we're done with the flush.
				}
				else
				{
					// Hmm. We failed to get a buffer but we're getting no indication yet
					// of a flush or stop. It could be that a flush, stop, or resume is just
					// working its way through the system. In any case, no streaming thread can
					// unilaterally exit because that throws off the thread count and makes Stop() fail.
					// So we'll sleep and simply try getting a buffer again.
					Sleep(50);  // pray that we're about to flush or stop or re-commit the buffer pool.
					if (WaitForSingleObject(m_hIsStreamingEvent, 0) != WAIT_OBJECT_0)
					{
						// We are no longer streaming -- the consumer will tell us where to seek
						// to before resuming streaming. Skip back to the beginning of this loop:
						goto ReadLoopStart;
					}
					DbgLog((LOG_ERROR, 2, _T("CReader::StreamingThreadProc():  GetDeliveryBuffer() failed, recovery code not written\n")));
				}

			}

			if (fShutdown)
			{
				rThread.SignalThreadToStop();
				break;
			}


			// Fill the buffer
			{
				CReaderReadTimeNotifier cReaderReadTimeNotifier(*this, m_piDecoderDriver);
				CAutoLock al(&m_csLockReader);

				// Clear the sample header we're working on.
				m_pCurSampHeader = NULL;

				// If we've been seeked by a different thread, then we need to release
				// the buffer and loop immediately.
				if (m_fForceStreamingThreadLoop || m_fForceStreamingModeChange)
				{
					DWORD dwError = ERROR_SUCCESS;

					DbgLog((LOG_SOURCE_STATE, 2, _T("CReader::StreamingThreadProc(): m_fForceStreamingXXX... is now set, releasing sample after reading header\n")));

					if (!m_fForceStreamingThreadLoop)
					{
						DbgLog((LOG_SOURCE_STATE, 2, _T("CReader::StreamingThreadProc(): backing up to before the sample's header\n")));
						dwError = m_cRecSet.ResetSampleHeaderRead();
					}
					pSample.Release();
					m_fForceStreamingThreadLoop = false;
					m_fForceStreamingModeChange = false;
					if (ERROR_SUCCESS != dwError)
					{
						// EOF
						fSendEOSOnExit = true;
						break;
					}
					continue;
				}

				if (ERROR_SUCCESS != m_cRecSet.ReadSample(sampHeader, *pSample))
				{
					// EOF
					fSendEOSOnExit = true;
					break;
				}
			}

			// Deliver the sample downstream
			try {
				m_router.ProcessOutputSample(*pSample, *pcPin);
			} catch (const std::exception &rcException)
			{
				UNUSED(rcException);
#ifdef UNICODE
				DbgLog((LOG_ERROR, 1,
						_T("CReader -- exception in ProcessOutputSample: %S\n"),
						rcException.what()));
#else
				DbgLog((LOG_ERROR, 1,
						_T("CReader -- exception in ProcessOutputSample: %s\n"),
						rcException.what()));
#endif
			}

			// End inside while loop
		}


		// Send EOF if necessary
		if (fSendEOSOnExit)
		{
			// Send EOS and stop the streaming thread
			SetIsStreaming(false);
			m_router.EndOfStream();
			fSendEOSOnExit = false;
		}

		// End outside while loop
	}

	// Scope for locking the reader
	{
		CAutoLock al(&m_csLockReader);

		// We're terminating.  We want GetCurSeekPosition to still work though.
		// So reset the file position back to the beginning of the sample header.
		if (m_pCurSampHeader)
		{
			m_cRecSet.ResetSampleHeaderRead();
			m_pCurSampHeader = NULL;
		}
	}

	DbgLog((LOG_SOURCE_STATE, 3, _T("CReader::StreamingThreadProc():  exit\n")));
	return 0;
}

LONGLONG CReader::GetMinSeekPosition()
{
	CAutoLock al(&m_csLockReader);
	return m_cRecSet.GetMinSampleTime(m_fBoundToLive);
}

LONGLONG CReader::GetMaxSeekPosition()
{
	CAutoLock al(&m_csLockReader);
	return m_cRecSet.GetMaxSampleTime();
}

LONGLONG CReader::GetCurrentPosition()
{
	CAutoLock al(&m_csLockReader);

	// See if the streaming thread is mid-sample waiting for a buffer.
	if (m_pCurSampHeader)
		return m_pCurSampHeader->tStart;

	// Get the next sample in the file
	SAMPLE_HEADER sh;
	DWORD dw = m_cRecSet.ReadSampleHeader(	sh,
											!m_fBoundToLive,
											true,
											m_skipMode != SKIP_MODE_NORMAL,
											(m_skipMode & SKIP_MODE_FORWARD_FLAG) != 0);
	if (dw == ERROR_SUCCESS)
		return sh.tStart;

	if (dw == ERROR_HANDLE_EOF)
		return GetMaxSeekPosition();

	return -1;
}

DWORD CReader::SetCurrentPosition(LONGLONG newPos, bool fSeekToKeyframe)
{
	DbgLog((LOG_SOURCE_STATE, 3, _T("CReader::SetCurrentPosition(%I64d, %s)\n"),
		newPos, fSeekToKeyframe ? _T("key-frame") : _T("any frame")));
	CAutoLock al(&m_csLockReader);
	if (!m_cRecSet.IsOpen() || m_cRecSet.m_vecFiles.empty())
	{
		return ERROR_INVALID_STATE;
	}
	DWORD dwStatus = m_cRecSet.SeekToSampleTime(newPos, fSeekToKeyframe);

	// Force the streaming thread to refresh it's loop
	m_fForceStreamingThreadLoop = true;

	return dwStatus;
}

bool CReader::SetIsStreaming(bool fEnabled)
{
	CAutoLock al(&m_csLockReader);

	DbgLog((LOG_SOURCE_STATE, 3, _T("CReader::SetIsStreaming(%u) -- current state is %u\n"),
		(unsigned) fEnabled, (unsigned) m_fIsStreaming));
	// If the value is changing
	if (fEnabled ^ m_fIsStreaming)
	{
		m_fIsStreaming = !m_fIsStreaming;
		ASSERT(fEnabled == m_fIsStreaming);
		if (m_fIsStreaming)
			SetEvent(m_hIsStreamingEvent);
		else
			ResetEvent(m_hIsStreamingEvent);
	}
#if defined(DEBUG) || defined(_DEBUG)
	else
	{
		ASSERT((fEnabled && m_fIsStreaming) ||
			   (!fEnabled && !m_fIsStreaming));
	}

	// Iff we're streaming, then make sure the event is set.
	// Likewise, iff we're not streaming, then make sure it isn't.
	DWORD dw = WaitForSingleObject(m_hIsStreamingEvent, 0);
	ASSERT(!(m_fIsStreaming ^ (dw == WAIT_OBJECT_0)));

#endif
	return m_fIsStreaming;
}

bool CReader::CanReadFile(LPCOLESTR wszFullPathName)
{
	if (!wszFullPathName || !*wszFullPathName
		|| CSampleProducerLocator::IsLiveTvToken(wszFullPathName))
	{
		return true;
	}

	// Create a new one on the heap so we don't blow the stack
	CRecordingSetRead *pTempRecSet = NULL;
	try
	{
		pTempRecSet = new CRecordingSetRead;
	}
	catch (std::bad_alloc)
	{
		RETAILMSG(1, (L"!CReader::CanReadFile out of memory creating recording set.\n"));
		return false;
	}


	bool fSuccess = pTempRecSet->Open(wszFullPathName, false, true) == ERROR_SUCCESS;
	delete pTempRecSet;

	return fSuccess;
}

FRAME_SKIP_MODE CReader::GetFrameSkipMode()
{
	CAutoLock al(&m_csLockReader);
	return m_skipMode;
}

DWORD CReader::GetFrameSkipModeSeconds()
{
	CAutoLock al(&m_csLockReader);
	return m_skipModeSeconds;
}

DWORD CReader::SetFrameSkipMode(FRAME_SKIP_MODE skipMode, DWORD skipModeSeconds)
{
	CAutoLock al(&m_csLockReader);

	// No-op if it's not changing
	if (skipMode == m_skipMode)
	{
		if ((skipModeSeconds == m_skipModeSeconds) ||
			((skipMode != SKIP_MODE_FAST_NTH) &&
			 (skipMode != SKIP_MODE_REVERSE_FAST_NTH)))
		{
			return ERROR_SUCCESS;
		}
	}

	switch (skipMode)
	{
		case SKIP_MODE_NORMAL:
			// No need to inform the reader thread of anything.
			m_skipMode = skipMode;
			return ERROR_SUCCESS;

		case SKIP_MODE_FAST:
		case SKIP_MODE_FAST_NTH:
		case SKIP_MODE_REVERSE_FAST:
		case SKIP_MODE_REVERSE_FAST_NTH:
			// Force the streaming thread to refresh it's loop
			m_fForceStreamingModeChange = true;
			m_skipMode = skipMode;
			m_skipModeSeconds = skipModeSeconds;
			return ERROR_SUCCESS;
	};

	// We don't support any other modes.
	ASSERT(FALSE);
	return ERROR_INVALID_PARAMETER;
}

CReaderAutoPosition::~CReaderAutoPosition()
{
	// Seek back to where we were
	if (m_rReader.m_cRecSet.SeekToSampleTime(m_tCur, false))
		ASSERT(FALSE);

	// Force the streaming thread to refresh it's loop
	m_rReader.m_fForceStreamingThreadLoop = true;
}

HRESULT CReader::GetRecordingSize(	LONGLONG tStart,
									LONGLONG tEnd,
									ULONGLONG* pcbSize)
{
	if (!pcbSize)
		_CHR(E_POINTER);

	CAutoLock al(&m_csLockReader);

	// Retrieve the min, max, and current positions
	LONGLONG tMin = GetMinSeekPosition();
	LONGLONG tMax = GetMaxSeekPosition();
	LONGLONG tCur = GetCurrentPosition();
	if ((tMin == -1) || (tMax == -1) || (tCur == -1))
	{
		_CHR(E_FAIL);
	}
	ASSERT((tMin <= tCur) && (tCur <= tMax));

	// Make sure the times are in bounds.
	tStart = max(tMin, tStart);
	tEnd = min(tMax, tEnd);

	// We need to restore the position of the stream
	CReaderAutoPosition ap(*this, tCur);

	// Seek to the beginning
	if (m_cRecSet.SeekToSampleTime(tStart, true))
		_CHR(E_FAIL);

	// Read the sample header
	SAMPLE_HEADER sh;
	if (m_cRecSet.ReadSampleHeader(	sh,		// by ref
									false,	// Check for new files?
									false,	// Reset file pos?
									true,	// Skip to index?
									true))	// Forward?
	{
		_CHR(E_FAIL);
	}
	*pcbSize = sh.llStreamOffset;

	// Seek to the end
	if(m_cRecSet.SeekToSampleTime(tEnd, true))
		_CHR(E_FAIL);

	// Read the sample header
	if (m_cRecSet.ReadSampleHeader(	sh,		// by ref
									false,	// Check for new files?
									false,	// Reset file pos?
									true,	// Skip to index?
									true))	// Forward?
	{
		_CHR(E_FAIL);
	}

	// Calculate the diff
	*pcbSize = sh.llStreamOffset - *pcbSize;

	return S_OK;
}

HRESULT CReader::GetRecordingDurationMax(	LONGLONG tStart,
									        ULONGLONG cbSizeMax,
									        LONGLONG* ptDurationMax)
{
	if (cbSizeMax == 0)
		_CHR(E_INVALIDARG);
	if (!ptDurationMax)
		_CHR(E_POINTER);

	CAutoLock al(&m_csLockReader);

	// Retrieve the min, max, and current positions
	LONGLONG tMin = GetMinSeekPosition();
	LONGLONG tMax = GetMaxSeekPosition();
	LONGLONG tCur = GetCurrentPosition();
	if ((tMin == -1) || (tMax == -1) || (tCur == -1))
	{
		_CHR(E_FAIL);
	}
	ASSERT((tMin <= tCur) && (tCur <= tMax));

	// Make sure the times are in bounds.
	LONGLONG tStartSearch = max(tMin, tStart);

	// We need to restore the position of the stream
	CReaderAutoPosition ap(*this, tCur);

	// Seek to the beginning
	if (m_cRecSet.SeekToSampleTime(tStartSearch, true))
		_CHR(E_FAIL);

	// Read the sample header
	SAMPLE_HEADER sh;
	if (m_cRecSet.ReadSampleHeader(	sh,		// by ref
									false,	// Check for new files?
									false,	// Reset file pos?
									true,	// Skip to index?
									true))	// Forward?
	{
		_CHR(E_FAIL);
	}

	// Seek to the end
	LONGLONG tEnd = m_cRecSet.GetNavPackMediaTimeFullSearch(sh.llStreamOffset + cbSizeMax);
	ASSERT(tMax >= tEnd);

	// The sizes better be sane
	if (tEnd <= tStart)
		_CHR(E_FAIL);

	// Calculate the diff
	*ptDurationMax = tEnd - tStart;

	return S_OK;
}

long CReader::GetNavPackOffset(LONGLONG tStart, LONGLONG tSearchStartBound, LONGLONG tSearchEndBound, LONGLONG llInitialOffset, bool fFullSearch)
{
	static bool fLastSearchSucceeded = false;
	LONGLONG llOffset = -1;
	if (tStart >= tSearchStartBound && tStart <= tSearchEndBound)
	{
		if (!fLastSearchSucceeded || fFullSearch)
			llOffset = m_cRecSet.GetNavPackStreamOffsetFullSearch(tStart);
		else
			llOffset = m_cRecSet.GetNavPackStreamOffsetFast(tStart);
	}
	if (llOffset == -1)
	{
		fLastSearchSucceeded = false;
		return 0x3fffffff;
	}

	fLastSearchSucceeded = true;
	return static_cast<long>(llOffset - llInitialOffset);
}


// ISourceVOBUInfo
HRESULT CReader::GetVOBUOffsets(LONGLONG tStart,
								LONGLONG tSearchStartBound,
								LONGLONG tSearchEndBound,
								VOBU_SRI_TABLE *pVobuTable)
{
	if (!pVobuTable)
		_CHR(E_POINTER);

	// Lock the reader
	CAutoLock al(&m_csLockReader);

	// Make sure the recording set is open
	if (!m_cRecSet.IsOpen())
		_CHR(E_UNEXPECTED);

	// Save our current seek position because it'll get hosed by the below operations
	m_cRecSet.MarkSeekPosition();

	// todo Cache recently filled nav_packs to allow this one to be
	// populated much easier.

	// Get the base offset
	LONGLONG llInitialOffset = m_cRecSet.GetNavPackStreamOffsetFullSearch(tStart);
	if (llInitialOffset == -1)
	{
		m_cRecSet.RestoreSeekPostion();
		_CHR(E_FAIL);
	}

	// Fill the backwards nav pack info
	pVobuTable->BWDI240 = abs(GetNavPackOffset(tStart - UNITS/2*240,tSearchStartBound, tSearchEndBound, llInitialOffset, true));
	pVobuTable->BWDI120 = abs(GetNavPackOffset(tStart - UNITS/2*120,tSearchStartBound, tSearchEndBound, llInitialOffset, true));
	pVobuTable->BWDI60 = abs(GetNavPackOffset(tStart - UNITS/2*60,tSearchStartBound, tSearchEndBound, llInitialOffset, true));
	pVobuTable->BWDI20 = abs(GetNavPackOffset(tStart - UNITS/2*20,tSearchStartBound, tSearchEndBound, llInitialOffset, false));
	pVobuTable->BWDI15 = abs(GetNavPackOffset(tStart - UNITS/2*15,tSearchStartBound, tSearchEndBound, llInitialOffset, false));
	pVobuTable->BWDI14 = abs(GetNavPackOffset(tStart - UNITS/2*14,tSearchStartBound, tSearchEndBound, llInitialOffset, false));
	pVobuTable->BWDI13 = abs(GetNavPackOffset(tStart - UNITS/2*13,tSearchStartBound, tSearchEndBound, llInitialOffset, false));
	pVobuTable->BWDI12 = abs(GetNavPackOffset(tStart - UNITS/2*12,tSearchStartBound, tSearchEndBound, llInitialOffset, false));
	pVobuTable->BWDI11 = abs(GetNavPackOffset(tStart - UNITS/2*11,tSearchStartBound, tSearchEndBound, llInitialOffset, false));
	pVobuTable->BWDI10 = abs(GetNavPackOffset(tStart - UNITS/2*10,tSearchStartBound, tSearchEndBound, llInitialOffset, false));
	pVobuTable->BWDI9 = abs(GetNavPackOffset(tStart - UNITS/2*9,tSearchStartBound, tSearchEndBound, llInitialOffset, false));
	pVobuTable->BWDI8 = abs(GetNavPackOffset(tStart - UNITS/2*8,tSearchStartBound, tSearchEndBound, llInitialOffset, false));
	pVobuTable->BWDI7 = abs(GetNavPackOffset(tStart - UNITS/2*7,tSearchStartBound, tSearchEndBound, llInitialOffset, false));
	pVobuTable->BWDI6 = abs(GetNavPackOffset(tStart - UNITS/2*6,tSearchStartBound, tSearchEndBound, llInitialOffset, false));
	pVobuTable->BWDI5 = abs(GetNavPackOffset(tStart - UNITS/2*5,tSearchStartBound, tSearchEndBound, llInitialOffset, false));
	pVobuTable->BWDI4 = abs(GetNavPackOffset(tStart - UNITS/2*4,tSearchStartBound, tSearchEndBound, llInitialOffset, false));
	pVobuTable->BWDI3 = abs(GetNavPackOffset(tStart - UNITS/2*3,tSearchStartBound, tSearchEndBound, llInitialOffset, false));
	pVobuTable->BWDI2 = abs(GetNavPackOffset(tStart - UNITS/2*2,tSearchStartBound, tSearchEndBound, llInitialOffset, false));
	pVobuTable->BWDI1 = abs(GetNavPackOffset(tStart - UNITS/2*1,tSearchStartBound, tSearchEndBound, llInitialOffset, false));

	// Fill the forward nav pack info
	pVobuTable->FWDI1 = GetNavPackOffset(tStart + UNITS/2*1,tSearchStartBound, tSearchEndBound, llInitialOffset, false);
	pVobuTable->FWDI2 = GetNavPackOffset(tStart + UNITS/2*2,tSearchStartBound, tSearchEndBound, llInitialOffset, false);
	pVobuTable->FWDI3 = GetNavPackOffset(tStart + UNITS/2*3,tSearchStartBound, tSearchEndBound, llInitialOffset, false);
	pVobuTable->FWDI4 = GetNavPackOffset(tStart + UNITS/2*4,tSearchStartBound, tSearchEndBound, llInitialOffset, false);
	pVobuTable->FWDI5 = GetNavPackOffset(tStart + UNITS/2*5,tSearchStartBound, tSearchEndBound, llInitialOffset, false);
	pVobuTable->FWDI6 = GetNavPackOffset(tStart + UNITS/2*6,tSearchStartBound, tSearchEndBound, llInitialOffset, false);
	pVobuTable->FWDI7 = GetNavPackOffset(tStart + UNITS/2*7,tSearchStartBound, tSearchEndBound, llInitialOffset, false);
	pVobuTable->FWDI8 = GetNavPackOffset(tStart + UNITS/2*8,tSearchStartBound, tSearchEndBound, llInitialOffset, false);
	pVobuTable->FWDI9 = GetNavPackOffset(tStart + UNITS/2*9,tSearchStartBound, tSearchEndBound, llInitialOffset, false);
	pVobuTable->FWDI10 = GetNavPackOffset(tStart + UNITS/2*10,tSearchStartBound, tSearchEndBound, llInitialOffset, false);
	pVobuTable->FWDI11 = GetNavPackOffset(tStart + UNITS/2*11,tSearchStartBound, tSearchEndBound, llInitialOffset, false);
	pVobuTable->FWDI12 = GetNavPackOffset(tStart + UNITS/2*12,tSearchStartBound, tSearchEndBound, llInitialOffset, false);
	pVobuTable->FWDI13 = GetNavPackOffset(tStart + UNITS/2*13,tSearchStartBound, tSearchEndBound, llInitialOffset, false);
	pVobuTable->FWDI14 = GetNavPackOffset(tStart + UNITS/2*14,tSearchStartBound, tSearchEndBound, llInitialOffset, false);
	pVobuTable->FWDI15 = GetNavPackOffset(tStart + UNITS/2*15,tSearchStartBound, tSearchEndBound, llInitialOffset, false);
	pVobuTable->FWDI20 = GetNavPackOffset(tStart + UNITS/2*20,tSearchStartBound, tSearchEndBound, llInitialOffset, false);
	pVobuTable->FWDI60 = GetNavPackOffset(tStart + UNITS/2*60,tSearchStartBound, tSearchEndBound, llInitialOffset, true);
	pVobuTable->FWDI120 = GetNavPackOffset(tStart + UNITS/2*120,tSearchStartBound, tSearchEndBound, llInitialOffset, true);
	pVobuTable->FWDI240 = GetNavPackOffset(tStart + UNITS/2*240,tSearchStartBound, tSearchEndBound, llInitialOffset, true);

	// The FWDINext field can be determined from the data we already have
	if (pVobuTable->FWDI1 != 0)
		pVobuTable->FWDINext = pVobuTable->FWDI1;
	else if (pVobuTable->FWDI2 != 0)
		pVobuTable->FWDINext = pVobuTable->FWDI2;
	else if (pVobuTable->FWDI3 != 0)
		pVobuTable->FWDINext = pVobuTable->FWDI3;
	else if (pVobuTable->FWDI4 != 0)
		pVobuTable->FWDINext = pVobuTable->FWDI4;
	//else
	//	ASSERT(FALSE);

	// The BWDIPrev field can be determined from the data we already have
	if (pVobuTable->BWDI1 != 0)
		pVobuTable->BWDIPrev = pVobuTable->BWDI1;
	else if (pVobuTable->BWDI2 != 0)
		pVobuTable->BWDIPrev = pVobuTable->BWDI2;
	else if (pVobuTable->BWDI3 != 0)
		pVobuTable->BWDIPrev = pVobuTable->BWDI3;
	else if (pVobuTable->BWDI4 != 0)
		pVobuTable->BWDIPrev = pVobuTable->BWDI4;
	else
		ASSERT(FALSE);

	// Save the last two fields
	pVobuTable->FWDIVideo = pVobuTable->FWDINext;
	pVobuTable->BWDIVideo = pVobuTable->BWDIPrev;

	// Done - let's get out of Dodge
	m_cRecSet.RestoreSeekPostion();

	return S_OK;
}

HRESULT CReader::GetKeyFrameTime(	LONGLONG tTime,
									LONG nSkipCount,
									LONGLONG* ptKeyFrame)
{
	if (!ptKeyFrame)
		_CHR(E_POINTER);

	CAutoLock al(&m_csLockReader);

	// Retrieve the min, max, and current positions
	LONGLONG tMin = GetMinSeekPosition();
	LONGLONG tMax = GetMaxSeekPosition();
	LONGLONG tCur = GetCurrentPosition();
	if ((tMin == -1) || (tMax == -1) || (tCur == -1))
	{
		_CHR(E_FAIL);
	}
	ASSERT((tMin <= tCur) && (tCur <= tMax));

	// Make sure the times are in bounds.
	LONGLONG tSearch = max(tMin, tTime);
	tSearch = min(tMax, tSearch);

	// We need to restore the position of the stream
	CReaderAutoPosition ap(*this, tCur);

	bool fForward = (nSkipCount >= 0);
	LONGLONG llBias = (fForward) ? 1 : -1;
	ULONG cReadCount = abs(nSkipCount);

	// Seek to the sample
	if (m_cRecSet.SeekToSampleTime(tSearch, true))
		_CHR(E_FAIL);

	// Read the sample header
	SAMPLE_HEADER sh;
	if (m_cRecSet.ReadSampleHeader( sh, 	// by ref
									false,	// Check for new files?
									false,	// Reset file pos?
									true,	// Skip to index?
									true))	// Forward?
	{
		_CHR(E_FAIL);
	}

	// Loop over the skip count reading the sample headers
	while (cReadCount-- > 0)
	{
		// Seek back to the start of the sample header so we can do
		// the skip
		if (m_cRecSet.ResetSampleHeaderRead())
		{
			_CHR(E_FAIL);
		}

		LONGLONG llBoundary = sh.tStart + llBias;
		if (m_cRecSet.ReadSampleHeaderHyperFast(sh,				// by ref
												false,			// Check for new files?
												llBoundary, 	// Skip boundary
												fForward))		// Forward?
		{
			_CHR(E_FAIL);
		}
	}

	// Pass back the last key frame found
	*ptKeyFrame = sh.tStart;

	return S_OK;
}

HRESULT CReader::GetRecordingData(	LONGLONG tStart,
									BYTE* rgbBuffer,
									ULONG cbBuffer,
									ULONG* pcbRead,
									LONGLONG* ptEnd)
{
	if ((!pcbRead) || (!ptEnd))
		_CHR(E_POINTER);

	CAutoLock al(&m_csLockReader);

	// Retrieve the min, max, and current positions
	LONGLONG tMin = GetMinSeekPosition();
	LONGLONG tMax = GetMaxSeekPosition();
	LONGLONG tCur = GetCurrentPosition();
	if ((tMin == -1) || (tMax == -1) || (tCur == -1))
	{
		_CHR(E_FAIL);
	}
	ASSERT((tMin <= tCur) && (tCur <= tMax));

	// Make sure the times are in bounds.
	LONGLONG tSearch = max(tMin, tStart);
	tSearch = min(tMax, tSearch);

	// We need to restore the position of the stream
	CReaderAutoPosition ap(*this, tCur);

	// Seek to the beginning
	if (m_cRecSet.SeekToSampleTime(tSearch, false))
		_CHR(E_FAIL);

	// Read the sample header
	SAMPLE_HEADER sh;
	if (m_cRecSet.ReadSampleHeader(	sh,		// by ref
									false,	// Check for new files?
									false,	// Reset file pos?
									false,	// Skip to index?
									true))	// Forward?

	{
		_CHR(E_FAIL);
	}

	// Read the sample data
	if (m_cRecSet.ReadSampleData(	sh,		// by ref
									rgbBuffer,	// Check for new files?
									cbBuffer,	// Reset file pos?
									pcbRead))	// Forward?
	{
		_CHR(E_FAIL);
	}

	// Read the next sample header to get the end time
	if (m_cRecSet.ReadSampleHeader(	sh,		// by ref
									false,	// Check for new files?
									false,	// Reset file pos?
									false,	// Skip to index?
							true))	// Forward?
	{
		// If this is the last sample, pass back the maximum time a sample
		// could ever be
		*ptEnd = -1LL;
	}
	else
	{
		*ptEnd = sh.tStart;
	}

	return S_OK;
}

/* IPipelineComponentIdleAction */

#ifdef _WIN32_WCE

class CLazyOpenThreadPriorityHelper 
{
public:
	CLazyOpenThreadPriorityHelper(DWORD dwPriority);
	~CLazyOpenThreadPriorityHelper();

private:
	DWORD m_dwPriority;
	DWORD m_dwCurrentPriority;
	bool m_fRestorePriority;
};

CLazyOpenThreadPriorityHelper::CLazyOpenThreadPriorityHelper(DWORD dwPriority)
	: m_dwPriority(dwPriority)
	, m_dwCurrentPriority(THREAD_PRIORITY_ERROR_RETURN)
	, m_fRestorePriority(false)
{
	if (m_dwPriority != THREAD_PRIORITY_ERROR_RETURN)
	{
		m_dwCurrentPriority = CeGetThreadPriority(GetCurrentThread());
		if ((m_dwCurrentPriority != THREAD_PRIORITY_ERROR_RETURN) && (m_dwPriority < m_dwCurrentPriority))
		{
			CeSetThreadPriority(GetCurrentThread(), m_dwPriority);
			m_fRestorePriority = true;
		}
	}
}

CLazyOpenThreadPriorityHelper::~CLazyOpenThreadPriorityHelper()
{
	if (m_fRestorePriority)
	{
		CeSetThreadPriority(GetCurrentThread(), m_dwCurrentPriority);
	}
}

#endif // _WIN32_WCE


void CReader::DoAction()
{
	// Close one file per holding of the lock. This is to let the reader
	// streaming thread or other urgent threads in.

	if (m_uIncompleteOpenCountdown == 1)
	{
		DbgLog((LOG_FILE_OTHER, 3, TEXT("CReader::DoAction() -- checking for lazy-opened files\n") ));
	}

#ifdef _WIN32_WCE

	CLazyOpenThreadPriorityHelper cLazyOpenThreadPriorityHelper(
			m_fEnableBackgroundPriority ? g_dwLazyReadOpenPriority + 1 : g_dwLazyReadOpenPriority);

#endif // _WIN32_WCE

	if (m_uIncompleteOpenCountdown)
	{
		CAutoLock al(&m_csLockReader);

		if (m_uIncompleteOpenCountdown != 1)
		{
			// It is not yet time to do the lazy opens.
			if (m_uIncompleteOpenCountdown > 0)
				--m_uIncompleteOpenCountdown;
			return;
		}

		if (!m_cRecSet.IsOpen())
		{
			m_uIncompleteOpenCountdown = 0;
			return;
		}

		// We can't afford to iterate, opening and reading many files.  
		// The open call is too expensive:

		size_t uNumFiles = m_cRecSet.m_vecFiles.size();
		size_t uFileIdx;
		for (uFileIdx = 0; uFileIdx < uNumFiles; ++uFileIdx)
		{
			// If this file was lazy-opened, fully open it:

			CRecordingFile *pRecordingFile = m_cRecSet.m_vecFiles[uFileIdx];
			if (pRecordingFile->UsedOptimizedOpen())
			{
				pRecordingFile->FullyOpen();

				// We've done enough work for now.

				return;
			}
		}

		// We'll read the header of files that are either marked as recording-incomplete
		// despite not being the first recording or marked as file-incomplete. We know
		// that no optimized-opens are left to complete.  The read is not as cheap as we'd
		// like but there is no good sentinel that we've just updated one of the files on
		// a previous pass.

		bool fLatestRecordingInProgress = true;
		for (uFileIdx = uNumFiles; uFileIdx > 0; --uFileIdx)
		{
			// uFileIdx is a one-based count (so that we don't get in trouble decrementing
			// from 0=first index).  Subtract -1 to get a zero-based index for m_vecFiles:

			CRecordingFile *pRecordingFile = m_cRecSet.m_vecFiles[uFileIdx-1];
			FILE_HEADER &rHeader = pRecordingFile->GetHeader();
			if (rHeader.fInProgress)
			{
				// Recording is in progress -- something might change:

				if (fLatestRecordingInProgress && (uFileIdx < uNumFiles))
				{
					// This might not be the latest recording after all.  We want this test to
					// be efficient and can afford to have it sometimes be wrong so we test
					// to see if the pair of files have the same index within their (possibly
					// different) recordings:

					if (pRecordingFile->GetCreationIndex() + 1 != m_cRecSet.m_vecFiles[uFileIdx]->GetCreationIndex())
					{
						fLatestRecordingInProgress = false;
					}
				}

				if (pRecordingFile->GetHeader().fFileInProgress || !fLatestRecordingInProgress)
				{
					pRecordingFile->ReadHeader();
				}
			}
			else
			{
				fLatestRecordingInProgress = false;
			}
		}

		m_uIncompleteOpenCountdown = 0;
#ifdef DEBUG
		m_cRecSet.Dump();
#endif // DEBUG
	}
} // CReader::DoAction

void CReader::SetBackgroundPriorityMode(BOOL fUseBackgroundPriority)
{
	// Not critical enough to warrant locking:

	m_fEnableBackgroundPriority = fUseBackgroundPriority;
}

} // namespace MSDvr
