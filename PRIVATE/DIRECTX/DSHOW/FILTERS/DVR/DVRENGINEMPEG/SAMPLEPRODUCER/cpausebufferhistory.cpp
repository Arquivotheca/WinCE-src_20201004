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
#include "DvrInterfaces.h"
#include "..\\PauseBuffer.h"
#include "CPauseBufferHistory.h"

using namespace MSDvr::SampleProducer;

CPauseBufferSegment::CPauseBufferSegment(
					CPauseBufferHistory *pcPauseBufferHistory,
					const std::wstring &pwszRecordingName,
					LONGLONG hyStartTime,
					LONGLONG hyEndTime)
	: m_uRefs(1)
	, m_hyEndTime(hyEndTime)
	, m_fInProgress(false)
	, m_uFirstRecordingFile(0)
	, m_uNumRecordingFiles(1)
	, m_pcPauseBufferData(NULL)
	, m_fIsOrphaned(false)
	, m_fIsPermanent(true)
	, m_pcPauseBufferHistory(pcPauseBufferHistory)
	, m_hyMinimumStartTime(-1)
	, m_fValidData(false)
{
	try {
		m_pcPauseBufferData = CPauseBufferData::CreateNew();

		SPauseBufferEntry sPauseBufferEntry;
		sPauseBufferEntry.strRecordingName = pwszRecordingName;
		// TODO:  How to fix the below?
		sPauseBufferEntry.strFilename = pwszRecordingName;
		sPauseBufferEntry.tStart = hyStartTime;
		sPauseBufferEntry.fPermanent = true;
		sPauseBufferEntry.pOriginal = NULL;
		m_pcPauseBufferData->m_vecFiles.push_back(sPauseBufferEntry);
		m_pcPauseBufferData->m_llActualBuffer_100nsec = hyEndTime - hyStartTime;
		m_pcPauseBufferData->m_llLastSampleTime_100nsec = hyEndTime;
	} catch (const std::exception &) {
		return;
	}
	m_fValidData = true;
}

CPauseBufferSegment::CPauseBufferSegment(CPauseBufferHistory *pcPauseBufferHistory,
										 MSDvr::CPauseBufferData &rcPauseBufferData,
										 size_t recordingStartIdx)
	: m_uRefs(1)
	, m_hyEndTime(0)
	, m_fInProgress(true)
	, m_uFirstRecordingFile(recordingStartIdx)
	, m_uNumRecordingFiles(1)
	, m_pcPauseBufferData(&rcPauseBufferData)
	, m_fIsOrphaned(false)
	, m_fIsPermanent(false)
	, m_pcPauseBufferHistory(pcPauseBufferHistory)
	, m_hyMinimumStartTime(-1)
	, m_fValidData(false)
{
	try {
		m_pcPauseBufferData->AddRef();
		UpdateFromPauseBuffer(rcPauseBufferData, recordingStartIdx);
		if (m_uNumRecordingFiles > 0)
		{
			m_fIsPermanent = (*m_pcPauseBufferData)[m_uFirstRecordingFile].fPermanent;
		}
	} catch (const std::exception &) {
		return;
	}
	m_fValidData = true;
}

CPauseBufferSegment::CPauseBufferSegment(const CPauseBufferSegment &rcPauseBufferSegment)
	: m_uRefs(1)
	, m_hyEndTime(rcPauseBufferSegment.m_hyEndTime)
	, m_fInProgress(rcPauseBufferSegment.m_fInProgress)
	, m_uFirstRecordingFile(rcPauseBufferSegment.m_uFirstRecordingFile)
	, m_uNumRecordingFiles(rcPauseBufferSegment.m_uNumRecordingFiles)
	, m_pcPauseBufferData(rcPauseBufferSegment.m_pcPauseBufferData)
	, m_fIsOrphaned(rcPauseBufferSegment.m_fIsOrphaned)
	, m_fIsPermanent(rcPauseBufferSegment.m_fIsPermanent)
 	, m_pcPauseBufferHistory(rcPauseBufferSegment.m_pcPauseBufferHistory)
	, m_hyMinimumStartTime(-1)
	, m_fValidData(false)
{
	if (m_pcPauseBufferData)
		m_pcPauseBufferData->AddRef();
	m_fValidData = rcPauseBufferSegment.m_fValidData;
}

CPauseBufferSegment::~CPauseBufferSegment()
{
	if (m_pcPauseBufferData)
		m_pcPauseBufferData->Release();
}

void CPauseBufferSegment::Import(MSDvr::CPauseBufferData &rcPauseBufferData, size_t uStartingIndex)
{
	rcPauseBufferData.AddRef();
	m_pcPauseBufferData->Release();
	m_pcPauseBufferData = &rcPauseBufferData;
	UpdateFromPauseBuffer(rcPauseBufferData, uStartingIndex);
}

void CPauseBufferSegment::UpdateFromPauseBuffer(MSDvr::CPauseBufferData &rcPauseBufferData, size_t recordingStartIdx)
{
	m_uFirstRecordingFile = recordingStartIdx;
	m_uNumRecordingFiles = 1;
	size_t uNumRecordingFiles = rcPauseBufferData.GetCount();
	const std::wstring &recordingName = rcPauseBufferData[recordingStartIdx].strRecordingName;
	bool fIsPermanent = rcPauseBufferData[recordingStartIdx].fPermanent;
	size_t uIdxRecordingFile;
	for (uIdxRecordingFile = recordingStartIdx + 1;
		 uIdxRecordingFile < uNumRecordingFiles;
		 ++uIdxRecordingFile)
	{
		if ((rcPauseBufferData[uIdxRecordingFile].strRecordingName != recordingName) ||
			(fIsPermanent != rcPauseBufferData[uIdxRecordingFile].fPermanent))
		{
			if (!fIsPermanent ||
				m_fInProgress ||
				(m_hyEndTime == -1))
			{
				m_hyEndTime = rcPauseBufferData[uIdxRecordingFile].tStart - 1;
				m_fInProgress = false;
#ifdef UNICODE
				DbgLog((LOG_PAUSE_BUFFER, 3,
					_T("CPauseBufferSegment::UpdateFromPauseBuffer():  marking %s as complete with [%I64d, %I64d]\n"),
					GetRecordingName().c_str(), rcPauseBufferData[recordingStartIdx].tStart, m_hyEndTime ));
#else
				DbgLog((LOG_PAUSE_BUFFER, 3,
					_T("CPauseBufferSegment::UpdateFromPauseBuffer():  marking %S as complete with [%I64d, %I64d]\n"),
					GetRecordingName().c_str(), rcPauseBufferData[recordingStartIdx].tStart, m_hyEndTime ));
#endif
			}
			break;
		}
		++m_uNumRecordingFiles;
	}
	if (m_fInProgress)
	{
		if (rcPauseBufferData[uNumRecordingFiles-1].tStart < 0)
			m_hyEndTime = -1;
		else
			m_hyEndTime = rcPauseBufferData.GetLastSampleTime();
	}
}

LONGLONG CPauseBufferSegment::CurrentStartTime()
{
	if (m_uNumRecordingFiles == 0)
		return -1;

	LONGLONG hyStartTime = (m_hyMinimumStartTime < 0) ?
		m_pcPauseBufferData->m_vecFiles[m_uFirstRecordingFile].tStart : m_hyMinimumStartTime;
	if ((hyStartTime >= 0) && !IsPermanent())
	{
		LONGLONG hyTruncationTime = m_pcPauseBufferHistory->GetTruncationPosition();
		if (hyTruncationTime > hyStartTime)
			hyStartTime = hyTruncationTime;
	}
	return hyStartTime;
} // CPauseBufferSegment::CurrentStartTime()

LONGLONG CPauseBufferSegment::CurrentEndTime()
{
	if (m_uNumRecordingFiles == 0)
		return -1;
	if (m_fInProgress)
	{
		m_hyEndTime = m_pcPauseBufferHistory->GetLatestProducerSampleTime();
	}
	LONGLONG tStart = m_pcPauseBufferData->m_vecFiles[m_uFirstRecordingFile].tStart;
	return ((tStart < 0)  || (tStart > m_hyEndTime)) ? -1 : m_hyEndTime;
}

bool CPauseBufferSegment::IsPermanent() const
{
	return m_fIsPermanent;
}

const std::wstring &CPauseBufferSegment::GetRecordingName() const
{
	return m_pcPauseBufferData->m_vecFiles[m_uFirstRecordingFile].strRecordingName;
}

bool CPauseBufferSegment::ContainsFile(const std::wstring &strFileName)
{
	if (m_fValidData)
	{
		unsigned uFileIdx;

		for (uFileIdx = 0; uFileIdx < m_uNumRecordingFiles; ++uFileIdx)
		{
			if (strFileName.compare(m_pcPauseBufferData->m_vecFiles[m_uFirstRecordingFile + uFileIdx].strFilename) == 0)
				return true;
		}
	}

	return false;
} // CPauseBufferSegment::ContainsFile

void CPauseBufferSegment::RevertToTemporary()
{
	m_fIsPermanent = false;

	if (m_fValidData)
	{
		unsigned uFileIdx;

		for (uFileIdx = 0; uFileIdx < m_uNumRecordingFiles; ++uFileIdx)
		{
			m_pcPauseBufferData->m_vecFiles[m_uFirstRecordingFile + uFileIdx].fPermanent = false;
		}
	}
} // CPauseBufferSegment::RevertToTemporary


CPauseBufferHistory::CPauseBufferHistory(const CPauseBufferHistory &rcPauseBufferHistory)
	: m_uRefs(1)
	, m_listpcPauseBufferSegments()
	, m_pcPauseBufferSegmentCurrrent(0)
	, m_cCritSec()
	, m_pcPauseBufferData(rcPauseBufferHistory.m_pcPauseBufferData)
	, m_riSampleSource(rcPauseBufferHistory.m_riSampleSource)
	, m_fValidData(false)
{
	CPauseBufferSegment *pcPauseBufferSegmentCopy = NULL;

	try {
		CAutoLock cAutoLock(&rcPauseBufferHistory.m_cCritSec);

		if (m_pcPauseBufferData)
			m_pcPauseBufferData->AddRef();

		std::vector<CPauseBufferSegment*>::const_iterator iter;

		for (iter = rcPauseBufferHistory.m_listpcPauseBufferSegments.begin();
			 iter != rcPauseBufferHistory.m_listpcPauseBufferSegments.end();
			iter++)
		{
			const CPauseBufferSegment *pcPauseBufferSegmentOrig = *iter;
			pcPauseBufferSegmentCopy = new CPauseBufferSegment(*pcPauseBufferSegmentOrig);
			if (!pcPauseBufferSegmentCopy->WasSuccessfullyConstructed())
			{
				pcPauseBufferSegmentCopy->Release();
				return;
			}
			m_listpcPauseBufferSegments.push_back(pcPauseBufferSegmentCopy);
			m_pcPauseBufferSegmentCurrrent = pcPauseBufferSegmentCopy;
			pcPauseBufferSegmentCopy = NULL;
		}
	} catch (const std::exception &) {
		pcPauseBufferSegmentCopy->Release();
		return;
	}
	m_fValidData = (m_pcPauseBufferData != NULL);
}

CPauseBufferHistory::CPauseBufferHistory(ISampleSource &riSampleSource,
										 const std::wstring &pwszRecordingName,
								LONGLONG hyStartTime,
								LONGLONG hyEndTime)
	: m_uRefs(1)
	, m_listpcPauseBufferSegments()
	, m_pcPauseBufferSegmentCurrrent(0)
	, m_cCritSec()
	, m_pcPauseBufferData(0)
	, m_riSampleSource(riSampleSource)
	, m_fValidData(false)
{
	CPauseBufferSegment *pcPauseBufferSegment = NULL;

	try {
		CAutoLock cAutoLock(&m_cCritSec);

		pcPauseBufferSegment =
			new CPauseBufferSegment(this, pwszRecordingName, hyStartTime, hyEndTime);
		if (!pcPauseBufferSegment->WasSuccessfullyConstructed())
		{
			pcPauseBufferSegment->Release();
			return;
		}
		m_pcPauseBufferData = pcPauseBufferSegment->m_pcPauseBufferData;
		m_pcPauseBufferData->AddRef();
		m_listpcPauseBufferSegments.push_back(pcPauseBufferSegment);
		m_pcPauseBufferSegmentCurrrent = pcPauseBufferSegment;
		pcPauseBufferSegment = NULL;
	} catch (const std::exception &) {
		if (pcPauseBufferSegment)
			pcPauseBufferSegment->Release();
		return;
	}
	m_fValidData = (m_pcPauseBufferData != NULL);
}

CPauseBufferSegment *CPauseBufferHistory::GetCurrentSegment()
{
	CAutoLock cAutoLock(&m_cCritSec);

	if (m_pcPauseBufferSegmentCurrrent)
		m_pcPauseBufferSegmentCurrrent->AddRef();
	return m_pcPauseBufferSegmentCurrrent;
}

CPauseBufferSegment *CPauseBufferHistory::FindSegmentByTime(
	LONGLONG hyCurTime,
	FIND_TIME_RECOVERY_MODE eFindTimeRecoveryMode,
	CPauseBufferSegment *pcPauseBufferSegmentExcluded)
{
	CAutoLock cAutoLock(&m_cCritSec);

	std::vector<CPauseBufferSegment*>::iterator iter;
	CPauseBufferSegment *pcPauseBufferSegmentFound = NULL;

	for (iter = m_listpcPauseBufferSegments.begin();
		 iter != m_listpcPauseBufferSegments.end();
		 iter++)
	{
		CPauseBufferSegment *pcPauseBufferSegment = *iter;
		if ((pcPauseBufferSegment->CurrentStartTime() >= 0) &&
			(pcPauseBufferSegment->CurrentStartTime() > hyCurTime))
		{
			// This segment started (and so ended) too late. All later
			// ones will also be too late.
			switch (eFindTimeRecoveryMode)
			{
			case EXACT_RANGE_ONLY:
			case EARLIER_TIME_ONLY:
				// A later time is not acceptable.
				break;
			case PREFER_EARLIER_TIME:
				// If we found an earlier segment, go with it instead:
				if (pcPauseBufferSegmentFound)
					break;
				// FALLTHROUGH to cases keeping a later segment
			case LATER_TIME_ONLY:
			case PREFER_LATER_TIME:
				// We'll accept an earlier time. We keep going in
				// the hopes of an exact match:
				if (pcPauseBufferSegmentExcluded != pcPauseBufferSegment)
					pcPauseBufferSegmentFound = pcPauseBufferSegment;
				break;
			}
			break;
		}
		else if ((pcPauseBufferSegment->m_hyEndTime >= 0) &&
			(pcPauseBufferSegment->m_hyEndTime <= hyCurTime))
		{
			// This segment ended (and so started) too early:
			switch (eFindTimeRecoveryMode)
			{
			case EXACT_RANGE_ONLY:
			case LATER_TIME_ONLY:
				// An earlier time is not acceptable.
				break;
			case EARLIER_TIME_ONLY:
			case PREFER_EARLIER_TIME:
			case PREFER_LATER_TIME:
				// We'll accept an earlier time. We keep going in
				// the hopes of an exact match:
				if (pcPauseBufferSegmentExcluded != pcPauseBufferSegment)
					pcPauseBufferSegmentFound = pcPauseBufferSegment;
				break;
			}
		}
		else
		{
			// This segment is just right:

			if (pcPauseBufferSegmentExcluded != pcPauseBufferSegment)
				pcPauseBufferSegmentFound = pcPauseBufferSegment;
			break;
		}
	}
	if (pcPauseBufferSegmentFound)
		pcPauseBufferSegmentFound->AddRef();
	return pcPauseBufferSegmentFound;
}

void CPauseBufferHistory::MarkRecordingDone()
{
	CAutoLock cAutoLock(&m_cCritSec);
	std::vector<CPauseBufferSegment*>::reverse_iterator iter;

	iter = m_listpcPauseBufferSegments.rbegin();
	if (iter != m_listpcPauseBufferSegments.rend())
	{
		CPauseBufferSegment *pcPauseBufferSegment = *iter;
		pcPauseBufferSegment->m_fInProgress = false;
#ifdef UNICODE
		DbgLog((LOG_PAUSE_BUFFER, 3,
			_T("CPauseBufferHistory::MarkRecordingDone():  marked recording %s as complete\n"),
			pcPauseBufferSegment->GetRecordingName().c_str() ));
#else
		DbgLog((LOG_PAUSE_BUFFER, 3,
			_T("CPauseBufferHistory::MarkRecordingDone():  marked recording %S as complete\n"),
			pcPauseBufferSegment->GetRecordingName().c_str() ));
#endif
	}
}

CPauseBufferHistory::~CPauseBufferHistory()
{

	ClearHistory();
}

void CPauseBufferHistory::ClearHistory()
{
	std::vector<CPauseBufferSegment*>::iterator iter;

	for (iter = m_listpcPauseBufferSegments.begin();
		 iter != m_listpcPauseBufferSegments.end();
		 iter++)
	{
		CPauseBufferSegment *pcPauseBufferSegment = *iter;
		pcPauseBufferSegment->Release();
	}
	m_listpcPauseBufferSegments.clear();
	if (m_pcPauseBufferData)
	{
		m_pcPauseBufferData->Release();
		m_pcPauseBufferData = NULL;
	}
}

#ifndef SHIP_BUILD
void MSDvr::SampleProducer::DumpPauseBufferHistory(CPauseBufferHistory *pcPauseBufferHistory)
{
	DbgLog((LOG_PAUSE_BUFFER, 4, _T("\nPause buffer recordings:\n")));

	std::vector<CPauseBufferSegment*>::iterator iter;
	for (iter = pcPauseBufferHistory->m_listpcPauseBufferSegments.begin();
		iter != pcPauseBufferHistory->m_listpcPauseBufferSegments.end();
		++iter)
	{
		CPauseBufferSegment *pcPauseBufferSegment = *iter;
		DbgLog((LOG_PAUSE_BUFFER, 4,
			_T("    %s @ %p [%I64d, %I64d] %s %s %s: %u files starting with file #%u\n"),
			pcPauseBufferSegment->GetRecordingName().c_str(),
			pcPauseBufferSegment,
			pcPauseBufferSegment->CurrentStartTime(),
			pcPauseBufferSegment->CurrentEndTime(),
			pcPauseBufferSegment->IsPermanent() ? _T("permanent") : _T("temporary"),
			pcPauseBufferSegment->m_fInProgress ? _T("in-progress") : _T("complete"),
			pcPauseBufferSegment->m_fIsOrphaned ? _T("orphan") : _T("contiguous"),
			pcPauseBufferSegment->m_uNumRecordingFiles,
			pcPauseBufferSegment->m_uFirstRecordingFile ));


		size_t uFileIndex;
		for (uFileIndex = pcPauseBufferSegment->m_uFirstRecordingFile;
			 uFileIndex < pcPauseBufferSegment->m_uFirstRecordingFile + pcPauseBufferSegment->m_uNumRecordingFiles;
			 ++uFileIndex)
		{
			MSDvr::SPauseBufferEntry sPauseBufferEntry;

			sPauseBufferEntry = (*pcPauseBufferSegment->m_pcPauseBufferData)[uFileIndex];
#ifdef UNICODE
			DbgLog((LOG_PAUSE_BUFFER, 4,
				_T("        %s:  %I64d (%s)\n"),
				sPauseBufferEntry.strFilename.c_str(),
				sPauseBufferEntry.tStart,
				sPauseBufferEntry.fPermanent ? _T("permanent") : _T("temporary") ));
#else
			DbgLog((LOG_PAUSE_BUFFER, 4,
				_T("        %S:  %I64d (%s)\n"),
				sPauseBufferEntry.strFilename.c_str(),
				sPauseBufferEntry.tStart,
				sPauseBufferEntry.fPermanent ? _T("permanent") : _T("temporary") ));
#endif
		}
	}
}
#endif // !SHIP_BUILD

void CPauseBufferHistory::Import(MSDvr::CPauseBufferData &rcPauseBufferData)
{
	DbgLog((LOG_PAUSE_BUFFER, 3, _T("CPauseBufferHistory::Import() ... entry\n")));

	CAutoLock cAutoLock(&m_cCritSec);

#ifndef SHIP_BUILD
	DumpPauseBufferHistory(this);
#endif

	// If there are no recordings in the updated pause buffer,
	// clear out our pause buffer:

	rcPauseBufferData.AddRef();
	if (m_pcPauseBufferData)
		m_pcPauseBufferData->Release();
	m_pcPauseBufferData = &rcPauseBufferData;

	size_t uNumRecordings = rcPauseBufferData.GetCount();
	if (uNumRecordings == 0)
	{
		ClearHistory();
		DbgLog((LOG_PAUSE_BUFFER, 3, _T("CPauseBufferHistory::Import() ... cleared history -- no recordings to import\n")));
		return;
	}

	// Scan our recordings and the new recordings in parallel, looking
	// for old recordings that are no longer around and new recordings
	// that we hadn't heard of before. Merge info on others.

	std::vector<CPauseBufferSegment*>::iterator iter = m_listpcPauseBufferSegments.begin();
	size_t uIdxRecordingStart = 0;
	std::wstring recordingName;
	bool fIsPermanent;
	while ((uIdxRecordingStart < uNumRecordings) &&
		   (iter != m_listpcPauseBufferSegments.end()))
	{
		CPauseBufferSegment *pcPauseBufferSegment = *iter;
		recordingName = rcPauseBufferData[uIdxRecordingStart].strRecordingName;
		fIsPermanent = rcPauseBufferData[uIdxRecordingStart].fPermanent;
		if ((recordingName == pcPauseBufferSegment->GetRecordingName()) &&
			(fIsPermanent == pcPauseBufferSegment->IsPermanent()))
		{
			pcPauseBufferSegment->Import(rcPauseBufferData, uIdxRecordingStart);
			uIdxRecordingStart += pcPauseBufferSegment->m_uNumRecordingFiles;
			++iter;
		}
		else
		{
			iter = m_listpcPauseBufferSegments.erase(iter);
			if (pcPauseBufferSegment)
				pcPauseBufferSegment->Release();  // no longer in the vector, needs to be released
		}
	}

	// Since we supposedly had the ordering forgettable-retained in
	// our old segments and we should've received retained-new in
	// the new pause buffer, at this point we should've finished
	// all of forgettable-retained and the retained part of retained-new.

   ASSERT(iter == m_listpcPauseBufferSegments.end());

	while (uIdxRecordingStart < uNumRecordings)
	{
		CPauseBufferSegment *pcPauseBufferSegment =
			new CPauseBufferSegment(this, rcPauseBufferData, uIdxRecordingStart);
		uIdxRecordingStart += pcPauseBufferSegment->m_uNumRecordingFiles;
		try {
			m_listpcPauseBufferSegments.push_back(pcPauseBufferSegment);
		} catch (const std::exception &) {
			// clean up the pause buffer created by new before returning.
			pcPauseBufferSegment->Release();
			throw;
		};
	}

	size_t uNumSegments = m_listpcPauseBufferSegments.size();
	m_pcPauseBufferSegmentCurrrent = (uNumSegments == 0) ? NULL : m_listpcPauseBufferSegments[uNumSegments - 1];

	DbgLog((LOG_PAUSE_BUFFER, 3, _T("CPauseBufferHistory::Import() ... exit\n")));
#ifndef SHIP_BUILD
	DumpPauseBufferHistory(this);
#endif
}

MSDvr::CPauseBufferData *CPauseBufferHistory::GetPauseBufferData()
{
	CAutoLock cAutoLock(&m_cCritSec);

	if (!m_pcPauseBufferData)
		return NULL;
	m_pcPauseBufferData->AddRef();
	return m_pcPauseBufferData;
}

LONGLONG CPauseBufferHistory::GetTruncationPosition()
{
	if (!m_pcPauseBufferData)
		return -0x7fffffffffffffff;
	LONGLONG hyPosition = m_riSampleSource.GetProducerTime();
	LONGLONG hyRetainedWindowSize = m_pcPauseBufferData->GetMaxBufferDuration();
	if (hyPosition < hyRetainedWindowSize)
		return -0x7fffffffffffffff;
	return hyPosition - hyRetainedWindowSize;
}

LONGLONG CPauseBufferHistory::GetSafePlaybackStart(LONGLONG hySafetyMarginFromTruncationPoint)
{
	size_t uNumSegments = m_listpcPauseBufferSegments.size();
	if (uNumSegments == 0)
		return -1;
	CPauseBufferSegment *pcPauseBufferSegmentFirst = m_listpcPauseBufferSegments[0];
	LONGLONG hyPlaybackStart = pcPauseBufferSegmentFirst->CurrentStartTime();
	if (!pcPauseBufferSegmentFirst->IsPermanent())
	{
		LONGLONG hySafelyAheadOfTruncation = GetTruncationPosition() + hySafetyMarginFromTruncationPoint;
		if (hyPlaybackStart < hySafelyAheadOfTruncation)
			hyPlaybackStart = hySafelyAheadOfTruncation;
	}
	return hyPlaybackStart;
} // CPauseBufferHistory::GetSafePlaybackStart


bool CPauseBufferHistory::IsInProgress() const
{
	CAutoLock cAutoLock(&m_cCritSec);

	size_t uNumSegments = m_listpcPauseBufferSegments.size();
	if (uNumSegments == 0)
		return true;
	CPauseBufferSegment *pcPauseBufferSegmentLast = m_listpcPauseBufferSegments[uNumSegments - 1];
	return pcPauseBufferSegmentLast->m_fInProgress;
} // CPauseBufferHistory::IsInProgress

CPauseBufferSegment *CPauseBufferHistory::FindRecording(const WCHAR *pwszRecordingName)
{
	CAutoLock cAutoLock(&m_cCritSec);

	std::vector<CPauseBufferSegment*>::iterator iter;

	for (iter = m_listpcPauseBufferSegments.begin();
		 iter != m_listpcPauseBufferSegments.end();
		 iter++)
	{
		CPauseBufferSegment *pcPauseBufferSegment = *iter;
		if (0 == wcscmp(pwszRecordingName, pcPauseBufferSegment->GetRecordingName().c_str()))
		{
			pcPauseBufferSegment->AddRef();
			return pcPauseBufferSegment;
		}
	}
	return NULL;
} // CPauseBufferHistory::FindRecording

HRESULT CPauseBufferHistory::DiscardOrphanRecording(CPauseBufferSegment *pcPauseBufferSegmentOrphan)
{
	CAutoLock cAutoLock(&m_cCritSec);

	std::vector<CPauseBufferSegment*>::iterator iter;
	bool fFoundRecording = false;
	size_t uFilesDropped = 0;

	for (iter = m_listpcPauseBufferSegments.begin();
		 iter != m_listpcPauseBufferSegments.end();
		 )
	{
		CPauseBufferSegment *pcPauseBufferSegment = *iter;
		ASSERT(pcPauseBufferSegment->m_pcPauseBufferData == m_pcPauseBufferData);
		if (fFoundRecording)
		{
			pcPauseBufferSegment->m_uFirstRecordingFile -= uFilesDropped;

			if (pcPauseBufferSegment->m_fIsOrphaned)
			{
				std::vector<SPauseBufferEntry>::iterator fileIter;
				size_t idx = 0;
				for (fileIter = m_pcPauseBufferData->m_vecFiles.begin();
					fileIter != m_pcPauseBufferData->m_vecFiles.end();
					)
				{
					if (idx >= pcPauseBufferSegment->m_uFirstRecordingFile + pcPauseBufferSegment->m_uNumRecordingFiles)
					{
						// We've removed all the files for this recording
						break;
					}
					else if (idx >= pcPauseBufferSegment->m_uFirstRecordingFile)
					{
						fileIter = m_pcPauseBufferData->m_vecFiles.erase(fileIter);
					}
					else
					{
						++fileIter;
					}
					++idx;
				}
				uFilesDropped += pcPauseBufferSegment->m_uNumRecordingFiles;
				iter = m_listpcPauseBufferSegments.erase(iter);
			}
			else
			{
				++iter;
			}
		}
		else if (pcPauseBufferSegmentOrphan == pcPauseBufferSegment)
		{
			fFoundRecording = true;

			std::vector<SPauseBufferEntry>::iterator fileIter;
			size_t idx = 0;
			for (fileIter = m_pcPauseBufferData->m_vecFiles.begin();
				fileIter != m_pcPauseBufferData->m_vecFiles.end();
				)
			{
				if (idx >= pcPauseBufferSegment->m_uFirstRecordingFile + pcPauseBufferSegment->m_uNumRecordingFiles)
				{
					// We've removed all the files for this recording
					break;
				}
				else if (idx >= pcPauseBufferSegment->m_uFirstRecordingFile)
				{
					fileIter = m_pcPauseBufferData->m_vecFiles.erase(fileIter);
				}
				else
				{
					++fileIter;
				}
				++idx;
			}
			uFilesDropped = pcPauseBufferSegment->m_uNumRecordingFiles;
			iter = m_listpcPauseBufferSegments.erase(iter);
		}
		else
		{
			++iter;
		}
	}
	return S_OK;
} // CPauseBufferHistory::DiscardOrphanRecording()

