//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//
#include "TUXMAIN.H"
#include "TEST_DSNDIN.H"
#include "TEST_DSNDTEST.H"
#include "ERRORMAP.H"
#include "MAP.H"

BEGINMAP(DWORD,dsccapflags)
ENTRY(DSCCAPS_EMULDRIVER)
#ifndef UNDER_CE
ENTRY(DSCCAPS_CERTIFIED)
#endif
ENDMAP

void LogDirectSoundCaptureCaps(UINT uDeviceID,DSCCAPS *pWIC) {
	DWORD i;

	LOG(TEXT("Capabilities for DirectSound Input Device:  %u\n"),uDeviceID);
	for(i=0;dsccapflags[i].szName;i++) 
		LOG(TEXT("      %s %s\n"),dsccapflags[i].szName,
			(pWIC->dwFlags&dsccapflags[i].value?TEXT(" Supported"):TEXT(" *****Not Supported*****")));
    	for(i=0;lpFormats[i].szName;i++)
		LOG(TEXT("      %s %s\n"),lpFormats[i].szName,
			(pWIC->dwFormats&lpFormats[i].value?TEXT(" Supported"):TEXT(" *****Not Supported*****")));
} 
	
DWORD RecordWaveFormat(TCHAR *szWaveFormat,DWORD dwSeconds,DWORD dwNotification,HRESULT *hrReturn)
{
	HRESULT hr;
	int iResponse;
	char *data=NULL;
	WAVEFORMATEX wfx;
	HANDLE hEvent=NULL;
	DSBUFFERDESC DSBDesc;
	LPDIRECTSOUND pDS=NULL;
	DSCBUFFERDESC DSCBDesc;
	DSBPOSITIONNOTIFY DSBPN;       
	LPDIRECTSOUNDBUFFER pDSB=NULL;
	LPDIRECTSOUNDNOTIFY pDSN=NULL;
	LPDIRECTSOUNDCAPTURE pDSC=NULL;
	LPDIRECTSOUNDCAPTUREBUFFER pDSCB=NULL;
	LPVOID lpvAudioPtr1,lpvAudioPtr2,lpvAudioPtr1c,lpvAudioPtr2c;
	DWORD dwAudioBytes1,dwAudioBytes2,dwAudioBytes1c,dwAudioBytes2c;
	DWORD tr=TPR_PASS,time=0,dwRead=0,dwWrite=0,dwExpectedCapturetime,dwStatus;
	timer_type start_count,finish_count,duration,expected,m_Resolution;

	QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(&m_Resolution));
	StringFormatToWaveFormatEx(&wfx,szWaveFormat);
	hr=g_pfnDSCCreate(g_DSC,&pDSC,NULL);
	CheckMMRESULT(hr,"ERROR:  Failed to create.  DirectSoundCaptureCreate",TPR_ABORT,"Unable to get driver interface");
	Priority(pDS,0);
	ZeroMemory(&DSBDesc,sizeof(DSBDesc));
	DSBDesc.dwSize=sizeof(DSBDesc);
	DSBDesc.dwBufferBytes=dwSeconds*wfx.nAvgBytesPerSec;
	DSBDesc.lpwfxFormat=&wfx;
	ZeroMemory(&DSCBDesc,sizeof(DSCBDesc));
	DSCBDesc.dwSize=sizeof(DSCBDesc);
	DSCBDesc.dwBufferBytes=dwSeconds*wfx.nAvgBytesPerSec;
	DSCBDesc.lpwfxFormat=&wfx;
	hr=pDSC->CreateCaptureBuffer((DSCBUFFERDESC*)&DSCBDesc,&pDSCB,NULL);
	if((hr!=DS_OK)&&(hrReturn))
		*hrReturn=hr;
	CheckMMRESULT(hr,"ERROR:  Failed to create buffer.  CreateCaptureBuffer",
		TPR_ABORT,"Driver doesn't really support this format");
	dwExpectedCapturetime=DSBDesc.dwBufferBytes*1000/wfx.nAvgBytesPerSec;
	if(g_useSound) {
		hr=g_pfnDSCreate(g_DS,&pDS,NULL);
		CheckMMRESULT(hr,"ERROR:  Failed to create.  DirectSoundCreate",
			TPR_ABORT,"Unable to get driver interface");
		hr=pDS->CreateSoundBuffer(&DSBDesc,&pDSB,NULL);
		CheckMMRESULT(hr,"ERROR:  Failed to create buffer.  CreateSoundBuffer",
			TPR_ABORT,"Driver doesn't really support this format");
		hr=pDSB->Lock(0,0,&lpvAudioPtr1,&dwAudioBytes1,&lpvAudioPtr2,&dwAudioBytes2,DSBLOCK_ENTIREBUFFER);
		CheckMMRESULT(hr,"ERROR:  Failed to Lock (Sound).  Lock",
			TPR_ABORT,"Driver doesn't really support this format");
		SineWave((char*)lpvAudioPtr1,dwAudioBytes1,&wfx);
		hr=pDSB->Unlock(lpvAudioPtr1,dwAudioBytes1,lpvAudioPtr2,dwAudioBytes2);
		CheckMMRESULT(hr,"ERROR:  Failed to Unlock (Sound).  Unlock",
			TPR_ABORT,"Driver doesn't really support this format");
	}
	if(dwNotification==CALLBACK_DIRECTSOUNDNOTIFY) {
		hEvent=CreateEvent(NULL,FALSE,FALSE,NULL);
		CheckCondition(hEvent==NULL,"ERROR:  Unable to CreateEvent",
			TPR_ABORT,"Refer to GetLastError for a Possible Cause");
		DSBPN.dwOffset=DSBPN_OFFSETSTOP;
		DSBPN.hEventNotify=hEvent;
		hr=pDSCB->QueryInterface(IID_IDirectSoundNotify,(LPVOID*)&pDSN);
		CheckMMRESULT(hr,"ERROR:  Failed to create DirectSoundNotify.  QueryInterface",
			TPR_ABORT,"Driver doesn't really support notifications for this format");
		hr=pDSN->SetNotificationPositions(1,&DSBPN);
		CheckMMRESULT(hr,"ERROR:  Failed to set notifications.  SetNotificationPositions",
			TPR_ABORT,"Driver doesn't really support this format");
	}
StartRecording:
	hr=pDSCB->Lock(0,0,&lpvAudioPtr1c,&dwAudioBytes1c,&lpvAudioPtr2c,&dwAudioBytes2c,DSCBLOCK_ENTIREBUFFER);
	CheckMMRESULT(hr,"ERROR:  Failed to Lock (Capture).  Lock",TPR_ABORT,"Driver doesn't really support this format");
	ZeroMemory(lpvAudioPtr1c,dwAudioBytes1c);
	hr=pDSCB->Unlock(lpvAudioPtr1c,dwAudioBytes1c,lpvAudioPtr2c,dwAudioBytes2c);
	CheckMMRESULT(hr,"ERROR:  Failed to Unlock (Capture).  Unlock",
		TPR_ABORT,"Driver doesn't really support this format");
	if(g_interactive) 
		iResponse=MessageBox(NULL,TEXT("Click OK to start recording"),
			TEXT("Interactive Response"),MB_OK|MB_ICONQUESTION);
	QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&start_count));
	hr=pDSCB->Start(0);
	CheckMMRESULT(hr,"ERROR:  Failed to start.  Start", TPR_ABORT,"Driver doesn't really support this format");
	LOG(TEXT("DirectSoundCapture just started\n"));
	if(g_useSound) {
		hr=pDSB->Play(0,0,0);
		CheckMMRESULT(hr,"ERROR:  Failed to play.  Play",TPR_ABORT,"Driver doesn't really support this format");
		LOG(TEXT("DirectSound just started\n"));
	} 
	switch(dwNotification) {
		case CALLBACK_NONE:
			time=0;
			hr=pDSCB->GetStatus(&dwStatus);
			CheckMMRESULT(hr,"ERROR:  DirectSoundBuffer failed to GetStatus",
				TPR_ABORT,"Driver responded incorrectly");
			while((dwStatus&DSCBSTATUS_CAPTURING)&&(time<(dwSeconds+1)*1000)) {
				Sleep(g_dwSleepInterval);
				time+=g_dwSleepInterval;
				hr=pDSCB->GetStatus(&dwStatus);
				CheckMMRESULT(hr,"ERROR:  DirectSoundBuffer failed to GetStatus",
					TPR_ABORT,"Driver responded incorrectly");
			}
			QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&finish_count));			
		break;
		case CALLBACK_DIRECTSOUNDNOTIFY:
			hr=WaitForSingleObject(hEvent,dwExpectedCapturetime+1000); 
			QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&finish_count));			
		break;
	}
	LOG(TEXT("DirectSoundCapture just finished\n"));
    	duration=(finish_count-start_count)*1000/m_Resolution;
	expected=dwExpectedCapturetime;
	LOG(TEXT("Resolution:  %I64i Start:  %I64i Finish:  %I64i Duration:  %I64i Expected:  %I64i"),
		m_Resolution,start_count,finish_count, duration, expected);
    	expected=duration-expected;
	if(expected<0)	{
		QueryCondition((-expected>g_dwInAllowance),"ERROR:  expected capturetime is too short",
			TPR_ABORT,"Driver notifying the end of capture too early");
	}
	else {
		QueryCondition((expected>g_dwInAllowance),"ERROR:  expected capturetime is too long",
			TPR_ABORT,"Driver notifying the end of capture too late");
		QueryCondition((expected>=1000),"ERROR:  notification not recieved within one second after expected capturetime",TPR_ABORT,"Driver is not signalling notification at all");
	}
	if(g_interactive) {
		iResponse=MessageBox(NULL,TEXT("Click Yes to playback recording or No to re-record recording"),
			TEXT("Interactive Response"),MB_YESNO|MB_ICONQUESTION);
		switch(iResponse) {
		case IDNO:
			hr=pDSCB->Stop();
			QueryMMRESULT(hr,"ERROR:  Failed to stop (Capture).  Stop",
				TPR_ABORT,"Driver didn't stop device properly");
			goto StartRecording;
	    	}
    	}
	if(g_useSound) {
		Sleep(1000);
		hr=pDSB->GetStatus(&dwStatus);
		QueryCondition(dwStatus&DSBSTATUS_PLAYING,"ERROR:  Play buffer still DSBSTATUS_PLAYING (waited a whole extra second)",TPR_ABORT,"Driver didn't signal end of play");
		hr=pDSB->Stop();
		CheckMMRESULT(hr,"ERROR:  Failed to stop buffer (Sound).  Stop",
			TPR_ABORT,"Driver responded incorrectly");
		hr=pDSB->Lock(0,0,&lpvAudioPtr1,&dwAudioBytes1,&lpvAudioPtr2,&dwAudioBytes2,DSBLOCK_ENTIREBUFFER);
		CheckMMRESULT(hr,"ERROR:  Failed to lock (Sound).  Lock",
			TPR_ABORT,"Driver doesn't really support this format");
		hr=pDSCB->Lock(0,0,&lpvAudioPtr1c,&dwAudioBytes1c,&lpvAudioPtr2c,&dwAudioBytes2c,DSCBLOCK_ENTIREBUFFER);
		CheckMMRESULT(hr,"ERROR:  Failed to lock (Capture).  Lock",
			TPR_ABORT,"Driver doesn't really support this format");
		CopyMemory(lpvAudioPtr1,lpvAudioPtr1c,dwAudioBytes1c);
		hr=pDSCB->Unlock(lpvAudioPtr1c,dwAudioBytes1c,lpvAudioPtr2c,dwAudioBytes2c);
		CheckMMRESULT(hr,"ERROR:  Failed to Unlock (Capture).  Unlock",
			TPR_ABORT,"Driver doesn't really support this format");
		hr=pDSB->Unlock(lpvAudioPtr1,dwAudioBytes1,lpvAudioPtr2,dwAudioBytes2);
		CheckMMRESULT(hr,"ERROR:  Failed to Unlock (Sound).  Unlock",
			TPR_ABORT,"Driver doesn't really support this format");	
	}
	else {
		hr=pDSCB->Lock(0,0,&lpvAudioPtr1c,&dwAudioBytes1c,&lpvAudioPtr2c,&dwAudioBytes2c,DSCBLOCK_ENTIREBUFFER);
		CheckMMRESULT(hr,"ERROR:  Failed to lock (Capture).  Lock",
			TPR_ABORT,"Driver doesn't really support this format");
		data=new char[dwAudioBytes1c];
		CopyMemory(data,lpvAudioPtr1c,dwAudioBytes1c);
		hr=pDSCB->Unlock(lpvAudioPtr1c,dwAudioBytes1c,lpvAudioPtr2c,dwAudioBytes2c);
		CheckMMRESULT(hr,"ERROR:  Failed to Unlock (Capture).  Unlock",
			TPR_ABORT,"Driver doesn't really support this format");
		if(pDSCB) {
			hr=pDSCB->Stop();
			QueryMMRESULT(hr,"ERROR:  DirectSoundCaptureBuffer failed to stop.  Stop", 
				TPR_ABORT,"Driver responded incorrectly");
			pDSCB->Release();
			pDSCB=NULL;
		}
		if(pDSC) {
			pDSC->Release();
			pDSC=NULL;
		}
		hr=g_pfnDSCreate(g_DS,&pDS,NULL);
		CheckMMRESULT(hr,"ERROR:  Failed to create.  DirectSoundCreate",TPR_ABORT,"Unable to get driver interface");
		hr=pDS->CreateSoundBuffer(&DSBDesc,&pDSB,NULL);
		CheckMMRESULT(hr,"ERROR:  Failed to create buffer.  CreateSoundBuffer",
			TPR_ABORT,"Driver doesn't really support this format");
		hr=pDSB->Lock(0,0,&lpvAudioPtr1,&dwAudioBytes1,&lpvAudioPtr2,&dwAudioBytes2,DSBLOCK_ENTIREBUFFER);
		CheckMMRESULT(hr,"ERROR:  Failed to lock (Sound).  Lock",
			TPR_ABORT,"Driver doesn't really support this format");
		CopyMemory(lpvAudioPtr1,data,dwAudioBytes1c);
		hr=pDSB->Unlock(lpvAudioPtr1,dwAudioBytes1,lpvAudioPtr2,dwAudioBytes2);
		CheckMMRESULT(hr,"ERROR:  Failed to Unlock (Sound).  Unlock",
			TPR_ABORT, "Driver doesn't really support this format");
	}
StartPlayback:
	LOG(TEXT("Playing back what has been recorded\n"));
	hr=pDSB->SetCurrentPosition(0);
	CheckMMRESULT(hr,"ERROR:  Failed to set position to zero.  SetCurrentPosition",TPR_ABORT,"Driver doesn't support this correctly");
	hr=pDSB->Play(0,0,0);
	CheckMMRESULT(hr,"ERROR:  Failed to play.  Play",TPR_ABORT,"Driver doesn't really support this format");
	Sleep((dwSeconds+1)*1000);
	if(g_interactive) {
		iResponse=MessageBox(NULL,TEXT("Click Yes to continue, click no to playback recording again"),TEXT("Interactive Response"),MB_YESNO|MB_ICONQUESTION);
		if(iResponse==IDNO) 	goto StartPlayback;
	}
Error:
	if(data) 
		delete data;
	if(pDSN) 
		pDSN->Release();
	if(pDSB) {
		hr=pDSB->Stop();
		QueryMMRESULT(hr,"ERROR:  DirectSoundBuffer failed to stop.  Stop", TPR_ABORT,"Driver responded incorrectly");
		pDSB->Release();
	}
	if(pDS) 
		pDS->Release();
	if(pDSCB) {
		hr=pDSCB->Stop();
		QueryMMRESULT(hr,"ERROR:  DirectSoundCaptureBuffer failed to stop.  Stop", TPR_ABORT,"Driver responded incorrectly");
		pDSCB->Release();
	}
	if(pDSC) 
		pDSC->Release();
	if(hEvent) 
		CloseHandle(hEvent);
	return tr;
}

TESTPROCAPI CaptureCapabilities(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE) {
	BEGINTESTPROC 
	HRESULT hr;
	DSCCAPS DSCCaps;
	DWORD tr=TPR_PASS;
	LPDIRECTSOUNDCAPTURE pDS;

 	if(g_bSkipIn) return TPR_SKIP;
	LOG(TEXT("INSTRUCTIONS:  This test case displays driver capabilities.\n"));
 	LOG(TEXT("INSTRUCTIONS:  Driver capabilies need to be confirmed manually\n"));
	LOG(TEXT("INSTRUCTIONS:  Please confirm that your driver is capable of performing the functionality that it reports\n"));
	ZeroMemory(&DSCCaps,sizeof(DSCCaps));
	DSCCaps.dwSize=sizeof(DSCCaps);
	hr=g_pfnDSCCreate(g_DSC,&pDS,NULL);
	CheckMMRESULT(hr,"ERROR:  DirectSoundCaptureDriver failed to open.  DirectSoundCaptureCreate",
		TPR_FAIL,"Driver failed to load");
	hr=pDS->GetCaps(&DSCCaps);
	CheckMMRESULT(hr,"ERROR:  GetCaps failed.  GetCaps", TPR_FAIL,"Driver responded incorrectly");
	hr=pDS->Release();
	CheckMMRESULT(hr,"ERROR:  Release failed.  Release", TPR_FAIL,"Driver responded incorrectly");
  	LogDirectSoundCaptureCaps(g_dwDeviceNum-1,&DSCCaps);
Error:
	return tr;
}


TESTPROCAPI Capture(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE) {
	BEGINTESTPROC 
	int iResponse;
	DSCCAPS DSCCaps;
	HRESULT hr,hrReturn;
	DWORD tr=TPR_PASS,itr,i;
	LPDIRECTSOUNDCAPTURE pDS;

 	if(g_bSkipIn) return TPR_SKIP;
	LOG(TEXT("INSTRUCTIONS:  This test case will attempt to record and playback a tone for all types of lpFormats\n"));
 	LOG(TEXT("INSTRUCTIONS:  Please listen to playback to ensure that a tone is played\n"));
	ZeroMemory(&DSCCaps,sizeof(DSCCaps));
	DSCCaps.dwSize=sizeof(DSCCaps);
	hr=g_pfnDSCCreate(g_DSC,&pDS,NULL);
	CheckMMRESULT(hr,"ERROR:  DirectSoundCaptureDriver failed to open.  DirectSoundCaptureCreate",
		TPR_FAIL,"Driver failed to load");
	hr=pDS->GetCaps(&DSCCaps);
	CheckMMRESULT(hr,"ERROR:  GetCaps failed.  GetCaps", TPR_FAIL,"Driver responded incorrectly");
	hr=pDS->Release();
	CheckMMRESULT(hr,"ERROR:  Release failed.  Release", TPR_FAIL,"Driver responded incorrectly");
	for(i=0;lpFormats[i].szName;i++) {
    		LOG(TEXT("Attempting to record with %s\n"), lpFormats[i].szName);
		itr=RecordWaveFormat(lpFormats[i].szName,g_duration,CALLBACK_NONE,&hrReturn);
		if((g_interactive)&&(itr==TPR_PASS)) {
			iResponse=MessageBox(NULL,TEXT("Did you hear the recorded sound? (cancel to retry)"),
				TEXT("Interactive Response"),MB_YESNOCANCEL|MB_ICONQUESTION);
			switch(iResponse) {
				case IDNO:
					LOG(TEXT("ERROR:  User said there was no tone produced for %s\n"),
						lpFormats[i].szName);
					tr|=TPR_FAIL;
				break;
				case IDCANCEL:
					i--;
			}
		} else tr|=itr;
		if(DSCCaps.dwFormats&lpSecondaryFormats[i%4].value) {
			if(itr!=TPR_PASS)
				LOG(TEXT("ERROR:  GetCaps reports %s is supported, but %s was returned\n"),
					lpFormats[i].szName,g_ErrorMap[hrReturn]);
		}
		else if(itr==TPR_PASS) {
			LOG(TEXT("ERROR:  GetCaps reports %s is unsupported, but DSERR_BADFORMAT was not returned\n"),
				lpFormats[i].szName);
			itr=TPR_FAIL;
		}
		else {
			itr=TPR_PASS;
		}
	}
Error:
	return tr;
}

TESTPROCAPI CaptureNotifications(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE) {
	BEGINTESTPROC 
	HRESULT hr;
	int iResponse;
	DSCCAPS DSCCaps;
	LPDIRECTSOUNDCAPTURE pDS;
	DWORD tr=TPR_PASS,itr,i,j;

 	if(g_bSkipIn) return TPR_SKIP;
	LOG(TEXT("INSTRUCTIONS:  This test case will attempt to record and playback a tone for all types of lpCallbacks\n"));
 	LOG(TEXT("INSTRUCTIONS:  Please listen to playback to ensure that a tone is played\n"));
	ZeroMemory(&DSCCaps,sizeof(DSCCaps));
	DSCCaps.dwSize=sizeof(DSCCaps);
	hr=g_pfnDSCCreate(g_DSC,&pDS,NULL);
	CheckMMRESULT(hr,"ERROR:  DirectSoundCaptureDriver failed to open.  DirectSoundCaptureCreate",
		TPR_FAIL,"Driver failed to load");
	hr=pDS->GetCaps(&DSCCaps);
	CheckMMRESULT(hr,"ERROR:  GetCaps failed.  GetCaps", TPR_FAIL,"Driver responded incorrectly");
	hr=pDS->Release();
	CheckMMRESULT(hr,"ERROR:  Release failed.  Release", TPR_FAIL,"Driver responded incorrectly");
	for(i=0;lpFormats[i].szName;i++) 
		if(DSCCaps.dwFormats&lpSecondaryFormats[i%4].value) break;
	if(!lpFormats[i].szName) {
	    	LOG(TEXT("ERROR:  There are no supported lpFormats\n"));
	    	return TPR_SKIP;
	}
	for(j=0;lpCallbacks[j].szName;j++) {
    		LOG(TEXT("Attempting to record with %s\n"),lpCallbacks[j].szName);
		itr=RecordWaveFormat(lpFormats[i].szName,g_duration,lpCallbacks[j].value,NULL);
		if((g_interactive)&&(itr==TPR_PASS)) {
			iResponse=MessageBox(NULL,TEXT("Did you hear the recorded sound? (cancel to retry)"),
				TEXT("Interactive Response"),MB_YESNOCANCEL|MB_ICONQUESTION);
			switch(iResponse) {
				case IDNO:
					LOG(TEXT("ERROR:  User said there was no tone produced for %s\n"),
						lpFormats[i].szName);
					tr|=TPR_FAIL;
				break;
				case IDCANCEL:
					j--;
			}
		}
		tr|=itr;
	}
Error:
	return tr;
}

