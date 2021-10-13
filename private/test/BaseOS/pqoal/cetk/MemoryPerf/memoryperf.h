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

// ***************************************************************************************************************
//
// These functions organize the top level testing of memory performance
// as such, they are composed of collections of sergeant patterns which test individual parts
// and gradually collect the information on the whole system
//
// ***************************************************************************************************************

#pragma once
#include <windows.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include "StridePattern.h"
#include "globals.h"


// ***************************************************************************************************************
// for DE testing, define SIMULATE to simulate systematic variability in the latencies
// ***************************************************************************************************************
//#define SIMULATE

// ***************************************************************************************************************
// Enable testing with a DDRaw surface
// ***************************************************************************************************************
#define BUILD_WITH_DDRAW

// ***************************************************************************************************************
// some constants
// ***************************************************************************************************************
const int ci1K = 1024;
const int ci1M = 1024*1024;
const int ciPageSize = 4096;
const int ciSectionSize = ci1M;
const int ciMinAllocSize = max( ci1M, 800*480*sizeof(DWORD) );  // show be at least as large as a DDRaw Screen buffer


// Repeat each test this many times with different offsets to test various virtual to physical mappings
// 16 is the typical value.  Larger values can be used but don't seem to change the results significantly.
const int ciTestLotSize = 16;  


// ***************************************************************************************************************
// test loops
// ***************************************************************************************************************

extern int (* pfiReadLoop)( const DWORD *, const int, const tStridePattern & );
void ReadLoopSetup( const tStridePattern &ctStrideInfo );

// only define VERIFY_OPTIMIZED when you want to verify the optimized and unoptimized access the same memory
// #define VERIFY_OPTIMIZED

// Read test with a single fixed stride
int iReadLoop1D( const DWORD *pdwBase, const int ciRepeats, const tStridePattern &tStrideInfo );

// Write test with a fixed stride 
int iWriteLoop1D( _Out_cap_(cbBase) DWORD *pdwBase, const unsigned int cbBase, const int ciRepeats, const tStridePattern &tStrideInfo, const DWORD cdw );

// Read test with two strides
int iReadLoop2D( const DWORD *pdwBase, const int ciRepeats, const tStridePattern &tStrideInfo );
int iRead2D4( const DWORD *pdwBase, const int ciRepeats,  const tStridePattern &ctStrideInfo );
int iRead2DBig( const DWORD *pdwBase, const int ciRepeats,  const tStridePattern &ctStrideInfo );

// Write test with two strides
int iWriteLoop2D( _Out_cap_(cbBase) DWORD *pdwBase, const unsigned int cbBase, const int ciRepeats,  const tStridePattern &ctStrideInfo, const DWORD cdw );

// a very special purpose 3-D like pattern
int iReadLoop3D( const DWORD *pdwBase, const int ciRepeats, const tStridePattern &ctStrideInfo );

int iMemCpyLoop( _Out_cap_(cbBase) DWORD *pdwDstBase, const unsigned int cbBase, const DWORD *pdwSrcBase, const int ciRepeats, const tStridePattern &tStrideInfo );

// CPU test 
int iCPULoop( const int ciRepeats );
int iCPULoad( const DWORD *pdwBase, const int ciRepeats, const tStridePattern &ctStrideInfo );

// ***************************************************************************************************************
// utility functions located mainly in utilities.cpp and main test functions located in MemoryPerf.cpp
// ***************************************************************************************************************

__out_range(0,31) int iLog2( int iX );

extern void MemPerfMiscInit();
extern void MemPerfClose(void);
extern bool MemPerfBlockAlloc(bool, bool);
extern void MemPerfBlockFree();

extern bool CachedMemoryPerf(void);
extern bool UnCachedMemoryPerf();
extern bool DDrawMemoryPerf();
extern bool ScoreMemoryPerf();

extern void LogPrintf( __format_string const char* format, ... );

extern int iRoundUp( const int ciNum, const int ciModulo );

extern bool bCSVOpen();
extern void CSVPrintf( __format_string const char* format, ... );
static inline void CSVMessage( const char *pstr ) { if (gtGlobal.bChartDetails) CSVPrintf( pstr ); };


// ***************************************************************************************************************
// a global stride pattern needed only when SIMULATE
// ***************************************************************************************************************

#ifdef SIMULATE
    extern tStridePattern gtspThreshold;
#endif
