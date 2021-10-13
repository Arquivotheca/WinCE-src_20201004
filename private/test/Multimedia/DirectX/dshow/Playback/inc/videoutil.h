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
////////////////////////////////////////////////////////////////////////////////

#ifndef VIDEO_UTIL_H
#define VIDEO_UTIL_H

#ifdef UNDER_CE
#include "TestGraph.h"

const DWORD ALPHA_BITMAP_SIZE = 16;

// 
IDirectDrawSurface *
CreateDDrawSurface( int width, int height, BOOL bAlpha = FALSE );

void 
PumpMessages( DWORD dwDuration = 0 );

HWND 
OpenAppWindow( LONG lLeft, 
               LONG lTop, 
               LONG lVideoWidth, 
               LONG lVideoHeight, 
               RECT *pClientRect,
               CComPtr<IVMRWindowlessControl> pVMRWindowlessControl = NULL );

void 
CloseAppWindow( HWND hwnd );

HRESULT 
SetWindowPosition( IVMRWindowlessControl *pVMRControl, HWND hwnd );

void 
InitAlphaParams( VMRALPHABITMAP *pAlphaBitmap, 
                 DWORD dwFlags, 
                 HDC hdc, 
                 IDirectDrawSurface *pDDS, 
                 RECT *prcSrc, 
                 NORMALIZEDRECT *prcDest, 
                 float fAlpha, 
                 COLORREF clrKey );

HRESULT 
RotateUI (AM_ROTATION_ANGLE angle, BOOL bTest = FALSE);

HRESULT 
CenterWindow (IVideoWindow *pVideoWindow, int winWidth, int winHeight);

HRESULT 
GetRegKey (const WCHAR *pPath, const WCHAR *pKey, DWORD *pdwVal);

HRESULT 
SetRegKey (const WCHAR *pPath, const WCHAR *pKey, DWORD dwVal);

HRESULT 
GetDirectDraw(IDirectDraw **ppDirectDraw, HMODULE *phDirectDrawDLL);

void    
ReleaseDirectDraw(IDirectDraw *pDirectDraw, HMODULE hDirectDrawDLL);

BOOL    
IsYV12HardwareSupported();

BOOL    
IsOverlayHardwareSupported();

HRESULT 
ConfigMaxBackBuffers(DWORD uNumOfBuffers, DWORD* uOrigNumOfBuffers);

HRESULT 
RestoreMaxBackBuffers(DWORD uNumOfBuffers);

HRESULT 
DisableYUVSurfaces(bool* bDisableYUVSurfaces);

HRESULT 
EnableYUVSurfaces(void);

HRESULT 
DisableWMVDecoderFrameDrop(void);

HRESULT 
EnableWMVDecoderFrameDrop(void);

HRESULT 
EnableUpstreamScale(bool bUpstreamPreferred, DWORD* dwOrigUpstreamScalePreferred);

HRESULT 
RestoreOrigUpstreamScale(DWORD dwOrigUpstreamScalePreferred);

HRESULT 
EnableUpstreamRotate(bool bUpstreamPreferred, DWORD* dwOrigUpstreamRotationPreferred);

HRESULT 
RestoreOrigUpstreamRotate(DWORD dwOrigUpstreamRotationPreferred);

HRESULT 
SetupGDIMode(void);

HRESULT 
RestoreRenderMode(void);

HRESULT 
SetLogFile(TestGraph* pTestGraph);

HRESULT 
CenterWindow(TestGraph* pTestGraph, bool bRotate);

HRESULT 
VMRUpdateWindow (IBaseFilter *pVMR, HWND hWnd, bool bRotate);

#endif

#endif // VIDEO_UTIL_H
