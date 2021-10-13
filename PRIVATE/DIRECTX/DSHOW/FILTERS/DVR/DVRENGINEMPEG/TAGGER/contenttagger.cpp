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
#include "stdafx.h"
#include "ContentTagger.h"
#include "..\\DVREngine.h"
#include "..\\SampleProducer\\CPinMapping.h"
#include "..\\Plumbing\\Sink.h"
#include "..\\HResultError.h"
#include "..\\DVREngineTestHooks.h"

using namespace MSDvr;

#define OLESTRLEN(x)        wcslen(x)
#define OLESTRCMP(s1,s2)    wcscmp(s1,s2)

ULONG CContentTagger::s_uVBICopyProtectionEventMask =
	 (1 << EVENT_FIELD_OFFSET_CGMSA_PRESENT) |
	 (SAMPLE_FIELD_MASK_CGMSA << EVENT_FIELD_OFFSET_CGMSA) |
	 (SAMPLE_FIELD_MASK_MACROVISION << EVENT_FIELD_OFFSET_VBI_MACROVISION);
ULONG CContentTagger::s_uCopyProtectionEventMask =
	 (1 << EVENT_FIELD_OFFSET_CGMSA_PRESENT) |
	 (SAMPLE_FIELD_MASK_CGMSA << EVENT_FIELD_OFFSET_CGMSA) |
	 (SAMPLE_FIELD_MASK_MACROVISION << EVENT_FIELD_OFFSET_VBI_MACROVISION) |
	 (1 << EVENT_FIELD_OFFSET_KS_MACROVISION_PRESENT) |
	 (SAMPLE_FIELD_MASK_MACROVISION << EVENT_FIELD_OFFSET_KS_MACROVISION);

///////////////////////////////////////////////////////////////////////
//
//  Class CContentTaggerInputPinHelper -- helper class for exposing
//         IKsPropertySet on the input pin
//
///////////////////////////////////////////////////////////////////////

CContentTaggerInputPinHelper::CContentTaggerInputPinHelper(
			CDVRInputPin &rcDVRInputPin, CContentTagger &rcContentTagger)
	: m_rcDVRInputPin(rcDVRInputPin)
	, m_piKsPropertySetNext(0)
	, m_rcContentTagger(rcContentTagger)
{
} // CContentTaggerInputPinHelper::CContentTaggerInputPinHelper

CContentTaggerInputPinHelper::~CContentTaggerInputPinHelper()
{
} // CContentTaggerInputPinHelper::~CContentTaggerInputPinHelper

// IKsPropertySet:
STDMETHODIMP CContentTaggerInputPinHelper::Set(
        /* [in] */ REFGUID guidPropSet,
        /* [in] */ DWORD dwPropID,
        /* [size_is][in] */ LPVOID pInstanceData,
        /* [in] */ DWORD cbInstanceData,
        /* [size_is][in] */ LPVOID pPropData,
        /* [in] */ DWORD cbPropData)
{
	HRESULT hr = E_PROP_SET_UNSUPPORTED;

	if (IsEqualGUID(guidPropSet, KSPROPSETID_CopyProt) &&
		(dwPropID == KSPROPERTY_COPY_MACROVISION))
	{
		if (cbPropData < sizeof(KS_COPY_MACROVISION))
		{
			hr = E_INVALIDARG;
		}
		else
		{
			hr = S_OK;
			KS_COPY_MACROVISION *pKsCopyMacrovision = static_cast<KS_COPY_MACROVISION *>(pPropData);

			DbgLog((LOG_COPY_PROTECTION, 3, _T("ContentTagger::Set() -- macrovision property set to %d\n"),
				(int) pKsCopyMacrovision->MACROVISIONLevel ));

			CAutoLock(&m_rcContentTagger.m_cCritSecState);

			switch (pKsCopyMacrovision->MACROVISIONLevel)
			{
			case KS_MACROVISION_DISABLED:
				m_rcContentTagger.SetMacrovisionPropertyPolicy(Macrovision_Disabled);
				break;

			case KS_MACROVISION_LEVEL1:
				m_rcContentTagger.SetMacrovisionPropertyPolicy(Macrovision_CopyNever_AGC_01);
				break;

			case KS_MACROVISION_LEVEL2:
				m_rcContentTagger.SetMacrovisionPropertyPolicy(Macrovision_CopyNever_AGC_ColorStripe2);
				break;

			case KS_MACROVISION_LEVEL3:
				m_rcContentTagger.SetMacrovisionPropertyPolicy(Macrovision_CopyNever_AGC_ColorStripe4);
				break;

			default:
				hr = E_INVALIDARG;
				break;
			}
		}
	}
	else
	{
		if (m_piKsPropertySetNext)
			hr = m_piKsPropertySetNext->Set(guidPropSet, dwPropID, pInstanceData,
					cbInstanceData, pPropData, cbPropData);
		if (FAILED(hr))
		{
			if (IsEqualGUID(guidPropSet, KSPROPSETID_CopyProt))
				hr = E_PROP_ID_UNSUPPORTED;
			else if (hr != E_PROP_ID_UNSUPPORTED)
				hr = E_PROP_SET_UNSUPPORTED;
		}
	}
	return hr;
} // CContentTaggerInputPinHelper::Set

STDMETHODIMP CContentTaggerInputPinHelper::Get(
        /* [in] */ REFGUID guidPropSet,
        /* [in] */ DWORD dwPropID,
        /* [size_is][in] */ LPVOID pInstanceData,
        /* [in] */ DWORD cbInstanceData,
        /* [size_is][out] */ LPVOID pPropData,
        /* [in] */ DWORD cbPropData,
        /* [out] */ DWORD *pcbReturned)
{
	HRESULT hr = E_PROP_SET_UNSUPPORTED;

	if (IsEqualGUID(guidPropSet, KSPROPSETID_CopyProt) &&
		(dwPropID == KSPROPERTY_COPY_MACROVISION))
	{
		if (cbPropData < sizeof(KS_COPY_MACROVISION))
			hr = E_INVALIDARG;
		else
		{
			hr = S_OK;
			*pcbReturned  = sizeof(KS_COPY_MACROVISION);
			KS_COPY_MACROVISION *pKsCopyMacrovision = static_cast<KS_COPY_MACROVISION *>(pPropData);
			CAutoLock(&m_rcContentTagger.m_cCritSecState);

			switch (m_rcContentTagger.GetMacrovisionPropertyPolicy())
			{
			case Macrovision_NotApplicable:
			case Macrovision_Disabled:
				pKsCopyMacrovision->MACROVISIONLevel = KS_MACROVISION_DISABLED;
				break;

			case Macrovision_CopyNever_AGC_01:
				pKsCopyMacrovision->MACROVISIONLevel = KS_MACROVISION_LEVEL1;
				break;

			case Macrovision_CopyNever_AGC_ColorStripe2:
				pKsCopyMacrovision->MACROVISIONLevel = KS_MACROVISION_LEVEL2;
				break;

			case Macrovision_CopyNever_AGC_ColorStripe4:
				pKsCopyMacrovision->MACROVISIONLevel = KS_MACROVISION_LEVEL3;
				break;

			default:
				ASSERT(0);
				pKsCopyMacrovision->MACROVISIONLevel = KS_MACROVISION_DISABLED;
				break;
			}
		}
	}
	else
	{
		if (m_piKsPropertySetNext)
			hr = m_piKsPropertySetNext->Get(guidPropSet, dwPropID, pInstanceData,
					cbInstanceData, pPropData, cbPropData, pcbReturned);
		if (FAILED(hr))
		{
			if (IsEqualGUID(guidPropSet, KSPROPSETID_CopyProt))
				hr = E_PROP_ID_UNSUPPORTED;
			else if (hr != E_PROP_ID_UNSUPPORTED)
				hr = E_PROP_SET_UNSUPPORTED;
		}
	}
return hr;
} // CContentTaggerInputPinHelper::Get

STDMETHODIMP CContentTaggerInputPinHelper::QuerySupported(
        /* [in] */ REFGUID guidPropSet,
        /* [in] */ DWORD dwPropID,
        /* [out] */ DWORD *pTypeSupport)
{
	HRESULT hr = E_PROP_SET_UNSUPPORTED;

	if (!pTypeSupport)
		hr = E_POINTER;
	else
	{
		*pTypeSupport = 0;
		if (IsEqualGUID(guidPropSet, KSPROPSETID_CopyProt) &&
			(dwPropID == KSPROPERTY_COPY_MACROVISION))
		{
			*pTypeSupport = KSPROPERTY_SUPPORT_SET | KSPROPERTY_SUPPORT_GET;
			hr = S_OK;
		}
		else
		{
			if (m_piKsPropertySetNext)
				hr = m_piKsPropertySetNext->QuerySupported(guidPropSet, dwPropID, pTypeSupport);
			if (FAILED(hr))
			{
				if (IsEqualGUID(guidPropSet, KSPROPSETID_CopyProt))
					hr = E_PROP_ID_UNSUPPORTED;
				else if (hr != E_PROP_ID_UNSUPPORTED)
					hr = E_PROP_SET_UNSUPPORTED;
			}
		}
	}
	return hr;
} // CContentTaggerInputPinHelper::QuerySupported

///////////////////////////////////////////////////////////////////////
//
//  Class CContentTagger -- constructor and destructor
//
///////////////////////////////////////////////////////////////////////

CContentTagger::CContentTagger(void)
	: m_piSampleProducer(0)
	, m_pippmgr(0)
	, m_piXDSCodecNext(0)
	, m_piContentRestrictionCaptureNext(0)
	, m_pcDVRInputPinVBI(0)
	, m_cCritSecState()
	, m_fConfiguredPins(false)
	, m_vectorCContentTaggerInputPinHelper()
	, m_eCGMSAPolicy(CGMS_A_NotApplicable)
	, m_eMacrovisionPolicyVBI(Macrovision_NotApplicable)
	, m_eMacrovisionPolicyProperty(Macrovision_NotApplicable)
	, m_eCGMSAPolicyCurrent(CGMS_A_NotApplicable)
	, m_eMacrovisionPolicyVBICurrent(Macrovision_NotApplicable)
	, m_eMacrovisionPolicyPropertyCurrent(Macrovision_NotApplicable)
	, m_fUsingTestHookOverride(false)
	, m_uTestHookOverrideVersion(0)
	, m_piMediaEventSink(0)
	, m_fEncryptSamples(FALSE)
	, m_rtStartFlags(0)
	, m_rtEndFlags((UINT64)0x4000000000000000)
	, m_cXDSCodecHelper(this)
	, m_fSawFlush(false)
	, m_fSawNewSegment(false)
	, m_fSawSample(false)

{
	DbgLog((LOG_ENGINE_OTHER, 2, _T("CContentTagger: constructed %p\n"), this));
} // CContentTagger::CContentTagger

CContentTagger::~CContentTagger(void)
{
	DbgLog((LOG_ENGINE_OTHER, 2, _T("CContentTagger: destroying %p\n"), this));

	{
		CAutoLock cAutoLock(&m_cCritSecState);

		Cleanup();
	}
} // CContentTagger::~CContentTagger

///////////////////////////////////////////////////////////////////////
//
//  Class CContentTagger -- IXDSCodecCallbacks
//
///////////////////////////////////////////////////////////////////////

void CContentTagger::OnNewXDSPacket()
{
	DbgLog((LOG_EVENT_DETECTED, 3, _T("CContentTagger(%p)::OnNewXDSPacket()\n"), this));
	HRESULT hr;

	if (m_piMediaEventSink)
	{
		hr = m_piMediaEventSink->Notify(DVRENGINE_XDSCODEC_NEWXDSPACKET, 0, 0);
		ASSERT(SUCCEEDED(hr));
	}

	// We need to obtain info on the new packet and decide whether it supplies a
	// new CGMS-A copy protection policy:

	long lXDSClassPkt, lXDSTypePkt, lPktSeqID, lCallSeqID;
	REFERENCE_TIME rtStart, rtEnd;
    CComBSTR bstrPktData;
	hr = m_cXDSCodecHelper.GetXDSPacket(&lXDSClassPkt, &lXDSTypePkt, &bstrPktData,
										&lPktSeqID, &lCallSeqID, &rtStart, &rtEnd);
	if (SUCCEEDED(hr) &&
		((lXDSTypePkt == XDS_1Type_CGMS_A) && (lXDSClassPkt == XDS_Cls_Current_Start)) &&
		(bstrPktData.Length() >= 2))
	{
		CAutoLock cAutoLock(&m_cCritSecState);

		DbgLog((LOG_EVENT_DETECTED, 3, _T("CContentTagger(%p)::OnNewXDSPacket() -- received CGMS-A data\n"), this));
		LONGLONG rtOrigStartFlags = m_rtStartFlags;

		// CGMS-A packet is only 2 bytes long (ignoring start and stop byte pairs...)
		//
		// Byte 1:
		//		Bit 0:  Analog source bit
		//      Bits 2 & 1:  Macrovision bits:  (0,0) off; (0,1) On; (1,0) 2 line; (1,1) 4 line
		//                  (applicable only if CGMS-A bits are (1,1)
		//		Bits 4 & 3:  CGMS-A bits:  (0,0) freely; (0,1) reserved; (1,0) 1 copy; (1,1) no copies

		switch (bstrPktData[0] & (1 << 4))  // bit 4
		{
		case 0:
			switch (bstrPktData[0] & (1 << 3))  // bit 3
			{
			case 0:
				// CGMS-A is (0,0) -- copy freely
				SetCGMSAPolicy(CGMS_A_CopyFreely, Macrovision_NotApplicable);
				break;

			default:
				// CGMS-A is (0,1) -- reserved
				SetCGMSAPolicy(CGMS_A_Reserved, Macrovision_NotApplicable);
				break;
			}
			break;

		default:
			switch (bstrPktData[0] & (1 << 3))  // bit 3
			{
			case 0:
				// CGMS-A is (1,0) -- copy once

				SetCGMSAPolicy(CGMS_A_CopyOnce, Macrovision_NotApplicable);
				break;

			default:
				// CGMS-A is (1,0) -- copy never.  The Macrovision bits
				// (aka APS bits) are applicable:


				switch (bstrPktData[0] & (1 << 2))  // bit 2
				{
				case 0:
					switch (bstrPktData[0] & (1 << 1))  // bit 1
					{
					case 0:
						// Macrovision bits are (0,0) -- disabled
						SetCGMSAPolicy(CGMS_A_CopyNever, Macrovision_Disabled);
						break;
					default:
						// Macrovision bits are (0,1) -- On with no split burst
						SetCGMSAPolicy(CGMS_A_CopyNever, Macrovision_CopyNever_AGC_01);
						break;
					}
					break;

				default:
					switch (bstrPktData[0] & (1 << 1))  // bit 1
					{
					case 0:
						// Macrovision bits are (1,0) -- On with 2 line split burst
						SetCGMSAPolicy(CGMS_A_CopyNever, Macrovision_CopyNever_AGC_ColorStripe2);
						break;
					
					default:
						// Macrovision bits are (1,1) -- On with 4 line split burst
						SetCGMSAPolicy(CGMS_A_CopyNever, Macrovision_CopyNever_AGC_ColorStripe4);
						break;
					}
					break;
				}  // end switch on high order Macrovision bit
				break;  // end CGMS-A low order bit is 1
			}
			break;  // end CGMS-A high order bit is 1
		}  // end switch on CGMS-A high order bit

		if (rtOrigStartFlags == m_rtStartFlags)
		{
			// We have not modified the copy protection policy. We need to issue an
			// event reporting the lack of modification:

			NotifyNoCopyProtectionChange();
		}
	}  // end have CGMS-A packet
} // CContentTagger::OnNewXDSPacket

void CContentTagger::OnNewXDSRating()
{
	// LATER:  when we start supporting ratings, get the rating and start
	//                  tagging with it.

	DbgLog((LOG_EVENT_DETECTED, 3, _T("CContentTagger(%p)::OnNewXDSRating()\n"), this));

	if (m_piMediaEventSink)
	{
		HRESULT hr = m_piMediaEventSink->Notify(DVRENGINE_XDSCODEC_NEWXDSRATING, 0, 0);
		ASSERT(SUCCEEDED(hr));
	}
} // CContentTagger::OnNewXDSRating

void CContentTagger::OnDuplicateXDSRating()
{
	// LATER:  when we start supporting ratings, get the rating and start
	//                  tagging with it.

	DbgLog((LOG_EVENT_DETECTED, 3, _T("CContentTagger(%p)::OnDuplicateXDSRating()\n"), this));

	if (m_piMediaEventSink)
	{
		HRESULT hr = m_piMediaEventSink->Notify(DVRENGINE_XDSCODEC_DUPLICATEXDSRATING, 0, 0);
		ASSERT(SUCCEEDED(hr));
	}
} // CContentTagger::OnDuplicateXDSRating

///////////////////////////////////////////////////////////////////////
//
//  Class CContentTagger -- IXDSCodec
//
///////////////////////////////////////////////////////////////////////

STDMETHODIMP CContentTagger::get_XDSToRatObjOK(/* [retval][out] */ HRESULT *pHrCoCreateRetVal)
{
	HRESULT hr = E_NOTIMPL;

	DbgLog((LOG_USER_REQUEST, 3, _T("CContentTagger(%p)::get_XDSToRatObjOK(%p)\n"), this, pHrCoCreateRetVal));

	if (!pHrCoCreateRetVal)
		hr = E_POINTER;
	else if (m_piXDSCodecNext)
		hr = m_piXDSCodecNext->get_XDSToRatObjOK(pHrCoCreateRetVal);

	if (FAILED(hr))
	{
		DbgLog((LOG_ERROR, 2, _T("CContentTagger(%p)::get_XDSToRatObjOK(() failed with status %d\n"),
			this, hr));
	}
	return hr;
} // CContentTagger::get_XDSToRatObjOK

STDMETHODIMP CContentTagger::put_CCSubstreamService(/* [in] */ long SubstreamMask)
{
	HRESULT hr = E_NOTIMPL;

	DbgLog((LOG_USER_REQUEST, 3, _T("CContentTagger(%p)::put_CCSubstreamService(%ld)\n"), this, SubstreamMask));

	if (m_piXDSCodecNext)
		hr = m_piXDSCodecNext->put_CCSubstreamService(SubstreamMask);

	if (FAILED(hr))
	{
		DbgLog((LOG_ERROR, 2, _T("CContentTagger(%p)::put_CCSubstreamService(() failed with status %d\n"),
			this, hr));
	}
	return hr;
} // CContentTagger::put_CCSubstreamService

STDMETHODIMP CContentTagger::get_CCSubstreamService(/* [retval][out] */ long *pSubstreamMask)
{
	HRESULT hr = E_NOTIMPL;

	DbgLog((LOG_USER_REQUEST, 3, _T("CContentTagger(%p)::get_CCSubstreamService(%p)\n"), this, pSubstreamMask));

	if (!pSubstreamMask)
		hr = E_POINTER;
	else if (m_piXDSCodecNext)
		hr = m_piXDSCodecNext->get_CCSubstreamService(pSubstreamMask);

	if (FAILED(hr))
	{
		DbgLog((LOG_ERROR, 2, _T("CContentTagger(%p)::get_CCSubstreamService(() failed with status %d\n"),
			this, hr));
	}
	return hr;
} // CContentTagger::get_CCSubstreamService

STDMETHODIMP CContentTagger::GetContentAdvisoryRating(
            /* [out] */ PackedTvRating *pRat,
            /* [out] */ long *pPktSeqID,
            /* [out] */ long *pCallSeqID,
            /* [out] */ REFERENCE_TIME *pTimeStart,
            /* [out] */ REFERENCE_TIME *pTimeEnd)
{
	HRESULT hr = E_NOTIMPL;

	DbgLog((LOG_USER_REQUEST, 3, _T("CContentTagger(%p)::GetContentAdvisoryRating(%p, %p, %p, %p, %p)\n"),
		this, pRat, pPktSeqID, pCallSeqID, pTimeStart, pTimeEnd));

	if (m_piXDSCodecNext)
		hr = m_piXDSCodecNext->GetContentAdvisoryRating(pRat, pPktSeqID, pCallSeqID, pTimeStart, pTimeEnd);

	if (FAILED(hr))
	{
		DbgLog((LOG_ERROR, 2, _T("CContentTagger(%p)::GetContentAdvisoryRating(() failed with status %d\n"),
			this, hr));
	}
	return hr;
} // CContentTagger::GetContentAdvisoryRating

STDMETHODIMP CContentTagger::GetXDSPacket(
            /* [out] */ long *pXDSClassPkt,
            /* [out] */ long *pXDSTypePkt,
            /* [out] */ BSTR *pBstrXDSPkt,
            /* [out] */ long *pPktSeqID,
            /* [out] */ long *pCallSeqID,
            /* [out] */ REFERENCE_TIME *pTimeStart,
            /* [out] */ REFERENCE_TIME *pTimeEnd)
{
	HRESULT hr = E_NOTIMPL;

	DbgLog((LOG_USER_REQUEST, 3, _T("CContentTagger(%p)::GetXDSPacket(%p, %p, %p, %p, %p, %p, %p)\n"),
		this, pXDSClassPkt, pXDSTypePkt, pBstrXDSPkt,
				pPktSeqID, pCallSeqID, pTimeStart, pTimeEnd));

	hr = m_cXDSCodecHelper.GetXDSPacket(pXDSClassPkt, pXDSTypePkt, pBstrXDSPkt,
					pPktSeqID, pCallSeqID, pTimeStart, pTimeEnd);

	if (FAILED(hr))
	{
		DbgLog((LOG_ERROR, 2, _T("CContentTagger(%p)::GetXDSPacket(() failed with status %d\n"),
			this, hr));
	}
	return hr;
} // CContentTagger::GetXDSPacket

///////////////////////////////////////////////////////////////////////
//
//  Class CContentTagger -- IPipelineComponent
//
///////////////////////////////////////////////////////////////////////

void CContentTagger::RemoveFromPipeline()
{
	DbgLog((LOG_ENGINE_OTHER, 3, _T("CContentTagger(%p)::RemoveFromPipeline\n"), this));

	CAutoLock cAutoLock(&m_cCritSecState);

	Cleanup();
} // CContentTagger::RemoveFromPipeline

ROUTE CContentTagger::ConfigurePipeline(
	UCHAR iNumPins,
	CMediaType cMediaTypes[],
	UINT iSizeCustom,
	BYTE Custom[])
{
	DbgLog((LOG_ENGINE_OTHER, 3, _T("CContentTagger(%p)::ConfigurePipeline()\n"), this));

	// This is the point that we grab references to other pipeline and
	// filter components -- the pipeline has been fully configured and we're
	// still safely running on an app thread. Note that the pins, however,
	// may not yet be fully configured so we need to wait until later if
	// this is the first call to ConfigurePipeline();
	//
	CAutoLock cAutoLock(&m_cCritSecState);

	ISampleProducer *piSampleProducer = GetSampleProducer();
	if (!piSampleProducer)
	{
		DbgLog((LOG_ERROR, 2, _T("CContentTagger(%p)::ConfigurePipeline() -- no sample producer\n"), this));
		throw CHResultError(E_FAIL, "No sample producer found");
	}

	if (m_fConfiguredPins)
		ComputeVBIPin();
	else
	{
		// Export IKsPropertySet on the pins in order to be given Macrovision settings:

		UCHAR bPinIndex;
		for (bPinIndex = 0; bPinIndex < iNumPins; ++bPinIndex)
		{
			CDVRSinkFilter &rcDVRSinkFilter = m_pippmgr->GetSinkFilter();
			CDVRInputPin *pcDVRInputPin = static_cast<CDVRInputPin *>(rcDVRSinkFilter.GetPin(bPinIndex));
			ASSERT(pcDVRInputPin);
			CContentTaggerInputPinHelper *pcContentTaggerInputPinHelper =
				new CContentTaggerInputPinHelper(*pcDVRInputPin, *this);
			m_vectorCContentTaggerInputPinHelper.push_back(pcContentTaggerInputPinHelper);
			pcContentTaggerInputPinHelper->m_piKsPropertySetNext = (IKsPropertySet *)
				m_pippmgr->RegisterCOMInterface(static_cast<TRegisterableCOMInterface<IKsPropertySet> &>(*pcContentTaggerInputPinHelper),
								*pcDVRInputPin);
		}
	}

	return HANDLED_CONTINUE;
} // CContentTagger::ConfigurePipeline

ROUTE CContentTagger::NotifyFilterStateChange(
						// State being put into effect
						FILTER_STATE	eState)
{
	if ((eState != State_Stopped) && !m_fConfiguredPins)
		ComputeVBIPin();
	SyncPolicyWithTestHooks();

	CComPtr<IReferenceClock> piReferenceClock;
	HRESULT hr = m_pippmgr->GetFilter().GetSyncSource(&piReferenceClock);
	ASSERT(SUCCEEDED(hr));  // use NULL if the call fails

	m_cXDSCodecHelper.CacheClock(piReferenceClock);

	switch (eState)
	{
	case State_Running:
		m_cXDSCodecHelper.OnRun(m_pippmgr->GetSinkFilter().GetStreamBase());
		break;

	case State_Paused:
		m_cXDSCodecHelper.OnPause();
		break;

	case State_Stopped:
		m_cXDSCodecHelper.OnStop();
		break;
	}
	return HANDLED_CONTINUE;
} // CContentTagger::NotifyFilterStateChange

///////////////////////////////////////////////////////////////////////
//
//  Class CContentTagger -- ICapturePipelineComponent
//
///////////////////////////////////////////////////////////////////////

unsigned char CContentTagger::AddToPipeline(
	ICapturePipelineManager &rcManager)
{
	DbgLog((LOG_ENGINE_OTHER, 3, _T("CContentTagger(%p)::AddToPipeline()\n"), this));
	CAutoLock cAutoLock(&m_cCritSecState);

	IPipelineManager *rpManager = (IPipelineManager *) &rcManager;
	m_pippmgr = &rcManager;
	m_cPipelineRouterForEvents = m_pippmgr->GetRouter(this, false, true);
	m_piXDSCodecNext = (IXDSCodec *)
		rpManager->RegisterCOMInterface(static_cast<TRegisterableCOMInterface<IXDSCodec> &>(*this));
	m_piContentRestrictionCaptureNext = (IContentRestrictionCapture *)
		rpManager->RegisterCOMInterface(static_cast<TRegisterableCOMInterface<IContentRestrictionCapture> &>(*this));
	IFilterGraph *piFilterGraph = rcManager.GetFilter().GetFilterGraph();
	if (piFilterGraph)
	{
		HRESULT hr = piFilterGraph->QueryInterface(IID_IMediaEventSink, (void **) &m_piMediaEventSink);
		if (SUCCEEDED(hr) && m_piMediaEventSink)
		{
			// We don't want to hold onto the ref because if we do, we create a circular
			// reference chain that keeps the DVR sink filter alive forever.

			m_piMediaEventSink->Release();
		}
	}

	return 0;	// no streaming thread
} // CContentTagger::AddToPipeline

ROUTE CContentTagger::NotifyEndFlush(
						// @parm Receiving pin
						CDVRInputPin&	rcInputPin)
{
	m_fSawFlush = true;
	return HANDLED_CONTINUE;
} // CContentTagger::NotifyEndFlush

ROUTE CContentTagger::NotifyNewSegment(
						CDVRInputPin&	rcInputPin,
						REFERENCE_TIME rtStart,
						REFERENCE_TIME rtEnd,
						double dblRate)
{
	if (m_fSawSample)
		m_fSawNewSegment = true;
	return HANDLED_CONTINUE;
} // CContentTagger::NotifyNewSegment

ROUTE CContentTagger::ProcessInputSample(
	IMediaSample &riSample,
	CDVRInputPin &rcInputPin)
{
	ROUTE eRoute = HANDLED_CONTINUE;
	CAutoLock cAutoLock(&m_cCritSecState);
	SyncPolicyWithTestHooks();

	if (m_pcDVRInputPinVBI == &rcInputPin)
	{

		// This is VBI data. This tagger will parse the data and
		// block the sample from being sent downstream.

		HRESULT hrVBI = m_cXDSCodecHelper.Process(&riSample);
		if (FAILED(hrVBI))
		{
			DbgLog((LOG_ERROR, 3, _T("CContentTagger(%p)::ProcessInputSample(): VBI data parsing failed with error %d\n"),
				this, hrVBI));
		}
		eRoute = HANDLED_STOP;
	}
	else
	{
		m_fSawSample = true;

		REFERENCE_TIME startTime = m_rtStartFlags;
		REFERENCE_TIME endTime = m_rtEndFlags;
		HRESULT hr = riSample.SetTime(&startTime, &endTime);
		if (FAILED(hr))
		{
			DbgLog((LOG_ERROR, 3, _T("CContentTagger(%p)::ProcessInputSample(): SetTime() failed with error %d\n"),
				this, hr));
		}
		if (m_pippmgr)
		{
			// Re-dispatch to get correct interleaving of notifications versus samples:

			m_pippmgr->GetRouter(this, false, true).ProcessInputSample(riSample, rcInputPin);
			eRoute = HANDLED_STOP;
		}

		if (m_fSawFlush)
		{
			m_fSawFlush = false;
			if (m_piSampleProducer &&
				m_piSampleProducer->IndicatesFlushWasTune(riSample, rcInputPin))
			{
				m_cXDSCodecHelper.DoTuneChanged();
				DVREngine_ExpireVBICopyProtectionInput();
				SetCGMSAPolicy(CGMS_A_NotApplicable, Macrovision_NotApplicable);
				SetMacrovisionPolicy();
			}
		}
		if (m_fSawNewSegment)
		{
			m_fSawNewSegment = false;
			if (m_piSampleProducer &&
				m_piSampleProducer->IndicatesNewSegmentWasTune(riSample, rcInputPin))
			{
				m_cXDSCodecHelper.DoTuneChanged();
				DVREngine_ExpireVBICopyProtectionInput();
				SetCGMSAPolicy(CGMS_A_NotApplicable, Macrovision_NotApplicable);
				SetMacrovisionPolicy();
			}
		}
	}

	// VBI never goes downstream.  Other stuff does with the newly stamped sample.

	return eRoute;
} // CContentTagger::ProcessInputSample

ROUTE CContentTagger::DispatchExtension(
						// @parm The extension to be dispatched.
						CExtendedRequest &rcExtendedRequest)
{
	// We need to re-dispatch so that this is queued correctly wrt
	// our samples vs notifications.

	if (m_pippmgr)
	{
		m_pippmgr->GetRouter(this, false, true).DispatchExtension(rcExtendedRequest);
		return HANDLED_STOP;
	}
	return UNHANDLED_CONTINUE;
} // CContentTagger::DispatchExtension

///////////////////////////////////////////////////////////////////////
//
//  Class CContentTagger -- IContentRestrictionCapture
//
///////////////////////////////////////////////////////////////////////

STDMETHODIMP CContentTagger::SetEncryptionEnabled(/* [in] */ BOOL fEncrypt)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("CContentTagger(%p)::SetEncryptionEnabled(%u)\n"),
		this, (unsigned) fEncrypt));
	CAutoLock cAutoLock(&m_cCritSecState);

	m_fEncryptSamples = fEncrypt;
#ifdef DVR_ENGINE_FEATURE_IN_PROGRESS
	if (fEncrypt)
		m_rtStartFlags |= (((LONGLONG)1) << SAMPLE_FIELD_OFFSET_KS_SHOULD_BE_ENCRYPTED);
	else
		m_rtStartFlags &= ~(((LONGLONG)1) << SAMPLE_FIELD_OFFSET_KS_SHOULD_BE_ENCRYPTED);
#else DVR_ENGINE_FEATURE_IN_PROGRESS
	if (fEncrypt)
		m_rtStartFlags |= (((LONGLONG)1) << SAMPLE_FIELD_OFFSET_KS_ENCRYPTED);
	else
		m_rtStartFlags &= ~(((LONGLONG)1) << SAMPLE_FIELD_OFFSET_KS_ENCRYPTED);
#endif // DVR_ENGINE_FEATURE_IN_PROGRESS
	HRESULT hr = S_OK;
	if (m_piContentRestrictionCaptureNext)
	{
		hr = m_piContentRestrictionCaptureNext->SetEncryptionEnabled(fEncrypt);
		if (hr == E_NOTIMPL)
			hr = S_OK;
		// To do:  back out of the change
	}

	return hr;
} // CContentTagger::SetEncryptionEnabled

STDMETHODIMP CContentTagger::GetEncryptionEnabled(/* [out] */ BOOL *pfEncrypt)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("CContentTagger(%p)::GetEncryptionEnabled(%p)\n"), this, pfEncrypt));

	HRESULT hr = S_OK;
	CAutoLock cAutoLock(&m_cCritSecState);

	if (!pfEncrypt)
		hr = E_POINTER;
	else
		*pfEncrypt = m_fEncryptSamples ? TRUE : FALSE;
	return hr;
} // CContentTagger::GetEncryptionEnabled

STDMETHODIMP CContentTagger::ExpireVBICopyProtection(/*[in] */ULONG uExpiredPolicy)
{
	DbgLog((LOG_USER_REQUEST, 3, _T("CContentTagger(%p)::ExpireVBICopyProtection(%u)\n"), this, uExpiredPolicy));

	HRESULT hr = S_OK;

	CAutoLock cAutoLock(&m_cCritSecState);

	SyncPolicyWithTestHooks();

	ULONG uOldFlags = (ULONG) ((m_rtStartFlags >> SAMPLE_FIELD_OFFSET_CGMSA_PRESENT) & 0xff);
	uOldFlags &= s_uVBICopyProtectionEventMask;
	uExpiredPolicy &= s_uVBICopyProtectionEventMask;
	if (uOldFlags != uExpiredPolicy)
		hr = XACT_E_WRONGSTATE;
	else
	{
		DVREngine_ExpireVBICopyProtectionInput();
		if (m_fUsingTestHookOverride && (m_uTestHookOverrideVersion > 0))
			--m_uTestHookOverrideVersion;
		SetCGMSAPolicy(CGMS_A_NotApplicable, Macrovision_NotApplicable);
	}
	return hr;
} // CContentTagger::ExpireVBICopyProtection

///////////////////////////////////////////////////////////////////////
//
//  Class CContentTagger -- Utilities
//
///////////////////////////////////////////////////////////////////////

ISampleProducer *CContentTagger::GetSampleProducer(void)
{
	if (!m_piSampleProducer && m_pippmgr)
	{
		CPipelineRouter cPipelineRouter = m_pippmgr->GetRouter(NULL, false, false);
		void *pvSampleProducer = NULL;
		cPipelineRouter.GetPrivateInterface(IID_ISampleProducer, pvSampleProducer);
		m_piSampleProducer = static_cast<ISampleProducer*>(pvSampleProducer);
	}
	return m_piSampleProducer;
} // CContentTagger::GetSampleProducer

void CContentTagger::Cleanup()
{
	m_cXDSCodecHelper.OnDisconnect();
	m_cXDSCodecHelper.CacheClock(NULL);
	m_cXDSCodecHelper.OnStop();
	m_pcDVRInputPinVBI = NULL;
	m_piSampleProducer = NULL;
	m_pippmgr = NULL;
	m_piXDSCodecNext = NULL;
	m_piContentRestrictionCaptureNext = NULL;
	m_fConfiguredPins = false;
	m_rtStartFlags = 0;
	m_rtEndFlags = (UINT64)0x4000000000000000;

	std::vector<CContentTaggerInputPinHelper *>::iterator iter;
	for (iter = m_vectorCContentTaggerInputPinHelper.begin();
		 iter != m_vectorCContentTaggerInputPinHelper.end();
		 ++iter)
	{
		CContentTaggerInputPinHelper *pcContentTaggerInputPinHelper = *iter;
		delete pcContentTaggerInputPinHelper;
	}
	m_vectorCContentTaggerInputPinHelper.clear();
	m_piMediaEventSink = NULL;
	m_eCGMSAPolicyCurrent = CGMS_A_NotApplicable;
	m_eMacrovisionPolicyVBICurrent = Macrovision_NotApplicable;
	m_eMacrovisionPolicyPropertyCurrent = Macrovision_NotApplicable;
	m_eCGMSAPolicy = CGMS_A_NotApplicable;
	m_eMacrovisionPolicyVBI = Macrovision_NotApplicable;
	m_eMacrovisionPolicyProperty = Macrovision_NotApplicable;
	m_fUsingTestHookOverride = false;
	m_uTestHookOverrideVersion = 0;
} // CContentTagger::Cleanup()

void CContentTagger::ComputeVBIPin()
{
	m_pcDVRInputPinVBI = NULL;

	ASSERT(m_pippmgr);
	if (!m_pippmgr)
	{
		DbgLog((LOG_ERROR, 2, _T("CContentTagger(%p)::ComputeVBIPin() -- no pipeline manager\n"), this));
		throw CHResultError(E_FAIL, "No pipeline manager");
	}

	CDVRSinkFilter &rcDVRSinkFilter = m_pippmgr->GetSinkFilter();
	int iPinCount = rcDVRSinkFilter.GetPinCount();
	int iPinIndex;

	for (iPinIndex = 0; (iPinIndex < iPinCount); ++iPinIndex)
	{
		CBasePin *pcBasePin = rcDVRSinkFilter.GetPin(iPinIndex);
		ASSERT(pcBasePin);
		AM_MEDIA_TYPE mt;
		if (FAILED(pcBasePin->ConnectionMediaType(&mt)))
			continue;
		CDVRInputPin *pcDVRInputPin = static_cast<CDVRInputPin*>(pcBasePin);
		if (IsEqualGUID(mt.majortype, MEDIATYPE_AUXLine21Data) &&
			IsEqualGUID(mt.subtype, MEDIASUBTYPE_Line21_BytePair))
		{
			// This is our VBI pin:

			ASSERT(!m_pcDVRInputPinVBI);
			if (m_pcDVRInputPinVBI)
			{
				DbgLog((LOG_ERROR, 2, _T("CContentTagger(%p)::ComputeVBIPin() -- 2 or more VBI pins\n"), this));
				throw CHResultError(E_FAIL, "Multiple VBI pins");
			}
			m_pcDVRInputPinVBI = pcDVRInputPin;
		}
		FreeMediaType(mt);
	}
	m_fConfiguredPins = true;
	if (m_pcDVRInputPinVBI)
		m_cXDSCodecHelper.OnConnect(m_pcDVRInputPinVBI);
	else
		m_cXDSCodecHelper.OnDisconnect();
} // CContentTagger::ComputeVBIPin

void CContentTagger::SetMacrovisionPropertyPolicy(MacrovisionPolicy eMacrovisionPolicy)
{
	SyncPolicyWithTestHooks();
	if (eMacrovisionPolicy != m_eMacrovisionPolicyProperty)
	{
		DbgLog((LOG_COPY_PROTECTION, 3, _T("CContentTagger::SetMacrovisionPropertyPolicy():  new macrovision setting %d\n"),
				(int) eMacrovisionPolicy));
		m_eMacrovisionPolicyProperty = eMacrovisionPolicy;
		if (!m_fUsingTestHookOverride)
		{
			m_eMacrovisionPolicyPropertyCurrent = m_eMacrovisionPolicyProperty;
			NotifyCopyProtectionPolicy();
		}
	}
} // CContentTagger::SetMacrovisionPropertyPolicy

void CContentTagger::SetCGMSAPolicy(CGMS_A_Policy eCGMSAPolicy, MacrovisionPolicy eMacrovisionPolicy)
{
	SyncPolicyWithTestHooks();
	if ((eCGMSAPolicy != m_eCGMSAPolicy) ||
		((eCGMSAPolicy == CGMS_A_CopyNever) && (eMacrovisionPolicy != m_eMacrovisionPolicyVBI)))
	{
		DbgLog((LOG_COPY_PROTECTION, 3, _T("CContentTagger::SetCGMSAPolicy():  new CGMS-A property %d, new APS %d\n"),
				(int) eCGMSAPolicy, (int) eMacrovisionPolicy));
		m_eCGMSAPolicy = eCGMSAPolicy;
		if (m_eCGMSAPolicy != CGMS_A_CopyNever)
			m_eMacrovisionPolicyVBI = Macrovision_NotApplicable;
		else
			m_eMacrovisionPolicyVBI = eMacrovisionPolicy;
		if (!m_fUsingTestHookOverride)
		{
			m_eCGMSAPolicyCurrent = m_eCGMSAPolicy;
			m_eMacrovisionPolicyVBICurrent = m_eMacrovisionPolicyVBI;
			NotifyCopyProtectionPolicy();
		}
	}
} // CContentTagger::SetCGMSAPolicy

void CContentTagger::SetMacrovisionPolicy()
{
	

	MacrovisionPolicy eSavedPolicy = m_eMacrovisionPolicyPropertyCurrent;

	m_eMacrovisionPolicyPropertyCurrent = Macrovision_NotApplicable;

	NotifyCopyProtectionPolicy();

	m_eMacrovisionPolicyPropertyCurrent = eSavedPolicy;

	NotifyCopyProtectionPolicy();



}


MacrovisionPolicy CContentTagger::GetMacrovisionVBIPolicy()
{
	SyncPolicyWithTestHooks();
	return m_eMacrovisionPolicyVBICurrent;
}

MacrovisionPolicy CContentTagger::GetMacrovisionPropertyPolicy()
{
	SyncPolicyWithTestHooks();
	return m_eMacrovisionPolicyPropertyCurrent;
}

CGMS_A_Policy CContentTagger::GetCGMSAPolicy()
{
	SyncPolicyWithTestHooks();
	return m_eCGMSAPolicyCurrent;
}

void CContentTagger::NotifyCopyProtectionPolicy()
{
	ASSERT(m_piSampleProducer);
	if (!m_piSampleProducer)
		return;

	DWORD dwCopyProtectionStamp = 0;

	switch (m_eCGMSAPolicyCurrent)
	{
	case CGMS_A_NotApplicable:
		// leave all bits zero
		break;
	case CGMS_A_CopyFreely:
		dwCopyProtectionStamp = 0x1 | (SAMPLE_ENUM_CGMSA_COPYFREELY << 1);
		break;

	case CGMS_A_CopyOnce:
		dwCopyProtectionStamp = 0x1 | (SAMPLE_ENUM_CGMSA_COPYONCE << 1);
		break;

	case CGMS_A_CopyNever:
		dwCopyProtectionStamp = 0x1 | (SAMPLE_ENUM_CGMSA_COPYNEVER << 1);

		// Only in this case can the Macrovision VBI be applied:
		switch (m_eMacrovisionPolicyVBICurrent)
		{
		case Macrovision_NotApplicable:
		case Macrovision_Disabled:
			dwCopyProtectionStamp |= (SAMPLE_ENUM_MACROVISION_DISABLED << 3);
			break;

		case Macrovision_CopyNever_AGC_01:
			dwCopyProtectionStamp |= (SAMPLE_ENUM_MACROVISION_COPYNEVER_AGC_01 << 3);
			break;

		case Macrovision_CopyNever_AGC_ColorStripe2:
			dwCopyProtectionStamp |= (SAMPLE_ENUM_MACROVISION_COPYNEVER_AGC_COLORSTRIPE_2 << 3);
			break;

		case Macrovision_CopyNever_AGC_ColorStripe4:
			dwCopyProtectionStamp |= (SAMPLE_ENUM_MACROVISION_COPYNEVER_AGC_COLORSTRIPE_4 << 3);
			break;
		}
		break;

	case CGMS_A_Reserved:
		dwCopyProtectionStamp = 0x1 | (SAMPLE_ENUM_CGMSA_RESERVED << 1);
		break;
	};

	switch (m_eMacrovisionPolicyPropertyCurrent)
	{
	case Macrovision_NotApplicable:
		// Nothing ever set -- nothing to add
		break;

	case Macrovision_Disabled:
		dwCopyProtectionStamp |= (0x1 << 5) |
								 (SAMPLE_ENUM_MACROVISION_DISABLED << 6);
		break;

	case Macrovision_CopyNever_AGC_01:
		dwCopyProtectionStamp |= (0x1 << 5) |
								 (SAMPLE_ENUM_MACROVISION_COPYNEVER_AGC_01 << 6);
		break;

	case Macrovision_CopyNever_AGC_ColorStripe2:
		dwCopyProtectionStamp |= (0x1 << 5) |
								 (SAMPLE_ENUM_MACROVISION_COPYNEVER_AGC_COLORSTRIPE_2 << 6);
		break;

	case Macrovision_CopyNever_AGC_ColorStripe4:
		dwCopyProtectionStamp |= (0x1 << 5) |
								 (SAMPLE_ENUM_MACROVISION_COPYNEVER_AGC_COLORSTRIPE_4 << 6);
		break;
	}

	// Swap out the old copy protection flags for the new ones:

    m_rtStartFlags &= ~(((LONGLONG)0xff) << SAMPLE_FIELD_OFFSET_CGMSA_PRESENT);
			// 8 bits total in copy protection -- bits 49 to 56
	m_rtStartFlags |= ((LONGLONG)dwCopyProtectionStamp) << SAMPLE_FIELD_OFFSET_CGMSA_PRESENT;

	CEventWithAVPosition *pcEventWithPosition = new CEventWithAVPosition(
		m_piSampleProducer->GetPipelineComponent(),
		DVRENGINE_EVENT_COPYPROTECTION_CHANGE_DETECTED,
		dwCopyProtectionStamp);
	try {
		m_cPipelineRouterForEvents.DispatchExtension(*pcEventWithPosition);
	} catch (const std::exception &)
	{
		pcEventWithPosition->Release();
		throw;
	}
	pcEventWithPosition->Release();

} // CContentTagger::NotifyCopyProtectionChange

void CContentTagger::NotifyNoCopyProtectionChange()
{
	DWORD dwCopyProtectionStamp = (DWORD) ((m_rtStartFlags >> SAMPLE_FIELD_OFFSET_CGMSA_PRESENT) & 0xff);
	dwCopyProtectionStamp &= s_uCopyProtectionEventMask;

	CEventWithAVPosition *pcEventWithPosition = new CEventWithAVPosition(
		m_piSampleProducer->GetPipelineComponent(),
		DVRENGINE_EVENT_COPYPROTECTION_DUPLICATE_RECEIVED,
		dwCopyProtectionStamp);
	try {
		m_cPipelineRouterForEvents.DispatchExtension(*pcEventWithPosition);
	} catch (const std::exception &)
	{
		pcEventWithPosition->Release();
		throw;
	}
	pcEventWithPosition->Release();
} // CContentTagger::NotifyNoCopyProtectionChange

void CContentTagger::SyncPolicyWithTestHooks()
{
	CGMS_A_Policy eCGMSA;
	MacrovisionPolicy eMacrovisionVBI;
	MacrovisionPolicy eMacrovisionProperty;
	ULONG uCopyProtectionOverrideVersion;

	bool fUseHooks = DVREngine_GetOverrideCopyProtectionInput(eCGMSA,
			eMacrovisionVBI, eMacrovisionProperty, uCopyProtectionOverrideVersion);
	bool fSignalChange = false;
	if (eCGMSA != CGMS_A_CopyNever)
		eMacrovisionVBI = (m_eMacrovisionPolicyVBICurrent >= Macrovision_CopyNever_AGC_01) ?
			Macrovision_Disabled : m_eMacrovisionPolicyVBICurrent;

	if (uCopyProtectionOverrideVersion != m_uTestHookOverrideVersion)
	{
		m_uTestHookOverrideVersion = uCopyProtectionOverrideVersion;
		m_fUsingTestHookOverride = fUseHooks;
		if (fUseHooks)
		{
			fSignalChange = (m_eCGMSAPolicyCurrent != eCGMSA);
			m_eCGMSAPolicyCurrent = eCGMSA;

			fSignalChange = fSignalChange || (m_eMacrovisionPolicyVBICurrent != eMacrovisionVBI);
			m_eMacrovisionPolicyVBICurrent = eMacrovisionVBI;

			fSignalChange = fSignalChange || (m_eMacrovisionPolicyPropertyCurrent != eMacrovisionProperty);
			m_eMacrovisionPolicyPropertyCurrent = eMacrovisionProperty;
		}
		else
		{
			fSignalChange = (m_eCGMSAPolicyCurrent != m_eCGMSAPolicy);
			m_eCGMSAPolicyCurrent = m_eCGMSAPolicy;

			fSignalChange = fSignalChange || (m_eMacrovisionPolicyVBICurrent != m_eMacrovisionPolicyVBI);
			m_eMacrovisionPolicyVBICurrent = m_eMacrovisionPolicyVBI;

			fSignalChange = fSignalChange || (m_eMacrovisionPolicyPropertyCurrent != m_eMacrovisionPolicyProperty);
			m_eMacrovisionPolicyPropertyCurrent = m_eMacrovisionPolicyProperty;
		}
		if (!fSignalChange)
		{
			NotifyNoCopyProtectionChange();
		}
	}
	if (fSignalChange)
		NotifyCopyProtectionPolicy();
}
