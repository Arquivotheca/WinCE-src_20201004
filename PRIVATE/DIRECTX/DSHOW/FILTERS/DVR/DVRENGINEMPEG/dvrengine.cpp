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
// DVREngine.cpp : Defines the entry points for the DLL application.
//

#include "stdafx.h"
#include "DVREngine.h"
#include "Plumbing\\Sink.h"
#include "Plumbing\\Source.h"

extern "C" BOOL WINAPI DllEntryPoint(HINSTANCE, ULONG, LPVOID);

REFERENCE_TIME g_rtMinimumBufferingUpstreamOfLive;
double g_dblMPEGReaderFullFrameRate;
double g_dblMPEGDecoderDriverKeyFrameRatio;
double g_dblMPEGDecoderDriverKeyFramesPerSecond;
double g_dblMaxFullFrameRateBackward;
double g_dblMaxFullFrameRateForward;
double g_dblMaxSupportedSpeed;
DWORD g_dwReaderPriority;
DWORD g_dwWriterPriority;
DWORD g_dwLazyReadOpenPriority;
DWORD g_dwWritePreCreatePriority;
DWORD g_dwLazyDeletePriority;
DWORD g_dwStreamingThreadPri;
ULONGLONG g_uhyMaxPermanentRecordingFileSize;
ULONGLONG g_uhyMaxTemporaryRecordingFileSize;
DWORD g_dwMinAcceptableClockDebug;
DWORD g_dwMaxAcceptableClockDebug;
DWORD g_dwMinAcceptableClockRetail;
DWORD g_dwMaxAcceptableClockRetail;
BOOL  g_bEnableFilePreCreation;
DWORD g_dwExtraSampleProducerBuffers;

bool GetRegistryOverride(const TCHAR *pszRegistryProperyName, DWORD &dwValue)
{
	bool fGotValue = false;
	HKEY hkeyDVREngine = NULL;

    if ( ( ERROR_SUCCESS == RegOpenKeyEx(
					HKEY_LOCAL_MACHINE,
					TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\MediaCenter\\MCShell"), 0, 0, &hkeyDVREngine ) ) &&
         hkeyDVREngine )
    {
        // Get the desired property:

        DWORD cbData = sizeof( dwValue );
		DWORD dwType = 0;

        if ( ( ERROR_SUCCESS == RegQueryValueEx(
               hkeyDVREngine, pszRegistryProperyName, NULL, &dwType, (PBYTE)&dwValue, &cbData ) ) &&
             (REG_DWORD == dwType) )
        {
			fGotValue = true;
        }
        RegCloseKey( hkeyDVREngine );
    }
	return fGotValue;
} // GetRegistryValue

static void InitDVREngineRegistryParameters()
{
	DWORD dwValue;
	
	if (GetRegistryOverride(TEXT("MillisecBufferingForLive"), dwValue))
		g_rtMinimumBufferingUpstreamOfLive = ((LONGLONG)dwValue) * 10000LL;
	else
		g_rtMinimumBufferingUpstreamOfLive = 1150LL * 10000LL;

	if (GetRegistryOverride(TEXT("MPEGMaxFullFrameRate"), dwValue))
		g_dblMPEGReaderFullFrameRate = ((double)dwValue) / 1000.0;
	else
		g_dblMPEGReaderFullFrameRate = 2.0;

	if (GetRegistryOverride(TEXT("MPEGKeyFramesPerSecond"), dwValue))
		g_dblMPEGDecoderDriverKeyFramesPerSecond = ((double)dwValue) / 1000.0;
	else
		g_dblMPEGDecoderDriverKeyFramesPerSecond = 2.0;

	if (GetRegistryOverride(TEXT("MPEGKeyFrameRatio"), dwValue))
		g_dblMPEGDecoderDriverKeyFrameRatio = ((double)dwValue) / 1000.0;
	else
		g_dblMPEGDecoderDriverKeyFrameRatio = 2.5;

	if (GetRegistryOverride(TEXT("MaxFullFrameRateForward"), dwValue))
		g_dblMaxFullFrameRateForward = ((double)dwValue);
	else
		g_dblMaxFullFrameRateForward = 2.0;

	if (GetRegistryOverride(TEXT("MaxFullFrameRateBackward"), dwValue))
		g_dblMaxFullFrameRateBackward = ((double)dwValue);
	else
		g_dblMaxFullFrameRateBackward = 2.0;

	if (GetRegistryOverride(TEXT("MaxSupportedSpeed"), dwValue))
		g_dblMaxSupportedSpeed = ((double)dwValue);
	else
		g_dblMaxSupportedSpeed = 250.0;

	if (GetRegistryOverride(TEXT("DVRReaderPriority"), dwValue))
		g_dwReaderPriority = dwValue;
	else
		g_dwReaderPriority = 251;

	if (GetRegistryOverride(TEXT("DVRWriterPriority"), dwValue))
		g_dwWriterPriority = dwValue;
	else
		g_dwWriterPriority = 250;

	if (GetRegistryOverride(TEXT("DVRPreCreatePriority"), dwValue))
		g_dwWritePreCreatePriority = dwValue;
	else
		g_dwWritePreCreatePriority = 252;

	if (GetRegistryOverride(TEXT("DVRLazyOpenPriority"), dwValue))
		g_dwLazyReadOpenPriority = dwValue;
	else
		g_dwLazyReadOpenPriority = 251;

	if (GetRegistryOverride(TEXT("DVRLazyDeletePriority"), dwValue))
		g_dwLazyDeletePriority = dwValue;
	else
		g_dwLazyDeletePriority = 251;

	if (GetRegistryOverride(TEXT("StreamingThreadPri"), dwValue))
		g_dwStreamingThreadPri = dwValue;
	else
		g_dwStreamingThreadPri = 251;

	if (GetRegistryOverride(TEXT("MaxPermFileSizeCb"), dwValue))
		g_uhyMaxPermanentRecordingFileSize = (ULONGLONG) dwValue;
	else
		g_uhyMaxPermanentRecordingFileSize = (ULONGLONG) 0xFF000000;	// Little less than 4 GB

	if (GetRegistryOverride(TEXT("MaxTempFileSizeCb"), dwValue))
		g_uhyMaxTemporaryRecordingFileSize = (ULONGLONG) dwValue;
	else
		g_uhyMaxTemporaryRecordingFileSize = (ULONGLONG) 134217728; // ~128 MB

	if (GetRegistryOverride(TEXT("DVRMinClockDebug"), dwValue))
		g_dwMinAcceptableClockDebug = dwValue;
	else
		g_dwMinAcceptableClockDebug = 8500;	// clock rate ratio of 0.85

	if (GetRegistryOverride(TEXT("DVRMaxClockDebug"), dwValue))
		g_dwMaxAcceptableClockDebug = dwValue;
	else
		g_dwMaxAcceptableClockDebug = 11500;	// clock rate ratio of 1.15

	if (GetRegistryOverride(TEXT("DVRMinClockRetail"), dwValue))
		g_dwMinAcceptableClockRetail = dwValue;
	else
		g_dwMinAcceptableClockRetail = 9800;	// clock rate ratio of 0.98

	if (GetRegistryOverride(TEXT("DVRMaxClockRetail"), dwValue))
		g_dwMaxAcceptableClockRetail = dwValue;
	else
		g_dwMaxAcceptableClockRetail = 10200;	// clock rate ratio of 1.02

	if (GetRegistryOverride(TEXT("EnableFilePreCreation"), dwValue))
		g_bEnableFilePreCreation = dwValue;
	else
		g_bEnableFilePreCreation = false;           // disable file pre-creation and padding by default

    if (GetRegistryOverride(TEXT("ExtraSampleProducerBuffers"), dwValue))
		g_dwExtraSampleProducerBuffers = dwValue;
	else
		g_dwExtraSampleProducerBuffers = 0;        // default is no extra buffers
} // InitDVREngineRegistryParameters

BOOL APIENTRY DllMain( HANDLE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved )
{
	if (DLL_PROCESS_ATTACH == ul_reason_for_call)
	{
		InitDVREngineRegistryParameters();
	}
#ifndef SHIP_BUILD
#ifdef DEBUG
    DEBUGREGISTER((HINSTANCE)hModule);
#else
    RETAILREGISTERZONES((HINSTANCE)hModule);
#endif
#endif

    return DllEntryPoint(static_cast <HINSTANCE> (hModule), ul_reason_for_call, lpReserved);
}

STDAPI DllRegisterServer()
{
    return AMovieDllRegisterServer2(TRUE);
}

STDAPI DllUnregisterServer()
{
    return AMovieDllRegisterServer2(FALSE);
}

void * __cdecl operator new(size_t size)
{
    const static std::bad_alloc cExcept;
    void *p = malloc(size);
    if (!p)
        throw cExcept;
    return p;
}

void * __cdecl operator new[](size_t size)
{
    const static std::bad_alloc cExcept;
    void *p = malloc(size);
    if (!p)
        throw cExcept;
    return p;
}

using MSDvr::CDVRSinkFilter;
using MSDvr::CDVRSourceFilter;
CFactoryTemplate g_Templates[] = {
    {
        CDVRSinkFilter::s_wzName,               // name
        &CDVRSinkFilter::s_clsid,               // CLSID
        CDVRSinkFilter::CreateInstance,         // creation function
        NULL,
        &CDVRSinkFilter::s_sudFilterReg         // pointer to filter information
    },
    {
        CDVRSourceFilter::s_wzName,
        &CDVRSourceFilter::s_clsid,
        CDVRSourceFilter::CreateInstance,
        NULL,
        &CDVRSourceFilter::s_sudFilterReg
    }
};
int g_cTemplates = sizeof(g_Templates) / sizeof(g_Templates[0]);
