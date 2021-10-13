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

namespace MSDvr
{
	class CPauseBufferData;


	namespace SampleProducer
	{
#ifndef SHIP_BUILD
		// This routine is often needed for debugging, but you'll likely need to do
		// this debugging with a non-shipping retail build. TODO - better define
		extern void DumpPauseBufferHistory(SampleProducer::CPauseBufferHistory *pcPauseBufferHistory);
#endif // !SHIP_BUILD

		class ISampleSource
		{
		public:
			virtual LONGLONG GetLatestSampleTime() const = 0;
			virtual LONGLONG GetProducerTime() const = 0;
		};

		class CPauseBufferHistory;

		class CPauseBufferSegment
		{
		public:
			CPauseBufferSegment(CPauseBufferHistory *pcPauseBufferHistory,
								const std::wstring &pwszRecordingName,
								LONGLONG hyStartTime,
								LONGLONG hyEndTime);
			CPauseBufferSegment(CPauseBufferHistory *pcPauseBufferHistory,
								CPauseBufferData &rcPauseBufferData,
								size_t recordingStartIdx);
			CPauseBufferSegment(const CPauseBufferSegment &);

			LONG AddRef() { return InterlockedIncrement(&m_uRefs); }
			LONG Release() { LONG uRefs = InterlockedDecrement(&m_uRefs); if (!uRefs) delete this; return uRefs; }
			LONGLONG CurrentStartTime();
			LONGLONG CurrentEndTime();
			bool IsPermanent() const;
			const std::wstring &GetRecordingName() const;
			void SetMinimumSeekPosition(LONGLONG hyTrueMinSeek) { m_hyMinimumStartTime = hyTrueMinSeek; }

			void Import(CPauseBufferData &rcPauseBufferData, size_t uStartingIndex);

			bool WasSuccessfullyConstructed() { return m_fValidData; }
			bool ContainsFile(const std::wstring &strFileName);
			void RevertToTemporary();

			LONGLONG m_hyEndTime;
			bool m_fInProgress;
			bool m_fIsOrphaned;
			bool m_fIsPermanent;
			size_t m_uFirstRecordingFile;
			size_t m_uNumRecordingFiles;
			CPauseBufferData *m_pcPauseBufferData;
	
		protected:
			~CPauseBufferSegment();
			void UpdateFromPauseBuffer(CPauseBufferData &rcPauseBufferData, size_t recordingStartIdx);

			LONG m_uRefs;
			CPauseBufferHistory *m_pcPauseBufferHistory;
			LONGLONG m_hyMinimumStartTime;
			bool m_fValidData;


		private:
			// The following is not implemented -- deliberately:
			CPauseBufferSegment & operator = (const CPauseBufferSegment&);
		};

		class CPauseBufferHistory
		{
		public:
			enum FIND_TIME_RECOVERY_MODE {
				EXACT_RANGE_ONLY,
				LATER_TIME_ONLY,
				EARLIER_TIME_ONLY,
				PREFER_EARLIER_TIME,
				PREFER_LATER_TIME
			};

			CPauseBufferHistory(ISampleSource &riSampleSource)
				: m_uRefs(1)
				, m_listpcPauseBufferSegments()
				, m_pcPauseBufferSegmentCurrrent(0)
				, m_cCritSec()
				, m_pcPauseBufferData(0)
				, m_riSampleSource(riSampleSource)
				, m_fValidData(false)
					{ m_fValidData = true; }
			CPauseBufferHistory(const CPauseBufferHistory &);
			CPauseBufferHistory(ISampleSource &riSampleSource,
								const std::wstring &pwszRecordingName,
								LONGLONG hyStartTime,
								LONGLONG hyEndTime);

			LONG AddRef() { return InterlockedIncrement(&m_uRefs); }
			LONG Release() { LONG uRefs = InterlockedDecrement(&m_uRefs); if (!uRefs) delete this; return uRefs; }

			// The following four routines all return a pause buffer
			// segment whose reference count has been incremented.
			// The caller is responsible for decrementing it when
			// done:
			CPauseBufferSegment *GetCurrentSegment();
			CPauseBufferSegment *FindSegmentByTime(LONGLONG hyCurTime,
								FIND_TIME_RECOVERY_MODE eFindTimeRecoveryMode,
								CPauseBufferSegment *pcPauseBufferSegmentExcluded = NULL);

			void MarkRecordingDone();
			LONGLONG GetLatestProducerSampleTime() const
				{
					return m_riSampleSource.GetLatestSampleTime();
				}

			void Import(CPauseBufferData &rcPauseBufferData);
			CPauseBufferData *GetPauseBufferData(); // adds a ref to the result
			LONGLONG GetCurrentTime() {
				return m_riSampleSource.GetProducerTime();
			}
			LONGLONG GetTruncationPosition();
			bool WasSuccessfullyConstructed() { return m_fValidData; }
			LONGLONG GetSafePlaybackStart(LONGLONG hySafetyMarginFromTruncationPoint);
			bool IsInProgress() const;
			CPauseBufferSegment *FindRecording(const WCHAR *pwszRecordingName);
			HRESULT DiscardOrphanRecording(CPauseBufferSegment *pcPauseBufferSegment);

			std::vector<CPauseBufferSegment*> m_listpcPauseBufferSegments;

		protected:
			~CPauseBufferHistory();
			void ClearHistory();

			mutable CCritSec m_cCritSec;

			LONG m_uRefs;
			CPauseBufferSegment *m_pcPauseBufferSegmentCurrrent;
			CPauseBufferData *m_pcPauseBufferData;
			ISampleSource &m_riSampleSource;
			bool m_fValidData;


		private:
			// The following is not implemented -- deliberately:
			CPauseBufferHistory & operator = (const CPauseBufferHistory&);
		};
	}
} //
