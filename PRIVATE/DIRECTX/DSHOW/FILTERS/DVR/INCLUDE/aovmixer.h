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
#ifndef _AOVMIXER__H
#define _AOVMIXER__H

#ifndef EC_USER
#define EC_USER 0x8000
#endif // EC_USER

#define EC_ALPHABLEND  EC_USER | 850
#define EC_FIRSTSAMPLE_RENDERED  EC_USER | 851

// Inform video rate change due to dropping frame and extending frame
typedef VOID (*PFNNOTIFYVIDEOGLITCHCALLBACK)(DWORD dwRate, DWORD cFrameDropped);

// Inform primary surface must be change to have the proper back buffer count for this filter to run
typedef HRESULT (*PFNNOTIFYDDSPRIMARYCHANGECALLBACK)(DWORD cBackBuffer, LPDIRECTDRAWSURFACE* lppDDSPrimary);


// {1b96e2d2-c4e4-4dff-9f06-30014e262739}
DEFINE_GUID(CLSID_AOvMixer, 
	0x1b96e2d2, 0xc4e4, 0x4dff, 0x9f, 0x06, 0x30, 0x01, 0x4e, 0x26, 0x27, 0x39);

#undef INTERFACE
#define INTERFACE IAlphaMixer

// {0e243e74-52e9-4a40-a5bb-d964817f81ca}
DEFINE_GUID(IID_IAlphaMixer, 
	0x0e243e74, 0x52e9, 0x4a40, 0xa5, 0xbb, 0xd9, 0x64, 0x81, 0x7f, 0x81, 0xca);


	
DECLARE_INTERFACE_(IAlphaMixer, IUnknown )
{
    /*** IUnknown methods ***/
    STDMETHOD(QueryInterface) (THIS_ REFIID riid, LPVOID FAR * ppvObj) PURE;
    STDMETHOD_(ULONG,AddRef) (THIS)  PURE;
    STDMETHOD_(ULONG,Release) (THIS) PURE;
   /*** IAlpha methods ***/	
#ifndef _WIN32_WCE
    STDMETHOD(Init) (THIS_ LPDIRECTDRAWSURFACE5 lpDDSGraphics) PURE;
#endif
	STDMETHOD(SetNotfiyCallback)(THIS_ PFNNOTIFYDDSPRIMARYCHANGECALLBACK pfnCallback) PURE;
    STDMETHOD(AlphaBlend) (THIS_ BOOL bEnable) PURE;
    STDMETHOD(AlphaBlendRefresh) (THIS_ LPRECT prcBlend) PURE;
    STDMETHOD(WaitForBltGateOpen) (THIS_ LPRECT prcBlt) PURE;
    STDMETHOD(SamplesToDrop)(DWORD dwSamplesPerSecond) PURE;
    STDMETHOD(InsertDelay) (THIS_ LONGLONG llDelay) PURE;
    STDMETHOD(EnableBlend) (THIS_ BOOL bEnable) PURE;
};	

#undef INTERFACE
#define INTERFACE IMixerQA

// {2d17c92b-3534-45e6-b546-fc61f43c8c59}
DEFINE_GUID(IID_IMixerQA,    
	0x2d17c92b, 0x3534, 0x45e6, 0xb5, 0x46, 0xfc, 0x61, 0xf4, 0x3c, 0x8c, 0x59);


DECLARE_INTERFACE_( IMixerQA, IUnknown )
{
    /*** IUnknown methods ***/
    STDMETHOD(QueryInterface) (THIS_ REFIID riid, LPVOID FAR * ppvObj) PURE;
    STDMETHOD_(ULONG,AddRef) (THIS)  PURE;
    STDMETHOD_(ULONG,Release) (THIS) PURE;
	/*** IMixerQA methods ***/	
#ifndef _WIN32_WCE
    STDMETHOD(Snapshot) (THIS_ LPDIRECTDRAWSURFACE5 lpDDSScreenBuffer) PURE;
#endif
    STDMETHOD(SetNotifyVideoGlitch) (THIS_ PFNNOTIFYVIDEOGLITCHCALLBACK pfnCallback) PURE;
    STDMETHOD(GetFrameRate) (THIS_ DWORD *pRate) PURE;
	STDMETHOD(GetVideoDeliveryLateness) (THIS_ DWORD *pAvgLateness, DWORD *pMaxLateness) PURE;
};	


#endif // _AOVMIXER__H
