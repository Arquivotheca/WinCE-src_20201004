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

#ifndef POSITION_H
#define POSITION_H

#include <windows.h>
#include <vector>

typedef std::vector<LONGLONG> PositionList;
typedef std::vector<double>   RateList;

struct StartStopPosition
{
	// Percentages compare to the whole duration.
	LONGLONG start;
	LONGLONG stop;
};

struct PosRate
{
	// BUGBUG: Since we have RateList defined we don't need to use two booleans here. 
	// bool bPositions;
	bool bRate;
	LONGLONG start;
	LONGLONG stop;
	double rate;
	PosRate() { start = stop = 0; rate = 1.0; bRate = true; }
	~PosRate() {}
	PosRate &operator=( const PosRate &data )
	{
		bRate = data.bRate;
		start = data.start;
		stop  = data.stop;
		rate  = data.rate;
		return *this;
	}
};

struct PlaybackDurationTolerance
{
	DWORD pctDurationTolerance;
	DWORD absDurationTolerance;
};

typedef std::vector<StartStopPosition> StartStopPosList;
typedef std::vector<PosRate> PosRateList;

#define MSEC_TO_TICKS(x) (((LONGLONG)(x))*10000)
#define TICKS_TO_MSEC(x) ((DWORD)((x)/10000))

#define INFINITE_TIME 0xffffffffffffffff

#endif
