//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//
#include "TUXMAIN.H"
#include "TEST_WAVETEST.H"
#include "TEST_WAVEIN.H"
#include "ERRORMAP.H"

void LogWaveInDevCaps(UINT uDeviceID,WAVEINCAPS *pWIC) {
	DWORD i;
   
	LOG(TEXT("Capabilities for Wave In Device:  %u"),	uDeviceID);
	LOG(TEXT("    Product:  %s"),			pWIC->szPname);
	LOG(TEXT("    Manufacture ID:  %u"),		pWIC->wMid);
	LOG(TEXT("    Product ID:  %u"),			pWIC->wPid);
	LOG(TEXT("    Driver Version:  %u"),		pWIC->vDriverVersion);
	LOG(TEXT("    Channels:  %u"),			pWIC->wChannels);
	LOG(TEXT("    Audio Formats:"));
	for(i=0;lpFormats[i].szName;i++)
		LOG(TEXT("        %s %s"),lpFormats[i].szName,(pWIC->dwFormats&lpFormats[i].value?TEXT(" Supported"):TEXT("*****Not Supported*****")));
	LOG(TEXT("    Extended functionality supported:"));
} 

void CALLBACK waveInProcCallback(HWAVEIN hwo2, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2) {
	switch(uMsg) {
	case WIM_CLOSE:
		SetEvent(hEvent);
		if(state==WIM_DATA) {
			state=WIM_CLOSE;
		}
		else LOG(TEXT("ERROR:  WIM_CLOSE received when not done"));
		break;
	case WIM_OPEN:
		SetEvent(hEvent);
		if(state==WIM_CLOSE) {
			state=WIM_OPEN;
		}
		else LOG(TEXT("ERROR:  WIM_OPEN received when not closed"));
		break;
	case WIM_DATA:
		SetEvent(hEvent);
		if(state==WIM_OPEN) {
			state=WIM_DATA;
		}
		else LOG(TEXT("ERROR:  WIM_DATA received when not opened"));
		break;
	default:
		LOG(TEXT("ERROR:  Unknown Message received (%u)"),uMsg);
	}
}

LRESULT CALLBACK windowInProcCallback(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch(uMsg) {
	case MM_WIM_CLOSE:
		waveInProcCallback((HWAVEIN)wParam,WIM_CLOSE,(DWORD)hEvent,0,0);
		return 0;
	case MM_WIM_OPEN:
		waveInProcCallback((HWAVEIN)wParam,WIM_OPEN,(DWORD)hEvent,0,0);
		return 0;
	case MM_WIM_DATA:
		waveInProcCallback((HWAVEIN)wParam,WIM_DATA,(DWORD)hEvent,lParam,0);
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd,uMsg,wParam,lParam);
}

DWORD WINAPI WindowInMessageThreadProc(LPVOID lpParameter) {
	MSG msg;
	WNDCLASS wndclass;

	if(lpParameter) {
		  wndclass.style          =   CS_HREDRAW | CS_VREDRAW;
		  wndclass.lpfnWndProc    =   windowInProcCallback;
		  wndclass.cbClsExtra     =   0;
		  wndclass.cbWndExtra     =   0;
		  wndclass.hInstance      =   g_hInst;
		  wndclass.hIcon          =   NULL;
		  wndclass.hCursor        =   NULL;
		  wndclass.hbrBackground  =   NULL;
		  wndclass.lpszMenuName   =   NULL;
		  wndclass.lpszClassName  =   TEXT("WaveTest");
	  
		  if(RegisterClass( &wndclass )==0) {
		  	LOG(TEXT("Register Class FAILED"));
		  	}
	
		  hwnd = CreateWindow( TEXT("WaveTest"),
			  TEXT("WaveTest"),
			  WS_DISABLED,
			  CW_USEDEFAULT, CW_USEDEFAULT, 
			  CW_USEDEFAULT, CW_USEDEFAULT,
			  NULL, NULL, 
			  g_hInst,
		  	  &g_hInst );
		  if(hwnd==NULL) 
		  	LOG(TEXT("CreateWindow Failed"));
		  SetEvent(hEvent);
	}
	
	while(GetMessage(&msg,NULL,0,0)) {
		if(NULL==msg.hwnd) {
			windowInProcCallback(msg.hwnd,msg.message,msg.wParam,msg.lParam);
		}
		else {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	if(lpParameter) {
		DestroyWindow(hwnd);
		UnregisterClass(wndclass.lpszClassName,g_hInst);
	}
	return 0;

}

TESTPROCAPI RecordWaveFormat(TCHAR *szWaveFormat,DWORD dwSeconds,DWORD dwNotification,DWORD *hrReturn) {
	HRESULT hr;
	HWAVEIN hwi;
	HWAVEOUT hwo;
	int iResponse;
	WAVEFORMATEX wfx;
	WAVEHDR wh,wh_out;
	HANDLE hThread=NULL;
	char *data=NULL,*data2=NULL;
	DWORD dwExpectedPlaytime,dwTime=0,tr=TPR_PASS,dwCallback;
	timer_type start_count,finish_count,duration,expected,m_Resolution;

	state=WIM_CLOSE;
	hwo=NULL;
	hwi=NULL;
	QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(&m_Resolution));
	if(StringFormatToWaveFormatEx(&wfx,szWaveFormat)!=TPR_PASS) {
		tr|=TPR_FAIL;
		goto Error;
	}
	data=new char[dwSeconds*wfx.nAvgBytesPerSec];
	CheckCondition(!data,"ERROR:  New Failed for Data",TPR_ABORT,"Out of Memory");
	ZeroMemory(data,dwSeconds*wfx.nAvgBytesPerSec);
	data2=new char[dwSeconds*wfx.nAvgBytesPerSec];
	CheckCondition(!data,"ERROR:  New Failed for Data",TPR_ABORT,"Out of Memory");
	hEvent=CreateEvent(NULL,FALSE,FALSE,NULL);
	CheckCondition(hEvent==NULL,"ERROR:  Unable to CreateEvent",TPR_ABORT,"Refer to GetLastError for a Possible Cause");
	switch(dwNotification) {
		case CALLBACK_NULL:
			dwCallback=NULL;
		break;
		case CALLBACK_EVENT:
			dwCallback=(DWORD)hEvent;
		break;
		case CALLBACK_FUNCTION:
			dwCallback=(DWORD)waveInProcCallback;
		break;
		case CALLBACK_THREAD:
			hThread=CreateThread(NULL,0,WindowInMessageThreadProc,(void*)FALSE,0,&dwCallback);
			CheckCondition(hThread==NULL,"ERROR:  Unable to CreateThread",TPR_ABORT,"Refer to GetLastError for a Possible Cause");
		break;
		case CALLBACK_WINDOW:
			hThread=CreateThread(NULL,0,WindowInMessageThreadProc,(void*)TRUE,0,NULL);
			CheckCondition(hThread==NULL,"ERROR:  Unable to CreateThread",TPR_ABORT,"Refer to GetLastError for a Possible Cause");
	 		hr=WaitForSingleObject(hEvent,10000); 
			CheckCondition(hr==WAIT_TIMEOUT,"ERROR:  Window didn't notify its creation within ten seconds",TPR_ABORT,"Unknown");
			CheckCondition(hr!=WAIT_OBJECT_0,"ERROR:  Unknown Error waiting for Window Creation to notify its creation",TPR_ABORT,"Unknown");
			dwCallback=(DWORD)hwnd;
			break;			
	}
	hr=waveInOpen(&hwi,0,&wfx,dwCallback,(DWORD)hEvent,dwNotification);
	if((hr!=MMSYSERR_NOERROR)&&(hrReturn)) {
		*hrReturn=hr;
		tr|=TPR_FAIL;
		goto Error;
	}
	CheckMMRESULT(hr,"ERROR:  Failed to open.  waveInOpen",TPR_ABORT,"Driver doesn't really support this format");
	switch(dwNotification) {
 		case CALLBACK_FUNCTION:
		case CALLBACK_THREAD:
		case CALLBACK_WINDOW:
	 		hr=WaitForSingleObject(hEvent,1000); 
 			QueryCondition(hr==WAIT_TIMEOUT,"ERROR:  Function, Thread or Window didn't notify that it received the open message within one second",TPR_ABORT,"Open message wasn't sent to handler");
			QueryCondition(hr!=WAIT_OBJECT_0,"ERROR:  Unknown error while waiting for driver to open",TPR_ABORT,"Unknown");
		break;
	}
StartRecording:
	ZeroMemory(data2,dwSeconds*wfx.nAvgBytesPerSec);
	ZeroMemory(&wh,sizeof(WAVEHDR));
	wh.lpData=data;
	wh.dwBufferLength=dwSeconds*wfx.nAvgBytesPerSec;
	wh.dwLoops=1;
	wh.dwFlags=WHDR_BEGINLOOP|WHDR_ENDLOOP;
	memcpy(&wh_out,&wh,sizeof(wh));
	wh.lpData=data2;
	dwExpectedPlaytime=wh.dwBufferLength*1000/wfx.nAvgBytesPerSec;
	if(g_useSound) {
		hr=waveOutOpen(&hwo,0,&wfx,NULL,NULL,CALLBACK_NULL);
		if((hr!=MMSYSERR_NOERROR)&&(hrReturn)) {
			*hrReturn=hr;
			tr|=TPR_FAIL;
			goto Error;
		}
		CheckMMRESULT(hr,"ERROR:  Failed to open driver.  waveOutOpen",TPR_ABORT,"Driver doesn't really support this format");
		SineWave(wh_out.lpData,wh_out.dwBufferLength,&wfx);
		hr=waveOutPrepareHeader(hwo,&wh_out,sizeof(WAVEHDR));
		CheckMMRESULT(hr,"ERROR:  Failed to prepare header.  waveOutPrepareHeader",TPR_ABORT,"Driver doesn't really support this format");
	}
	hr=waveInPrepareHeader(hwi,&wh,sizeof(WAVEHDR));
	CheckMMRESULT(hr,"ERROR:  Failed to prepare header.  waveInPrepareHeader",TPR_ABORT,"Driver doesn't really support this format");
	hr=waveInAddBuffer(hwi,&wh,sizeof(WAVEHDR));
	CheckMMRESULT(hr,"ERROR:  Failed to add buffer.  waveInAddBuffer",TPR_ABORT,"Driver doesn't really support this format");
	if(g_interactive) 
		iResponse=MessageBox(NULL,TEXT("Click OK to start recording"),TEXT("Interactive Response"),MB_OK|MB_ICONQUESTION);
	QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&start_count));
	hr=waveInStart(hwi);
	CheckMMRESULT(hr,"ERROR:  Failed to start.  waveInStart",TPR_ABORT,"Driver doesn't really support this format");
	if(g_useSound) {
		hr=waveOutWrite(hwo,&wh_out,sizeof(WAVEHDR));
		CheckMMRESULT(hr,"ERROR:  Failed to write.  waveOutWrite",TPR_ABORT,"Driver doesn't really support this format");
	}
	switch(dwNotification) {
		case CALLBACK_NULL:
			dwTime=0;
			while((!(wh.dwFlags&WHDR_DONE))&&(dwTime<dwExpectedPlaytime+1000)) {
				Sleep(g_dwSleepInterval);
				dwTime+=g_dwSleepInterval;
			}
			QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&finish_count));			
		break;
		case CALLBACK_FUNCTION:
		case CALLBACK_THREAD:
		case CALLBACK_WINDOW:
		case CALLBACK_EVENT:
			hr=WaitForSingleObject(hEvent,dwExpectedPlaytime+1000); 
			QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&finish_count));			
		break;
	}
    	duration=(finish_count-start_count)*1000/m_Resolution;
	expected=dwExpectedPlaytime;
	LOG(TEXT("Resolution:  %I64i Start:  %I64i Finish:  %I64i  Duration:  %I64i Expected:  %I64i\n"),
		m_Resolution,start_count,finish_count,duration,expected);
    	expected=duration-expected;
	if(expected<0)	{
		QueryCondition((-expected>g_dwInAllowance),"ERROR:  expected capturetime is too short",TPR_ABORT,"Driver notifying the end of capture too early");
	}
	else {
		QueryCondition((expected>g_dwInAllowance),"ERROR:  expected capturetime is too long",TPR_ABORT,"Driver notifying the end of capture too late");
		QueryCondition((expected>=1000),"ERROR:  notification not recieved within one second after expected capturetime",TPR_ABORT,"Driver is not signalling notification at all");
	}
	if(g_interactive) {
		iResponse=MessageBox(NULL,TEXT("Click Yes to playback recording or No to re-record recording"),TEXT("Interactive Response"),MB_YESNO|MB_ICONQUESTION);
		switch(iResponse) {
			case IDNO:
				hr=waveOutReset(hwo);
				QueryMMRESULT(hr,"ERROR:  Failed to reset.  waveOutReset",TPR_ABORT,"Driver didn't reset device properly");
				hr=waveOutUnprepareHeader(hwo,&wh_out,sizeof(WAVEHDR));
				CheckMMRESULT(hr,"ERROR:  Failed unprepare header.  waveOutPrepareHeader",TPR_ABORT,"Driver doesn't really support this format");
				hr=waveInStop(hwi);
				QueryMMRESULT(hr,"ERROR:  Failed to stop.  waveInStop",TPR_ABORT,"Driver didn't stop properly");
				hr=waveInReset(hwi);
				QueryMMRESULT(hr,"ERROR:  Failed to reset.  waveInReset",TPR_ABORT,"Driver didn't reset properly");
				hr=waveInUnprepareHeader(hwi,&wh,sizeof(WAVEHDR));
				QueryMMRESULT(hr,"ERROR:  Failed to unprepare.  waveInUnprepare",TPR_ABORT,"Driver doesn't really support this format");
				goto StartRecording;
    		}
	}

	if(g_useSound) {
		LOG(TEXT("Playing back what has been recorded"));
		hr=waveOutReset(hwo);
		QueryMMRESULT(hr,"ERROR:  Failed to reset.  waveOutReset",TPR_ABORT,"Driver didn't reset device properly");
		hr=waveOutUnprepareHeader(hwo,&wh_out,sizeof(WAVEHDR));
		CheckMMRESULT(hr,"ERROR:  Failed unprepare header.  waveOutPrepareHeader",TPR_ABORT,"Driver doesn't really support this format");
	}
	else {
		if(hwi) {
			hr=waveInStop(hwi);
			QueryMMRESULT(hr,"ERROR:  Failed to stop.  waveInStop",TPR_ABORT,"Driver didn't stop properly");
			hr=waveInReset(hwi);
			QueryMMRESULT(hr,"ERROR:  Failed to reset.  waveInReset",TPR_ABORT,"Driver didn't reset properly");
			hr=waveInUnprepareHeader(hwi,&wh,sizeof(WAVEHDR));
			QueryMMRESULT(hr,"ERROR:  Failed to unprepare header.  waveInUnprepareHeader",TPR_ABORT,"Driver didn't unprepare properly");
			hr=waveInClose(hwi);
			QueryMMRESULT(hr,"ERROR:  Failed to close.  waveInClose",TPR_ABORT,"Driver didn't close device properly");
			switch(dwNotification) {
			 	case CALLBACK_FUNCTION:
				case CALLBACK_THREAD:
				case CALLBACK_WINDOW:
		 			hr=WaitForSingleObject(hEvent,1000); 
		  			QueryCondition(hr==WAIT_TIMEOUT,"ERROR:  Function, Thread or Window didn't notify that it received the close message within one second",TPR_ABORT,"Close message wasn't sent to handler");
					QueryCondition(hr!=WAIT_OBJECT_0,"ERROR:  Unknown Error while waiting for driver to close",TPR_ABORT,"Unknown");
	
				break;
			}
			hwi=NULL;
		}

		hr=waveOutOpen(&hwo,0,&wfx,NULL,NULL,CALLBACK_NULL);
		if((hr!=MMSYSERR_NOERROR)&&(hrReturn)) {
			*hrReturn=hr;
			tr|=TPR_FAIL;
			goto Error;
		}
		CheckMMRESULT(hr,"ERROR:  Failed to open driver.  waveOutOpen",TPR_ABORT,"Driver doesn't really support this format");
	}
	memcpy(wh_out.lpData,wh.lpData,wh.dwBytesRecorded);
	hr=waveOutPrepareHeader(hwo,&wh_out,sizeof(WAVEHDR));
	CheckMMRESULT(hr,"ERROR:  Failed to prepare header.  waveOutPrepareHeader",TPR_ABORT,"Driver doesn't really support this format");
StartPlayback:
	wh_out.dwLoops=1;
	hr=waveOutWrite(hwo,&wh_out,sizeof(WAVEHDR));
	CheckMMRESULT(hr,"ERROR:  Failed to write.  waveOutWrite",TPR_ABORT,"Driver doesn't really support this format");
	Sleep((dwSeconds+1)*1000);
	if(g_interactive) {
		iResponse=MessageBox(NULL,TEXT("Click Yes to continue, click no to playback recording again"),TEXT("Interactive Response"),MB_YESNO|MB_ICONQUESTION);
		if(iResponse==IDNO) 	goto StartPlayback;
	}
Error:
	if(hwo) {
		hr=waveOutReset(hwo);
		QueryMMRESULT(hr,"ERROR:  Failed to reset.  waveOutReset",TPR_ABORT,"Driver didn't reset device properly");
		hr=waveOutUnprepareHeader(hwo,&wh_out,sizeof(WAVEHDR));
		QueryMMRESULT(hr,"ERROR:  Failed unprepare header.  waveOutPrepareHeader",TPR_ABORT,"Driver doesn't really support this format");
		hr=waveOutClose(hwo);
		QueryMMRESULT(hr,"ERROR:  Failed to close.  waveOutClose",TPR_ABORT,"Driver didn't close device properly");
	}
	if(hwi) {
		hr=waveInStop(hwi);
		QueryMMRESULT(hr,"ERROR:  Failed to stop.  waveInStop",TPR_ABORT,"Driver didn't stop properly");
		hr=waveInReset(hwi);
		QueryMMRESULT(hr,"ERROR:  Failed to reset.  waveInReset",TPR_ABORT,"Driver didn't reset properly");
		hr=waveInUnprepareHeader(hwi,&wh,sizeof(WAVEHDR));
		QueryMMRESULT(hr,"ERROR:  Failed to unprepare header.  waveInUnprepareHeader",TPR_ABORT,"Driver didn't unprepare properly");
		hr=waveInClose(hwi);
		QueryMMRESULT(hr,"ERROR:  Failed to close.  waveInClose",TPR_ABORT,"Driver didn't close device properly");
		switch(dwNotification) {
		 	case CALLBACK_FUNCTION:
			case CALLBACK_THREAD:
			case CALLBACK_WINDOW:
	 		hr=WaitForSingleObject(hEvent,1000); 
	  		QueryCondition(hr==WAIT_TIMEOUT,"ERROR:  Function, Thread or Window didn't notify that it received the close message within one second",TPR_ABORT,"Close message wasn't sent to handler");
			QueryCondition(hr!=WAIT_OBJECT_0,"ERROR:  Unknown Error while waiting for driver to close",TPR_ABORT,"Unknown");
			break;
		}
	}
	switch(dwNotification) {
		case CALLBACK_WINDOW:
			PostMessage(hwnd,WM_QUIT,0,0);
		break;
		case CALLBACK_THREAD:
			PostThreadMessage(dwCallback,WM_QUIT,0,0);
		break;
	}	
	switch(dwNotification) {
		case CALLBACK_WINDOW:
		case CALLBACK_THREAD:
			hr=WaitForSingleObject(hThread,1000);
		 	QueryCondition(hr==WAIT_TIMEOUT,"ERROR:  Thread didn't release within one second",TPR_ABORT,"Unknown");
			QueryCondition(hr!=WAIT_OBJECT_0,"ERROR:  Unknown Error while waiting for thread to release",TPR_ABORT,"Unknown");
	}
	if(data) {
		delete data;
	}
	if(data2) {
		delete data2;
	}
	if(hEvent) 
		CloseHandle(hEvent);
	if(hThread)
		CloseHandle(hThread);
	return tr;
}

TESTPROCAPI CaptureCapabilities(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE) {
	BEGINTESTPROC
	MMRESULT mmRtn;
	WAVEINCAPS wic;
	DWORD tr=TPR_PASS;

	if(g_bSkipIn) return TPR_SKIP;
	LOG(TEXT("INSTRUCTIONS:  This test case displays driver capabilities."));
 	LOG(TEXT("INSTRUCTIONS:  Driver capabilies need to be confirmed manually"));
	LOG(TEXT("INSTRUCTIONS:  Please confirm that your driver is capable of performing the functionality that it reports"));
 	mmRtn=waveInGetDevCaps(g_dwDeviceNum-1,&wic,sizeof(wic));
	CheckMMRESULT(mmRtn,"ERROR:  Failed to get device caps.  waveInGetDevCaps",TPR_FAIL,"Unknown");
	LogWaveInDevCaps(g_dwDeviceNum-1,&wic);
Error:
	return tr;
}

TESTPROCAPI Capture(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE) {
	BEGINTESTPROC
	int iResponse;
	WAVEINCAPS woc;
	MMRESULT mmRtn;
	DWORD hrReturn,i,itr,tr=TPR_PASS;

	if(g_bSkipIn) return TPR_SKIP;
 	LOG(TEXT("INSTRUCTIONS:  This test case will attempt to record and playback a tone for all common formats"));
 	LOG(TEXT("INSTRUCTIONS:  Please listen to playback to ensure that a tone is played"));
	mmRtn=waveInGetDevCaps(g_dwDeviceNum-1,&woc,sizeof(woc));
	CheckMMRESULT(mmRtn,"ERROR:  Failed to get device caps.  waveInGetDevCaps",TPR_FAIL,"Driver responded incorrectly");
	for(i=0;lpFormats[i].szName;i++) {
    	    	hrReturn=MMSYSERR_NOERROR;
    		LOG(TEXT("Attempting to record %s"),lpFormats[i].szName);
		itr=RecordWaveFormat(lpFormats[i].szName,g_duration,CALLBACK_NULL,&hrReturn);
		if(woc.dwFormats&lpFormats[i].value) {
			if(itr==TPR_FAIL)
				LOG(TEXT("ERROR:  waveInGetDevCaps reports %s is supported, but %s was returned."),lpFormats[i].szName,g_ErrorMap[hrReturn]);
		}
		else if(itr==TPR_PASS) {
			LOG(TEXT("ERROR:  waveInGetDevCaps reports %s is unsupported, but WAVERR_BADFORMAT was not returned."),lpFormats[i].szName);
			itr=TPR_FAIL;
		}
		else {
			itr=TPR_PASS;
		}
		if(g_interactive) {
			iResponse=MessageBox(NULL,TEXT("Did you hear the recorded sound? (cancel to retry)"),TEXT("Interactive Response"),MB_YESNOCANCEL|MB_ICONQUESTION);
			switch(iResponse) {
				case IDNO:
					LOG(TEXT("ERROR:  User said there was no tone produced for %s"),lpFormats[i].szName);
					tr|=TPR_FAIL;
				break;
				case IDCANCEL:
					i--;
			}
		} else tr|=itr;
    	}
Error:
	return tr;
}

TESTPROCAPI CaptureNotifications(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE) {
	BEGINTESTPROC
	int iResponse;
	WAVEINCAPS woc;
	MMRESULT mmRtn;
	DWORD itr,tr=TPR_PASS,i,j;

	if(g_bSkipIn) return TPR_SKIP;
	LOG(TEXT("INSTRUCTIONS:  This test case will attempt to playback a tone for all types of callbacks"));
	LOG(TEXT("INSTRUCTIONS:  Please listen to playback to ensure that a tone is played"));
 	mmRtn=waveInGetDevCaps(g_dwDeviceNum-1,&woc,sizeof(woc));
	CheckMMRESULT(mmRtn,"ERROR:  Failed to get device caps.  waveInGetDevCaps",TPR_FAIL,"Driver responded incorrectly");
	for(i=0;lpFormats[i].szName;i++) 
		if(woc.dwFormats&lpFormats[i].value) break;
	if(!lpFormats[i].szName) {
		LOG(TEXT("ERROR:  There are no supported formats"));
    		return TPR_SKIP;
    	}	
	for(j=0;lpCallbacks[j].szName;j++) {
    		LOG(TEXT("Attempting to record with %s"),lpCallbacks[j].szName);
		itr=RecordWaveFormat(lpFormats[i].szName,g_duration,lpCallbacks[j].value,NULL);
		if(g_interactive) {
			iResponse=MessageBox(NULL,TEXT("Did you hear the recorded sound? (cancel to retry)"),TEXT("Interactive Response"),MB_YESNOCANCEL|MB_ICONQUESTION);
			switch(iResponse) {
				case IDNO:
					LOG(TEXT("ERROR:  User said there was no tone produced for %s"),lpFormats[i].szName);
					tr|=TPR_FAIL;
				break;
				case IDCANCEL:
					j--;
			}
		} else tr|=itr;
	}
Error:
	return tr;
}