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
// --------------------------------------------------------------------
//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
//                                                                     
// --------------------------------------------------------------------
#pragma once

#include <map>
#include <string>
#include <qnetwork.h>
#include <dshow.h>
#include <cpumon.h>

class CPerformanceMonitor
{
public:
	struct Recording
	{
		DWORD time;
		int utilization;
		int decdrops;
		int rendrops;
		int drawn;
		int recvdpkts;
		int lostpkts;
		int recoveredpkts;
		int pagesUsed;
		int MemLoad;
	};
	
	enum State {Paused, Running, NotStarted, Stopped}; 
	CPerformanceMonitor(DWORD samplingPeriod = 1000, TCHAR *log = NULL);
	~CPerformanceMonitor();
	//reset all data we have collected
	void ResetData();
	//pause the monitor thread
	HRESULT Pause();
	//run the monitor thread
	HRESULT UnPause();
	//what state is the monitor thread in
	State GetState();
	//returns if CPU was ever over 90%
	bool WasPegged();
	//returns avg CPU utalization for the clip
	float GetAvgUtilization();
	//whether we want to track memory information too
	void SetMemoryMonitor();
	//set interface pointers that we can query
	void SetStatsInterfaces(IAMDroppedFrames* pDroppedFrames, IQualProp *pQualProp, IAMNetworkStatus* pNetworkStatus);

private:
	//write all the data to log 
	void FlushLog();
	//internal thread proc
	DWORD Monitor();
	//static thread proc that will in turn call our internal threadproc
	static DWORD WINAPI StaticThreadProc(PVOID pArg);

private:
	//handle to log file
	HANDLE m_hLogFile;
	//name of log file
	TCHAR m_szLogFile[MAX_PATH];

	//handle to start event
	HANDLE m_evStartThread;
	//handle to stop event
	HANDLE m_evDestroyThread;
	//should we end thread
	bool m_bEndThread;
	//current state of the thread
	State m_sCurrentState;

	//threadID
	DWORD m_dwThreadID;
	//handle to performance thread
	HANDLE m_hPerformanceThread;

	//gap between each time we sample
	DWORD m_dwSamplingPeriod;
	//did CPU ever go over 90%
	bool m_bWasPegged;
	double m_TotalCpuUtil;
	int m_nRecordings;
	//if we reset recordings, how many times did we do that
	DWORD dwOverflow;

	//do they also want memory status ?
	BOOL bMemoryStatus;

	// interface information we get from the test who created us, used for logging and debug purposes only
	IAMDroppedFrames* m_pDroppedFrames;
	IQualProp *m_pRendererQualProp;
	IAMNetworkStatus *m_pNetworkStatus;

	enum {
		MAX_RECORDINGS=1024
	};
	
	Recording recordingBuffer[MAX_RECORDINGS];
};
