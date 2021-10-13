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
////////////////////////////////////////////////////////////////////////////////

#ifndef _TAP_FILTER_H
#define _TAP_FILTER_H

#include <dshow.h>
#include "streams.h"
#include "GraphEvent.h"

// Custom Behavior HRESULTS
#define TF_S_DROPSAMPLE 0x28880001      // Don't forward samples to downstream filters

// The callback method
typedef HRESULT (*TapFilterCallbackFunction)(GraphEvent event, void* pGraphEventData, void* pCallbackData, void* pObject);

// The TapFilter interface
class ITapFilter
{
public:
	virtual HRESULT Insert(IGraphBuilder* pGraph, IPin* pFromPin) = 0;	
	virtual HRESULT Disconnect() = 0;
	virtual HRESULT GetMediaType(AM_MEDIA_TYPE* pMediaType, PIN_DIRECTION pindir) = 0;

	// Register a callback for an event
	virtual HRESULT RegisterEventCallback(TapFilterCallbackFunction callback, void* pCallbackData, void* pObject) = 0;
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
	virtual HRESULT STDMETHODCALLTYPE ReceiveConnection(IPin *pConnector, const AM_MEDIA_TYPE *pmt);
	virtual HRESULT STDMETHODCALLTYPE Disconnect();
	virtual HRESULT STDMETHODCALLTYPE CompleteConnect(const AM_MEDIA_TYPE* pmt);
	virtual HRESULT STDMETHODCALLTYPE BeginFlush();
	virtual HRESULT STDMETHODCALLTYPE EndFlush();
	virtual HRESULT STDMETHODCALLTYPE EndOfStream();
	virtual HRESULT STDMETHODCALLTYPE NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate);

	// Override this to avoid making unnecessary checks for the allocator among other things
	virtual HRESULT Active();
	virtual HRESULT Inactive();

	// Private interface
	HRESULT SetConnectionParameters(CTapFilterOutputPin* pTapFilterOutputPin, IPin* pFromPin, IPin* pToPin, AM_MEDIA_TYPE* pMediaType);
	HRESULT UpdateMediaType(AM_MEDIA_TYPE* pmt);

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
	virtual HRESULT STDMETHODCALLTYPE Connect(IPin* pReceivePin, const AM_MEDIA_TYPE* pmt);
	virtual HRESULT STDMETHODCALLTYPE Disconnect();

	// CBaseOutputPin overrides
	virtual HRESULT DecideBufferSize(IMemAllocator* pAlloc, ALLOCATOR_PROPERTIES *pProp);
	// Override this to avoid making unnecessary checks for the allocator
	virtual HRESULT Active();
	virtual HRESULT Inactive();

	// Pass through for quality control messages
	virtual HRESULT STDMETHODCALLTYPE Notify(IBaseFilter * pSender, Quality q);

	// Private interface
	HRESULT SetInputConnected(bool connected, const AM_MEDIA_TYPE* pmt = NULL);
	HRESULT SetConnectionParameters(CTapFilterInputPin* pTapFilterInputPin, IPin* pFromPin, IPin* pToPin, AM_MEDIA_TYPE* pMediaType);

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

typedef vector<Callback> CallbackList;

class CTapFilter : public CBaseFilter, public ITapFilter
{
	friend class CTapFilterInputPin;
	friend class CTapFilterOutputPin;
public:
    static HRESULT CreateInstance(CTapFilter** ppTapFilter);

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
	HRESULT GetMediaType(AM_MEDIA_TYPE* pMediaType, PIN_DIRECTION pindir);

private:
	CallbackList callbackList;
	CTapFilterOutputPin *m_pOutputPin;
	CTapFilterInputPin *m_pInputPin;
	IPin* m_pTappedInputPin;
	IPin* m_pTappedOutputPin;
    CCritSec m_csFilter;
};

#endif
