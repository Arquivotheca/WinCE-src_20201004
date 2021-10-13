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
// @doc
// CClockState.h : utility class for maintaining the correspondence
//      between the presentation stream times and the filter graph
//       clock.
//
// (c) Copyright August, 2004 Microsoft, Inc. All rights reserved.
//

#pragma once

namespace MSDvr
{
	class CClockState
	{
	public:
		CClockState();
		~CClockState();

		void CacheFilterClock(CBaseFilter &rcBaseFilter);
		void Clear();
		void Stop();
		void Pause();
		void Run(REFERENCE_TIME rtRunTime);

		REFERENCE_TIME GetClockTime() const;
		REFERENCE_TIME GetStreamTime() const;
		HRESULT GetClock(IReferenceClock** ppIReferenceClock) const
		{
			if (ppIReferenceClock == NULL)
			{
				return E_INVALIDARG;
			}
			*ppIReferenceClock = const_cast<IReferenceClock*>(m_piReferenceClock);
			if (*ppIReferenceClock == NULL)
			{
				ASSERT(FALSE);
				return E_FAIL;
			}
			(*ppIReferenceClock)->AddRef();
			return S_OK;
		}
		LONGLONG GetTimeInPause() const { return m_hyTimeInPause; }
		REFERENCE_TIME GetPauseBoundary() const { return m_rtPauseEndStreamTime; }

	protected:
		IReferenceClock *m_piReferenceClock;
		REFERENCE_TIME m_rtStreamBase;
		REFERENCE_TIME m_rtPauseStart;
		LONGLONG m_hyTimeInPause;
		REFERENCE_TIME m_rtPauseEndStreamTime;
		
		enum CLOCK_FILTER_STATE
		{
			FILTER_STATE_STOPPED,
			FILTER_STATE_PAUSED_AFTER_STOP,
			FILTER_STATE_PAUSED_AFTER_RUN,
			FILTER_STATE_RUNNING
		};

		CLOCK_FILTER_STATE m_eClockFilterState;
	};
}
