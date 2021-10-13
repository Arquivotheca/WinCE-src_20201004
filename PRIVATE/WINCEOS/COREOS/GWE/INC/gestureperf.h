//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//

// **************************************************************************
// TouchPerf.h
// 
// Variable declarations and helper routines for
// using CePerf instrumentation.
//
//

#pragma once

#ifdef GESTUREPERF_ENABLE

#include "GwePerf.h"

typedef GWE_PERF GESTURE_PERF;

inline
void GesturePerfOpen()
{
    GwePerfOpen();
}

inline
void GesturePerfClose()
{
    GwePerfClose();
}

inline
void GestureBeginDuration(GESTURE_PERF gpID)
{
    GweBeginDuration(gpID);
}

inline
void GestureEndDuration(GESTURE_PERF gpID)
{
    GweEndDuration(gpID);
}

inline
void GestureIncrementStatistic(GESTURE_PERF gpID)
{
    GweIncrementStatistic(gpID);
}

inline
void GestureStatistic(GESTURE_PERF gpID, DWORD dwValue)
{
    GweSetStatistic(gpID, dwValue);
}


#else

#define GesturePerfOpen()
#define GesturePerfClose()

#define GestureBeginDuration(gPerf)
#define GestureEndDuration(gPerf)
#define GestureIncrementStatistic(gPerf)
#define GestureStatistic(gPerf, dwValue)

#endif // GESTUREPERF_ENABLE

