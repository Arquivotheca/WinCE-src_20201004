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

#pragma once

#ifndef _TAP_FILTER_H
#define _TAP_FILTER_H

#include <vector>
#include <dshow.h>
#include <streams.h>
#include <transip.h>
#include "GraphEvent.h"
#include "Verification.h"


// {F32139D8-8C9E-48ba-82B4-50B7CA65F052}
DEFINE_GUID(CLSID_TapFilter, 
0xf32139d8, 0x8c9e, 0x48ba, 0x82, 0xb4, 0x50, 0xb7, 0xca, 0x65, 0xf0, 0x52);

// {1EFA9F4F-BCBB-41d7-AA4F-39C7CF6CCD06}
DEFINE_GUID(IID_ITapFilter, 
0x1efa9f4f, 0xbcbb, 0x41d7, 0xaa, 0x4f, 0x39, 0xc7, 0xcf, 0x6c, 0xcd, 0x6);

// Custom Behavior HRESULTS
#define TF_S_DROPSAMPLE 0x28880001      // Don't forward samples to downstream filters

#define DEFAULT_INIT_QUEUE_NAME _T("TapFilter Init Queue")

// The callback method
typedef HRESULT (*TapFilterCallbackFunction)(GraphEvent event, void* pGraphEventData, void* pCallbackData, void* pObject);

// The ITapFilter interface
MIDL_INTERFACE("1EFA9F4F-BCBB-41d7-AA4F-39C7CF6CCD06")
ITapFilter : public IUnknown
{
public:
    virtual HRESULT Insert(IGraphBuilder* pGraph, IPin* pFromPin) = 0;    
    virtual HRESULT Disconnect() = 0;
    virtual HRESULT GetMediaType(AM_MEDIA_TYPE* pMediaType, PIN_DIRECTION pindir) = 0;

    // Register a callback for an event
    virtual HRESULT RegisterEventCallback(TapFilterCallbackFunction callback, void* pCallbackData, void* pObject) = 0;
    virtual HRESULT RemoveEventCallback(void* pObject) = 0;

    virtual STDMETHODIMP GetStatParam(DWORD dwParam, DWORD* pdwParam1, DWORD* pdwParam2);    
    virtual HRESULT AreFiltersInGraph(TCHAR* szFilterNames, DWORD dwCount);
};

// The tap filter works as a pass through
// The tap filter will only connect filters that support the IMemInputPin transport - the input pin has to support this interface
// The tap filter assumes a certain protocol for connecting and disconnecting
// When connecting, 
// - the two pins FromPin and ToPin must already be connected. 
// - the TapFilter::Insert method disconnects the two pins
// - then it connects the FromPin to the TapFilterInputPin
// - during this connection, the negotiation calls are forwarded to the ToPin
// - so the connection for (FromPin->TapFilterInputPin) and (TapFilterOutputPin->ToPin) happens simultaneously
// - since the FGM is unaware of the second connection, TapFilter::Insert makes a call to connect the pins which results in default handling by the TapFilterOutputPin
//
// When disconnecting
// - the 4 pins are disconnected individually 
// - when the TapFilterInputPin is disconnected, this sets the TapFilterOutputPin as disconnected internally
// - a call to disconnect TapFilterOutputPin results in default handling


class CTapFilter;
class CTapFilterInputPin;
class CTapFilterOutputPin;

class CAsyncReaderPassThru : public IAsyncReader, CUnknown
{
public:
    CAsyncReaderPassThru(IAsyncReader* pPeerAsync, IPin* pPin, LPUNKNOWN pUnknown);
    virtual ~CAsyncReaderPassThru();

    DECLARE_IUNKNOWN;

    virtual HRESULT STDMETHODCALLTYPE NonDelegatingQueryInterface(REFIID riid, void **ppv);

    virtual HRESULT STDMETHODCALLTYPE BeginFlush();
    virtual HRESULT STDMETHODCALLTYPE EndFlush();
    virtual HRESULT STDMETHODCALLTYPE Length(LONGLONG *pTotal, LONGLONG *pAvailable);
    virtual HRESULT STDMETHODCALLTYPE RequestAllocator(IMemAllocator *pPreferred, ALLOCATOR_PROPERTIES *pProps, IMemAllocator **ppActual);
    virtual HRESULT STDMETHODCALLTYPE Request(IMediaSample *pSample, DWORD_PTR dwUser);
    virtual HRESULT STDMETHODCALLTYPE SyncReadAligned(IMediaSample *pSample);
    virtual HRESULT STDMETHODCALLTYPE SyncRead(LONGLONG llPosition, LONG lLength, BYTE *pBuffer);
    virtual HRESULT STDMETHODCALLTYPE WaitForNext(DWORD dwTimeout, IMediaSample **ppSample, DWORD_PTR *pdwUser);
private:
    IAsyncReader* m_pPeerAsync;
    IPin* m_pPin;
};

class CTapFilterInputPin : public CBaseInputPin
{
    friend class CTapFilterInputPin;
    friend class CTapFilter;
public:
    CTapFilterInputPin( CTapFilter* pFilter, CCritSec* pLoc, HRESULT* phr, TCHAR* szPinName );
    virtual ~CTapFilterInputPin();

    virtual HRESULT STDMETHODCALLTYPE NonDelegatingQueryInterface(REFGUID riid, void **ppv);


    // Overriding IMemInputPin methods - forward to the tapped input pin
    virtual HRESULT STDMETHODCALLTYPE Receive(IMediaSample* pSample);
    virtual HRESULT STDMETHODCALLTYPE GetAllocatorRequirements(ALLOCATOR_PROPERTIES *pProps);
    virtual HRESULT STDMETHODCALLTYPE GetAllocator(IMemAllocator** pMemAllocator);
    virtual HRESULT STDMETHODCALLTYPE NotifyAllocator(IMemAllocator *pAllocator, BOOL bReadOnly);
    
    // Media type methods - forward the call to the tapped input pin
    virtual HRESULT CheckMediaType(const CMediaType* pmt);
    virtual HRESULT GetMediaType(int iPosition,CMediaType *pMediaType);

    // IPin methods that will be handled/forwarded on
    virtual HRESULT STDMETHODCALLTYPE QueryAccept(const AM_MEDIA_TYPE* pmt);
    virtual HRESULT STDMETHODCALLTYPE Disconnect();
    virtual HRESULT CompleteConnect(IPin *pOutputPin);
    virtual HRESULT STDMETHODCALLTYPE BeginFlush();
    virtual HRESULT STDMETHODCALLTYPE EndFlush();
    virtual HRESULT STDMETHODCALLTYPE EndOfStream();
    virtual HRESULT STDMETHODCALLTYPE NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate);

    // Override this to avoid making unnecessary checks for the allocator among other things
    virtual HRESULT Active();
    virtual HRESULT Inactive();

    // Private interface
    HRESULT SetTapFilterOutputPin(CTapFilterOutputPin* pTapFilterOutputPin);
    HRESULT SetTappedInputPin(IPin* pToPin);
    HRESULT SetTappedOutputPin(IPin* pFromPin);
    
    //HRESULT SetConnectionParameters(CTapFilterOutputPin* pTapFilterOutputPin, IPin* pFromPin, IPin* pToPin, AM_MEDIA_TYPE* pMediaType);

private:
    IPin* m_pTappedInputPin;
    IPin* m_pTappedOutputPin;
    IMemInputPin* m_pTappedMemInputPin;
    CTapFilterOutputPin* m_pTapFilterOutputPin;
    CTapFilter* m_pTapFilter;
};


class CTapPosPassThru : public CPosPassThru
{
public:
    CTapPosPassThru(const TCHAR *pName, LPUNKNOWN pUnk, HRESULT *phr, IPin *pPin);
    virtual HRESULT STDMETHODCALLTYPE get_Rate(double *pRate);
    virtual HRESULT STDMETHODCALLTYPE get_Duration(REFTIME *pDuration);
};

class CTapFilterOutputPin : public CBaseOutputPin
{
    friend class CTapFilterInputPin;
    friend class CTapFilter;
public:
    CTapFilterOutputPin( CTapFilter* pFilter, CCritSec* pLoc, HRESULT* phr, TCHAR* szPinName );
    virtual ~CTapFilterOutputPin();

    virtual HRESULT STDMETHODCALLTYPE NonDelegatingQueryInterface(REFGUID riid, void **ppv);

    // Overriding the media type methods
    virtual HRESULT CheckMediaType(const CMediaType* pmt);          
    virtual HRESULT GetMediaType(int iPosition,CMediaType *pMediaType);

    // Overriding the IPin methods
    virtual HRESULT STDMETHODCALLTYPE QueryAccept(const AM_MEDIA_TYPE* pmt);
    virtual HRESULT STDMETHODCALLTYPE ReceiveConnection(IPin *pConnector, const AM_MEDIA_TYPE *pmt);

    virtual HRESULT STDMETHODCALLTYPE Connect(IPin * pReceivePin, const AM_MEDIA_TYPE *pmt);    
    virtual HRESULT CompleteConnect(IPin *pReceivePin);

    // CBaseOutputPin overrides
    virtual HRESULT DecideBufferSize(IMemAllocator* pAlloc, ALLOCATOR_PROPERTIES *pProp);
    // Override this to avoid making unnecessary checks for the allocator
    virtual HRESULT Active();
    virtual HRESULT Inactive();

    // Pass through for quality control messages
    virtual HRESULT STDMETHODCALLTYPE Notify(IBaseFilter * pSender, Quality q);

    // Private interface
    HRESULT SetTapFilterInputPin(CTapFilterInputPin* pTapFilterInputPin);
    HRESULT SetTappedInputPin(IPin* pToPin);
    HRESULT SetTappedOutputPin(IPin* pFromPin);
    
    HRESULT SetInputConnected(bool connected, const AM_MEDIA_TYPE* pmt = NULL);
    //HRESULT SetConnectionParameters(CTapFilterInputPin* pTapFilterInputPin, IPin* pFromPin, IPin* pToPin, AM_MEDIA_TYPE* pMediaType);

    // Return the filter pointer
    CTapFilter *GetFilter() { return m_pTapFilter; }
    
private:
    // Pos pass thru
    CTapPosPassThru* m_pPosPassThru;
    CCritSec m_csPassThru;

    // IAsyncReader pass throug
    CAsyncReaderPassThru* m_pAsyncReader;


    // The output pin of the connected filter
    IPin* m_pTappedOutputPin;

    // The input pin of the connected filter
    IPin* m_pTappedInputPin;

    // The input pin of the tap filter
    CTapFilterInputPin* m_pTapFilterInputPin;
    CTapFilter* m_pTapFilter;

    // 
    
};

struct Callback
{
    TapFilterCallbackFunction callbackfn;
    void* pCallbackData;
    void* pObject;

    Callback(TapFilterCallbackFunction fn, void* pData, void* Object)
    {
        callbackfn = fn;
        pCallbackData = pData;
        pObject = Object;
    }

};

typedef std::vector<Callback> CallbackList;

#ifdef UNDER_CE
class CTapFilter : public CBaseFilter, public ITapFilter, public IAMVideoTransformComponent
{
    friend class CTapFilterInputPin;
    friend class CTapFilterOutputPin;
public:
    
    static CUnknown * WINAPI CreateInstance(LPUNKNOWN punk, HRESULT *phr); 

    CTapFilter(HRESULT *phr);
    ~CTapFilter();

    DECLARE_IUNKNOWN
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void **ppv);

    // Interface to connect the pins
    virtual HRESULT Insert(IGraphBuilder* pGraph, IPin* pFromPin);    
    virtual HRESULT Disconnect();
    virtual HRESULT TriggerGraphEvent(GraphEvent event, void* pEventData);

    // CBaseFilter Overrides
    virtual CBasePin *GetPin(int n);
    virtual int GetPinCount(void);
    virtual HRESULT STDMETHODCALLTYPE Pause();
    virtual HRESULT STDMETHODCALLTYPE Stop();
    virtual HRESULT STDMETHODCALLTYPE Run(REFERENCE_TIME tStart);

    // ITapFilter methods
    HRESULT RegisterEventCallback(TapFilterCallbackFunction callback, void* data, void* pObject);
    HRESULT RemoveEventCallback(void* pObject);
    HRESULT GetMediaType(AM_MEDIA_TYPE* pMediaType, PIN_DIRECTION pindir);
    STDMETHODIMP JoinFilterGraph(IFilterGraph *pGraph, LPCWSTR pName);
    STDMETHODIMP GetStatParam(DWORD dwParam, DWORD* pdwParam1, DWORD* pdwParam2);
    HRESULT AreFiltersInGraph(TCHAR* szFilterNames, DWORD dwCount);

    // IPersist methods
    STDMETHODIMP GetClassID(CLSID* pClassID);


    //
    // IAMVideoTransformComponent
    //
    STDMETHODIMP SetBufferParams(AM_ROTATION_ANGLE SurfaceAngle, int XPitch, int YPitch, const AM_MEDIA_TYPE *pmt);
    STDMETHODIMP GetBufferParams(AM_ROTATION_ANGLE *pSurfaceAngle, int *pXPitch, int *pYPitch);

    STDMETHODIMP SetScalingMode(AM_TRANSFORM_SCALING_MODE scalingMode);
    STDMETHODIMP GetScalingMode(AM_TRANSFORM_SCALING_MODE *pScalingMode);

    STDMETHODIMP GetRotationCaps(const AM_MEDIA_TYPE *pmt, DWORD *pdwRotationCapsMask);
    STDMETHODIMP GetScalingCaps(const AM_MEDIA_TYPE *pmt, DWORD *pdwScalingCapsMask, AMScalingRatio **pRatios, ULONG *pCount);
    STDMETHODIMP GetScalingModeCaps(const AM_MEDIA_TYPE *pmt, DWORD *pdwScalingModeCapsMask);
    HRESULT QueryTransformOnPeer(IAMVideoTransformComponent **ppTransformOperation);
    
    virtual LPAMOVIESETUP_FILTER GetSetupData();

private:
    CallbackList callbackList;
    CTapFilterOutputPin *m_pOutputPin;
    CTapFilterInputPin *m_pInputPin;
    IPin* m_pTappedInputPin;
    IPin* m_pTappedOutputPin;
    IAMVideoTransformComponent *m_pUpstreamTransform;
    CCritSec m_csFilter;
    static CVerificationMgr *m_pVerifMgr;
    VerifierList m_VerifierList;
    HANDLE m_hInQueue;
    IFilterGraph* m_pFilterGraph;
    
};
#else
class CTapFilter : public CBaseFilter, public ITapFilter
{
    friend class CTapFilterInputPin;
    friend class CTapFilterOutputPin;
public:
    
    static CUnknown * WINAPI CreateInstance(LPUNKNOWN punk, HRESULT *phr); 

    CTapFilter(HRESULT *phr);
    ~CTapFilter();

    // Interface to connect the pins
    virtual HRESULT Insert(IGraphBuilder* pGraph, IPin* pFromPin);    
    virtual HRESULT Disconnect();
    virtual HRESULT TriggerGraphEvent(GraphEvent event, void* pEventData);

    // CBaseFilter Overrides
    virtual CBasePin *GetPin(int n);
    virtual int GetPinCount(void);
    virtual HRESULT STDMETHODCALLTYPE Pause();
    virtual HRESULT STDMETHODCALLTYPE Stop();
    virtual HRESULT STDMETHODCALLTYPE Run(REFERENCE_TIME tStart);

    // ITapFilter methods
    HRESULT RegisterEventCallback(TapFilterCallbackFunction callback, void* data, void* pObject);
    HRESULT RemoveEventCallback(void* pObject);
    HRESULT GetMediaType(AM_MEDIA_TYPE* pMediaType, PIN_DIRECTION pindir);
    STDMETHODIMP JoinFilterGraph(IFilterGraph *pGraph, LPCWSTR pName);


private:
    CallbackList callbackList;
    CTapFilterOutputPin *m_pOutputPin;
    CTapFilterInputPin *m_pInputPin;
    IPin* m_pTappedInputPin;
    IPin* m_pTappedOutputPin;
    CCritSec m_csFilter;
    static CVerificationMgr *m_pVerifMgr;
    VerifierList m_VerifierList;
    HANDLE m_hInQueue;
    IFilterGraph* m_pFilterGraph;
};
#endif

typedef std::vector<CTapFilter *>    TapFilterList;

#endif
