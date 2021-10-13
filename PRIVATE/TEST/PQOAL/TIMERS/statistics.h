//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#ifndef __STATISTICS_H
#define __STATISTICS_H

#include <windows.h>
#include "math.h"

#include <assert.h>

/* for dwords */
BOOL CalcStatisticsFromList (
			     DWORD * dwNumList,
			     DWORD dwNumInList,
			     double & doAverage, 
			     double & doStdDev
			     );


/* for doubles */
BOOL CalcStatisticsFromList (
			     double * dwNumList,
			     DWORD dwNumInList,
			     double & doAverage, 
			     double & doStdDev
			     );

BOOL CalcStatisticsFromList (
			     ULONGLONG * dwNumList,
			     DWORD dwNumInList,
			     double & doAverage, 
			     double & doStdDev
			     );

#endif /* __STATISTICS_H */
