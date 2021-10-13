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
////////////////////////////////////////////////////////////////////////////////

#ifndef POSITION_H
#define POSITION_H

#include <windows.h>
#include <vector>

using namespace std;

typedef vector<LONGLONG> PositionList;
// For tests only to verify rate change correctly
typedef vector<double>   RateList;

struct PosRate
{
	bool bPositions;
	LONGLONG start;
	LONGLONG stop;
	bool bRate;
	double rate;
};

struct PlaybackDurationTolerance
{
	DWORD pctDurationTolerance;
	DWORD absDurationTolerance;
};

typedef vector<PosRate> PosRateList;

#define MSEC_TO_TICKS(x) ((LONGLONG)((x)*10000))
#define TICKS_TO_MSEC(x) ((DWORD)((x)/10000))

#define INFINITE_TIME 0xffffffffffffffff

#endif
