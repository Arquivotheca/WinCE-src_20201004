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

#include <qnetwork.h>
#include <dshow.h>

#define SWAPBYTE(x,y) { BYTE temp = x; x = y; y = temp; }

typedef  HRESULT ( __stdcall *pFuncpfv)();

HRESULT FindInterfaceOnGraph(IGraphBuilder *pFilterGraph, REFIID riid, void **ppInterface);
HRESULT FindInterfaceOnGraph( IGraphBuilder *pFilterGraph, REFIID riid, void **ppInterface, 
                                    IEnumFilters* pEnumFilter, BOOL begin );
HRESULT FindDroppedFrames(REFIID refiid, IGraphBuilder* pGraph, IAMDroppedFrames** pDroppedFrames);
HRESULT FindQualProp(REFIID refiid, IGraphBuilder* pGraph, IQualProp** pQualProp);
HRESULT FindAudioGlitch(REFIID refiid, IGraphBuilder* pGraph, IAMAudioRendererStats** pDroppedFrames);
long WaitForEvent(IMediaEvent* pMediaEvent, long eventCode, long *pParam1, long* pParam2, DWORD dwTimeout = INFINITE);
void DisplayECEvent(long lEventCode);
bool SetupNetworkBuffers();
bool ChkExt(wchar_t *desired,const wchar_t *ToTest);
// switch = 1 enables switching, 0 disables
bool EnableBWSwitch(bool bwswitch);
bool EnableWMFrameDropping(bool bEnable);
HRESULT SetMaxBackBuffers(int dwMaxBackBuffers);
HRESULT SetScanLineUsage(bool bUseScanLine);
HRESULT SetVRPrimaryFlipping(bool bUseScanLine);
HRESULT GetScreenResolution(DWORD* pWidth, DWORD* pHeight);

TCHAR* GetNextToken(TCHAR* string, int* pos, TCHAR delimiter);
HRESULT UrlDownloadFile(TCHAR* szUrlFile, TCHAR* szLocalFile, HANDLE* pFileHandle);

const TCHAR* GetStateName(FILTER_STATE state);

// Returns true if actual is within (%pctTolerance or abs(tolerance)) of expected else false
bool ToleranceCheck(DWORD expected, DWORD actual, DWORD pctTolerance);
bool ToleranceCheck(LONGLONG expected, LONGLONG actual, DWORD pctTolerance);
bool ToleranceCheckAbs(LONGLONG expected, LONGLONG actual, DWORD tolerance);
bool LimitCheck(DWORD value, DWORD actual, DWORD pctLimit);

// Printing functions
void PrintMediaType(AM_MEDIA_TYPE* pMediaType);

// DDraw utility methods
HRESULT LockSurface(IMediaSample *pSample, bool bLock);
bool IsDDrawSample(IMediaSample *pSample);
HRESULT GetBackBuffers(IMediaSample *pSample, DWORD* pBackBuffers);

// Imaging
void InvertBitmap(BYTE* pBits, int width, int height, DWORD bitcount);
void SwapRedGreen(BYTE* pBits, int width, int height, DWORD bitcount);

// for waitforcompletion
long NearlyInfinite( LONGLONG llDuration, double rate );

// Wrapper for IMediaEvent->FreeEventParams() dealingi with EC_OLE_EVENT.
HRESULT 
FreeEventParams( IMediaEvent *pMediaEvent, long lEventCode, long lParam1, long lParam2 );

// Register a DLL by the given name
HRESULT
RegisterDll(TCHAR * tszDllName);