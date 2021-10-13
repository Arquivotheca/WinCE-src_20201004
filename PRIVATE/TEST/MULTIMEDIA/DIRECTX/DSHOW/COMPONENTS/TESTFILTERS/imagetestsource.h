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
//------------------------------------------------------------------------------
// File: ImageTestSource.h
//
//------------------------------------------------------------------------------

#ifndef _IMAGE_TEST_SOURCE_H
#define _IMAGE_TEST_SOURCE_H

#include <streams.h>

#include "Imaging.h"
#include "ImageTestUids.h"
#include "TestInterfaces.h"

enum DataSource
{
	IMAGE,
	VIDEO,
	NONE
};

enum ControlEvent {
	EvExit,
	EvActive,
	EvInactive,
	EvBeginStreaming,
	EvEndStreaming,
	EvBeginFlush,
	EvEndFlush,
	EvNewSegment,
	EvEndOfStream
};

// the filter class (defined below)
class CImageTestSource;
extern const AMOVIESETUP_FILTER ImageTestSourceDesc;

// the output pin class
class CImageTestOutputPin
  : public CBaseOutputPin
{
	friend class CImageTestSource;

public:
    // constructor and destructor
	CImageTestOutputPin(HRESULT * phr, CImageTestSource* pFilter, CCritSec* pLock);

    virtual ~CImageTestOutputPin();

    // --- CUnknown ---
    DECLARE_IUNKNOWN
    STDMETHODIMP NonDelegatingQueryInterface(REFIID, void**);

    // --- CBasePin methods ---
    // Return the types we prefer - this will return the known file types
    virtual HRESULT GetMediaType(int iPosition, CMediaType *pMediaType);
	// Implement the Quality Control Notify method to return E_NOTIMPL. The base class does the same but asserts in debug mode.
	virtual HRESULT STDMETHODCALLTYPE Notify(IBaseFilter * pSender, Quality q);
    
    // can we support this type?
    virtual HRESULT CheckMediaType(const CMediaType* pType);

	// We need a way of enabling / disabling data transfers
	HRESULT Active();
	HRESULT Inactive();

    // --- CBaseOutputPin methods ---
	// DecideBufferSize is a pure virtual method in CBaseOutputPin. Must be implemented here.
	virtual HRESULT DecideBufferSize(IMemAllocator * pAlloc, ALLOCATOR_PROPERTIES * pAllocProps);
	// Override DecideAllocator in case we need to force our allocator to be used.
	virtual HRESULT DecideAllocator(IMemInputPin* pPin, IMemAllocator **ppAlloc);

private:
	static DWORD WINAPI ThreadFunction(LPVOID pPin);
	void WorkerThread();
	void ProcessControlEvent(ControlEvent event);
	void ProcessData();

private:
	ControlEvent m_Event;
	HRESULT m_hr;
	HANDLE m_hControlEvent;
	HANDLE m_hEventServiced;
	HANDLE m_hThread;
	bool m_bIsActive;
	bool m_bStreaming;
	bool m_bFlushing;
	int m_nSamplesSourced;
	CImageTestSource *m_pTestSource;
	REFERENCE_TIME m_rtCurrentFrame;
	int m_nSamplesLate;

private:
	// Private interface
	HRESULT SetControlEvent(ControlEvent ev);
};

class CImageTestSource : public ITestSource, public CBaseFilter
{
	friend class CImageTestOutputPin;
public:
	CImageTestSource(const TCHAR* szName, LPUNKNOWN pUnk, HRESULT *phr);

    virtual ~CImageTestSource();

    static CUnknown * WINAPI CreateInstance(LPUNKNOWN, HRESULT *);

    DECLARE_IUNKNOWN

    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void **ppv);

	// CBaseFilter pure virtual methods
	virtual int GetPinCount();
    virtual CBasePin *GetPin(int n);

	// CBaseFilter overrides to avoid overcome locking issues
	virtual STDMETHODIMP Stop();
	virtual STDMETHODIMP Pause();
	virtual STDMETHODIMP Run(REFERENCE_TIME tStart);

	// ITestSource implementation
	virtual HRESULT BeginFlushTest();
	virtual HRESULT EndFlushTest();
	virtual HRESULT NewSegmentTest();
	virtual HRESULT EndOfStreamTest();
	virtual HRESULT GenerateMediaSample(IMediaSample* ppSample, IPin *pPin);
	virtual HRESULT BeginStreaming();
	virtual HRESULT EndStreaming();
	virtual bool IsStreaming();
	virtual int GetSamplesSourced(int pin = 0);
	virtual HRESULT SetSampleLatenessProbability(float p);
	virtual HRESULT SetVerifier(IVerifier *pVerifier);
	virtual IVerifier *GetVerifier();
	virtual int GetNumMediaTypeCombinations();
	virtual HRESULT SelectMediaTypeCombination(int iPosition);
	virtual HRESULT GetMediaTypeCombination(int iPosition, AM_MEDIA_TYPE** ppMediaType);
	virtual int GetSelectedMediaTypeCombination();
	virtual HRESULT SetStreamingMode(StreamingMode mode, int nUnits, StreamingFlags flags);
	virtual HRESULT SetDataSource(const TCHAR* file);
	virtual bool SetInvalidConnectionParameters();
	virtual bool SetInvalidAllocatorParameters();
	virtual HRESULT UseOwnAllocator();

	
	// Helper methods
	HRESULT GetRefTime(REFERENCE_TIME* pCurrentTime);
	HRESULT GetStreamTime(REFERENCE_TIME* pStreamTime);
	float GetSampleLatenessProbability();
	bool IsUsingOwnAllocator();

protected:
	// The lock.
	CCritSec m_Lock;

	// The output pin.
	CImageTestOutputPin *m_pOutputPin;

	// The selected media type if there was one selected.
	int m_SelectedMediaType;

	// The verifier to be used if any.
	IVerifier* m_pVerifier;
	
	// Source of the image/video data if specified. Otherwise random data will be generated.
	TCHAR* m_szImageFile;

	// Streaming mode specification.
	StreamingMode m_StreamingMode;
    int m_nUnits;
	StreamingFlags m_StreamingFlags;

	// Data source
	DataSource m_bDataSource;

	// Lateness probability
	float m_LatenessProbability;

	// Should we use our own allocator?
	bool m_bOwnAllocator;

	// The allocator
	IMemAllocator* m_pAllocator;
};

#endif //_IMAGE_TEST_SOURCE_H
