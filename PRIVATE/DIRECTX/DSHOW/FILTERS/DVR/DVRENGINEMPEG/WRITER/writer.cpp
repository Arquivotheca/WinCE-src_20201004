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
#include "writer.h"
#include "..\\plumbing\\sink.h"
#include "..\\PauseBuffer.h"
#include "..\\SampleProducer\\ProducerConsumerUtilities.h"
#include "IsNavPack.h"
#include "DVREngine.h"
#include "AVGlitchEvents.h"

// todo - Should this be calculated or hardcoded?
#define MAX_SAMPLE_LATENCY		20000000		// 2 seconds

#define YEAR_IN_MILLISECS (((LONGLONG) 365)*24*60*60*1000)

using std::wstring;
using std::vector;

#ifdef _WIN32_WCE
// #define WIN_CE_KERNEL_TRACKING_EVENTS
#endif

#ifdef WIN_CE_KERNEL_TRACKING_EVENTS
#include "Celog.h"
#endif

// This timing class helps diagnose where the bottlenecks are
// that contribute to a/v glitches. Because only retail builds
// have the performance needed to distinguish true product
// issues from debug-build-inefficiency issues, the class is
// not protected via #idef DEBUG

struct WriterTimeLogger
{
	WriterTimeLogger(const _TCHAR *pszAction, DWORD dwThreshold)
		: m_dwStartTick(GetTickCount())
		, m_pszAction(pszAction)
		, m_dwThreshold(dwThreshold)
	{
#ifdef WIN_CE_KERNEL_TRACKING_EVENTS

		wchar_t pwszMsg[128];

#ifdef UNICODE
		swprintf(pwszMsg, L"%s -- entry", pszAction);
#else // !UNICODE
		swprintf(pwszMsg, L"%S -- entry", pszAction);
#endif // !UNICODE
		CELOGDATA(TRUE,
				CELID_RAW_WCHAR,
				(PVOID)pwszMsg,
				(1 + wcslen(pwszMsg)) * sizeof(wchar_t),
				1,
				CELZONE_MISC);

#endif // WIN_CE_KERNEL_TRACKING_EVENTS
	}

	~WriterTimeLogger()
	{
		DWORD dwStopTick = GetTickCount();
		DWORD dwElapsedTicks = 0;

		if (dwStopTick < m_dwStartTick)
		{
			dwElapsedTicks = dwStopTick + 1 + (0xffffffff - m_dwStartTick);
		}
		else
		{
			dwElapsedTicks = dwStopTick - m_dwStartTick;
		}
#ifdef WIN_CE_KERNEL_TRACKING_EVENTS

		wchar_t pwszMsg[128];

 #ifdef UNICODE
		swprintf(pwszMsg, L"%s -- exit", m_pszAction);
#else // !UNICODE
		swprintf(pwszMsg, L"%S -- exit", m_pszAction);
#endif // !UNICODE
		CELOGDATA(TRUE,
				CELID_RAW_WCHAR,
				(PVOID)pwszMsg,
				(1 + wcslen(pwszMsg)) * sizeof(wchar_t),
				1,
				CELZONE_MISC);

#endif // WIN_CE_KERNEL_TRACKING_EVENTS
		if (dwElapsedTicks >= m_dwThreshold)
		{
			DbgLog((LOG_WARNING, 3, TEXT("%s:  elapsed time of %u ms\n"), m_pszAction, dwElapsedTicks));
#ifdef WIN_CE_KERNEL_TRACKING_EVENTS
			CELOGDATA(TRUE,
					CELID_RAW_ULONG,
					(PVOID)dwElapsedTicks,
					sizeof(DWORD),
					1,
					CELZONE_MISC);

#endif // WIN_CE_KERNEL_TRACKING_EVENTS
		}
	}

private:
	DWORD m_dwStartTick;
	DWORD m_dwThreshold;
	const _TCHAR *m_pszAction;
};

namespace MSDvr
{
#define SERIOUSLY_LONG_WRITE_TIME	200
#define AV_PERIODIC_CAPTURE_EVENT	30000

class CProcessLongWriteNotifier
{
public:
	CProcessLongWriteNotifier(IMediaEventSink *piMediaEventSink, DWORD &rdwLastCheck)
		: m_piMediaEventSink(piMediaEventSink)
		, m_dwWriteStartTime(GetTickCount())
		, m_rdwLastCheck(rdwLastCheck)
    {
    }

	~CProcessLongWriteNotifier()
	{
		if (m_piMediaEventSink)
		{
			DWORD dwTimeNow = GetTickCount();
			DWORD dwInterval = 0;
			if (dwTimeNow > m_dwWriteStartTime)
		dwInterval = dwTimeNow - m_dwWriteStartTime;
			else
				dwInterval = 1 + dwTimeNow + (0xffffffff - m_dwWriteStartTime);
			if (dwInterval >= SERIOUSLY_LONG_WRITE_TIME)
			{
				m_piMediaEventSink->Notify(AV_GLITCH_LONG_DVR_WRITE, dwTimeNow, dwInterval);
			}

			if ((dwTimeNow <= m_rdwLastCheck) || (dwTimeNow - m_rdwLastCheck >= AV_PERIODIC_CAPTURE_EVENT))
			{
				m_rdwLastCheck = dwTimeNow;
				m_piMediaEventSink->Notify(AV_NO_GLITCH_AV_IS_BEING_CAPTURED, dwTimeNow, 0);
			}
		}
	}

private:
	CComPtr<IMediaEventSink> m_piMediaEventSink;
	DWORD m_dwWriteStartTime;
	DWORD &m_rdwLastCheck;
};

CWriter::CWriter()
	:	m_pPipelineMgr(NULL),
		m_pbCustomPipelineData(NULL),
		m_dwCustomPipelineDataLen(0),
		m_rgMediaTypes(NULL),
		m_dwNumMediaTypes(0),
		m_capMode(STRMBUF_TEMPORARY_RECORDING),
		m_fJustStartedRecording(false),
		m_pcRecPrev(NULL),
		m_dwNegotiatedBufferSize(0),
		m_tLastDiscontinuity(0),
		m_piMediaEventSink(NULL),
		m_dwLastHaveAVEvent(0),
#ifdef UNDER_CE
		m_strRecPath(L"\\hard disk4\\temprec\\"),
#endif
		m_dwBytesWritten(0),
		m_hyGetTimeValueOfLastSampleWritten(-1),
		m_vecPendingFileDeletes()
{
    // Get the max size of a permanent recording
    m_cbMaxPermanentFileSize = g_uhyMaxPermanentRecordingFileSize;

    // Get the max size of a temporary recording
    m_cbMaxTemporaryFileSize = g_uhyMaxTemporaryRecordingFileSize;

    // Register the writer
    g_cDVREngineHelpersMgr.RegisterWriter(this);
#ifdef MONITOR_WRITE_FILE
	BeginFileWriteMonitoring();
#endif // MONITOR_WRITE_FILE
}

CWriter::~CWriter()
{
	Cleanup();
#ifdef MONITOR_WRITE_FILE
	EndFileWriteMonitoring();
#endif // MONITOR_WRITE_FILE
}

void CWriter::Cleanup()
{
	RetryPendingFileDeletions();

	g_cDVREngineHelpersMgr.UnregisterWriter(this);

    if (g_bEnableFilePreCreation)
    {
	m_cRecSet.ClearPreppedRecording();
    }

	m_pPipelineMgr = NULL;
	m_dwNegotiatedBufferSize = 0;

	// Stop remembering pointers to other pipeline components that may be going away:
	m_pNextFileSink2 = NULL;
	m_pNextSBC = NULL;

	// Close our recordings, first we close all of the handles, then we clear out the vectors.
	m_cRecSet.Close();
	if (m_pcRecPrev)
	{
		m_pcRecPrev->Close();
		delete m_pcRecPrev;
		m_pcRecPrev = NULL;
	}

	// now that the file handles are closed, delete the temporary files in the pause buffer
	for (vector<SPauseBufferEntry>::reverse_iterator it = m_cPB.m_vecFiles.rbegin();
		it != m_cPB.m_vecFiles.rend();
		it++)
	{
		// Convert this entry
		if(!(it->fPermanent))
		{
			if(!DeleteFile((it->strFilename + TEXT(".dvr-dat")).c_str()))
				RETAILMSG(1, (L"*** FAILED TO DELETE A TEMPORARY .DVR-DAT FILE ON CLEANUP ***\n"));

			if(!DeleteFile((it->strFilename + TEXT(".dvr-idx")).c_str()))
				RETAILMSG(1, (L"*** FAILED TO DELETE A TEMPORARY .DVR-IDX FILE ON CLEANUP ***\n"));
		}
	}
	// m_vecPBPermFiles contains permanent recordings which are in the pause buffer. Nothing should need
	// to be deleted, but clean it out anyway just in case.
	for (vector<SPauseBufferEntry>::reverse_iterator it = m_vecPBPermFiles.rbegin();
		it != m_vecPBPermFiles.rend();
		it++)
	{
		// Convert this entry
		if(!(it->fPermanent))
		{
			if(!DeleteFile((it->strFilename + TEXT(".dvr-dat")).c_str()))
				RETAILMSG(1, (L"*** FAILED TO DELETE A TEMPORARY .DVR-DAT FILE ON CLEANUP ***\n"));

			if(!DeleteFile((it->strFilename + TEXT(".dvr-idx")).c_str()))
				RETAILMSG(1, (L"*** FAILED TO DELETE A TEMPORARY .DVR-IDX FILE ON CLEANUP ***\n"));
		}
	}
	// now clear out the vectors
	m_cPB.m_vecFiles.clear();
	m_vecPBPermFiles.clear();

	// Clean up the stored media types
	if (m_rgMediaTypes)
	{
		delete[] m_rgMediaTypes;
		m_rgMediaTypes = NULL;
	}
	m_dwNumMediaTypes = 0;

	// Clean up the custom pipeline data
	m_dwCustomPipelineDataLen = 0;
	if (m_pbCustomPipelineData)
	{
		delete[] m_pbCustomPipelineData;
		m_pbCustomPipelineData = NULL;
	}

	// Deregister all our callbacks
	ClearPauseBufferCallbacks();
}

void CWriter::RemoveFromPipeline()
{
	Cleanup();
}

ROUTE CWriter::GetPrivateInterface(REFIID riid, void *&rpInterface)
{
    if (riid == IID_IWriter)
    {
        rpInterface = ((IWriter *) this);
        return HANDLED_STOP;
    }
    else if (riid == IID_IPauseBufferMgr)
    {
        rpInterface = ((IPauseBufferMgr *) this);
        return HANDLED_STOP;
    }
    return UNHANDLED_CONTINUE;
}

unsigned char CWriter::AddToPipeline(ICapturePipelineManager& rcManager)
{
	// Keep the pipeline mgr
	m_pPipelineMgr = &rcManager;

	// If I call m_pPipelineMgr->IPipelineManager::RegisterCOMInterface without
	// going throug this temporary variable I get an unresolved external symbol.
	IPipelineManager *pBasePipelineMgr = m_pPipelineMgr;

	// Register our com interfaces
	m_pNextFileSink2 = static_cast<IFileSinkFilter2*>
		(pBasePipelineMgr->RegisterCOMInterface(
						*((TRegisterableCOMInterface<IFileSinkFilter2> *) this)));

	pBasePipelineMgr->RegisterCOMInterface(
						*((TRegisterableCOMInterface<IFileSinkFilter> *) this));

	m_pNextSBC = static_cast<IStreamBufferCapture*>
		(pBasePipelineMgr->RegisterCOMInterface(
					*((TRegisterableCOMInterface<IStreamBufferCapture> *) this)));

	return 0;
}

HRESULT CWriter::SetRecordingPath(LPCOLESTR pszPath)
{
	if (!pszPath)
		return E_POINTER;

	bool fBeganTempRecording = false;

	{ // Scope for lock
		CAutoLock al(&m_csRecSetLock);

		// Check if we can write to this path

		// Add a trailing backslash if necessary
		wstring strPath = pszPath;
		if (strPath.empty() || (strPath[strPath.length()-1] != '\\'))
			strPath += '\\';

		// Make sure that we can write to that path.
		// CanWriteFile() will add a filename.
		if (!CanWriteFile(strPath.c_str()))
			return E_FAIL;

		EnsureSubDirectories(strPath);

		// No updating the path if the recording set is open
		if (m_cRecSet.IsOpen())
			return XACT_E_WRONGSTATE;

		m_strRecPath = pszPath;

		// Add a trailing backslash if it doesn't have one
		if (m_strRecPath[m_strRecPath.size() - 1] != '\\')
			m_strRecPath += '\\';

		// Create a new recording file if necessary.  This code
		// intentionally exists in ProcessInputSample, GetPauseBuffer,
		// and SetRecordingPath.
		if (!m_cRecSet.IsOpen())
		{
			// No recording has been started so just create a default
			// LONG temporary recording.
			RETAILMSG(1, (L"*** RECORDING PATH SET -- ENSURING INITIAL TEMPORARY RECORDING ***\n"));

			_CHR(BeginTemporaryRecordingInternal(MAXLONGLONG));
			fBeganTempRecording = true;
		}
	}

	// See if we need to forward BeginTempRecording on
	if (fBeganTempRecording && m_pNextSBC)
		m_pNextSBC->BeginTemporaryRecording(MAXLONGLONG);

	return S_OK;
}

HRESULT CWriter::GetRecordingPath(LPOLESTR *ppszPath)
{
	// Protect the recording set
	CAutoLock al(&m_csRecSetLock);

	return WStringToOleString(m_strRecPath, ppszPath);
}

LONGLONG CWriter::GetMaxSampleLatency()
{
	return MAX_SAMPLE_LATENCY;
}

LONGLONG CWriter::GetLatestSampleTime()
{
	return m_hyGetTimeValueOfLastSampleWritten;
}

STDMETHODIMP CWriter::SetFileName(LPCOLESTR wszFileName, const AM_MEDIA_TYPE *pmt)
{
	// We don't support this method - as we generate random file names.
	return E_NOTIMPL;
}

STDMETHODIMP CWriter::GetCurFile(LPOLESTR *ppszFileName, AM_MEDIA_TYPE *pmt)
{
	DbgLog((LOG_USER_REQUEST, 3, TEXT("CWriter::GetCurFile() entry\n") ));

	// Protect the recording set
	CAutoLock al(&m_csRecSetLock);

	// Don't chain this method.

	if (!m_cRecSet.IsOpen())
	{
		DbgLog((LOG_USER_REQUEST, 3, TEXT("CWriter::GetCurFile() E_FAIL\n") ));
		return E_FAIL;
	}

	if(pmt)
	{
		ZeroMemory(pmt, sizeof(*pmt));
		pmt->majortype = MEDIATYPE_NULL;
		pmt->subtype = MEDIASUBTYPE_NULL;
	}

	DbgLog((LOG_USER_REQUEST, 3, TEXT("CWriter::GetCurFile() returning\n") ));

	return WStringToOleString(m_cRecSet.GetFileName(), ppszFileName);
}

STDMETHODIMP CWriter::GetBoundToLiveToken(LPOLESTR *ppszToken)
{
	if (m_pNextSBC)
		return m_pNextSBC->GetBoundToLiveToken(ppszToken);
	return E_NOTIMPL;
}

CPauseBufferData* CWriter::GetPauseBuffer()
{
	CPauseBufferData *pPB = NULL;
	bool fBeganTempRecording = false;

	{ // Scope for lock
		CAutoLock al(&m_csRecSetLock);

		// If we don't have a recording open yet and the pause buffer is
		// empty, start an infinite temp one.  This code intentionally exists
		// in ProcessInputSample, GetPauseBuffer, and SetRecordingPath.
		if ((m_cPB.GetCount() <= 0) && !m_cRecSet.IsOpen())
		{
			// No recording has been started so just create a default
			// LONG temporary recording.

			RETAILMSG(1, (L"*** GET PAUSE BUFFER -- ENSURING INITIAL TEMPORARY RECORDING ***\n"));

			if (FAILED(BeginTemporaryRecordingInternal(MAXLONGLONG)))
				return NULL;
			fBeganTempRecording = true;
		}

		// Duplicate the pause buffer
		pPB = m_cPB.Duplicate();

		// Retain the permanent recording files.
		// That is, copy them over from m_vecPBPermFiles to pPB
		for (	vector<SPauseBufferEntry>::reverse_iterator it = m_vecPBPermFiles.rbegin();
				it != m_vecPBPermFiles.rend();
				it++)
		{
			size_t i = pPB->SortedInsert(*it);

			// Set the original entry pointer (uncasting the const)
			((SPauseBufferEntry &)(*pPB)[i]).pOriginal = &(*it);
		}

		// Handle a special case.  If a recording has been started but we haven't written
		// any samples to it yet, then add it anyway because the sample consumer will need it.
		if (m_fJustStartedRecording && m_cRecSet.IsOpen() && (m_cRecSet.m_vecFiles.size() > 0))
		{
			SPauseBufferEntry entry;
			entry.tStart = -1;
			entry.strFilename = (*m_cRecSet.m_vecFiles.begin())->GetFileName();
			entry.fPermanent = m_capMode == STRMBUF_PERMANENT_RECORDING;
			entry.strRecordingName = m_cRecSet.GetFileName();
			entry.pOriginal = NULL;

			// Don't do a sorted insert.  Rather, make sure this new recording file
			// is the last entry.
			pPB->m_vecFiles.push_back(entry);
		}
	}	// End scope lock

	// See if we need to forward BeginTempRecording on
	if (fBeganTempRecording && m_pNextSBC)
		m_pNextSBC->BeginTemporaryRecording(MAXLONGLONG);

	return pPB;
}

ROUTE CWriter::ConfigurePipeline(	UCHAR iNumPins,
									CMediaType cMediaTypes[],
									UINT iSizeCustom,
									BYTE pbCustom[])
{
	// Make sure we're not already funning
	if (m_cRecSet.IsOpen())
	{
		// We don't handle dynamic format changes yet
		ASSERT(FALSE);
		return UNHANDLED_CONTINUE;
	}

    if (g_bEnableFilePreCreation)
    {
    	m_cRecSet.ClearPreppedRecording();
    }

	// Clean up the stored media types
	if (m_rgMediaTypes)
	{
		delete[] m_rgMediaTypes;
		m_rgMediaTypes = NULL;
	}
	m_dwNumMediaTypes = 0;

	// Clean up the custom pipeline data
	m_dwCustomPipelineDataLen = 0;
	if (m_pbCustomPipelineData)
	{
		delete[] m_pbCustomPipelineData;
		m_pbCustomPipelineData = NULL;
	}

	// Save each the media types array
	if (iNumPins > 0)
	{
		m_rgMediaTypes = new CMediaType[iNumPins];
		m_dwNumMediaTypes = iNumPins;

		for (UCHAR i = 0; i < iNumPins; i++)
			m_rgMediaTypes[i] = cMediaTypes[i];
	}

	// Handle a special case.  If pin 0 is mpeg program stream
	// and pin 1 is VBI, then we don't want to write the VBI stream
	// to disk.
	if ((m_dwNumMediaTypes == 2) &&
		(*m_rgMediaTypes[0].Subtype() == MEDIASUBTYPE_MPEG2_PROGRAM) &&
		(*m_rgMediaTypes[1].Subtype() == MEDIASUBTYPE_Line21_BytePair))
	{
		m_dwNumMediaTypes--;
	}

	// Save the custom pipeline buffer if we have one
	if (pbCustom && (iSizeCustom > 0))
	{
		m_pbCustomPipelineData = new BYTE[iSizeCustom];
		m_dwCustomPipelineDataLen = iSizeCustom;
		memcpy(m_pbCustomPipelineData, pbCustom, iSizeCustom);
	}

	return HANDLED_STOP;
}

ROUTE CWriter::ProcessInputSample(	IMediaSample& riSample,
									CDVRInputPin& rcInputPin)
{
	CProcessLongWriteNotifier cProcessLongWriteNotifier(m_piMediaEventSink, m_dwLastHaveAVEvent);
	bool fPauseBufferChanged = false;

	// When a file falls out of the pause buffer, we can't delete it right
	// away because the reader will have it open.  Rather we need to hold
	// on to the file name and then delete it after we fired the update
	// pause buffer event.
	wstring strAgedFileToBeDeleted;

	bool fBeganTempRecording = false;
	bool fIsSyncPoint = false;

	WriterTimeLogger cWriterTimeLogger(TEXT("CWriter::ProcessInputSample()"), 100);
	{ // Scope in which the rec set gets locked

		// Protect the recording set
		CAutoLock al(&m_csRecSetLock);

		// Get the sample media times
		LONGLONG tSampleStart, tSampleEnd;
		if (FAILED(riSample.GetMediaTime(&tSampleStart, &tSampleEnd)))
		{
			throw std::runtime_error("Error in CWriter::ProcessInputSample.  Media time not set.");
			return UNHANDLED_STOP;
		}

		// Create a new recording file if necessary.  This code
		// intentionally exists in ProcessInputSample, GetPauseBuffer,
		// and SetRecordingPath.
		if (!m_cRecSet.IsOpen())
		{
			// No recording has been started so just create a default
			// LONG temporary recording.
			if (!m_fJustStartedRecording)
			{
				RETAILMSG(1, (L"*** PROCESS SAMPLE -- NO RECORDING, STARTING TEMPORARY ONE ***\n"));

				if (FAILED(BeginTemporaryRecordingInternal(MAXLONGLONG)))
					return HANDLED_STOP;
			}
			// else we called BeginTemporaryRecordingInternal() earlier -- we just didn't
			//      have a sample to provide the last bit of info we needed to finalize
			//	    the new recording's first file.
			fBeganTempRecording = true;
		}

		// Update the pause buffer amount
		if (m_cPB.m_vecFiles.size() > 0)
			m_cPB.m_llActualBuffer_100nsec = tSampleStart - m_cPB.m_vecFiles[0].tStart;

		m_cPB.m_llLastSampleTime_100nsec = tSampleStart;

		// See if we should index this sample
		fIsSyncPoint = (riSample.IsSyncPoint() == S_OK);
#if 0
		// This code is better if you expect DVD-compatible MPEG because it
		// enforces the encoding expectation that a sync point sample contains
		// the start of a NAV-PAK and an i-frame:

		bool fDoWeIndex = fIsSyncPoint &&
							(&rcInputPin == &m_pPipelineMgr->GetPrimaryInput())
							&& IsMPEGNavPack(riSample);
#else // !0
		// This code is what you need if you only have sync point samples
		// containing an i-frame:
		bool fDoWeIndex = fIsSyncPoint &&
							(&rcInputPin == &m_pPipelineMgr->GetPrimaryInput());
#endif // !0

		// Continue to pad the old recording until we hit a key frame
		if (m_pcRecPrev)
		{
			if (!fDoWeIndex)
			{
				// Write this sample to the old recording
				if (m_pcRecPrev->WriteSample(	riSample,
												rcInputPin.GetPinNum(),
												m_dwBytesWritten,
												fDoWeIndex))
				{
                    if (m_piMediaEventSink)
                    {
                        m_piMediaEventSink->Notify(STREAMBUFFER_EC_WRITE_FAILURE, GetTickCount(), 0);
                    }
					throw std::runtime_error("Error writing to disk - likely insufficient disk space.");
					return HANDLED_STOP;
				}
				m_dwBytesWritten += riSample.GetActualDataLength();

				// No pause buffer updates while we're still padding the old recording
				return HANDLED_CONTINUE;
			}
			else
			{
				// We hit a key frame, no need to pad the old recording.
				m_pcRecPrev->Close();
				delete m_pcRecPrev;
				m_pcRecPrev = NULL;
			}
		}

		// Write the actual sample
		bool fAddedNewFile = false;
		if (m_cRecSet.WriteSample(	riSample,
									rcInputPin.GetPinNum(),
									m_dwBytesWritten,
									fDoWeIndex,
									&fAddedNewFile))
		{
            if (m_piMediaEventSink)
            {
                m_piMediaEventSink->Notify(STREAMBUFFER_EC_WRITE_FAILURE, GetTickCount(), 0);
            }
			throw std::runtime_error("Error writing to disk - likely insufficient disk space.");
			return HANDLED_STOP;
		}
		m_dwBytesWritten += riSample.GetActualDataLength();

		// Update our last discontinuity time if applicable
		if (riSample.IsDiscontinuity() == S_OK)
			m_tLastDiscontinuity = tSampleStart;

		// Drop a file out of the pause buffer if necessary.   We shouldn't drop
		// a file out if there's 1 or less files in there right now.  Permanent
		// recording files are dropped to m_vecPBPermFiles.
		if (m_cPB.m_vecFiles.size() > 1)
		{
			// The end of file 0 is approximately the same as the start of file 1
			LONGLONG tFile0End = m_cPB.m_vecFiles[1].tStart;

			if ((tSampleStart - tFile0End) >= m_cPB.GetMaxBufferDuration())
			{
				// If the oldest pause buffer file is temporary, then blow it away.
				if (!m_cPB.m_vecFiles[0].fPermanent)
				{
					strAgedFileToBeDeleted = m_cPB.m_vecFiles[0].strFilename;
					const WCHAR *p = strAgedFileToBeDeleted.c_str();

					// Remove the file from the recording set if it's in there
					m_cRecSet.RemoveSubFile(strAgedFileToBeDeleted);

					fPauseBufferChanged = true;
				}
				else
				{
					// Move the permanent recording over to our cache
					m_vecPBPermFiles.push_back(m_cPB.m_vecFiles.front());

					// See if the next file in the pause buffer belongs to the same
					// permanenent recording as the one we just bumped out.
					if (m_vecPBPermFiles.back().strRecordingName !=
						m_cPB.m_vecFiles[1].strRecordingName)
					{
						// This means that the permanent recording has been totally
						// 'aged'.  So clear m_vecPBPermFiles and fire the pause
						// buffer updated event.
						m_vecPBPermFiles.clear();
						fPauseBufferChanged = true;
					}
				}

				// Remove the oldest file from the pause buffer
				m_cPB.m_vecFiles.erase(m_cPB.m_vecFiles.begin());
			}
		}

		// Update the pause buffer if necessary
		if (m_fJustStartedRecording || fAddedNewFile)
		{
			DbgLog((LOG_FILE_OTHER, 3, TEXT("CWriter::ProcessInputSample() -- setting m_fJustStartedRecording to false\n") ));

			m_fJustStartedRecording = false;

			// Populate a new SPauseBufferEntry
			SPauseBufferEntry entry;
			entry.tStart = tSampleStart;
			entry.strFilename = m_cRecSet.m_vecFiles.back()->GetFileName();
			entry.fPermanent = m_capMode == STRMBUF_PERMANENT_RECORDING;
			entry.strRecordingName = m_cRecSet.GetFileName();
			entry.pOriginal = NULL;

			// Add the new entry to our pause buffer
			m_cPB.m_vecFiles.push_back(entry);

			fPauseBufferChanged = true;
		}

		m_hyGetTimeValueOfLastSampleWritten = tSampleEnd;
	}	// End of rec set critical section scope

	// Now that the critical section is unlocked, we can forward our BeginTempRecording and
	// fire our pause buffer update.  If we didn't unlock, we'd have a potential deadlock
	// because the sample consumer/producer each have their own crit sec that will get
	// locked on firing the pause buffer event/BeginTempRecording.

	// See if we need to forward BeginTempRecording on
	if (fBeganTempRecording && m_pNextSBC)
		m_pNextSBC->BeginTemporaryRecording(MAXLONGLONG);

	// See if we need to duplicate and fire off a new pause buffer
	if (fPauseBufferChanged)
		FirePauseBufferUpdated();

	// Now phsyically blow the aged file away off disk if necessary.  Note that
	// because we are now optimizing capture throughput by dispatching
	// pause buffer notification asynchronously, we expect the deletion to fail
	// frequently. If that happens, we'll queue it up for later deletion.
	// This is alway where we do our period deletion:

	if (fIsSyncPoint && !m_vecPendingFileDeletes.empty())
	{
		RetryPendingFileDeletions();
	}
	if (!strAgedFileToBeDeleted.empty())
	{
		if (!CRecordingFile::DeleteFile(strAgedFileToBeDeleted))
		{
#ifdef UNICODE
			DbgLog((LOG_FILE_OTHER, 3, _T("CWriter::ProcessInputSample():  unable to delete %s now, will retry later\n"),
				strAgedFileToBeDeleted.c_str() ));
#else // !UNICODE
			DbgLog((LOG_FILE_OTHER, 3, _T("CWriter::ProcessInputSample():  unable to delete %S now, will retry later\n"),
				strAgedFileToBeDeleted.c_str() ));
#endif // !UNICODE
			m_vecPendingFileDeletes.push_back(strAgedFileToBeDeleted);
		}
	}

	return HANDLED_STOP;
}

void CWriter::EnsureSubDirectories(const std::wstring strPath)
{
	const std::wstring strPathChars = L"0123456789ABCDEF";
	size_t iLevelOne, iLevelTwo;

	for (iLevelOne = 0; iLevelOne < 16; ++iLevelOne)
	{
		std::wstring strLevelOneDir = strPath + strPathChars[iLevelOne];

		(void) CreateDirectory(strLevelOneDir.c_str(), NULL);

		for (iLevelTwo = 0; iLevelTwo < 16; ++iLevelTwo)
		{
			std::wstring strLevelTwoDir = strLevelOneDir + L"\\" + strPathChars[iLevelTwo];

			(void) CreateDirectory(strLevelTwoDir.c_str(), NULL);
		}
	}
}

wstring CWriter::GenerateFullFileName(const std::wstring strRecOnly)
{
	return m_strRecPath + strRecOnly[0] + L"\\" + strRecOnly[1] + L"\\" + strRecOnly;
}

wstring CWriter::GenerateRandomFilename()
{
	// Seed the c runtime random number generator
	static bool fSeededRand = false;
	if (!fSeededRand)
	{
		SYSTEMTIME st;
		GetSystemTime(&st);

		// Empirically, st.wMilliseconds is 0 always on our platform,
		// and I am seeing a lot of duplicate sequences of file names.
		// So I'm trying a different way of computing the seed:
		srand(st.wSecond + 60*st.wMinute + 60*60*st.wHour + 60*60*24*st.wDay);
		fSeededRand = true;
	}

	// Generate
	WCHAR wszFileName[20];
	WIN32_FIND_DATAW fd;
	HANDLE hFind = NULL;		// NULL != INVALID_HANDLE_FILE so we will enter the loop

	// Try generating a random file name 5 times
	while (hFind != INVALID_HANDLE_VALUE)
	{
		// Generate a random filename -- for even distribution into directories,
		// we need to ensure this is padded with leading zeros:
		unsigned uRandom = (((double) rand()) / ((double) RAND_MAX)) * 0xffffffff;
		_snwprintf(wszFileName, 18, L"%8.8x", uRandom);

		// Build the search string
		wstring strSearch = GenerateFullFileName(wszFileName) + L"_*.*";

		// Make sure that there's no file names with that exact name
		hFind = FindFirstFileW(strSearch.c_str(), &fd);
		if (hFind != INVALID_HANDLE_VALUE)
			FindClose(hFind);
	}

	return GenerateFullFileName(wszFileName);
}

void CWriter::FirePauseBufferUpdated()
{
	WriterTimeLogger sWriteTimeLogger(TEXT("CWriter::FirePauseBufferUpdated()"), 30);
	CPauseBufferData *pPB = GetPauseBuffer();
/*
	OutputDebugStringW(L"*** PAUSE BUFFER UPDATED ***\n");

for (size_t i = 0; i < pPB->GetCount(); i++)
{
	const SPauseBufferEntry &s = (*pPB)[i];
	WCHAR w[1000];
	swprintf(w, L"Entry %d of %d\n"
				L"FileName - %s\n"
				L"RecName  - %s\n"
				L"tStart   - ?\n"
				L"fPerm    - %s\n"
				L"---------------\n",
				i+1, pPB->GetCount(),
				s.strFilename.c_str(),
				s.strRecordingName.c_str(),
				s.fPermanent ? L"TRUE" : L"FALSE");
	OutputDebugStringW(w);
}
*/
	FirePauseBufferUpdateEvent(pPB);
	pPB->Release();
}

HRESULT CWriter::GetCaptureMode(STRMBUF_CAPTURE_MODE *pMode,
								LONGLONG *pllMaxBufMillisecs)
{
	// Protect the recording set / pause buffer
	CAutoLock al(&m_csRecSetLock);

	if (!pMode || !pllMaxBufMillisecs)
		return E_INVALIDARG;

	// Convert 100 nanosecs to millisecs
	*pllMaxBufMillisecs = m_cPB.GetMaxBufferDuration() / 10000;
	*pMode = m_capMode;

	return S_OK;
}

HRESULT CWriter::BeginTemporaryRecordingInternal(LONGLONG llBufferSizeInMillisecs)
{
	RETAILMSG(1, (L"*** BEGIN TEMP RECORDING [INTERNALLY REQUESTED] ***\n"));

	// Protect the recording set
	CAutoLock al(&m_csRecSetLock);

	// Prevent overflow by limiting the max buffer size to a year
	if ((llBufferSizeInMillisecs <= 0) ||
		(llBufferSizeInMillisecs > YEAR_IN_MILLISECS))
	{
		llBufferSizeInMillisecs = YEAR_IN_MILLISECS;
	}

	// Close our existing recording set
	if (!m_pcRecPrev)
	{
		m_pcRecPrev = m_cRecSet.ReleaseLastRecordingFile();
	}
	m_cRecSet.Close();
	ASSERT(!m_cRecSet.IsOpen());

	// Set the max file size
	m_cRecSet.SetMaxFileSize(m_cbMaxTemporaryFileSize);

	// Determine if the pause buffer size should actually be bigger than
	// what was passed to this function.

	// Get the true size of the current pause buffer.
	LONGLONG llCurPB = min(m_cPB.GetActualBufferDuration(), m_cPB.GetMaxBufferDuration());

	// Grow our pause buffer if necessary
	m_cPB.m_llMaxBuffer_100nsec = max(llCurPB, llBufferSizeInMillisecs * 10000);

	// Create a new recording
	ASSERT(m_dwNegotiatedBufferSize);
	ASSERT(!m_cRecSet.IsOpen());

    DWORD dwCreateNew = E_FAIL;
    if (g_bEnableFilePreCreation)
    {
	    dwCreateNew = m_cRecSet.UsePreparedNew(m_dwNegotiatedBufferSize,
							    m_rgMediaTypes,
							    m_dwNumMediaTypes,
							    m_pbCustomPipelineData,
							    m_dwCustomPipelineDataLen,
							    true);
    }
	if (ERROR_SUCCESS != dwCreateNew)
	{
		dwCreateNew = m_cRecSet.CreateNew(GenerateRandomFilename().c_str(),
							m_dwNegotiatedBufferSize,
							m_rgMediaTypes,
							m_dwNumMediaTypes,
							m_pbCustomPipelineData,
							m_dwCustomPipelineDataLen,
							true);
	}
	if (dwCreateNew)
	{
		DbgLog((LOG_ERROR, 3, TEXT("CWriter::BeginTemporaryRecordingInternal() -- returning E_FAIL, CreateNew() returned %x\n"), dwCreateNew ));
        if (m_piMediaEventSink)
        {
            m_piMediaEventSink->Notify(STREAMBUFFER_EC_WRITE_FAILURE, GetTickCount(), 0);
        }
		return E_FAIL;
	}

	m_capMode = STRMBUF_TEMPORARY_RECORDING;
	m_fJustStartedRecording = true;

    if (g_bEnableFilePreCreation)
    {
    	m_cRecSet.PrepareNextRecording(GenerateRandomFilename());
    }
	
	DbgLog((LOG_FILE_OTHER, 3, TEXT("CWriter::BeginTemporaryRecordingInternal() -- setting m_fJustStartedRecording to true\n") ));

	RETAILMSG(1, (L"Recording (%s) started\n", m_cRecSet.GetFileName().c_str()));
	
	return S_OK;
}

HRESULT CWriter::BeginTemporaryRecording(LONGLONG llBufferSizeInMillisecs)
{
	RETAILMSG(1, (L"*** BEGIN TEMP RECORDING [EXTERNALLY REQUESTED] ***\n"));

	_CHR(BeginTemporaryRecordingInternal(llBufferSizeInMillisecs));

	RETAILMSG(1, (L"*** BEGIN TEMP RECORDING [EXTERNALLY REQUESTED] -- EXIT ***\n"));

	// Pass the call on but don't respect the return value. 
	// And do not hold the lock
	if (m_pNextSBC)
		m_pNextSBC->BeginTemporaryRecording(llBufferSizeInMillisecs);

	return S_OK;
}

HRESULT CWriter::BeginPermanentRecording(	LONGLONG llRetainedSizeInMillisecs,
											LONGLONG *pllActualRetainedSizeInMillisecs)
{
	WriterTimeLogger sWriteTimeLogger(TEXT("CWriter::BeginPermanentRecording()"), 100);
	RETAILMSG(1, (L"*** BEGIN PERMANENT RECORDING (%I64d sec requested) ***\n",
		llRetainedSizeInMillisecs / 1000));

	_CP(pllActualRetainedSizeInMillisecs);
	*pllActualRetainedSizeInMillisecs = 0;

	{
		// Protect the recording set
		CAutoLock al(&m_csRecSetLock);

		// Tell the recording set that we're in permanent mode now.
		m_cRecSet.SetRecordingMode(false);

		// Prevent overflow by limiting the max buffer size to a year
		if (llRetainedSizeInMillisecs > YEAR_IN_MILLISECS)
			llRetainedSizeInMillisecs = YEAR_IN_MILLISECS;

		bool fFireNewPauseBuffer = false;

		// See if we need to convert part of a temporary recording
		// over to permanent.
		//
		// We get faster response if we convert even if 0 milliseconds
		// are retained -- it is much faster to update the header of
		// a single file to retain 0 than to create a new recording:

		if ((m_capMode == STRMBUF_TEMPORARY_RECORDING)
			&& m_cRecSet.IsOpen()
			&& !m_cRecSet.m_vecFiles.empty()
			&& ((llRetainedSizeInMillisecs > 0) || (m_cRecSet.m_vecFiles.size() == 1)))
		{
			// Handle the trivial case, where the 1 file in m_vecFiles hasn't
			// been added to the pause buffer yet.
			CRecordingFile *pFile = m_cRecSet.m_vecFiles.back();
			ASSERT(pFile);
			if (!pFile->HasWroteFirstSample())
			{
				// This means our most recent recording file is empty.  So
				// we can't convert any of it.
				ASSERT(pFile->GetHeader().fTemporary);
				ASSERT(m_fJustStartedRecording);
				if (pFile->SetTempPermStatus(-1, false))
				{
					_CHR(E_FAIL);
				}

				pFile->SetSizeLimit(m_cbMaxTemporaryFileSize);
			}
			else // convert as many recording files as necessary
			{
				// Calculate where we want the conversion to occur.  We don't want
				// to pull in any data from before the last discontinuity.
				LONGLONG tSearch = max(m_tLastDiscontinuity, m_cPB.m_llLastSampleTime_100nsec - llRetainedSizeInMillisecs * 10000);

				// Walk through list of pause buffer entries. All entries which contain
				// samples >= tStart will be converted.  Then break out once we've converted
				// the last entry.
				wstring strFirstConverted;
				bool fConvertedFirst = false;
				for (vector<SPauseBufferEntry>::reverse_iterator it = m_cPB.m_vecFiles.rbegin();
					it != m_cPB.m_vecFiles.rend();
					it++)
				{
					fFireNewPauseBuffer = true;

					// When walking backwards through the pause buffer, once we hit a different recording
					// set, bail out.  We can only convert files in the current recording set.
					if (!fConvertedFirst)
					{
						// Store the root file name of the first recording file
						fConvertedFirst = true;
						strFirstConverted = it->strFilename.substr(0, it->strFilename.rfind(L"_"));
					}
					else
					{
						// See if we've backed into a different recording
						if (strFirstConverted != it->strFilename.substr(0, it->strFilename.rfind(L"_")))
							break;
					}

					// Convert this entry
					it->fPermanent = true;

					if (it->tStart >= tSearch)
					{
						// We want to convert this entire file - so use -1 as the start time
						m_cRecSet.SetTempPermStatus(it->strFilename, -1, false);

						// Update how much we've converted
						*pllActualRetainedSizeInMillisecs = (m_cPB.GetLastSampleTime() - it->tStart) / 10000;
					}
					else
					{
						// Front truncate the file.
						m_cRecSet.SetTempPermStatus(it->strFilename, tSearch, false);
						
						// Update how much we've converted
						*pllActualRetainedSizeInMillisecs = (m_cPB.GetLastSampleTime() - tSearch) / 10000;

						// Advance 'it' to the next file and bail.
						it++;
						break;
					}
				}
			
				// Now walk the rest of the pause buffer and make sure these entries
				// aren't in the recording set any more.
				for (; it != m_cPB.m_vecFiles.rend(); it++)
				{
					if (!m_cRecSet.RemoveSubFile(it->strFilename))
					{
						// Once a file can't be found in the recording set,
						// any previous files won't be there either.
						break;
					}
				}

				// We don't want to grow the recording file beyond its padded size -- because
				// that may glitch a/v:

				pFile = m_cRecSet.m_vecFiles.back();
				if (pFile)
				{
					pFile->SetSizeLimit(m_cbMaxTemporaryFileSize);
				}

			} // end else convert as many recording files as necessary
		}
		else
		{
			// We're not coverting an existing recording.  So close the existing
			// one and create a new one.  This check is correct, if m_pcRecPrev
			// is non-null, that means that we're still waiting for a keyframe.
			// So if the following check fails, then no samples were written to the
			// current recording set so it can be closed just fine.
			if (!m_pcRecPrev)
			{
				m_pcRecPrev = m_cRecSet.ReleaseLastRecordingFile();
			}
			m_cRecSet.Close();

			// Create a new recording
			ASSERT(m_dwNegotiatedBufferSize);

            DWORD dwCreateNew = E_FAIL;
            if (g_bEnableFilePreCreation)
            {
			    dwCreateNew = m_cRecSet.UsePreparedNew(m_dwNegotiatedBufferSize,
							    m_rgMediaTypes,
							    m_dwNumMediaTypes,
							    m_pbCustomPipelineData,
							    m_dwCustomPipelineDataLen,
							    false);
            }
			if (ERROR_SUCCESS != dwCreateNew)
			{
				dwCreateNew = m_cRecSet.CreateNew(GenerateRandomFilename().c_str(),
									m_dwNegotiatedBufferSize,
									m_rgMediaTypes,
									m_dwNumMediaTypes,
									m_pbCustomPipelineData,
									m_dwCustomPipelineDataLen,
									false);
				if (ERROR_SUCCESS != dwCreateNew)
				{
            		DbgLog((LOG_ERROR, 3, TEXT("CWriter::BeginPermanentRecording() -- returning E_FAIL, CreateNew() returned %x\n"), dwCreateNew ));
                    if (m_piMediaEventSink)
                    {
                        m_piMediaEventSink->Notify(STREAMBUFFER_EC_WRITE_FAILURE, GetTickCount(), 0);
                    }
					return E_FAIL;
				}
				CRecordingFile *pFile = m_cRecSet.m_vecFiles.back();
				if (pFile)
				{
					pFile->SetSizeLimit(m_cbMaxTemporaryFileSize);
				}
			}

			m_fJustStartedRecording = true;
            if (g_bEnableFilePreCreation)
            {
    			m_cRecSet.PrepareNextRecording(GenerateRandomFilename());
            }
			DbgLog((LOG_FILE_OTHER, 3, TEXT("CWriter::BeginPermanentRecording() -- setting m_fJustStartedRecording to true\n") ));
		}

		// Set the max file size
		m_cRecSet.SetMaxFileSize(m_cbMaxPermanentFileSize);

		m_capMode = STRMBUF_PERMANENT_RECORDING;

		// Fire off a new pause buffer
		if (fFireNewPauseBuffer)
			FirePauseBufferUpdated();
	}

	// Pass the call on but don't respect the return value.
	// And do not hold the lock
	if (m_pNextSBC)
	{
		LONGLONG llTemp;
		m_pNextSBC->BeginPermanentRecording(llRetainedSizeInMillisecs,
											&llTemp);
	}

	RETAILMSG(1, (L"Recording (%s) started (%I64d sec retained)\n",
		m_cRecSet.GetFileName().c_str(),
		*pllActualRetainedSizeInMillisecs / 1000));

	return S_OK;
}

HRESULT CWriter::ConvertRecordingFileToTempOnDisk(const SPauseBufferEntry &entry)
{
	WriterTimeLogger sWriteTimeLogger(TEXT("CWriter::ConvertRecordingFileToTempOnDisk()"), 30);
	// Protect the recording set
	CAutoLock al(&m_csRecSetLock);

	if (!entry.fPermanent)
	{
		// Nothing to do
		return S_OK;
	}

	// See if entry is currently being written to in m_pcRecPrev
	if (m_pcRecPrev &&
		!_wcsicmp(m_pcRecPrev->GetFileName().c_str(), entry.strFilename.c_str()))
	{
		return m_pcRecPrev->SetTempPermStatus(-1, true) ? E_FAIL : S_OK;
	}

	// Convert the file if it's part of the recording set
	if (!m_cRecSet.SetTempPermStatus(entry.strFilename, -1, true))
		return S_OK;

	// Harder case, we need to reopen the file and update it.
	CRecordingFile recFile;
	if (recFile.Open(entry.strFilename.c_str(), NULL, NULL, NULL))
	{
		_CHR(E_FAIL);
	}

	// Update the header
	recFile.GetHeader().fTemporary = true;
	recFile.GetHeader().tVirtualStartTime = -1;
	if (recFile.WriteHeader())
	{
		_CHR(E_FAIL);
	}
	return S_OK;
}

HRESULT CWriter::ConvertToTemporaryRecording(LPCOLESTR pszFileName)
{
	WriterTimeLogger sWriteTimeLogger(TEXT("CWriter::ConvertToTemporaryRecording()"), 30);
	OutputDebugStringW(L"*** CONVERT TO TEMPORARY RECORDING ***\n");

	{
		// Protect the recording set
		CAutoLock al(&m_csRecSetLock);

		if (!pszFileName || !*pszFileName)
			return E_INVALIDARG;

		bool fConverted = false;
		bool fTestedFirstFile = false;

		CSmartRefPtr<CPauseBufferData> pPB;
		pPB.Attach(GetPauseBuffer());
		if (!pPB)
			return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);

		// Walk through the pause buffer.
		for (size_t i = pPB->GetCount() - 1; (i+1) >= 1; i--)
		{
			const SPauseBufferEntry &entry = (*pPB)[i];

			wstring strTemp = entry.strFilename;

			// strTemp now looks something like 1951290_14 while pszFileName is
			// something like 1951290
			// So remove the file number off of strTemp;
			strTemp = strTemp.substr(0, strTemp.rfind(L"_"));

			// Now we can compare the strings
			if (!_wcsicmp(strTemp.c_str(), pszFileName))
			{
				if (!fTestedFirstFile)
				{
					// This means that the file we're converting is currently being
					// recorded.  So we need to tell the recording set to make small
					// files again.
					m_cRecSet.SetMaxFileSize(m_cbMaxTemporaryFileSize);
					m_capMode = STRMBUF_TEMPORARY_RECORDING;
				}

				// Update the file on disk
				_CHR(ConvertRecordingFileToTempOnDisk(entry));

				// Update the status in the pause buffer.  We're actually dealing
				// with a duplicate entry so we want to refer to the original.
				if (entry.pOriginal)
					entry.pOriginal->fPermanent = false;

				fConverted = true;
			}

			fTestedFirstFile = true;
		}

		if (!fConverted)
		{
			return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
		}

		// Blow away any of the files in m_vecPBPermFiles which are no longer permanent
		// This code isn't the most efficient since we might walk over items multiple
		// times.  However, we expect this list to be small wbich lets us use this
		// straightforward code without a big perf hit.
		vector<SPauseBufferEntry>::iterator it = m_vecPBPermFiles.begin();
		while(it != m_vecPBPermFiles.end())
		{
			if (!it->fPermanent)
			{
				// Nuke it and reset our iterator to the beginning
				RETAILMSG(LOG_LOCKING, (L"## WRITER deleting %s.\n", it->strFilename.c_str()));
				DeleteFileW((it->strFilename + INDEX_FILE_SUFFIX).c_str());
				DeleteFileW((it->strFilename + DATA_FILE_SUFFIX).c_str());
				m_vecPBPermFiles.erase(it);
				it = m_vecPBPermFiles.begin();
			}
			else
				it++;
		}

		// Fire off a new pause buffer
		FirePauseBufferUpdated();
	}

	// Pass the call on but don't respect the return value.
	if (m_pNextSBC)
		m_pNextSBC->ConvertToTemporaryRecording(pszFileName);

	return S_OK;
}

bool CWriter::CanWriteFile(LPCTSTR sFullPathName)
{
	// Try and open a dummy file
	wstring strFileName = sFullPathName;
	strFileName += L"__temp__.__temp__";
	HANDLE hFile = CreateFile(	strFileName.c_str(),
								GENERIC_WRITE,
								FILE_SHARE_WRITE | FILE_SHARE_READ,
								NULL,
								CREATE_ALWAYS,
								FILE_ATTRIBUTE_NORMAL,
								NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return false;

	// Close it and delete it
	CloseHandle(hFile);
	(void)DeleteFile(strFileName.c_str());
	return true;
}

ROUTE CWriter::NotifyAllocator(	IMemAllocator& riAllocator,
								bool fReadOnly,
								CDVRInputPin &rcInputPin)
{
	// Get the allocator properties
	ALLOCATOR_PROPERTIES prop;
	if (FAILED(riAllocator.GetProperties(&prop)))
		throw std::runtime_error("CWriter could not get allocator props.");


    if (g_bEnableFilePreCreation)
    {
    	m_cRecSet.ClearPreppedRecording();
    }

	// BUGBUG: this really should be changed to an array of buffer sizes
	// for true support of multiple streams.  However, I'm pushing this work off
	// until later since we still only have 1 stream written to disk.
	m_dwNegotiatedBufferSize = max((DWORD) prop.cbBuffer, m_dwNegotiatedBufferSize);
	ASSERT(m_dwNegotiatedBufferSize);

	return HANDLED_CONTINUE;
}


ROUTE CWriter::NotifyFilterStateChange(FILTER_STATE	eState)
{
	// Cache the IMediaEventSink so that long sample write notifications can be done
	// without the deadlock risk of doing a QueryInterface() call on a streaming thread
	// and without holding the whole shebang alive beyond the scope of the graph running:

	switch (eState)
	{
	case State_Stopped:
		m_piMediaEventSink.Release();
		break;

	default:
		if (m_pPipelineMgr && !m_piMediaEventSink)
		{
			CBaseFilter &rcBaseFilter = m_pPipelineMgr->GetFilter();
			IFilterGraph *piFilterGraph = rcBaseFilter.GetFilterGraph();
			if (piFilterGraph)
			{
				HRESULT hrQI = piFilterGraph->QueryInterface(IID_IMediaEventSink, (void **)&m_piMediaEventSink.p);
				if (FAILED(hrQI))
				{
					m_piMediaEventSink.p = NULL;
				}
			}
		};
		break;
	};

	return HANDLED_CONTINUE;
}

STDMETHODIMP CWriter::GetCurrentPosition(/* [out] */ LONGLONG *phyCurrentPosition)
{
	if (m_pNextSBC)
		return m_pNextSBC->GetCurrentPosition(phyCurrentPosition);
	return E_NOTIMPL;
}

// @cmember Calls into the helper function to see if rSample is a nav_pack
bool CWriter::IsMPEGNavPack(IMediaSample &rSample)
{
	BYTE *pBuf;
	if (FAILED(rSample.GetPointer(&pBuf)))
		return false;

	BOOL fIsNavPack = FALSE;
	
	if (FAILED(::IsMPEGNavPack(pBuf, rSample.GetActualDataLength(), &fIsNavPack)))
	{
		return false;
	}

	return !!fIsNavPack;
}

void CWriter::RetryPendingFileDeletions()
{
	vector<std::wstring>::iterator iter = m_vecPendingFileDeletes.begin();
	while(iter != m_vecPendingFileDeletes.end())
	{
		std::wstring &strFilename = *iter;
		if (CRecordingFile::DeleteFile(strFilename))
		{
#ifdef UNICODE
			DbgLog((LOG_FILE_OTHER, 3, TEXT("CWriter::RetryPendingFileDeletions() deleted %s\n"), 
				strFilename.c_str()));
#else // !UNICODE
			DbgLog((LOG_FILE_OTHER, 3, TEXT("CWriter::RetryPendingFileDeletions() deleted %S\n"), 
				strFilename.c_str()));
#endif // !UNICODE

			iter = m_vecPendingFileDeletes.erase(iter);
		}
		else
		{
#ifdef UNICODE
			DbgLog((LOG_FILE_OTHER, 3, TEXT("CWriter::RetryPendingFileDeletions() still unable to delete %s\n"), 
				strFilename.c_str() ));
#else // !UNICODE
			DbgLog((LOG_FILE_OTHER, 3, TEXT("CWriter::RetryPendingFileDeletions() still unable to delete %S\n"), 
				strFilename.c_str() ));
#endif // !UNICODE
			
			++iter;
		}
	}
}

} // namespace MSDvr

