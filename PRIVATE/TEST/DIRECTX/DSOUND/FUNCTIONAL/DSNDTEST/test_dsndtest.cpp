//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//
#include "TUXMAIN.H"
#include "TEST_DSNDTEST.H"
#include "ERRORMAP.H"
#include "MAP.H"

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

#define ext_def(_x_,_y_,_z_) \
	{TEXT(#_x_),_y_,_z_,TRUE},
#define ext_defstop(_x_,_y_,_z_) \
	{TEXT(#_x_),_y_,_z_,FALSE},

typedef struct DSINFO {
	DWORD dwDevices;	
	DWORD dwDeviceID;  
	LPGUID lpGuid;		
	LPCTSTR lpcstrDescription;
	LPCTSTR lpcstrModule;
} DSINFO;

typedef struct extension {
	TCHAR *szName;
	union {
		DWORD dwValue;
		LONG lValue;
	};
	DWORD flag;
	BOOL bContinue;
} extension;

DWORD	g_dwDeviceNum=0;
DWORD	g_bSkipIn=FALSE;
DWORD	g_bSkipOut=FALSE;
DWORD	g_useNOTIFY=TRUE;
DWORD	g_interactive=FALSE;
DWORD	g_duration=1;
DWORD	g_useSound=TRUE;
LPGUID	g_DS=NULL;
LPGUID	g_DSC=NULL;
DWORD	g_all=FALSE;
DWORD	g_dwSleepInterval=50;	//50 milliseconds
DWORD	g_dwAllowance=200;	//200 milliseconds
DWORD	g_dwInAllowance=200;	//200 milliseconds

HINSTANCE 			g_hInstDSound		= NULL;
PFNDSOUNDCREATE			g_pfnDSCreate   	= NULL;
PFNDSOUNDCAPTURECREATE 		g_pfnDSCCreate		= NULL;
PFNDSOUNDENUMERATE		g_pfnDSEnumerate	= NULL;
PFNDSOUNDCAPTUREENUMERATE	g_pfnDSCEnumerate	= NULL;

BEGINMAP(DWORD,lpDSCapFlags)
ENTRY(DSCAPS_CERTIFIED)
ENTRY(DSCAPS_CONTINUOUSRATE)
ENTRY(DSCAPS_EMULDRIVER)
ENTRY(DSCAPS_PRIMARY16BIT)
ENTRY(DSCAPS_PRIMARY8BIT)
ENTRY(DSCAPS_PRIMARYMONO)
ENTRY(DSCAPS_PRIMARYSTEREO)
ENTRY(DSCAPS_SECONDARY16BIT)
ENTRY(DSCAPS_SECONDARY8BIT)
ENTRY(DSCAPS_SECONDARYMONO)
ENTRY(DSCAPS_SECONDARYSTEREO)
ENDMAP

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

BEGINMAP(DWORD,lpSecondaryFormats)
ENTRY(DSCAPS_SECONDARYMONO|DSCAPS_SECONDARY8BIT)
ENTRY(DSCAPS_SECONDARYMONO|DSCAPS_SECONDARY16BIT)
ENTRY(DSCAPS_SECONDARYSTEREO|DSCAPS_SECONDARY8BIT)
ENTRY(DSCAPS_SECONDARYSTEREO|DSCAPS_SECONDARY16BIT)
ENDMAP

BEGINMAP(DWORD,lpPrimaryFormats)
ENTRY(DSCAPS_PRIMARYMONO|DSCAPS_PRIMARY8BIT)
ENTRY(DSCAPS_PRIMARYMONO|DSCAPS_PRIMARY16BIT)
ENTRY(DSCAPS_PRIMARYSTEREO|DSCAPS_PRIMARY8BIT)
ENTRY(DSCAPS_PRIMARYSTEREO|DSCAPS_PRIMARY16BIT)
ENDMAP

BEGINMAP(DWORD,lpCallbacks)
ENTRY(CALLBACK_NONE)
ENTRY(CALLBACK_DIRECTSOUNDNOTIFY)
ENDMAP

#define EXTENTRY(a,b,c,d)	{{TEXT(#b),c,d,TRUE},TEXT(#a)},
#define EXTSTOPENTRY(a,b,c,d)	{{TEXT(#b),c,d,FALSE},TEXT(#a)},

BEGINMAP(extension,lpExtensions)
EXTENTRY	("Change Frequency to 11025Hz",		Frequency,11025,DSBCAPS_CTRLFREQUENCY)
EXTENTRY	("Change Frequency to 22050Hz",		Frequency,22050,DSBCAPS_CTRLFREQUENCY)
EXTSTOPENTRY	("Change Frequency to 44100Hz",		Frequency,44100,DSBCAPS_CTRLFREQUENCY)
EXTSTOPENTRY	("Change Frequency to Original",	Frequency,DSBFREQUENCY_ORIGINAL,DSBCAPS_CTRLFREQUENCY)
EXTENTRY	("Change Pan:  No Attenuation",		Pan,DSBPAN_CENTER,DSBCAPS_CTRLPAN)
EXTENTRY	("Change Pan:  Attenuate Left  5dB",	Pan,500,DSBCAPS_CTRLPAN)
EXTENTRY	("Change Pan:  Attenuate Left 10dB",	Pan,1000,DSBCAPS_CTRLPAN)
EXTENTRY	("Change Pan:  Attenuate Left 15dB",	Pan,1500,DSBCAPS_CTRLPAN)
EXTSTOPENTRY	("Change Pan:  Attenuate Left 20dB",	Pan,2000,DSBCAPS_CTRLPAN)	
EXTENTRY	("Change Pan:  No Attenuation",		Pan,DSBPAN_CENTER,DSBCAPS_CTRLPAN)
EXTENTRY	("Change Pan:  Attenuate Right  5dB",	Pan,-500,DSBCAPS_CTRLPAN)
EXTENTRY	("Change Pan:  Attenuate Right 10dB",	Pan,-1000,DSBCAPS_CTRLPAN)
EXTENTRY	("Change Pan:  Attenuate Right 15dB",	Pan,-1500,DSBCAPS_CTRLPAN)
EXTSTOPENTRY	("Change Pan:  Attenuate Right 20dB",	Pan,-2000,DSBCAPS_CTRLPAN)
EXTENTRY	("Change Volume:  No Attenuation",	Volume,DSBVOLUME_MAX,DSBCAPS_CTRLVOLUME)
EXTENTRY	("Change Volume:  Attenuate Both  5dB",	Volume,-500,DSBCAPS_CTRLVOLUME)
EXTENTRY	("Change Volume:  Attenuate Both 10dB",	Volume,-1000,DSBCAPS_CTRLVOLUME)
EXTENTRY	("Change Volume:  Attenuate Both 15dB",	Volume,-1500,DSBCAPS_CTRLVOLUME)
EXTSTOPENTRY	("Change Volume:  Attenuate Both 20dB",	Volume,-2000,DSBCAPS_CTRLVOLUME)
ENDMAP

TCHAR *extensionMessages[]= {
	TEXT("Did you hear the frequency change from slow to fast? (cancel to retry)"),
	TEXT("Did you hear the frequency change back to normal? (cancel to retry)"),
	TEXT("Did you hear the left volume change from higher to lower (Pan Right)? (cancel to retry)"),
	TEXT("Did you hear the right volume change from higher to lower (Pan Left)? (cancel to retry)"),
	TEXT("Did you hear the volume change from higher to lower? (cancel to retry)"),
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

void LogDirectSoundCaps(UINT uDeviceID,DSCAPS *pWOC ) {
	DWORD i;

	LOG(TEXT("Capabilities for DirectSound Output Device:  %u\n"),	uDeviceID);
	LOG(TEXT("      dwMinSecondarySampleRate:  %u\n"), 		pWOC->dwMinSecondarySampleRate); 
	LOG(TEXT("      dwMaxSecondarySampleRate:  %u\n"), 		pWOC->dwMaxSecondarySampleRate); 
	LOG(TEXT("      dwPrimaryBuffers:  %u\n"), 			pWOC->dwPrimaryBuffers); 
	LOG(TEXT("      dwMaxHwMixingAllBuffers:  %u\n"),		pWOC->dwMaxHwMixingAllBuffers); 
	LOG(TEXT("      dwMaxHwMixingStaticBuffers:  %u\n"), 		pWOC->dwMaxHwMixingStaticBuffers); 
	LOG(TEXT("      dwMaxHwMixingStreamingBuffers:  %u\n"), 	pWOC->dwMaxHwMixingStreamingBuffers); 
	LOG(TEXT("      dwFreeHwMixingAllBuffers:  %u\n"), 		pWOC->dwFreeHwMixingAllBuffers); 
	LOG(TEXT("      dwFreeHwMixingStaticBuffers:  %u\n"), 		pWOC->dwFreeHwMixingStaticBuffers); 
	LOG(TEXT("      dwFreeHwMixingStreamingBuffers:  %u\n"), 	pWOC->dwFreeHwMixingStreamingBuffers); 
	LOG(TEXT("      dwMaxHw3DAllBuffers:  %u\n"), 			pWOC->dwMaxHw3DAllBuffers); 
	LOG(TEXT("      dwMaxHw3DStaticBuffers:  %u\n"), 		pWOC->dwMaxHw3DStaticBuffers); 
	LOG(TEXT("      dwMaxHw3DStreamingBuffers:  %u\n"), 		pWOC->dwMaxHw3DStreamingBuffers); 
	LOG(TEXT("      dwFreeHw3DAllBuffers:  %u\n"), 			pWOC->dwFreeHw3DAllBuffers); 
	LOG(TEXT("      dwFreeHw3DStaticBuffers:  %u\n"), 		pWOC->dwFreeHw3DStaticBuffers); 
	LOG(TEXT("      dwFreeHw3DStreamingBuffers:  %u\n"), 		pWOC->dwFreeHw3DStreamingBuffers); 
	LOG(TEXT("      dwTotalHwMemBytes:  %u\n"), 			pWOC->dwTotalHwMemBytes); 
	LOG(TEXT("      dwFreeHwMemBytes:  %u\n"), 			pWOC->dwFreeHwMemBytes); 
	LOG(TEXT("      dwMaxContigFreeHwMemBytes:  %u\n"), 		pWOC->dwMaxContigFreeHwMemBytes); 
	LOG(TEXT("      dwUnlockTransferRateHwBuffers:  %u\n"),		pWOC->dwUnlockTransferRateHwBuffers); 
	LOG(TEXT("      dwPlayCpuOverheadSwBuffers:  %u\n"), 		pWOC->dwPlayCpuOverheadSwBuffers); 
	LOG(TEXT("Extended Capabilities:  \n"));
	for(i=0;lpDSCapFlags[i].szName;i++) 
		LOG(TEXT("      %s %s\n"),
			lpDSCapFlags[i].szName,
			(pWOC->dwFlags&lpDSCapFlags[i].value?TEXT(" Supported"):TEXT("*****Not Supported*****")));
	
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

BOOL FullDuplex() {
	DWORD i;
	HRESULT hr;
	HWAVEIN hwi=NULL;
	WAVEFORMATEX wfx;
	HWAVEOUT hwo=NULL;
	DSBUFFERDESC DSBDesc;
	DSCBUFFERDESC DSCBDesc;
	LPDIRECTSOUND pDS=NULL;
	LPDIRECTSOUNDBUFFER pDSB=NULL;
	LPDIRECTSOUNDCAPTURE pDSC=NULL;
	LPDIRECTSOUNDCAPTUREBUFFER pDSCB=NULL;

	for(i=0;lpFormats[i].szName;i++) {
		if(StringFormatToWaveFormatEx(&wfx,lpFormats[i].szName)!=TPR_PASS) break;
		hr=g_pfnDSCCreate(g_DSC,&pDSC,NULL);
		if(hr==DS_OK) {
			Priority(pDS,0);
			ZeroMemory(&DSBDesc,sizeof(DSBDesc));
			DSBDesc.dwSize=sizeof(DSBDesc);
			DSBDesc.dwBufferBytes=wfx.nAvgBytesPerSec;	
			DSBDesc.lpwfxFormat=&wfx;
			ZeroMemory(&DSCBDesc,sizeof(DSCBDesc));
			DSCBDesc.dwSize=sizeof(DSCBDesc);
			DSCBDesc.dwBufferBytes=wfx.nAvgBytesPerSec;
			DSCBDesc.lpwfxFormat=&wfx;
			hr=pDSC->CreateCaptureBuffer((DSCBUFFERDESC*)&DSCBDesc,&pDSCB,NULL);
			if(hr==DS_OK) {	
				hr=g_pfnDSCreate(g_DS,&pDS,NULL);
				if(hr==DS_OK) {	
					hr=pDS->CreateSoundBuffer(&DSBDesc,&pDSB,NULL);
					if(hr==DS_OK) {
						if(pDSB) pDSB->Release();
						if(pDS) pDS->Release();
						if(pDSCB) pDSCB->Release();
						if(pDSC) pDSC->Release();
						return TRUE;
					}
				}
			}
		}
		if(pDSB) pDSB->Release();
		if(pDS) pDS->Release();
		if(pDSCB) pDSCB->Release();
		if(pDSC) pDSC->Release();
		pDSB=NULL;
		pDS=NULL;
		pDSCB=NULL;
		pDSC=NULL;
	}
	return FALSE;
}

DWORD BufferSize(TCHAR *szWaveFormat,DWORD dwSeconds) {
	WAVEFORMATEX wfx;
	
	wfx.nAvgBytesPerSec=0;	
	StringFormatToWaveFormatEx(&wfx,szWaveFormat);
	return wfx.nAvgBytesPerSec*dwSeconds;
}

TESTPROCAPI PlayWaveFormat(
	TCHAR *szWaveFormat,DWORD dwSeconds,DWORD dwNotification,DWORD dwType,extension* ext,HRESULT *hrReturn) {
	HRESULT hr;
	char *data=NULL;
	WAVEFORMATEX wfx;
	HANDLE hEvent=NULL;
	DSBUFFERDESC DSBDesc;
	LPDIRECTSOUND pDS=NULL;
	DSBPOSITIONNOTIFY DSBPN;       
	LPDIRECTSOUNDBUFFER pDSB=NULL;
	LPDIRECTSOUNDNOTIFY pDSN=NULL;
	LPVOID lpvAudioPtr1,lpvAudioPtr2;
	DWORD tr = TPR_PASS, dwStat,dwExpectedPlayTime,dwTime,dwAudioBytes1,dwAudioBytes2,dwStatus;
	timer_type start_count,finish_count,duration,expected,m_Resolution;


	QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(&m_Resolution));
	if(StringFormatToWaveFormatEx(&wfx,szWaveFormat)!=TPR_PASS) {
		tr|=TPR_FAIL;
		goto Error;
	}
	hr=g_pfnDSCreate(g_DS,&pDS,NULL);
	if((hr!=DS_OK)&&(hrReturn)) {
		*hrReturn=hr;
		tr|=TPR_FAIL;
		goto Error;
	}
	CheckMMRESULT(hr,"ERROR:  Failed to create.  DirectSoundCreate",TPR_ABORT,"Unable to get driver interface");
	Priority(pDS,dwType);
	ZeroMemory(&DSBDesc,sizeof(DSBDesc));
	DSBDesc.dwSize=sizeof(DSBDesc);
	if(dwNotification==CALLBACK_DIRECTSOUNDNOTIFY) 
		DSBDesc.dwFlags|=DSBCAPS_CTRLPOSITIONNOTIFY;
	if(ext)
		DSBDesc.dwFlags|=ext->flag;
	DSBDesc.dwFlags|=dwType;
	if(dwType!=DSBCAPS_PRIMARYBUFFER) {
		DSBDesc.dwBufferBytes=dwSeconds*wfx.nAvgBytesPerSec;
		DSBDesc.lpwfxFormat=&wfx;
		}
	hr=pDS->CreateSoundBuffer(&DSBDesc,&pDSB,NULL);
	if(hr!=DS_OK) {
		if (hrReturn)
			*hrReturn=hr;
		tr|=TPR_FAIL;
		goto Error;
	}
#ifndef UNDER_CE
	if(dwType==DSBCAPS_PRIMARYBUFFER) {
		hr=pDSB->SetFormat(&wfx);
		if(hr!=DS_OK) {
			if(hrReturn) 
				*hrReturn=hr;
			tr|=TPR_FAIL;
			goto Error;
		}
	}
#endif
	dwExpectedPlayTime=DSBDesc.dwBufferBytes*1000/wfx.nAvgBytesPerSec;
	if(ext) {
		switch(ext->flag) {
			case DSBCAPS_CTRLFREQUENCY:
				hr=pDSB->GetFrequency(&dwStat);
				break;
			case DSBCAPS_CTRLPAN:
				hr=pDSB->GetPan((long*)&dwStat);
				break;
			case DSBCAPS_CTRLVOLUME:
				hr=pDSB->GetVolume((long*)&dwStat);
		}
		if(hr!=DS_OK) {
			if(hrReturn)
				*hrReturn=hr;
			tr|=TPR_FAIL;
			goto Error;
		}
		switch(ext->flag) {
			case DSBCAPS_CTRLFREQUENCY:
				hr=pDSB->SetFrequency(ext->dwValue);
				if(ext->dwValue!=DSBFREQUENCY_ORIGINAL)
					dwExpectedPlayTime=(DWORD)(dwExpectedPlayTime*wfx.nSamplesPerSec/ext->dwValue);
				break;
			case DSBCAPS_CTRLPAN:
				hr=pDSB->SetPan(ext->lValue);
				break;
			case DSBCAPS_CTRLVOLUME:
				hr=pDSB->SetVolume(ext->lValue);
		}
		if(hr!=DS_OK) {
			if(hrReturn)
				*hrReturn=hr;
			tr|=TPR_FAIL;
			goto Error;
		}
		if(ext->flag==DSBCAPS_CTRLFREQUENCY)
			LOG(TEXT("DWORD changed from %08x to %08x.  %s set to %u\n"),
				dwStat,ext->dwValue,ext->szName,ext->dwValue);
		else
			LOG(TEXT("DWORD changed from %08x to %08x.  %s set to %li\n"),
				dwStat,ext->dwValue,ext->szName,ext->lValue);
	}
	hr=pDSB->Lock(0,0,&lpvAudioPtr1,&dwAudioBytes1,&lpvAudioPtr2,&dwAudioBytes2,DSBLOCK_ENTIREBUFFER);
	CheckMMRESULT(hr,"ERROR:  Failed to Lock (Sound).  Lock",TPR_ABORT,"Driver doesn't really support this format");
	SineWave(lpvAudioPtr1,dwAudioBytes1,&wfx);
	hr=pDSB->Unlock(lpvAudioPtr1,dwAudioBytes1,lpvAudioPtr2,dwAudioBytes2);
	CheckMMRESULT(hr,"ERROR:  Failed to Unlock (Sound).  Unlock",TPR_ABORT,"Driver doesn't really support this format");
	if(dwNotification==CALLBACK_DIRECTSOUNDNOTIFY) {
		hEvent=CreateEvent(NULL,FALSE,FALSE,NULL);
		CheckCondition(hEvent==NULL,"ERROR:  Unable to CreateEvent",
			TPR_ABORT,"Refer to GetLastError for a Possible Cause");
		DSBPN.dwOffset=DSBPN_OFFSETSTOP;
		DSBPN.hEventNotify=hEvent;
		hr=pDSB->QueryInterface(IID_IDirectSoundNotify,(LPVOID*)&pDSN);
		CheckMMRESULT(hr,"ERROR:  Failed to create DirectSoundNotify.  QueryInterface",
			TPR_ABORT,"Driver doesn't really support notifications for this format");
		hr=pDSN->SetNotificationPositions(1,&DSBPN);
		CheckMMRESULT(hr,"ERROR:  Failed to set notifications.  SetNotificationPositions",
			TPR_ABORT,"Driver doesn't really support this format");
	}
	QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&start_count));
#ifdef UNDER_CE
	hr=pDSB->Play(0,0,0);
	CheckMMRESULT(hr,"ERROR:  Failed to play.  Play",TPR_ABORT,"Driver doesn't really support this format");
#else
	if(dwType==DSBCAPS_PRIMARYBUFFER)
		hr=pDSB->Play(0,0,DSBPLAY_LOOPING);
	else
		hr=pDSB->Play(0,0,0);
	CheckMMRESULT(hr,"ERROR:  Failed to play.  Play",TPR_ABORT,"Driver doesn't really support this format");
#endif	
	if(dwType==DSBCAPS_PRIMARYBUFFER) {
		dwExpectedPlayTime=g_duration*1000;
		LOG(TEXT("Expected Playtime is %i ms\n"),dwExpectedPlayTime);
		Sleep(dwExpectedPlayTime);
		goto Error;
	}
	else LOG(TEXT("Expected Playtime is %i ms\n"),dwExpectedPlayTime);
	switch(dwNotification) {
		case CALLBACK_NONE:
			dwTime=0;
			hr=pDSB->GetStatus(&dwStatus);
			CheckMMRESULT(hr,"ERROR:  DirectSoundBuffer failed to GetStatus",
				TPR_ABORT,"Driver responded incorrectly");
			while((dwStatus&DSBSTATUS_PLAYING)&&(dwTime<dwExpectedPlayTime+5000)) {
				Sleep(g_dwSleepInterval);
				dwTime+=g_dwSleepInterval;
				hr=pDSB->GetStatus(&dwStatus);
				CheckMMRESULT(hr,"ERROR:  DirectSoundBuffer failed to GetStatus",
					TPR_ABORT,"Driver responded incorrectly");
			}
			QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&finish_count));			
		break;
		case CALLBACK_DIRECTSOUNDNOTIFY:
			hr=WaitForSingleObject(hEvent,dwExpectedPlayTime+1000); 
			QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&finish_count));			
		break;
	}
    	duration=(finish_count-start_count)*1000/m_Resolution;
	expected=dwExpectedPlayTime;
	LOG(TEXT("Resolution:  %I64i Start:  %I64i Finish:  %I64i  Duration:  %I64i Expected:  %I64i\n"),
		m_Resolution,start_count,finish_count,duration,expected);
    	expected=duration-expected;
	if(expected<0)	{
		QueryCondition((-expected>g_dwAllowance),
			"ERROR:  expected playtime is too short",TPR_ABORT,"Driver notifying the end of playback too early");
	}
	else {
		QueryCondition((expected>g_dwAllowance),
			"ERROR:  expected playtime is too long",TPR_ABORT,"Driver notifying the end of playback too late");
		QueryCondition((expected>=1000),
			"ERROR:  notification not recieved within one second after expected playtime",
			TPR_ABORT,"Driver is not signalling notification at all");
	}
Error:
	if(pDSN) 
		hr=pDSN->Release();
	if(pDSB) {
		hr=pDSB->Stop();
		QueryMMRESULT(hr,"ERROR:  DirectSoundBuffer failed to stop.  Stop",TPR_ABORT,"Driver responded incorrectly");
		hr=pDSB->Release();
	}
	if(pDS) 
		hr=pDS->Release();
	if(hEvent) 
		CloseHandle(hEvent);
	return tr;
}

BOOL CALLBACK DirectSoundCallback(LPGUID lpGuid, LPCTSTR lpcstrDescription, LPCTSTR lpcstrModule, LPVOID lpContext) {
	DSINFO *info=(DSINFO*)lpContext;
	
	LOG(TEXT("%x:%s(%s%)\n"),lpGuid,lpcstrDescription,lpcstrModule);
	info->dwDevices++;
	if(info->dwDevices==info->dwDeviceID) {
		info->lpGuid=lpGuid;
		info->lpcstrDescription=lpcstrDescription;
		info->lpcstrModule=lpcstrModule;
	}
	return TRUE;
}

BOOL InitComponent(void) {
	// LoadLibrary dsound.dll, to avoid staticly linking to it
	if (NULL == (g_hInstDSound = LoadLibrary(TEXT("dsound.dll")))) {
		LOG(TEXT("ERROR:  dsound.dll did not load\n"));		
		LOG(TEXT("Possible Cause:  DirectSound not included in image"));
        	return FALSE;
	}
	
	// Get Procedure Entry Points
	g_pfnDSCreate = (PFNDSOUNDCREATE)GetProcAddress(g_hInstDSound, TEXT("DirectSoundCreate"));
	if (NULL == g_pfnDSCreate) {
		LOG(TEXT("DirectSoundCreate entry point not found\n"));
		LOG(TEXT("Possible Cause:  DirectSound not built correctly"));
        	return FALSE;
	}
	g_pfnDSCCreate = (PFNDSOUNDCAPTURECREATE)GetProcAddress(g_hInstDSound, TEXT("DirectSoundCaptureCreate"));
	if (NULL == g_pfnDSCCreate) {
		LOG(TEXT("DirectSoundCaptureCreate entry point not found\n"));
		LOG(TEXT("Possible Cause:  DirectSound not built correctly"));
        	return FALSE;
	}
	g_pfnDSEnumerate = (PFNDSOUNDENUMERATE)GetProcAddress(g_hInstDSound, TEXT("DirectSoundEnumerateW"));
	if (NULL == g_pfnDSEnumerate) {
		LOG(TEXT("DirectSoundEnumerateW entry point not found\n"));
		LOG(TEXT("Possible Cause:  DirectSound not built correctly"));
        	return FALSE;
	}
	g_pfnDSCEnumerate = (PFNDSOUNDCAPTUREENUMERATE)GetProcAddress(g_hInstDSound, TEXT("DirectSoundCaptureEnumerateW"));
	if (NULL == g_pfnDSCEnumerate) {
		LOG(TEXT("DirectSoundCaptureEnumerateW entry point not found\n"));
		LOG(TEXT("Possible Cause:  DirectSound not built correctly"));
        	return FALSE;
	}

	return TRUE;
}

TESTPROCAPI BVT(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE) {
	BEGINTESTPROC 
	HRESULT hr;
	HANDLE hFile;
	UINT nOut,nIn;
	DWORD tr=TPR_PASS;
	DSINFO inInfo,outInfo;
	WIN32_FIND_DATA findData;
	TCHAR szDeviceName[MAX_PATH], szKeyPath[MAX_PATH]=TEXT("");

	if(!InitComponent()) {
		g_bSkipOut=TRUE;
		g_bSkipIn=TRUE;
		return TPR_SKIP;
	}
	LOG(TEXT("INSTRUCTIONS:  This test case searches the registry for active driver entries.\n"));
	LOG(TEXT("INSTRUCTIONS:  Please confirm that your driver is the one being listed.\n"));
	_stprintf(szDeviceName,TEXT("WAV%i:"),g_dwDeviceNum);
	ListActiveDrivers();
	ZeroMemory(&inInfo,sizeof(inInfo));
	ZeroMemory(&outInfo,sizeof(outInfo));
	inInfo.dwDeviceID=outInfo.dwDeviceID=g_dwDeviceNum;
 	LOG(TEXT("Active DirectSound GUID List\n"));
 	hr=g_pfnDSEnumerate(DirectSoundCallback,&outInfo);
 	QueryMMRESULT(hr,"ERROR:  DirectSoundEnumerate failed.  DirectSoundEnumerate",
		TPR_FAIL,"Driver responded incorrectly")
 	else {
 		LOG(TEXT("DirectSound GUID will be %x\n"),outInfo.lpGuid);
 		g_DS=outInfo.lpGuid;
 	}
 	LOG(TEXT("Active DirectSoundCapture GUID List\n"));
 	hr=g_pfnDSCEnumerate(DirectSoundCallback,&inInfo);
 	QueryMMRESULT(hr,"ERROR:  DirectSoundCaptureEnumerate failed.  DirectSoundCaptureEnumerate",
		TPR_FAIL,"Driver responded incorrectly")
 	else {
 		LOG(TEXT("DirectSoundCapture GUID will be %x4n"),inInfo.lpGuid);
 		g_DSC=inInfo.lpGuid;
 	}
	nOut=outInfo.dwDevices;
	nIn=inInfo.dwDevices;
#ifdef UNDER_CE
	LOG(TEXT("Verifying DirectSound Driver (DSS0:)\n" ));
	hr=GetActiveDriverKey(TEXT("DSS0:"),szKeyPath,MAX_PATH);
	QueryCondition(FAILED(hr),"ERROR:  DSS0: was not found",
		TPR_FAIL,"DirectSound not included in image (BSP_DSOUND flag not enabled)")
	else {
	    LOG(TEXT("DSS0:  was found at %s\n"),szKeyPath);
	}   
	LOG(TEXT("Verifying Audio Driver (%s)\n"),szDeviceName);
	hr=GetActiveDriverKey(szDeviceName,szKeyPath,MAX_PATH);
	if(FAILED(hr)) {
		LOG(TEXT("ERROR:  %s was not found\n"),szDeviceName);
		tr=TPR_FAIL;
		LOG(TEXT("Possible Cause:  Driver not included in image (BSP_driver flag missing)\n"));
		LOG(TEXT("Possible Cause:  Driver failed to load during startup\n"));
		LOG(TEXT("Possible Cause:  Registry entries missing or incomplete\n"));
	}
	else {
		LOG(TEXT("%s was found at %s\n"),szDeviceName,szKeyPath);
	}   
#endif
	if((nOut<g_dwDeviceNum)||(nOut==0)) {
   		LOG(TEXT("WARNING:  DirectSoundEnumerate reported %i device(s)\n"),nOut);
   		LOG(TEXT("WARNING:  All output tests will be skipped\n"));
   		g_bSkipOut=TRUE;
   	
   	}
   	if((nIn<g_dwDeviceNum)||(nIn==0)) {
   		LOG(TEXT("WARNING:  DirectSoundCaptureEnumerate reported %i device(s)\n"),nIn);
   		LOG(TEXT("WARNING:  All input tests will be skipped\n"));
   		g_bSkipIn=TRUE;
   	}
    	hFile=FindFirstFile(TEXT("\\windows\\*.wav"),&findData);
    	if(hFile==INVALID_HANDLE_VALUE) {
    		LOG(TEXT("WARNING:  There are no wave files in the Windows Directory\n"));
		LOG(TEXT("WARNING:  Please include dsnd files in your image to verify your driver\n"));
    		LOG(TEXT("Possible Cause:  wave files not included in image\n"));
    	}
   	else FindClose(hFile);
    	if(tr&TPR_FAIL) g_bSkipOut=g_bSkipIn=TRUE;    
    	if(!(g_bSkipOut&&g_bSkipIn)) {
	    	LOG(TEXT("Testing Driver's Ability to Handle Full-Duplex Operation\n"));
	    	if(g_useSound) {
		    	if(FullDuplex()) {
				LOG(TEXT("Success:  Able to open Sound and Capture Buffers at the same time"));
				LOG(TEXT("          Your driver claims Full-Duplex Operation" ));
				LOG(TEXT("          Your commandline claims Full-Duplex Operation" ));
				LOG(TEXT("Warning:  If you are unable to play and capture sound at the same time,"));
				LOG(TEXT("          your driver claims to have Full-Duplex Operation,"));
				LOG(TEXT("          but really doesn't support Full-Duplex Operation"));
			} 
			else {
				LOG(TEXT("Failure:  Unable to open Sound and Capture Buffers at the same time"));
				LOG(TEXT("          Your driver claims Half-Duplex Operation"));
				LOG(TEXT("          Your commandline claims Full-Duplex Operation"));
				LOG(TEXT("          Turning off Full-Duplex Operation"));
				g_useSound=FALSE;
				tr=TPR_FAIL;
		    	}
			LOG(TEXT("          Fix your driver to work in Full-Duplex,"));
			LOG(TEXT("          or test your driver as a Half-Duplex driver"));
			LOG(TEXT("          with commandline -c \"-e\" options."));
	    	}
	    	else {
			if(FullDuplex()) {
				LOG(TEXT("Failure:  Able to open Sound and Capture Buffers at the same time"));
				LOG(TEXT("          Your driver claims Full-Duplex Operation" ));
				LOG(TEXT("          Your commandline claims Half-Duplex Operation (-c \"-e\")" ));
				LOG(TEXT("          Fix your driver to work in Half-Duplex by making sure that"));
				LOG(TEXT("          Sound and Capture Buffers cannot be opened at the same time"));
				LOG(TEXT("          or test your driver as a Full-Duplex driver"));
				LOG(TEXT("          without commandline -c \"-e\" options."));
				tr=TPR_FAIL;
			} else {
				LOG(TEXT("Success:  Unable to open Sound and Capture Buffers at the same time"));
				LOG(TEXT("          Your driver claims Half-Duplex Operation" ));
				LOG(TEXT("          Your commandline claims Half-Duplex Operation (-c \"-e\")" ));
			}
	    	}
	}

	if(g_bSkipOut&&g_bSkipIn)
		tr=TPR_SKIP;
	return tr;
}


TESTPROCAPI PlaybackCapabilities(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE) {
	BEGINTESTPROC 
	HRESULT hr;
	DSCAPS DSCaps;
	DWORD tr=TPR_PASS;
	LPDIRECTSOUND pDS;

 	if(g_bSkipOut) return TPR_SKIP;
	LOG(TEXT("INSTRUCTIONS:  This test case displays Driver Capabilities."));
 	LOG(TEXT("INSTRUCTIONS:  Driver Capabilies need to be confirmed manually"));
	LOG(TEXT("INSTRUCTIONS:  Please confirm that your driver is capable of performing the functionality that it reports"));
	ZeroMemory(&DSCaps,sizeof(DSCaps));
	DSCaps.dwSize=sizeof(DSCaps);
	hr=g_pfnDSCreate(g_DS,&pDS,NULL);
	CheckMMRESULT(hr,"ERROR:  Failed to create.  DirectSoundCreate",TPR_ABORT,"Unable to get driver interface");
	hr=pDS->GetCaps(&DSCaps);
	CheckMMRESULT(hr,"ERROR:  GetCaps failed.  GetCaps", TPR_FAIL,"Driver responded incorrectly");
	hr=pDS->Release();
	CheckMMRESULT(hr,"ERROR:  Release failed.  Release", TPR_FAIL,"Driver responded incorrectly");
  	LogDirectSoundCaps(g_dwDeviceNum,&DSCaps);
Error:
	return tr;
}

BOOL ClaimsSupport(DWORD dwCaps,DSCAPS DSCaps,DWORD i,DWORD dwSeconds,BOOL hrReturn=FALSE) {
	if(DSBCAPS_LOCHARDWARE&dwCaps) {
		if(!((DSCaps.dwFlags&lpSecondaryFormats[i%4].value)==lpSecondaryFormats[i%4].value)) {
			if(hrReturn) 
				LOG(TEXT("GetCaps flags don't have %s\n"),lpSecondaryFormats[i%4].szName);
			return FALSE;
		}
		if(DSBCAPS_STATIC&dwCaps) 
			if(DSCaps.dwFreeHwMixingStaticBuffers==0) {
				if(hrReturn) 
					LOG(TEXT("There are not enough free static buffers (dwFreeHwMixingStaticBuffers=%i)\n"),DSCaps.dwFreeHwMixingStaticBuffers);
				return FALSE;
			}
		else 
			if(DSCaps.dwFreeHwMixingStreamingBuffers==0) {
				if(hrReturn) 
					LOG(TEXT("There are not enough free streaming buffers (dwFreeHwMixingStreamingBuffers=%i)\n"),
						DSCaps.dwFreeHwMixingStreamingBuffers);
				return FALSE;
			}
		if(DSCaps.dwMaxContigFreeHwMemBytes<BufferSize(lpFormats[i].szName,dwSeconds)) {
			if(hrReturn) 
				LOG(TEXT("There is not enough memory (dwMaxContigFreeHwMemBytes=%i)\n"),
					DSCaps.dwMaxContigFreeHwMemBytes);
			return FALSE;
		}
	}
	if(DSBCAPS_PRIMARYBUFFER&dwCaps) 
		if(!((DSCaps.dwFlags&lpPrimaryFormats[i%4].value)==lpPrimaryFormats[i%4].value)) {
			if(hrReturn) 
				LOG(TEXT("GetCaps flags don't have %s\n"),lpPrimaryFormats[i%4].szName);
			return FALSE;
		}
	return TRUE;
}

TESTPROCAPI Playback(DWORD dwCaps) {
	DSCAPS DSCaps;
	int iResponse;
	BOOL bResponse;
	LPDIRECTSOUND pDS;
	HRESULT hr,hrReturn;
	DWORD tr=TPR_PASS,itr,i;
	DWORD size,max,maxBuffer=0;

 	if(g_bSkipOut) return TPR_SKIP;
 	LOG(TEXT("INSTRUCTIONS:  This test case will attempt to playback a tone for all common formats" ));
 	LOG(TEXT("INSTRUCTIONS:  Please listen to playback to ensure that a tone is played"));
	ZeroMemory(&DSCaps,sizeof(DSCaps));
	DSCaps.dwSize=sizeof(DSCaps);
	hr=g_pfnDSCreate(g_DS,&pDS,NULL);
	CheckMMRESULT(hr,"ERROR:  Failed to create.  DirectSoundCreate",TPR_ABORT,"Unable to get driver interface");
	hr=pDS->GetCaps(&DSCaps);
	CheckMMRESULT(hr,"ERROR:  GetCaps failed.  GetCaps", TPR_FAIL,"Driver responded incorrectly");
	hr=pDS->Release();
	CheckMMRESULT(hr,"ERROR:  Release failed.  Release", TPR_FAIL,"Driver responded incorrectly");
	for(i=0;lpFormats[i].szName;i++) {
		bResponse=TRUE;
    	    	hrReturn=DS_OK;
    		LOG(TEXT("Attempting to playback %s\n"),lpFormats[i].szName);
		itr=PlayWaveFormat(lpFormats[i].szName,g_duration,CALLBACK_NONE,dwCaps,NULL,&hrReturn);
		if(itr==TPR_PASS) {
			LOG(TEXT("Result:  Succeeded\n"));
			size=BufferSize(lpFormats[i].szName,g_duration);
			if(size>=maxBuffer) {
				max=i;
				maxBuffer=size;
			}
		}
		if(ClaimsSupport(dwCaps,DSCaps,i,g_duration)) {
			if(itr==TPR_FAIL)
				LOG(TEXT("ERROR:  GetCaps reports %s is supported, but %s was returned.\n"),
					lpFormats[i].szName,g_ErrorMap[hrReturn]);
		}
		else if(itr==TPR_PASS) {
			LOG(TEXT("WARNING:  GetCaps reports %s is unsupported, but an error was not returned (test completed)\n"),lpFormats[i].szName);
			ClaimsSupport(dwCaps,DSCaps,i,g_duration,TRUE);
			
		}
		else {
			LOG(TEXT("GetCaps reports that this is not supported"));
			itr=TPR_PASS;
			bResponse=FALSE;
		}
		
		if((g_interactive)&&(itr==TPR_PASS)&&(bResponse)) {
			iResponse=MessageBox(NULL,TEXT("Did you hear the tone? (cancel to retry)"),
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
	}
Error:
	return tr;
}


TESTPROCAPI Playback(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE ) {
	BEGINTESTPROC 
	return ::Playback(0);
}

TESTPROCAPI Playback_Software(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE ) {
	BEGINTESTPROC 
	if(g_all)
		return ::Playback(DSBCAPS_LOCSOFTWARE);
	else return TPR_SKIP;
}

TESTPROCAPI Playback_Hardware_Streaming(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE ) {
	BEGINTESTPROC 
	if(g_all)
		return ::Playback(DSBCAPS_LOCHARDWARE);
	else return TPR_SKIP;
}

TESTPROCAPI Playback_Hardware_Static(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE ) {
	BEGINTESTPROC 
	if(g_all)
		return ::Playback(DSBCAPS_LOCHARDWARE|DSBCAPS_STATIC);
	else return TPR_SKIP;
}

TESTPROCAPI Playback_Primary(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE ) {
	BEGINTESTPROC 
#ifdef UNDER_CE
	LOG(TEXT("This test case is not supported on Windows CE\n"));
	LOG(TEXT("because SetCooperativeLevel only supports NORMAL\n"));
	return TPR_SKIP;
#else
	if(g_all)
		return Playback(DSBCAPS_PRIMARYBUFFER);
	else return TPR_SKIP;
#endif
}

TESTPROCAPI PlaybackNotifications(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE ) {
	BEGINTESTPROC 
	DWORD tr=TPR_PASS;
	DWORD itr,i,j;
	DSCAPS DSCaps;
	HRESULT hr,hrReturn;
	LPDIRECTSOUND pDS;
	int iResponse;
  
 	if(g_bSkipOut) return TPR_SKIP;
	LOG(TEXT("INSTRUCTIONS:  This test case will attempt to playback a tone for all lpCallbacks\n"));
 	LOG(TEXT("INSTRUCTIONS:  Please listen to playback to ensure that a tone is played\n"));
	ZeroMemory(&DSCaps,sizeof(DSCaps));
	DSCaps.dwSize=sizeof(DSCaps);
	hr=g_pfnDSCreate(g_DS,&pDS,NULL);
	CheckMMRESULT(hr,"ERROR:  Failed to create.  DirectSoundCreate",TPR_ABORT,"Unable to get driver interface");
	hr=pDS->GetCaps(&DSCaps);
	CheckMMRESULT(hr,"ERROR:  GetCaps failed.  GetCaps", TPR_FAIL,"Driver responded incorrectly");
	hr=pDS->Release();
	CheckMMRESULT(hr,"ERROR:  Release failed.  Release", TPR_FAIL,"Driver responded incorrectly");
	for(i=0;lpFormats[i].szName;i++) 
		if(DSCaps.dwFlags&lpSecondaryFormats[i%4].value) break;
	if(!lpFormats[i].szName) {
	    		LOG(TEXT("ERROR:  There are no supported lpFormats\n"));
	    		return TPR_SKIP;
	}
    	for(j=0;lpCallbacks[j].szName;j++) {
    		LOG(TEXT("Attempting to playback with %s\n"),lpCallbacks[j].szName);
		itr=PlayWaveFormat(lpFormats[i].szName,g_duration,lpCallbacks[j].value,DSBCAPS_LOCSOFTWARE,NULL,&hrReturn);
		if((g_interactive)&&(itr==TPR_PASS)) {
			iResponse=MessageBox(NULL,TEXT("Did you hear the tone? (cancel to retry)"),
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
		} else tr|=itr;
    }
Error:
    return tr;
}

TESTPROCAPI PlaybackExtended(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE ) {
	BEGINTESTPROC 
	DWORD tr=TPR_PASS;
	DWORD itr,i,j,m,dwRestart;
    	int iResponse;
    	HRESULT hr,hrReturn;
	DSCAPS DSCaps;
	LPDIRECTSOUND pDS;

 	if(g_bSkipOut) return TPR_SKIP;
	LOG(TEXT("INSTRUCTIONS:  This test case will attempt to playback a tone for using extended funcitons\n") );
  	LOG(TEXT("INSTRUCTIONS:  Please listen to playback to ensure that a tone is played and changed\n"));
 	LOG(TEXT("INSTRUCTIONS:  When Frequency is changed, the tone will change because the waveform is played at a different rate\n"));
 	LOG(TEXT("INSTRUCTIONS:      Also, the expected play time of the buffer will also change\n"));
 	LOG(TEXT("INSTRUCTIONS:  When Volume is changed, the tone should decrease on both channels\n"));
 	LOG(TEXT("INSTRUCTIONS:      Then it should decrease in volume on the right channel\n"));
 	LOG(TEXT("INSTRUCTIONS:  When Pan is changed, the tone should decrease on one channels\n"));
 	LOG(TEXT("INSTRUCTIONS:      If the value is negative, it should decrease the right channel\n"));
 	LOG(TEXT("INSTRUCTIONS:      If the value is positive, it should decrease the left channel\n")); 
	ZeroMemory(&DSCaps,sizeof(DSCaps));
	DSCaps.dwSize=sizeof(DSCaps);
	hr=g_pfnDSCreate(g_DS,&pDS,NULL);
	CheckMMRESULT(hr,"ERROR:  Failed to create.  DirectSoundCreate",TPR_ABORT,"Unable to get driver interface");
	hr=pDS->GetCaps(&DSCaps);
	CheckMMRESULT(hr,"ERROR:  GetCaps failed.  GetCaps", TPR_FAIL,"Driver responded incorrectly");
	hr=pDS->Release();
	CheckMMRESULT(hr,"ERROR:  Release failed.  Release", TPR_FAIL,"Driver responded incorrectly");
	for(i=0;lpFormats[i].szName;i++) 
		if(DSCaps.dwFlags&lpSecondaryFormats[i%4].value) break;
	if(!lpFormats[i].szName) {
	    		LOG(TEXT("ERROR:  There are no supported lpFormats\n"));
	    		return TPR_SKIP;
	}

	for(j=0,m=0,dwRestart=0;lpExtensions[j].szName;j++) {
Restart:
		hrReturn=DS_OK;
		itr=PlayWaveFormat(lpFormats[i].szName,g_duration,CALLBACK_NONE,
			DSBCAPS_LOCSOFTWARE,&lpExtensions[j].value,&hrReturn);
		if(!lpExtensions[j].value.bContinue) {
			if(itr==TPR_PASS) {
				if(g_interactive) {
					iResponse=MessageBox(NULL,extensionMessages[m],
						TEXT("Interactive Response"),MB_YESNOCANCEL|MB_ICONQUESTION);
					switch(iResponse) {
						case IDNO:
							LOG(TEXT("ERROR:  User said there was no difference in the tone %s\n"),lpExtensions[j].value.szName);
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
		if(DSCaps.dwFlags&lpExtensions[j].value.flag) {
			if(itr==TPR_FAIL)
				LOG(TEXT("DirectSound claims that %s changes are not supported and %s was returned.\n"),lpExtensions[j].value.szName,g_ErrorMap[hrReturn]);
		}
		else if(itr==TPR_PASS) {
				LOG(TEXT("DirectSound claims that %s changes are supported\n"),lpExtensions[j].value.szName);
		}
		else {
			itr=TPR_PASS;
		}
		tr|=itr;
		if(tr&TPR_FAIL) return tr;
    }
Error:
    return tr;
}
