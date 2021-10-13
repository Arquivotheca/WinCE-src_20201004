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
#include <stdlib.h>
#include <stdio.h>
#include <windows.h>
#include "cpumon.h"

HANDLE CPUDriver;

//Global variables.  These are needed due to the multiple threads that are spun up.
//using CS to protect them, etc. will impact the CPU and thus throw off the data
//This monitor is NOT threadsafe and only one application using it is menat to be run at
//any given time.

int stop;
bool bStarted=false;
long idleCounter=0L;
HANDLE hMeasureThread=NULL;
HANDLE hIdleThread=NULL;
long lMostrecentidlecount=0;
long CalibrateMax=0;
HANDLE hSharedMem=NULL;

/*
 *
 *  EXTERNAL
 *
 *  CalibrateCPUMon()
 *		This is the first function you must call before using the CPU monitor.  It calibrates
 *		the monitor and allows for accurate results.
 *	
 *	Inputs
 *		none
 *
 *	Returns HRESULT
 *		S_OK signals success
 *		E_FAIL signals failure.  Call GetLastError
 *			ERROR_ALREADY_EXISTS is passed if another application on the system is running the CPU monitor
 *			ERROR_INVALID_FUNCTION is passed if another function failed
 *			Any other functions that return HRs are simply bubbled up through Get/SetLastError
 *
 */
HRESULT CalibrateCPUMon()
{

DWORD prio,start;
HANDLE CalibrateThread;
int retval;

	if(hSharedMem!=NULL){
		SetLastError(ERROR_ALREADY_EXISTS);
		return(E_FAIL);
	}

	/*
	 *	Before we do anything, let's create some shared memory so other (future) CPUmons
	 *	know we are running.
	 */

	hSharedMem=NULL;
	hSharedMem=CreateFileMapping(INVALID_HANDLE_VALUE,NULL,PAGE_READWRITE,0,CPUMON_SHARED_MEM_SIZE,CPUMON_SHARED_MEM_NAME);

	if(hSharedMem==NULL){
		SetLastError(GetLastError());
		return(E_FAIL);
	}

	if(GetLastError()==ERROR_ALREADY_EXISTS){

		//we still need to close the handle to make sure we don't mess up any ref counting	
		CloseHandle(hSharedMem);
		hSharedMem=NULL;
		SetLastError(ERROR_ALREADY_EXISTS);
		return(E_FAIL);
	}

	//if we made it here, no one else is using the CPU Monitor
	stop=0;
	CalibrateThread=GetCurrentThread();
	prio=GetThreadPriority(CalibrateThread);
	
	retval=SetThreadPriority(CalibrateThread,THREAD_PRIORITY_TIME_CRITICAL);
	if(retval==0){
		//let's get rid of the shared memory so that we don't leave it around
		CloseHandle(hSharedMem);
		hSharedMem=NULL;

		SetLastError(ERROR_INVALID_FUNCTION);
		return(E_FAIL);
	}

	start=GetTickCount();
	CalibrateMax=0;

	do{
		UnitOfWork();
		if(stop>0){
			//DO NOTHING
		}

		CalibrateMax++;
	}while(((long)(GetTickCount()-start))<STANDARD_CALIBRATION_TIME);

	retval=SetThreadPriority(CalibrateThread,prio);
	if(retval==0){
		//let's get rid of the shared memory so that we don't leave it around
		CloseHandle(hSharedMem);
		hSharedMem=NULL;

		SetLastError(ERROR_INVALID_FUNCTION);
		return(E_FAIL);
	}

	CalibrateMax=(CalibrateMax*STANDARD_REFRESH_RATE)/STANDARD_CALIBRATION_TIME;

	return S_OK;
}


/*
 *
 *  EXTERNAL
 *
 *  StartCPUMonitor()
 *		no input.  This funciton simply spawns the 2 threads necessary to measure the CPU
 *
 *	Returns HRESULT
 *		S_OK signals success
 *		E_FAIL signals failure.  Call GetLastError:
 *			ERROR_ALREADY_EXISTS is passed if another application on the system is running the CPU monitor
 *			ERROR_NOT_READY is passed if CalibrateCPUMon() was not yet called
 *			ERROR_INVALID_FUNCTION is passed if another function failed
 *			Any other fucntions that return HRs are simply bubbled up through Get/SetLastError
 *
 */
HRESULT StartCPUMonitor()
{

	DWORD ThreadID;

	//Have we calibrated the CPUMon
	if(hSharedMem==NULL){
		SetLastError(ERROR_NOT_READY);
		return(E_FAIL);
	}

	//Start Worker thread
	hIdleThread=CreateThread(NULL,0,IdleWorkerThreadProc,(void *)NULL,0,&ThreadID);
	if(hIdleThread==NULL){
		//SetLastError already called by CreateThread	
		return(E_FAIL);
	}
	
	//Start Sampling thread
	hMeasureThread=CreateThread(NULL,0,UpdateMeasurementsThreadProc,(void *)NULL,0,&ThreadID);
	if(hMeasureThread==NULL){
		//SetLastError already called by CreateThread	
		return(E_FAIL);
	}
	
	bStarted=true;

	return S_OK;
}

/*
 *
 *  EXTERNAL
 *
 *  StopCPUMonitor()
 *		no input.  This function signals the 2 main threads of the monitor to stop and return.
 *
 *	Returns HRESULT
 *		S_OK signals success
 *		E_FAIL signals failure.  For more information, call GetLastError
 *			ERROR_ALREADY_EXISTS is passed if another application on the system is running the CPU monitor
 *			ERROR_NOT_READY is passed if CalibrateCPUMon() was not yet called
 *			ERROR_INVALID_FUNCTION is passed if another function failed
 *			Any other fucntions that return HRs are simply bubbled up through Get/SetLastError
 *
 */
HRESULT StopCPUMonitor()
{

	int rc=0;


	//Have we calibrated the CPUMon
	if(hSharedMem==NULL){
		SetLastError(ERROR_NOT_READY);
		return(E_FAIL);
	}

	//Have we actually started the CPUMon?
	if(bStarted==false){
		//let's get rid of the shared memory so that we don't leave it around
		CloseHandle(hSharedMem);
		hSharedMem=NULL;

		SetLastError(ERROR_NOT_READY);
		return(E_FAIL);
	}

	stop=2;

	rc=WaitForSingleObject(hIdleThread, INFINITE);
	if(rc==WAIT_ABANDONED){
		SetLastError(ERROR_INVALID_FUNCTION);
		return(E_FAIL);
	}

	rc=WaitForSingleObject(hMeasureThread, INFINITE);
	if(rc==WAIT_ABANDONED){
		SetLastError(ERROR_INVALID_FUNCTION);
		return(E_FAIL);
	}

	stop=0;
	bStarted=false;
	idleCounter=0L;
	hMeasureThread=NULL;
	hIdleThread=NULL;
	lMostrecentidlecount=0;
	CalibrateMax=0;

	//let's get rid of the shared memory
	CloseHandle(hSharedMem);
	hSharedMem=NULL;

	return S_OK;
}


/*
 *
 *  EXTERNAL
 *
 *  GetCurrentCPUUtilization()
 *		no input.  This function signals the 2 main threads of the monitor to stop and return.
 *
 *	Returns float (0.00 - 100.00) signifying the % of the CPU currently utilized.
 *	-1.00 indicates and error (call GetLastError):
 *		+ You did not calibrate and/or start the CPU monitor
 *
 */
float GetCurrentCPUUtilization()
{

	float fPercentfree,fPercentused;

	//Have we calibrated the CPUMon
	if(hSharedMem==NULL){
		SetLastError(ERROR_NOT_READY);
		return(-1.00);
	}

	//Have we actually started the CPUMon?
	if(bStarted==false){
		//let's get rid of the shared memory so that we don't leave it around
		CloseHandle(hSharedMem);
		hSharedMem=NULL;

		SetLastError(ERROR_NOT_READY);
		return(-1.00);
	}

	if(CalibrateMax>0){
		fPercentfree=((float)(lMostrecentidlecount)*100)/CalibrateMax;
	}
	else{
		fPercentfree=0;
	}
	fPercentused=(float)(100.000f-fPercentfree);

	return(fPercentused);

}

long RestartIdleCount()
{

long retval;

	retval=idleCounter;
	idleCounter=0;

	return(retval);
}

void UnitOfWork()
{
int counter = 0;

	for (int i=0; i<400; i++){
        counter += i;
	}
	InterlockedIncrement(&idleCounter);

}


DWORD WINAPI IdleWorkerThreadProc(void *arg)
{
	
	SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_IDLE);

	/*
	 *  How many "units of work" can an idle thread do?
	 *  This is how we are going to measure the CPU
	 */
	while(1){
		UnitOfWork();

		//Not useing an event-checking mechanism helps keep this lightweight.
		if(stop>0){
			ExitThread(5);
		}

	}

	return(0);
}

DWORD WINAPI UpdateMeasurementsThreadProc(void *arg)
{

long lStartofquant;

	SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_TIME_CRITICAL);

	RestartIdleCount();
	lStartofquant=GetTickCount();

	while(1){
		Sleep(STANDARD_REFRESH_RATE);

		/*
		 *  Wake up every N milliseconds, reset the idle count and store our last count.
		 *  This basically gives us the abilty to see how "used" the CPU was over the past
		 *  N milliseconds.  CPU utilization is never an instaneous measurement, 
		 *  it's a sliding window measurement.
		 */
		lMostrecentidlecount=RestartIdleCount();
		if(lMostrecentidlecount > CalibrateMax){
			CalibrateMax=lMostrecentidlecount;
		}
		lStartofquant=GetTickCount();

		//Not useing an event-checking mechanism helps keep this lightweight.
		if(stop>0){
			ExitThread(5);
		}
	}

	return(0);
}

