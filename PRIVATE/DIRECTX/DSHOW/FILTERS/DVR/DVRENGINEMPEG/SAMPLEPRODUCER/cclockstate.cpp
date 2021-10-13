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
#include "CClockState.h"
#include "../HResultError.h"

using namespace MSDvr;

///////////////////////////////////////////////////////////////////////
//
// Helper class CClockState:
//
///////////////////////////////////////////////////////////////////////

CClockState::CClockState()
	: m_piReferenceClock(0)
	, m_rtStreamBase(0)
	, m_rtPauseStart(0)
	, m_eClockFilterState(FILTER_STATE_STOPPED)
	, m_hyTimeInPause(0)
	, m_rtPauseEndStreamTime(0)
{
} // CClockState::CClockState

CClockState::~CClockState()
{
	Clear();
} // CClockState::~CClockState

void CClockState::CacheFilterClock(CBaseFilter &rcBaseFilter)
{
	if (m_piReferenceClock)
		return;

	HRESULT hr = hr = rcBaseFilter.GetSyncSource(&m_piReferenceClock);

	if (SUCCEEDED(hr) && !m_piReferenceClock)
		hr = E_FAIL;

	if (FAILED(hr))
		throw CHResultError(hr);
	m_piReferenceClock->Release();
} // CClockState::CacheFilterClock

void CClockState::Clear()
{
	m_piReferenceClock = NULL;
	m_rtStreamBase = 0;
	m_rtPauseStart = 0;
	m_eClockFilterState = FILTER_STATE_STOPPED;
	m_hyTimeInPause = 0;
	m_rtPauseEndStreamTime = 0;
} // CClockState::Clear

void CClockState::Stop()
{
	DbgLog((LOG_USER_REQUEST, 3, _T("CClockState::Stop() at time %I64d from state %d\n"), 
		GetClockTime(), m_eClockFilterState));
	m_eClockFilterState = FILTER_STATE_STOPPED;
} // CClockState::Stop

void CClockState::Pause()
{
	REFERENCE_TIME tNow = GetClockTime();

	DbgLog((LOG_USER_REQUEST, 3, _T("CClockState::Pause() at time %I64d from state %d\n"), 
		tNow, m_eClockFilterState));

	switch (m_eClockFilterState)
	{
	case FILTER_STATE_STOPPED:
		m_rtStreamBase = 0; // pending a real value from Run()
		m_rtPauseStart = 0;
		m_hyTimeInPause = 0;
		m_rtPauseEndStreamTime = 0;
		m_eClockFilterState = FILTER_STATE_PAUSED_AFTER_STOP;
		break;

	case FILTER_STATE_PAUSED_AFTER_STOP:
		// Huh?  We should not already be paused. Assert to spot
		// this then continue on.
		ASSERT(m_eClockFilterState != FILTER_STATE_PAUSED_AFTER_STOP);
		break;

	case FILTER_STATE_PAUSED_AFTER_RUN:
		// Huh?  We should not already be paused. Assert to spot
		// this then continue on.
		ASSERT(m_eClockFilterState != FILTER_STATE_PAUSED_AFTER_RUN);
		break;

	case FILTER_STATE_RUNNING:
		ASSERT(m_rtPauseStart == 0);
		if (m_rtPauseStart == 0)
			m_rtPauseStart = tNow;
		m_eClockFilterState = FILTER_STATE_PAUSED_AFTER_RUN;
		break;
	}
} // CClockState::Pause

void CClockState::Run(REFERENCE_TIME rtStart)
{
	REFERENCE_TIME tNow = GetClockTime();

	DbgLog((LOG_USER_REQUEST, 3, _T("CClockState::Run(%I64d) at time %I64d from state %d\n"), 
		rtStart, tNow, m_eClockFilterState));

	if (m_eClockFilterState == FILTER_STATE_PAUSED_AFTER_RUN)
	{
		m_hyTimeInPause += rtStart - m_rtStreamBase;
		m_rtPauseEndStreamTime = tNow - rtStart;
	}
	else
	{
		// Something is wrong -- we are already running. Assert so we can analyze
		// then continue without updating the clock state:

		ASSERT((m_eClockFilterState != FILTER_STATE_RUNNING) &&
			   (m_eClockFilterState != FILTER_STATE_STOPPED));

		if (m_eClockFilterState != FILTER_STATE_STOPPED)
		{
			// We are badly hosed if this is true and we don't clear out old state
			// regarding now long we spent in state paused since our last stop:

			m_hyTimeInPause = 0;
			m_rtPauseEndStreamTime = 0;
		}
	}

	m_rtStreamBase = rtStart;
	m_eClockFilterState = FILTER_STATE_RUNNING;
	m_rtPauseStart = 0;
} // CClockState::Run

REFERENCE_TIME CClockState::GetClockTime() const
{
	REFERENCE_TIME rtTime;
	HRESULT hr = m_piReferenceClock ? m_piReferenceClock->GetTime(&rtTime) : E_FAIL;
	if (FAILED(hr))
		throw CHResultError(hr);
	return rtTime;
} // CClockState::GetClockTime

REFERENCE_TIME CClockState::GetStreamTime() const
{
	REFERENCE_TIME rtStreamTime = 0;

	switch (m_eClockFilterState)
	{
	case FILTER_STATE_STOPPED:
	case FILTER_STATE_PAUSED_AFTER_STOP:
		// The next sample generated will have stream time 0 so we're in a
		// holding pattern in which we are logically keeping m_rtStreamBase
		// in sync with the clock:
		rtStreamTime = 0;
		break;

	case FILTER_STATE_PAUSED_AFTER_RUN:
		// The next sample generated will have roughly the stream time 
		// m_rtPauseStart - m_rtStreamBase (modulo the filter graph manager's
		// estimate of how long it will take to resume):
		rtStreamTime = m_rtPauseStart - m_rtStreamBase;
		break;

	case FILTER_STATE_RUNNING:
		rtStreamTime = GetClockTime() - m_rtStreamBase;
		break;
	}
	return rtStreamTime;
} // CClockState::GetStreamTime

