//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*
 * tuxtimers.h
 */

#ifndef __TUXTIMERS_H__
#define __TUXTIMERS_H__

#include "..\common\disableWarnings.h"

#include <windows.h>
#include <tux.h>

/*
 * This file include TESTPROCs for the cache test functions.  Anything
 * needed by the tux function table must be included here.  This file
 * is included by the file with the tux function table.
 *
 * Please keep non-tux related stuff (stuff local to timers) otu of
 * this file to keep from polluting the name space.
 */

/**************************************************************************
 * 
 *                           Timer Test Function Prototypes
 *
 **************************************************************************/

/*******  getTickCount ****************************************************/

TESTPROCAPI getTickCountLatencyAndResolution(
					     UINT uMsg, 
					     TPPARAM tpParam, 
					     LPFUNCTION_TABLE_ENTRY lpFTE);

TESTPROCAPI getTickCountBackwardsCheck(
				       UINT uMsg, 
				       TPPARAM tpParam, 
				       LPFUNCTION_TABLE_ENTRY lpFTE);

/*******  getTickCount ****************************************************/

/*******  High Performance  ***********************************************/

TESTPROCAPI hiPerfLatencyAndResolution(
				       UINT uMsg, 
				       TPPARAM tpParam, 
				       LPFUNCTION_TABLE_ENTRY lpFTE);

TESTPROCAPI hiPerfBackwardsCheck(
				       UINT uMsg, 
				       TPPARAM tpParam, 
				       LPFUNCTION_TABLE_ENTRY lpFTE);

/*******   System Time (RTC)  *********************************************/

TESTPROCAPI rtcLatencyAndResolution(
				    UINT uMsg, 
				    TPPARAM tpParam, 
				    LPFUNCTION_TABLE_ENTRY lpFTE);

TESTPROCAPI rtcBackwardsCheck(
			      UINT uMsg, 
			      TPPARAM tpParam, 
			      LPFUNCTION_TABLE_ENTRY lpFTE);

/*******  Compare timers  *************************************************/

TESTPROCAPI compareHiPerfToGetTickCount(
					UINT uMsg, 
					TPPARAM tpParam, 
					LPFUNCTION_TABLE_ENTRY lpFTE);

TESTPROCAPI compareRTCToGetTickCount(
					UINT uMsg, 
					TPPARAM tpParam, 
					LPFUNCTION_TABLE_ENTRY lpFTE);

TESTPROCAPI compareHiPerfToRTC(
					UINT uMsg, 
					TPPARAM tpParam, 
					LPFUNCTION_TABLE_ENTRY lpFTE);

/*******  Wall Clock Drift Test ******************************************/

TESTPROCAPI wallClockDriftTest(
			       UINT uMsg, 
			       TPPARAM tpParam, 
			       LPFUNCTION_TABLE_ENTRY lpFTE);

#endif // __TUXTIMERS_H__
