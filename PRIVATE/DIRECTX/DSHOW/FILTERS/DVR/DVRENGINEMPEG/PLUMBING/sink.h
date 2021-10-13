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
// Sink.h : media format-invariant sink filter and pin infrastructure.
//

#pragma once

#include "DvrInterfaces_private.h"
#include "..\\writer\\DvrEngineHelpers.h"
#include "Allocator.h"

namespace MSDvr
{

class		CDVRInputPin;
interface	ICapturePipelineManager;
struct      CPipelineUnknown;

//////////////////////////////////////////////////////////////////////////////
// @class SGuidCompare2 |
// A binary predicate class for establishing a strict weak ordering among
// GUID values.
//////////////////////////////////////////////////////////////////////////////
struct SGuidCompare2 : std::binary_function<REFGUID, REFGUID, bool>
{
	bool operator () (REFGUID arg1, REFGUID arg2) const;
};

//////////////////////////////////////////////////////////////////////////////
//	Sink filter.															//
//////////////////////////////////////////////////////////////////////////////

class CDVRSinkFilter :	public CBaseFilter,
						public IFileSinkFilter2,
						public IXDSCodec,
						public IStreamBufferCapture,
						public IContentRestrictionCapture,
						public CDVREngineHelpersImpl
{
	// Private constructor for use by the creation function only
	CDVRSinkFilter(LPUNKNOWN piAggUnk);
	virtual ~CDVRSinkFilter();

public:
	DECLARE_IUNKNOWN;

	// Creation function for the factory template
	static CUnknown* CreateInstance(LPUNKNOWN piAggUnk, HRESULT* phr);

#ifdef DEBUG
	STDMETHODIMP_(ULONG) NonDelegatingAddRef();
	STDMETHODIMP_(ULONG) NonDelegatingRelease();
	void AssertFilterLock() const
	{
		ASSERT (CritCheckIn(m_pLock));
	}
#endif // DEBUG

	// Add a pin to the filter (for pipeline manager only)
	void AddPin(LPCWSTR wzPinName);

	// Filter name
	static const LPCTSTR		s_wzName;

	// Filter CLSID
	static const CLSID			s_clsid;

	// Filter registration data
	static const AMOVIESETUP_FILTER s_sudFilterReg;

	// Override
	virtual int GetPinCount();

	// Override
	virtual CBasePin* GetPin(int n);

	// Override
	STDMETHOD(NonDelegatingQueryInterface) (REFIID riid, LPVOID* ppv);

	// Override
	STDMETHOD(Run)(REFERENCE_TIME tStart);

	// Override
	STDMETHOD(Pause)();

	// Override
	STDMETHOD(Stop)();

	// IFileSinkFilter2
	STDMETHODIMP GetCurFile(LPOLESTR *ppszFileName, AM_MEDIA_TYPE *pmt);
	STDMETHODIMP SetFileName(LPCOLESTR wszFileName, const AM_MEDIA_TYPE *pmt);
	STDMETHODIMP GetMode(DWORD *pdwMode);
	STDMETHODIMP SetMode(DWORD dwMode);

	// IStreamBufferCapture
	STDMETHODIMP GetCaptureMode(STRMBUF_CAPTURE_MODE *pMode,
								LONGLONG *pllMaxBuf);
	STDMETHODIMP BeginTemporaryRecording(LONGLONG llBufSize);
	STDMETHODIMP BeginPermanentRecording(LONGLONG llRetainedSize,
										LONGLONG *pllActualRetainedSize);
	STDMETHODIMP ConvertToTemporaryRecording(LPCOLESTR pszFileName);
	STDMETHODIMP SetRecordingPath(LPCOLESTR pszPath);
	STDMETHODIMP GetRecordingPath(LPOLESTR *ppszPath);
	STDMETHODIMP GetBoundToLiveToken(/* [out] */ LPOLESTR *ppszToken);
	STDMETHODIMP GetCurrentPosition(/* [out] */ LONGLONG *phyCurrentPosition);

	// IXDSCodec
	STDMETHODIMP get_XDSToRatObjOK(/* [retval][out] */ HRESULT *pHrCoCreateRetVal);
    STDMETHODIMP put_CCSubstreamService(/* [in] */ long SubstreamMask);
    STDMETHODIMP get_CCSubstreamService(/* [retval][out] */ long *pSubstreamMask);
    STDMETHODIMP GetContentAdvisoryRating(
            /* [out] */ PackedTvRating *pRat,
            /* [out] */ long *pPktSeqID,
            /* [out] */ long *pCallSeqID,
            /* [out] */ REFERENCE_TIME *pTimeStart,
            /* [out] */ REFERENCE_TIME *pTimeEnd);
   STDMETHODIMP GetXDSPacket(
            /* [out] */ long *pXDSClassPkt,
            /* [out] */ long *pXDSTypePkt,
            /* [out] */ BSTR *pBstrXDSPkt,
            /* [out] */ long *pPktSeqID,
            /* [out] */ long *pCallSeqID,
            /* [out] */ REFERENCE_TIME *pTimeStart,
            /* [out] */ REFERENCE_TIME *pTimeEnd);

	// IContentRestrictionCapture:
	STDMETHODIMP SetEncryptionEnabled(/* [in] */ BOOL fEncrypt);
	STDMETHODIMP GetEncryptionEnabled(/* [out] */ BOOL *pfEncrypt);
	STDMETHODIMP ExpireVBICopyProtection(/* [in] */ ULONG uExpiredPolicy);

	// So that the pipeline components can do computations based on
	// clock vs stream time:
	REFERENCE_TIME GetStreamBase() { return m_tStart; }

private:
	CCritSec					m_cStateChangeLock;
	std::vector<CDVRInputPin*>	m_rgPins;
	ICapturePipelineManager*	m_piPipelineManager;

	friend class CDVRInputPin;
	CDVRSinkFilter(const CDVRSinkFilter&);					// not implemented
	CDVRSinkFilter& operator = (const CDVRSinkFilter&);		// not implemented

	// Function to retrieve an interface implemented by a pipeline component.
	// Iff no component implements riid, then NULL is returned.
	template<class T> CComPtr<T> GetComponentInterface(REFIID riid = __uuidof(T));
};

//////////////////////////////////////////////////////////////////////////////
//	Input pin for sink filter.												//
//////////////////////////////////////////////////////////////////////////////

class CDVRInputPin :	public CBaseInputPin, public IKsPropertySet
{
	// Private constructor for use by the filter only
	CDVRInputPin(	CDVRSinkFilter& rcFilter,
					CCritSec& rcLock,
					LPCWSTR wzPinName,
					DWORD dwPinNum);
	~CDVRInputPin();

public:
	DECLARE_IUNKNOWN;

	// Override
	STDMETHOD(NonDelegatingQueryInterface) (REFIID riid, LPVOID* ppv);

	// Owner filter access
	CDVRSinkFilter& GetFilter();

	// Override
	virtual HRESULT CheckMediaType(const CMediaType* pmt);

	// Override
	virtual HRESULT CompleteConnect(IPin* pReceivePin);

	// Override
	STDMETHOD(Receive)(IMediaSample* pSample);

	// Override
	STDMETHOD(ReceiveCanBlock)();

	// Override
	STDMETHOD(BeginFlush)();

	// Override
	STDMETHOD(EndFlush)();

	// Override
	STDMETHOD(NewSegment)(REFERENCE_TIME rtStart, REFERENCE_TIME rtEnd, double dblRate);


	// Override
	STDMETHOD(GetAllocator)(IMemAllocator **ppAllocator);

	// Override
	STDMETHOD(GetAllocatorRequirements)(ALLOCATOR_PROPERTIES* pProps);

	// Override
	STDMETHOD(NotifyAllocator)(IMemAllocator* pAllocator, BOOL bReadOnly);

	// Allows components to identify the pin passed to ProcessInputSample
	DWORD GetPinNum() { return m_dwPinNum; }

	// IKsPropertySet:
	STDMETHOD(Set)(
            /* [in] */ REFGUID guidPropSet,
            /* [in] */ DWORD dwPropID,
            /* [size_is][in] */ LPVOID pInstanceData,
            /* [in] */ DWORD cbInstanceData,
            /* [size_is][in] */ LPVOID pPropData,
            /* [in] */ DWORD cbPropData);

   STDMETHOD(Get)(
            /* [in] */ REFGUID guidPropSet,
            /* [in] */ DWORD dwPropID,
            /* [size_is][in] */ LPVOID pInstanceData,
            /* [in] */ DWORD cbInstanceData,
            /* [size_is][out] */ LPVOID pPropData,
            /* [in] */ DWORD cbPropData,
            /* [out] */ DWORD *pcbReturned);

  STDMETHOD(QuerySupported)(
            /* [in] */ REFGUID guidPropSet,
            /* [in] */ DWORD dwPropID,
            /* [out] */ DWORD *pTypeSupport);

private:
	HRESULT						m_hrConstruction;
	DWORD						m_dwPinNum;
	CDVRAllocator *				m_pPrivateAllocator;

	// Function to retrieve an interface implemented by a pipeline component.
	// Iff no component implements riid, then NULL is returned.
	template<class T> CComPtr<T> GetComponentInterface(REFIID riid = __uuidof(T));

	friend class CDVRSinkFilter;
	friend class CDVRInputPin;
	CDVRInputPin(const CDVRInputPin&);						// not implemented
	CDVRInputPin& operator = (const CDVRInputPin&);			// not implemented

	
};

}
