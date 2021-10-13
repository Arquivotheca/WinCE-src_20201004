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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
////////////////////////////////////////////////////////////////////////////////

#include "Timer.h"
#include "Position.h"

// BUGBUG: - 32 bit timer
void Timer::Start()
{
	m_Start = GetTickCount();
}

LONGLONG Timer::Stop()
{
	LONGLONG stop = GetTickCount();
	stop = (stop >= m_Start) ? (stop - m_Start) : (0xffffffff - m_Start + stop);
	return MSEC_TO_TICKS(stop);
}

LONGLONG Timer::Elapsed()
{
	LONGLONG elapsed = GetTickCount();
	elapsed = (elapsed >= m_Start) ? (elapsed - m_Start) : (0xffffffff - m_Start + elapsed);
	return MSEC_TO_TICKS(elapsed);
}