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
#include "ERRORMAP.H"

#define CheckRegHRESULT(_hr,_function,_string,_cause) \
	if(_hr!=ERROR_SUCCESS) { \
		LOG(TEXT("ERROR:  %s %s failed with HRESULT=%x %s [%s:%u]\n"),TEXT(#_function),_string,hr,TEXT(__FILE__),__LINE__); \
		LOG(TEXT("Possible Cause:  %s\n"),TEXT(#_cause)); \
		return _hr; \
	}
#define QueryRegHRESULT(_hr,_function,_string,_cause) \
	if(_hr!=ERROR_SUCCESS) { \
		LOG(TEXT("ERROR:  %s %s failed with HRESULT=%x %s [%s:%u]\n"),TEXT(#_function),_string,hr,TEXT(__FILE__),__LINE__); \
		LOG(TEXT("Possible Cause:  %s\n"),TEXT(#_cause)); \
	}

typedef WINMMAPI MMRESULT (WINAPI *MMgetFunction)(HWAVEOUT,LPDWORD);
typedef WINMMAPI MMRESULT (WINAPI *MMsetFunction)(HWAVEOUT,DWORD);
typedef struct extension {
	TCHAR *szName;
	MMgetFunction get;
	MMsetFunction set;
	DWORD value;
	DWORD flag;
	double fvalue;
	BOOL bContinue;
} extension;

DWORD g_dwDeviceNum=1;
DWORD g_bSkipIn=FALSE;
DWORD g_bSkipOut=FALSE;
DWORD g_useNOTIFY=TRUE;
DWORD g_useSound=TRUE;
DWORD g_interactive=FALSE;
DWORD g_duration=1;
DWORD g_dwSleepInterval=50;	//50 milliseconds
DWORD g_dwAllowance=200;	//200 milliseconds
DWORD g_dwInAllowance=200;	//200 milliseconds

BEGINMAP(DWORD,lpFormats)
ENTRY(WAVE_FORMAT_1M08)
ENTRY(WAVE_FORMAT_1M16)
ENTRY(WAVE_FORMAT_1S08)
ENTRY(WAVE_FORMAT_1S16)
ENTRY(WAVE_FORMAT_2M08)
ENTRY(WAVE_FORMAT_2M16)
ENTRY(WAVE_FORMAT_2S08)
ENTRY(WAVE_FORMAT_2S16)
ENTRY(WAVE_FORMAT_4M08)
ENTRY(WAVE_FORMAT_4M16)
ENTRY(WAVE_FORMAT_4S08)
ENTRY(WAVE_FORMAT_4S16)
ENDMAP

BEGINMAP(DWORD,lpCallbacks)
ENTRY(CALLBACK_NULL)
ENTRY(CALLBACK_EVENT)
ENTRY(CALLBACK_THREAD)
ENTRY(CALLBACK_FUNCTION)
ENTRY(CALLBACK_WINDOW)
ENDMAP

#define EXTENTRY(a,b,c,d,e) 	{{TEXT(#b),waveOutGet##b,waveOutSet##b,c,d,e,TRUE},TEXT(#a)},
#define EXTSTOPENTRY(a,b,c,d,e)	{{TEXT(#b),waveOutGet##b,waveOutSet##b,c,d,e,FALSE},TEXT(#a)},

BEGINMAP(extension,lpExtensions)
EXTENTRY	("Change Pitch to 50%",		Pitch,0x00008000,WAVECAPS_PITCH,0.5*100.0)
EXTENTRY	("Change Pitch to 100%",	Pitch,0x00010000,WAVECAPS_PITCH,1.0*100.0)
EXTENTRY	("Change Pitch to 200%",	Pitch,0x00020000,WAVECAPS_PITCH,2.0*100.0)
EXTENTRY	("Change Pitch to 250%",	Pitch,0x00028000,WAVECAPS_PITCH,2.5*100.0)
EXTSTOPENTRY	("Change Pitch to 500%",	Pitch,0x00050000,WAVECAPS_PITCH,5.0*100.0)
EXTENTRY	("Change PlaybackRate to 50%",	PlaybackRate,0x00008000,WAVECAPS_PLAYBACKRATE,0.5*100.0)
EXTENTRY	("Change PlaybackRate to 100%",	PlaybackRate,0x00010000,WAVECAPS_PLAYBACKRATE,1.0*100.0)
EXTENTRY	("Change PlaybackRate to 200%",	PlaybackRate,0x00020000,WAVECAPS_PLAYBACKRATE,2.0*100.0)
EXTENTRY	("Change PlaybackRate to 250%",	PlaybackRate,0x00028000,WAVECAPS_PLAYBACKRATE,2.5*100.0)
EXTSTOPENTRY	("Change PlaybackRate to 300%",	PlaybackRate,0x00030000,WAVECAPS_PLAYBACKRATE,3.0*100.0)
#ifdef MORE_VOLUME_TESTS
EXTENTRY	("Change Right Volume to 100%",	Volume,0xFFFF0000,WAVECAPS_VOLUME,(float)0xFFFF/0xFFFF*100.0)
EXTENTRY	("Change Right Volume to 94%",	Volume,0xEFFF0000,WAVECAPS_VOLUME,(float)0xEFFF/0xFFFF*100.0)
EXTENTRY	("Change Right Volume to 87%",	Volume,0xDFFF0000,WAVECAPS_VOLUME,(float)0xDFFF/0xFFFF*100.0)
EXTENTRY	("Change Right Volume to 81%",	Volume,0xCFFF0000,WAVECAPS_VOLUME,(float)0xCFFF/0xFFFF*100.0)
EXTENTRY	("Change Right Volume to 75%",	Volume,0xBFFF0000,WAVECAPS_VOLUME,(float)0xBFFF/0xFFFF*100.0)
EXTENTRY	("Change Right Volume to 69%",	Volume,0xAFFF0000,WAVECAPS_VOLUME,(float)0xAFFF/0xFFFF*100.0)
EXTENTRY	("Change Right Volume to 62%",	Volume,0x9FFF0000,WAVECAPS_VOLUME,(float)0x9FFF/0xFFFF*100.0)
EXTENTRY	("Change Right Volume to 56%",	Volume,0x8FFF0000,WAVECAPS_VOLUME,(float)0x8FFF/0xFFFF*100.0)
EXTENTRY	("Change Right Volume to 50%",	Volume,0x7FFF0000,WAVECAPS_VOLUME,(float)0x7FFF/0xFFFF*100.0)
EXTENTRY	("Change Right Volume to 44%",	Volume,0x6FFF0000,WAVECAPS_VOLUME,(float)0x6FFF/0xFFFF*100.0)
EXTENTRY	("Change Right Volume to 37%",	Volume,0x5FFF0000,WAVECAPS_VOLUME,(float)0x5FFF/0xFFFF*100.0)
EXTENTRY	("Change Right Volume to 31%",	Volume,0x4FFF0000,WAVECAPS_VOLUME,(float)0x4FFF/0xFFFF*100.0)
EXTENTRY	("Change Right Volume to 25%",	Volume,0x3FFF0000,WAVECAPS_VOLUME,(float)0x3FFF/0xFFFF*100.0)
EXTENTRY	("Change Right Volume to 19%",	Volume,0x2FFF0000,WAVECAPS_VOLUME,(float)0x2FFF/0xFFFF*100.0)
EXTENTRY	("Change Right Volume to 12%",	Volume,0x1FFF0000,WAVECAPS_VOLUME,(float)0x1FFF/0xFFFF*100.0)
EXTSTOPENTRY	("Change Right Volume to 6%",	Volume,0x0FFF0000,WAVECAPS_VOLUME,(float)0x0FFF/0xFFFF*100.0)
EXTENTRY	("Change Left Volume to 100%",	Volume,0x0000FFFF,WAVECAPS_VOLUME,(float)0xFFFF/0xFFFF*100.0)
EXTENTRY	("Change Left Volume to 94%",	Volume,0x0000EFFF,WAVECAPS_VOLUME,(float)0xEFFF/0xFFFF*100.0)
EXTENTRY	("Change Left Volume to 87%",	Volume,0x0000DFFF,WAVECAPS_VOLUME,(float)0xDFFF/0xFFFF*100.0)
EXTENTRY	("Change Left Volume to 81%",	Volume,0x0000CFFF,WAVECAPS_VOLUME,(float)0xCFFF/0xFFFF*100.0)
EXTENTRY	("Change Left Volume to 75%",	Volume,0x0000BFFF,WAVECAPS_VOLUME,(float)0xBFFF/0xFFFF*100.0)
EXTENTRY	("Change Left Volume to 69%",	Volume,0x0000AFFF,WAVECAPS_VOLUME,(float)0xAFFF/0xFFFF*100.0)
EXTENTRY	("Change Left Volume to 62%",	Volume,0x00009FFF,WAVECAPS_VOLUME,(float)0x9FFF/0xFFFF*100.0)
EXTENTRY	("Change Left Volume to 56%",	Volume,0x00008FFF,WAVECAPS_VOLUME,(float)0x8FFF/0xFFFF*100.0)
EXTENTRY	("Change Left Volume to 50%",	Volume,0x00007FFF,WAVECAPS_VOLUME,(float)0x7FFF/0xFFFF*100.0)
EXTENTRY	("Change Left Volume to 44%",	Volume,0x00006FFF,WAVECAPS_VOLUME,(float)0x6FFF/0xFFFF*100.0)
EXTENTRY	("Change Left Volume to 37%",	Volume,0x00005FFF,WAVECAPS_VOLUME,(float)0x5FFF/0xFFFF*100.0)
EXTENTRY	("Change Left Volume to 31%",	Volume,0x00004FFF,WAVECAPS_VOLUME,(float)0x4FFF/0xFFFF*100.0)
EXTENTRY	("Change Left Volume to 25%",	Volume,0x00003FFF,WAVECAPS_VOLUME,(float)0x3FFF/0xFFFF*100.0)
EXTENTRY	("Change Left Volume to 19%",	Volume,0x00002FFF,WAVECAPS_VOLUME,(float)0x2FFF/0xFFFF*100.0)
EXTENTRY	("Change Left Volume to 12%",	Volume,0x00001FFF,WAVECAPS_VOLUME,(float)0x1FFF/0xFFFF*100.0)
EXTSTOPENTRY	("Change Left Volume to 6%",	Volume,0x00000FFF,WAVECAPS_VOLUME,(float)0x0FFF/0xFFFF*100.0)
EXTSTOPENTRY	("Change Volume:  Normal",	Volume,0xFFFFFFFF,WAVECAPS_VOLUME,(float)0xFFFF/0xFFFF*100.0)	
#else
EXTENTRY	("Change Right Volume to 100%",	Volume,0xFFFF0000,WAVECAPS_VOLUME,(float)0xFFFF/0xFFFF*100.0)
EXTENTRY	("Change Right Volume to 87%",	Volume,0xDFFF0000,WAVECAPS_VOLUME,(float)0xDFFF/0xFFFF*100.0)
EXTENTRY	("Change Right Volume to 75%",	Volume,0xBFFF0000,WAVECAPS_VOLUME,(float)0xBFFF/0xFFFF*100.0)
EXTENTRY	("Change Right Volume to 62%",	Volume,0x9FFF0000,WAVECAPS_VOLUME,(float)0x9FFF/0xFFFF*100.0)
EXTENTRY	("Change Right Volume to 50%",	Volume,0x7FFF0000,WAVECAPS_VOLUME,(float)0x7FFF/0xFFFF*100.0)
EXTENTRY	("Change Right Volume to 37%",	Volume,0x5FFF0000,WAVECAPS_VOLUME,(float)0x5FFF/0xFFFF*100.0)
EXTENTRY	("Change Right Volume to 25%",	Volume,0x3FFF0000,WAVECAPS_VOLUME,(float)0x3FFF/0xFFFF*100.0)
EXTSTOPENTRY	("Change Right Volume to 12%",	Volume,0x1FFF0000,WAVECAPS_VOLUME,(float)0x1FFF/0xFFFF*100.0)
EXTENTRY	("Change Left Volume to 100%",	Volume,0x0000FFFF,WAVECAPS_VOLUME,(float)0xFFFF/0xFFFF*100.0)
EXTENTRY	("Change Left Volume to 87%",	Volume,0x0000DFFF,WAVECAPS_VOLUME,(float)0xDFFF/0xFFFF*100.0)
EXTENTRY	("Change Left Volume to 75%",	Volume,0x0000BFFF,WAVECAPS_VOLUME,(float)0xBFFF/0xFFFF*100.0)
EXTENTRY	("Change Left Volume to 62%",	Volume,0x00009FFF,WAVECAPS_VOLUME,(float)0x9FFF/0xFFFF*100.0)
EXTENTRY	("Change Left Volume to 50%",	Volume,0x00007FFF,WAVECAPS_VOLUME,(float)0x7FFF/0xFFFF*100.0)
EXTENTRY	("Change Left Volume to 37%",	Volume,0x00005FFF,WAVECAPS_VOLUME,(float)0x5FFF/0xFFFF*100.0)
EXTENTRY	("Change Left Volume to 25%",	Volume,0x00003FFF,WAVECAPS_VOLUME,(float)0x3FFF/0xFFFF*100.0)
EXTSTOPENTRY	("Change Left Volume to 12%",	Volume,0x00001FFF,WAVECAPS_VOLUME,(float)0x1FFF/0xFFFF*100.0)
EXTSTOPENTRY	("Change Volume:  Normal",	Volume,0xFFFFFFFF,WAVECAPS_VOLUME,(float)0xFFFF/0xFFFF*100.0)	
#endif
ENDMAP

TCHAR *extensionMessages[]= {
	TEXT("Did you hear the pitch change from lower to higher? (cancel to retry)"),
	TEXT("Did you hear the playback rate change from slower to faster? (cancel to retry)"),
	TEXT("Did you hear the left volume change from higher to lower? (cancel to retry)"),
	TEXT("Did you hear the right volume change from higher to lower? (cancel to retry)"),
	TEXT("Did you hear the volume return to its original level? (cancel to retry)"),
};

HRESULT ListActiveDrivers() {
	HRESULT hr;
	HKEY hActive, hDriver;
	DWORD dwType, dwSize, i;
	TCHAR szKeyName[MAX_PATH], szDriverName[MAX_PATH], szDriverKey[MAX_PATH];

	hr=RegOpenKeyEx(HKEY_LOCAL_MACHINE,REG_HKEY_ACTIVE,0,0,&hActive);
	CheckRegHRESULT(hr,RegOpenKeyEx,TEXT("[Active Driver Key]"),"Unknown");
	LOG(TEXT("Active Drivers List\n"));
	i=0;
	do {
		dwSize=MAX_PATH;
		hr=RegEnumKeyEx(hActive,i,szKeyName,&dwSize,NULL,NULL,NULL,NULL);
		if(hr==ERROR_NO_MORE_ITEMS) 
			break;
		CheckRegHRESULT(hr,RegEnumKeyEx,TEXT("[Active Driver Key]"),"Unknown");
		hr=RegOpenKeyEx(hActive,szKeyName,0,0,&hDriver);
		CheckRegHRESULT(hr,RegOpenKeyEx,szKeyName,"Unknown");
		dwSize=sizeof(szDriverName);
		hr=RegQueryValueEx(hDriver,TEXT("Name"),NULL,&dwType,(BYTE*)szDriverName,&dwSize);
		if(hr!=ERROR_SUCCESS) 
			_tcscpy(szDriverName,TEXT("UNKN:"));
		dwSize=sizeof(szDriverKey);
		hr=RegQueryValueEx(hDriver,TEXT("Key"),NULL,&dwType,(BYTE*)szDriverKey,&dwSize);
		if(hr!=ERROR_SUCCESS) 
			_tcscpy(szDriverKey,TEXT("<unknown path>"));
		LOG(TEXT("%s %s\n"),szDriverName,szDriverKey);
		hr=RegCloseKey(hDriver);
		QueryRegHRESULT(hr,RegCloseKey,szKeyName,"Unknown");
		i++;
	} while(1); 
	hr=RegCloseKey(hActive);
	QueryRegHRESULT(hr,RegCloseKey,TEXT("[Active Driver Key]"),"Unknown");
	return ERROR_SUCCESS;
}

HRESULT GetActiveDriverKey(TCHAR *szDriver,TCHAR *szKey,DWORD dwSize) {
	HRESULT hr;
	HKEY hActive, hDriver;
	DWORD dwType, dwSizeKeyName, i;
	TCHAR szKeyName[MAX_PATH], szDriverName[MAX_PATH];

	hr=RegOpenKeyEx(HKEY_LOCAL_MACHINE,REG_HKEY_ACTIVE,0,0,&hActive);
	CheckRegHRESULT(hr,RegOpenKeyEx,TEXT("[Active Driver Key]"),"Unknown");
	i=0;
	while(1) {
		dwSizeKeyName=MAX_PATH;
		hr=RegEnumKeyEx(hActive,i,szKeyName,&dwSizeKeyName,NULL,NULL,NULL,NULL);
		if(hr==ERROR_NO_MORE_ITEMS) 
			break;
		CheckRegHRESULT(hr,RegEnumKeyEx,TEXT("[Active Driver Key]"),"Unknown");
		hr=RegOpenKeyEx(hActive,szKeyName,0,0,&hDriver);
		CheckRegHRESULT(hr,RegOpenKeyEx,szKeyName,"Unknown");
		dwSizeKeyName=sizeof(szDriverName);
		hr=RegQueryValueEx(hDriver,TEXT("Name"),NULL,&dwType,(BYTE*)szDriverName,&dwSizeKeyName);
		if(hr==ERROR_SUCCESS) {
			if(_tcscmp(szDriver,szDriverName)==0) {
				_tcscpy(szKey,TEXT("<unknown path>"));
				RegQueryValueEx(hDriver,TEXT("Key"),NULL,&dwType,(BYTE*)szKey,&dwSize);
				hr=RegCloseKey(hDriver);
				QueryRegHRESULT(hr,RegCloseKey,szKeyName,"Unknown");
				hr=RegCloseKey(hActive);
				QueryRegHRESULT(hr,RegCloseKey,"[Active Driver Key]","Unknown");
				return ERROR_SUCCESS;
			}
		}
		hr=RegCloseKey(hDriver);
		QueryRegHRESULT(hr,RegCloseKey,szKeyName,"Unknown");
		i++;
	}
	hr=RegCloseKey(hActive);
	QueryRegHRESULT(hr,RegCloseKey,TEXT("[Active Driver Key]"),"Unknown");
	return ERROR_PATH_NOT_FOUND;	
}

void LogWaveOutDevCaps(UINT uDeviceID, WAVEOUTCAPS *pWOC) {
	DWORD i;
   
	LOG(TEXT("Capabilities for Waveform Audio Output Device:  %u\n"),uDeviceID);
	LOG(TEXT("	Product:  %s\n") , pWOC->szPname);
	LOG(TEXT("	Manufacture ID:  %u\n") , pWOC->wMid);
	LOG(TEXT("	Product ID:  %u\n") , pWOC->wPid);
	LOG(TEXT("	Driver Version:  %u") , pWOC->vDriverVersion);
	LOG(TEXT("	Channels:  %u\n") , pWOC->wChannels);
	LOG(TEXT("	Audio lpFormats:"));

	for(i=0;lpFormats[i].szName;i++) 
		LOG(TEXT("		%s %s") , lpFormats[i].szName , (pWOC->dwFormats&lpFormats[i].value?TEXT(" Supported"):TEXT("*****Not Supported*****")) );

	LOG(TEXT("	Extended functionality supported:"));

	if( pWOC->dwSupport & WAVECAPS_LRVOLUME )
		LOG(TEXT("		Supports separate left and right volume control"));

	if( pWOC->dwSupport & WAVECAPS_PITCH )
			LOG(TEXT("		Supports pitch control"));

	if( pWOC->dwSupport & WAVECAPS_PLAYBACKRATE )
		LOG(TEXT("		Supports playback rate control"));

	if( pWOC->dwSupport & WAVECAPS_SYNC )
		LOG(TEXT("		The driver is synchronous and will block while playing a buffer"));

	if( pWOC->dwSupport & WAVECAPS_VOLUME )
		LOG(TEXT("		Supports volume control"));

	if( pWOC->dwSupport & WAVECAPS_SAMPLEACCURATE )
		LOG(TEXT("		Returns sample-accurate position information"));
} 

ULONG SineWave(void* pBuffer,ULONG ulNumBytes,WAVEFORMATEX *pwfx) {
	double dPhase = 0.0;
	double dAmplitude = 1.0;
	char *pClear, *pClearEnd;
	double dFrequency = 440.0;
	const double TWOPI = 2* 3.1415926535897931;

	pClearEnd=(char*)pBuffer+ulNumBytes;
	pClear=pClearEnd-ulNumBytes%4;
	while(pClear<pClearEnd) {
		*pClear=0;
		pClear++;
	}

	int nsamples = ulNumBytes / pwfx->nBlockAlign;
	int i,m_t0=0; 
	int m_dSampleRate=pwfx->nSamplesPerSec;
	double dMultiplier, dOffset, dSample;

	if (pwfx->wBitsPerSample == 8) {
		unsigned char * pSamples = (unsigned char *) pBuffer;
		dMultiplier = 127.0;
		dOffset = 128.0;
		if (pwfx->nChannels == 1) {
			for (i = 0; i < nsamples; i ++ ) {
				double t = ((double) (i+m_t0)) / m_dSampleRate;
				dSample = dAmplitude * sin (t*dFrequency* TWOPI + dPhase);
				pSamples[i] = (unsigned char) (dSample * dMultiplier + dOffset);
			}
		}
		else {
			for (i = 0; i < nsamples; i ++) {
				double t = ((double) (i+m_t0)) / m_dSampleRate;
				dSample = dAmplitude * sin (t*dFrequency* TWOPI + dPhase);
	 			pSamples[2*i] = (unsigned char) (dSample * dMultiplier + dOffset);
				pSamples[2*i+1] = pSamples[2*i]; // replicate across both channels
			}
		}
	}
	else {
		short * pSamples = (short *) pBuffer;
		const double dMultiplier = 32767.0;
		const double dOffset = 0.0;
		if (pwfx->nChannels == 1) {
			for (i = 0; i < nsamples; i += 1) {
				double t = ((double) (i+m_t0)) / m_dSampleRate;
				dSample = dAmplitude * sin (t*dFrequency* TWOPI + dPhase);
				pSamples[i] = (short) (dSample * dMultiplier + dOffset);
		   		}
	   		}
	   		else {
		   		for (i = 0; i < nsamples; i ++) {
			   			double t = ((double) (i+m_t0)) / m_dSampleRate;
			   			dSample = dAmplitude * sin (t*dFrequency* TWOPI + dPhase);
			   			pSamples[2*i] = (short) (dSample * dMultiplier + dOffset);
			   			pSamples[2*i+1] = pSamples[2*i]; // replicate across both channels
		   		}
	   		}
   	}
   	m_t0 += i;
   	return ulNumBytes;
}

HANDLE hEvent=NULL;
DWORD state=WOM_CLOSE;
HWND hwnd=NULL;

void CALLBACK waveOutProcCallback(HWAVEOUT hwo2, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2) {
	switch(uMsg) {
	case WOM_CLOSE:
		SetEvent(hEvent);
		if(state==WOM_DONE) {
			state=WOM_CLOSE;
		}
		else LOG(TEXT("ERROR:  WOM_CLOSE received when not done"));
		break;
	case WOM_OPEN:
		SetEvent(hEvent);
		if(state==WOM_CLOSE) {
			state=WOM_OPEN;
		}
		else LOG(TEXT("ERROR:  WOM_OPEN received when not closed"));
		break;
	case WOM_DONE:
		SetEvent(hEvent);
		if(state==WOM_OPEN) {
			state=WOM_DONE;
		}
		else LOG(TEXT("ERROR:  WOM_DONE received when not opened"));
		break;
	default:
		LOG(TEXT("ERROR:  Unknown Message received (%u)"),uMsg);
	}
}

LRESULT CALLBACK windowProcCallback(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch(uMsg) {
	case MM_WOM_CLOSE:
		waveOutProcCallback((HWAVEOUT)wParam,WOM_CLOSE,(DWORD)hEvent,0,0);
		return 0;
	case MM_WOM_OPEN:
		waveOutProcCallback((HWAVEOUT)wParam,WOM_OPEN,(DWORD)hEvent,0,0);
		return 0;
	case MM_WOM_DONE:
		waveOutProcCallback((HWAVEOUT)wParam,WOM_DONE,(DWORD)hEvent,lParam,0);
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd,uMsg,wParam,lParam);
}

DWORD WINAPI WindowMessageThreadProc(LPVOID lpParameter) {
	WNDCLASS wndclass;
	MSG msg;

	if(lpParameter) {
		  wndclass.style		  =   CS_HREDRAW | CS_VREDRAW;
		  wndclass.lpfnWndProc	=   windowProcCallback;
		  wndclass.cbClsExtra	 =   0;
		  wndclass.cbWndExtra	 =   0;
		  wndclass.hInstance	  =   g_hInst;
		  wndclass.hIcon		  =   NULL;
		  wndclass.hCursor		=   NULL;
		  wndclass.hbrBackground  =   NULL;
		  wndclass.lpszMenuName   =   NULL;
		  wndclass.lpszClassName  =   TEXT("WaveTest");
	  
		  if(RegisterClass( &wndclass )==0) {
		  	LOG(TEXT("Register Class FAILED"));
		  	}
	
		  hwnd = CreateWindow(TEXT("WaveTest"),
			  TEXT("WaveTest"),
			  WS_DISABLED,
			  CW_USEDEFAULT, CW_USEDEFAULT, 
			  CW_USEDEFAULT, CW_USEDEFAULT,
			  NULL, NULL, 
			  g_hInst,
		  	  &g_hInst );
		  if(hwnd==NULL) 
		  	LOG(TEXT("CreateWindow Failed" ));
		  SetEvent(hEvent);
	}
	
	while(GetMessage(&msg,NULL,0,0)) {
		if(NULL==msg.hwnd) {
			windowProcCallback(msg.hwnd,msg.message,msg.wParam,msg.lParam);
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

TESTPROCAPI StringFormatToWaveFormatEx(WAVEFORMATEX *wfx,TCHAR*szWaveFormat) {
	HRESULT hr;
	DWORD tr=TPR_PASS;
	TCHAR channels,bits1,bits2;

	ZeroMemory(wfx,sizeof(*wfx));
	hr=_stscanf(szWaveFormat,TEXT("WAVE_FORMAT_%i%c%c%c"),&wfx->nSamplesPerSec,&channels,&bits1,&bits2);
	if(hr!=4) {
		LOG(TEXT("ERROR:  %s not recognized\n"),szWaveFormat);
		LOG(TEXT("Possible Cause:  Supplied format not in the form of WAVE_FORMAT%%i%%c%%c%%c\n"));
		tr=TPR_FAIL;
		goto Error;
	}
	wfx->wFormatTag=WAVE_FORMAT_PCM;
	switch(channels) {
		case 'M': case 'm':
			wfx->nChannels=1;
			break;
		default:
			wfx->nChannels=2;
	}
	switch(wfx->nSamplesPerSec) {
		case 1: case 2: case 4:
			wfx->nSamplesPerSec=11025*wfx->nSamplesPerSec;
	}
	wfx->wBitsPerSample=(bits1-TEXT('0'))*10+(bits2-TEXT('0'));
	wfx->nBlockAlign=wfx->nChannels*wfx->wBitsPerSample/8;
	wfx->nAvgBytesPerSec=wfx->nSamplesPerSec*wfx->nBlockAlign;
Error:
	return tr;
}

TESTPROCAPI PlayWaveFormat(TCHAR *szWaveFormat,DWORD dwSeconds,DWORD dwNotification,extension* ext, DWORD *hrReturn)
{
	HRESULT hr;
	WAVEHDR wh;
	char *data=NULL;
	WAVEFORMATEX wfx;
	HWAVEOUT hwo=NULL;
	HANDLE hThread=NULL;
	DWORD tr=TPR_PASS,dwCallback,dwStat;
	timer_type start_count,finish_count,duration,expected,m_Resolution;
	DWORD dwExpectedPlaytime,dwTime=0;

	state=WOM_CLOSE;
	QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(&m_Resolution));
	if(StringFormatToWaveFormatEx(&wfx,szWaveFormat)!=TPR_PASS) {
		tr|=TPR_FAIL;
		goto Error;
	}
	data=new char[dwSeconds*wfx.nAvgBytesPerSec];
	CheckCondition(!data,"ERROR:  New Failed for Data",TPR_ABORT,"Out of Memory");
	ZeroMemory(data,dwSeconds*wfx.nAvgBytesPerSec);
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
			dwCallback=(DWORD)waveOutProcCallback;
		break;
		case CALLBACK_THREAD:
			hThread=CreateThread(NULL,0,WindowMessageThreadProc,(void*)FALSE,0,&dwCallback);
			CheckCondition(hThread==NULL,"ERROR:  Unable to CreateThread",TPR_ABORT,"Refer to GetLastError for a Possible Cause");
		break;
		case CALLBACK_WINDOW:
			hThread=CreateThread(NULL,0,WindowMessageThreadProc,(void*)TRUE,0,NULL);
			CheckCondition(hThread==NULL,"ERROR:  Unable to CreateThread",TPR_ABORT,"Refer to GetLastError for a Possible Cause");
	 		hr=WaitForSingleObject(hEvent,10000); 
			CheckCondition(hr==WAIT_TIMEOUT,"ERROR:  Window didn't notify its creation within ten seconds",TPR_ABORT,"Unknown");
			CheckCondition(hr!=WAIT_OBJECT_0,"ERROR:  Unknown Error waiting for Window Creation to notify its creation",TPR_ABORT,"Unknown");
			dwCallback=(DWORD)hwnd;
			break;			
	}
	hr=waveOutOpen(&hwo,g_dwDeviceNum-1,&wfx,dwCallback,(DWORD)hEvent,dwNotification);
	if((hr!=MMSYSERR_NOERROR)&&(hrReturn)) {
		*hrReturn=hr;
		tr|=TPR_FAIL;
		goto Error;
	}
	CheckMMRESULT(hr,"ERROR:  waveOutOpen failed to open driver",TPR_ABORT,"Driver doesn't really support this format");
	switch(dwNotification) {
 		case CALLBACK_FUNCTION:
		case CALLBACK_THREAD:
		case CALLBACK_WINDOW:
 		hr=WaitForSingleObject(hEvent,1000); 
 		QueryCondition(hr==WAIT_TIMEOUT,"ERROR:  Function, Thread or Window didn't notify that it received the open message within one second",TPR_ABORT,"Open message wasn't sent to handler");
		QueryCondition(hr!=WAIT_OBJECT_0,"ERROR:  Unknown Error while waiting for driver to open",TPR_ABORT,"Unknown");
		break;
	}
	ZeroMemory(&wh,sizeof(WAVEHDR));
	wh.lpData=data;
	wh.dwBufferLength=dwSeconds*wfx.nAvgBytesPerSec;
	wh.dwLoops=1;
	wh.dwFlags=WHDR_BEGINLOOP|WHDR_ENDLOOP;
	dwExpectedPlaytime=wh.dwBufferLength*1000/wfx.nAvgBytesPerSec;
	if(ext) {
		hr=ext->get(hwo,&dwStat);
		if((hr!=MMSYSERR_NOERROR)&&(hrReturn)) {
			*hrReturn=hr;
			tr|=TPR_FAIL;
			goto Error;
		}
		hr=ext->set(hwo,ext->value);
		if((hr!=MMSYSERR_NOERROR)&&(hrReturn)) {
			*hrReturn=hr;
			tr|=TPR_FAIL;
			goto Error;
		}
		LOG(TEXT("DWORD changed from %08x to %08x %s set to %f%%."),dwStat,ext->value,ext->szName,ext->fvalue);
		if(ext->flag==WAVECAPS_PLAYBACKRATE) {
			dwExpectedPlaytime=(DWORD)(dwExpectedPlaytime/(ext->fvalue/100.0));
		}
	}
	SineWave(wh.lpData,wh.dwBufferLength,&wfx);
	hr=waveOutPrepareHeader(hwo,&wh,sizeof(WAVEHDR));
	CheckMMRESULT(hr,"ERROR:  Failed to prepare header.  waveOutPrepareHeader",TPR_ABORT,"Driver doesn't really support this format");
	QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&start_count));
	hr=waveOutWrite(hwo,&wh,sizeof(WAVEHDR));
	CheckMMRESULT(hr,"ERROR:  Failed to write.  waveOutWrite",TPR_ABORT,"Driver doesn't really support this format");
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
		QueryCondition((-expected>g_dwAllowance),"ERROR:  expected playtime is too short",TPR_ABORT,"Driver notifying the end of playback too early");
	}
	else {
		QueryCondition((expected>g_dwAllowance),"ERROR:  expected playtime is too long",TPR_ABORT,"Driver notifying the end of playback too late");
		QueryCondition((expected>=1000),"ERROR:  notification not recieved within one second after expected playtime",TPR_ABORT,"Driver is not signalling notification at all");
	}
Error:
	if(hwo) {
		hr=waveOutReset(hwo);
		QueryMMRESULT(hr,"ERROR:  Failed to reset.  waveOutReset",TPR_ABORT,"Driver didn't reset device properly");
		hr=waveOutUnprepareHeader(hwo,&wh,sizeof(WAVEHDR));
		QueryMMRESULT(hr,"ERROR:  Failed unprepare header.  waveOutPrepareHeader",TPR_ABORT,"Driver doesn't really support this format");
		hr=waveOutClose(hwo);
		QueryMMRESULT(hr,"ERROR:  Failed to close.  waveOutClose",TPR_ABORT,"Driver didn't close device properly");
		switch(dwNotification) {
		 	case CALLBACK_FUNCTION:
			case CALLBACK_THREAD:
			case CALLBACK_WINDOW:
	 				
	 		hr=WaitForSingleObject(hEvent,1000); 
	 		QueryCondition(hr==WAIT_TIMEOUT,"ERROR:  Function, Thread or Window didn't notify that it received the close message within one second",TPR_ABORT,"Close message wasn't sent to handler");
			QueryCondition(hr!=WAIT_OBJECT_0,"ERROR:  Unknown Error while waiting for driver to close",TPR_ABORT,"Unknown");
			break;
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
			case CALLBACK_THREAD:
			hr=WaitForSingleObject(hThread,1000);
		 	QueryCondition(hr==WAIT_TIMEOUT,"ERROR:  Thread didn't release within one second",TPR_ABORT,"Unknown");
			QueryCondition(hr!=WAIT_OBJECT_0,"ERROR:  Unknown Error while waiting for thread to release",TPR_ABORT,"Unknown");

		}
	}
	if(data) 
		delete data;
	if(hEvent) 
		CloseHandle(hEvent);
	if(hThread)
		CloseHandle(hThread);
	return tr;
}

BOOL FullDuplex() {
	MMRESULT mmRtn;
	HWAVEOUT hwo=NULL;
	HWAVEIN hwi=NULL;
	WAVEFORMATEX wfx;
	DWORD i;

	for(i=0;lpFormats[i].szName;i++) {
		StringFormatToWaveFormatEx(&wfx,lpFormats[i].szName);
		mmRtn=waveOutOpen(&hwo,g_dwDeviceNum-1,&wfx,CALLBACK_NULL,NULL,NULL);
		if(mmRtn==MMSYSERR_NOERROR) {
			mmRtn=waveInOpen(&hwi,g_dwDeviceNum-1,&wfx,CALLBACK_NULL,NULL,NULL);
			if(mmRtn==MMSYSERR_NOERROR) {
				if(hwo) waveOutClose(hwo);
				if(hwi) waveInClose(hwi);
				return TRUE;
			}
		}
		if(hwo) waveOutClose(hwo);
		if(hwi) waveInClose(hwi);
		hwo=NULL;
		hwi=NULL;
		}
	return FALSE;
}

TESTPROCAPI BVT(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE) {
	BEGINTESTPROC
	HRESULT hr=0;
	HANDLE hFile;
	UINT nOut,nIn;
	DWORD tr=TPR_PASS;
	WIN32_FIND_DATA findData;
	TCHAR szDeviceName[MAX_PATH],szKeyPath[MAX_PATH]=TEXT("");

	if(g_bSkipOut) return TPR_SKIP;
	LOG(TEXT("INSTRUCTIONS:  This test case searches the registry for active driver entries." ));
	LOG(TEXT("INSTRUCTIONS:  Please confirm that your driver is the one being listed." ));
	_stprintf(szDeviceName,TEXT("WAV%i:"),g_dwDeviceNum);
	ListActiveDrivers();
	nOut=waveOutGetNumDevs();
	nIn=waveInGetNumDevs();
#ifdef UNDER_CE
	LOG(TEXT("Verifying Waveform Audio Driver (WAM1:)"));
	hr=GetActiveDriverKey(TEXT("WAM1:"),szKeyPath,MAX_PATH);
	QueryCondition(FAILED(hr),"ERROR:  WAM1: was not found",TPR_FAIL,"Waveform Audio not included in image (BSP_NOAUDIO flag enabled)")
	else {
		LOG(TEXT("WAM1:  was found at %s"), szKeyPath );
		}   
	LOG(TEXT("Verifying Waveform Audio Driver (%s)"),szDeviceName);
	hr=GetActiveDriverKey(szDeviceName,szKeyPath,MAX_PATH);
	if(FAILED(hr)) {
		LOG(TEXT("ERROR:  %s was not found"),szDeviceName);
		tr|=TPR_FAIL,
		LOG(TEXT("Possible Cause:  Driver failed to load during startup"));
		LOG(TEXT("Possible Cause:  Registry entries missing or incomplete"));
	}
		else {
		LOG(TEXT("%s was found at %s"),szDeviceName,szKeyPath);
		}   
#endif
	if(nOut<g_dwDeviceNum) {
   		LOG(TEXT("WARNING:  waveOutGetNumDevs reported %u device(s)"),nOut);
   		LOG(TEXT("WARNING:  All output tests will be skipped"));
   		g_bSkipOut=TRUE;
   	}
   	if(nIn<g_dwDeviceNum){
   		LOG(TEXT("WARNING:  waveOutGetNumDevs reported %u device(s)"), nIn);
   		LOG(TEXT("WARNING:  All input tests will be skipped"));
   		g_bSkipIn=TRUE;
	}
	hFile=FindFirstFile(TEXT("\\windows\\*.wav"),&findData);
	if(hFile==INVALID_HANDLE_VALUE) {
			LOG(TEXT("WARNING:  There are no wave files in the Windows Directory"));
		LOG(TEXT("WARNING:  Please include wave files in your image to verify your driver"));
			LOG(TEXT("Possible Cause:  Wave files not included in image"));
		}
	else FindClose(hFile);
	if(tr&TPR_FAIL) g_bSkipOut=g_bSkipIn=TRUE;	
	if(!(g_bSkipOut&&g_bSkipIn)) {
		LOG(TEXT("Testing Driver's Ability to Handle Full-Duplex Operation"));
		if(g_useSound) {
			if(FullDuplex()) {
				LOG(TEXT("Success:  Able to open waveIn and waveOut at the same time"));
				LOG(TEXT("          Your driver claims Full-Duplex Operation"));
				LOG(TEXT("          Your commandline claims Full-Duplex Operation"));
				LOG(TEXT("Warning:  If you are unable to play and capture sound at the same time,"));
				LOG(TEXT("          your driver claims to have Full-Duplex Operation,"));
				LOG(TEXT("          but really doesn't support Full-Duplex Operation"));
				}
			else {
				LOG(TEXT("Failure:  Unable to open waveIn and waveOut at the same time"));
				LOG(TEXT("          Your driver claims Half-Duplex Operation"));
				LOG(TEXT("          Your commandline claims Full-Duplex Operation"));
				LOG(TEXT("          Turning off Full-Duplex Operation"));
				g_useSound=FALSE;
				tr=TPR_FAIL;
				}
			LOG(TEXT("		  Fix your driver to work in Full-Duplex,"));
			LOG(TEXT("		  or test your driver as a Half-Duplex driver"));
			LOG(TEXT("		  with commandline -c \"-e\" options."));
			}
			else {
			if(FullDuplex()) {
				LOG(TEXT("Failure:  Able to open waveIn and waveOut at the same time"));
				LOG(TEXT("          Your driver claims Full-Duplex Operation"));
				LOG(TEXT("          Your commandline claims Half-Duplex Operation (-c \"-e\")"));
				LOG(TEXT("          Fix your driver to work in Half-Duplex by making sure that" ));
				LOG(TEXT("          waveIn and waveOut cannot be opened at the same time"));
				LOG(TEXT("          or test your driver as a Full-Duplex driver"));
				LOG(TEXT("		  without commandline -c \"-e\" options."));
				tr=TPR_FAIL;
			}
			else {
				LOG(TEXT("Success:  Unable to open waveIn and waveOut at the same time"));
				LOG(TEXT("		  Your driver claims Half-Duplex Operation"));
				LOG(TEXT("		  Your commandline claims Half-Duplex Operation (-c \"-e\")"));
			}
			}
	}

	if(g_bSkipOut&&g_bSkipIn)
		tr=TPR_SKIP;
	return tr;
}

TESTPROCAPI EasyPlayback(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE) {
	BEGINTESTPROC
	BOOL result;
	HANDLE hFile;
	int iResponse;
	DWORD tr=TPR_PASS;
	WIN32_FIND_DATA findData;

	if(g_bSkipOut) return TPR_SKIP;
	if(g_dwDeviceNum!=1) {
		LOG(TEXT("WARNING:  Easy Playback only works for the default device" ));
		LOG(TEXT("WARNING:  There is no way of specifying which device to use with the sndPlaySound and PlaySound APIs" ));
		LOG(TEXT("WARNING:  Reconfigure your system so that your driver becomes the default driver to run this test case"));
		LOG(TEXT("WARNING:  This test case will be skipped" ));
		return TPR_SKIP;
		}

	LOG(TEXT("INSTRUCTIONS:  This test case plays wave files from the Windows Directory" ));
	LOG(TEXT("INSTRUCTIONS:  Please verify that you hear files being played by the sndPlaySound and PlaySound APIs" ));

RunsndPlaySound:
	LOG(TEXT("Playing all files in Windows Directory with sndPlaySound API" ));	
	hFile=FindFirstFile(TEXT("\\windows\\*.wav"),&findData);
	LOG(TEXT("Using sndPlaySound to play %s"),findData.cFileName );
	result=sndPlaySound(findData.cFileName,SND_NODEFAULT);
	if(!result) goto Error;
	while(FindNextFile(hFile,&findData)) {
   		LOG(TEXT("Using sndPlaySound to play %s"), findData.cFileName );
			result=sndPlaySound(findData.cFileName,SND_NODEFAULT);
		if(!result) goto Error;
	}
	FindClose(hFile);
	if(g_interactive) {
		iResponse=MessageBox(NULL,TEXT("Did you hear the files from the Windows Directory? (cancel to retry)"),TEXT("Interactive Response"),MB_YESNOCANCEL|MB_ICONQUESTION);
			switch(iResponse) {
				case IDNO:
				LOG(TEXT("ERROR:  User said sndPlaySound failed "));
				tr|=TPR_FAIL;
				break;
				case IDCANCEL:
				goto RunsndPlaySound;
			}
	}
RunPlaySound:
	LOG(TEXT("Playing all files in Windows Directory with PlaySound API" ));
	hFile=FindFirstFile(TEXT("\\windows\\*.wav"),&findData);
	LOG(TEXT("Using PlaySound to play %s"), findData.cFileName );
	result=PlaySound(findData.cFileName,NULL,SND_NODEFAULT);
	if(!result) goto Error;

	while(FindNextFile(hFile,&findData)) {
			LOG(TEXT("Using PlaySound to play %s"), findData.cFileName );
		result=PlaySound(findData.cFileName,NULL,SND_NODEFAULT);
   		if(!result) goto Error;

	}
	FindClose(hFile);
	if(g_interactive) {
		iResponse=MessageBox(NULL,TEXT("Did you hear the files from the Windows Directory? (cancel to retry)"),TEXT("Interactive Response"),MB_YESNOCANCEL|MB_ICONQUESTION);
			switch(iResponse) {
				case IDNO:
				LOG(TEXT("ERROR:  User said PlaySound failed "));
				tr|=TPR_FAIL;
				break;
				case IDCANCEL:
				goto RunPlaySound;
			}
	}
	return tr;
Error:
	LOG(TEXT("ERROR:  The function returned FALSE (it was unsuccessful)" ));
	LOG(TEXT("Possible Cause:  Not Enough Memory" ));
	LOG(TEXT("Possible Cause:  %s is not playable on the device and/or driver"),findData.cFileName);
	FindClose(hFile);
	return TPR_FAIL;  	
}

	
TESTPROCAPI PlaybackCapabilities(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE) {
	BEGINTESTPROC
	DWORD tr=TPR_PASS;
	MMRESULT mmRtn;
	WAVEOUTCAPS woc;

 	if(g_bSkipOut) return TPR_SKIP;
	LOG(TEXT("INSTRUCTIONS:  This test case displays Driver Capabilities." ));
 	LOG(TEXT("INSTRUCTIONS:  Driver Capabilies need to be confirmed manually" ));
	LOG(TEXT("INSTRUCTIONS:  Please confirm that your driver is capable of performing the functionality that it reports" ));
 	mmRtn=waveOutGetDevCaps(g_dwDeviceNum-1,&woc,sizeof(woc));
	CheckMMRESULT(mmRtn,"ERROR:  Failed to get device caps.  waveOutGetDevCaps",TPR_FAIL,"Driver responded incorrectly");
	LogWaveOutDevCaps(g_dwDeviceNum-1,&woc);
Error:
	return tr;
}

TESTPROCAPI Playback(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE) {
	BEGINTESTPROC
	DWORD tr=TPR_PASS;
	DWORD itr;
	DWORD hrReturn;
	DWORD i;
	WAVEOUTCAPS woc;
	MMRESULT mmRtn;
	int iResponse;

 	if(g_bSkipOut) return TPR_SKIP;
 	LOG(TEXT("INSTRUCTIONS:  This test case will attempt to playback a tone for all common lpFormats"));
 	LOG(TEXT("INSTRUCTIONS:  Please listen to playback to ensure that a tone is played" ));
 	mmRtn=waveOutGetDevCaps(g_dwDeviceNum-1,&woc,sizeof(woc));
	CheckMMRESULT(mmRtn,"ERROR:  Failed to get device caps.  waveOutGetDevCaps",TPR_FAIL,"Driver responded incorrectly");
	for(i=0;lpFormats[i].szName;i++) {
		hrReturn=MMSYSERR_NOERROR;
		LOG(TEXT("Attempting to playback %s"),lpFormats[i].szName );
		itr=PlayWaveFormat(lpFormats[i].szName,g_duration,CALLBACK_NULL,NULL,&hrReturn);
		if(woc.dwFormats&lpFormats[i].value) {
			if(itr==TPR_FAIL)
				LOG(TEXT("ERROR:  waveOutGetDevCaps reports %s is supported, but %s was returned"),lpFormats[i].szName,g_ErrorMap[hrReturn]);
		}
		else if(itr==TPR_PASS) {
			LOG(TEXT("ERROR:  waveOutGetDevCaps reports %s is unsupported, but WAVERR_BADFORMAT was not returned"),lpFormats[i].szName);
			itr=TPR_FAIL;
		}
		else {
			itr=TPR_PASS;
		}
		if(g_interactive) {
			iResponse=MessageBox(NULL,TEXT("Did you hear the tone? (cancel to retry)"),TEXT("Interactive Response"),MB_YESNOCANCEL|MB_ICONQUESTION);
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

TESTPROCAPI PlaybackNotifications(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE) {
	BEGINTESTPROC
	DWORD tr=TPR_PASS;
	DWORD itr;
	DWORD i,j;
	WAVEOUTCAPS woc;
	MMRESULT mmRtn;
	int iResponse;

 	if(g_bSkipOut) return TPR_SKIP;
	LOG(TEXT("INSTRUCTIONS:  This test case will attempt to record and playback a tone for all types of lpCallbacks"));
 	LOG(TEXT("INSTRUCTIONS:  Please listen to playback to ensure that a tone is played"));
 	mmRtn=waveOutGetDevCaps(g_dwDeviceNum-1,&woc,sizeof(woc));
	CheckMMRESULT(mmRtn,"ERROR:  Failed to get device caps.  waveOutGetDevCaps",TPR_FAIL,"Driver responded incorrectly");
	for(i=0;lpFormats[i].szName;i++)
		if(woc.dwFormats&lpFormats[i].value) break;
	if(!lpFormats[i].szName) {
			LOG(TEXT("ERROR:  There are no supported lpFormats"));
			return TPR_SKIP;
	}
	for(j=0;lpCallbacks[j].szName;j++) {
		LOG(TEXT("Attempting to playback with %s"),lpCallbacks[j].szName);
		itr=PlayWaveFormat(lpFormats[i].szName,g_duration,lpCallbacks[j].value,NULL,NULL);
		if(g_interactive) {
			iResponse=MessageBox(NULL,TEXT("Did you hear the tone? (cancel to retry)"),TEXT("Interactive Response"),MB_YESNOCANCEL|MB_ICONQUESTION);
			switch(iResponse) {
				case IDNO:
					LOG(TEXT("ERROR:  User said there was no tone produced for %s"),lpCallbacks[j].szName);
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

TESTPROCAPI PlaybackExtended(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE) {
	BEGINTESTPROC
	DWORD tr=TPR_PASS;
	DWORD itr;
	DWORD i,j,m;
	WAVEOUTCAPS woc;
	MMRESULT mmRtn;
	DWORD hrReturn;
	DWORD dwRestart;
	int iResponse;

 	if(g_bSkipOut) return TPR_SKIP;
	LOG(TEXT("INSTRUCTIONS:  This test case will attempt to playback a tone for using extended funcitons"));
  	LOG(TEXT("INSTRUCTIONS:  Please listen to playback to ensure that a tone is played and changed"));
 	LOG(TEXT("INSTRUCTIONS:  When Pitch is changed, the pitch should change accordingly"));
 	LOG(TEXT("INSTRUCTIONS:  When Playback rate is changed, the tone will change because the waveform is played at a different rate"));
 	LOG(TEXT("INSTRUCTIONS:	  Also, the expected play time of the buffer will also change"));
 	LOG(TEXT("INSTRUCTIONS:  When Volume is changed, the tone should decrease in volume on the left channel"));
 	LOG(TEXT("INSTRUCTIONS:	  Then it should decrease in volume on the right channel"));
 	mmRtn=waveOutGetDevCaps(g_dwDeviceNum-1,&woc,sizeof(woc));
	CheckMMRESULT(mmRtn,"ERROR:  Failed to get device caps.  waveOutGetDevCaps",TPR_FAIL,"Driver responded incorrectly");
	for(i=0;lpFormats[i].szName;i++) 
		if(woc.dwFormats&lpFormats[i].value) break;
	if(!lpFormats[i].szName) {
			LOG(TEXT("ERROR:  There are no supported lpFormats"));
		return TPR_SKIP;
	}
	for(j=0,m=0,dwRestart=0;lpExtensions[j].szName;j++) {
Restart:
		hrReturn=MMSYSERR_NOERROR;
		itr=PlayWaveFormat(lpFormats[i].szName,g_duration,CALLBACK_NULL,&lpExtensions[j].value,&hrReturn);
		if(!lpExtensions[j].value.bContinue) {
			if(itr==TPR_PASS) {
				if(g_interactive) {
					iResponse=MessageBox(NULL,extensionMessages[m],TEXT("Interactive Response"),MB_YESNOCANCEL|MB_ICONQUESTION);
					switch(iResponse) {
						case IDNO:
							LOG(TEXT("ERROR:  User said there was no difference in the tone %s"),lpExtensions[j].szName);
							tr|=TPR_FAIL;
							break;
							case IDCANCEL:
								j=dwRestart;
								goto Restart;
					}
				} else tr|=itr;
			}
			m++;
			dwRestart=j+1;
		}
		if(woc.dwSupport&lpExtensions[j].value.flag) {
			if(itr==TPR_FAIL)
				LOG(TEXT("ERROR:  waveOutGetDevCaps reports is supported, but %s was returned"),lpExtensions[j].szName,g_ErrorMap[hrReturn]);
		}
		else if(itr==TPR_PASS) {
			LOG(TEXT("ERROR:  waveOutGetDevCaps reports is unsupported, but MMSYSERR_UNSUPPORTED was not returned"),lpExtensions[j].szName);
			itr=TPR_FAIL;
		}
		else {
			itr=TPR_PASS;
		}
		tr|=itr;
	}
Error:
	return tr;
}
