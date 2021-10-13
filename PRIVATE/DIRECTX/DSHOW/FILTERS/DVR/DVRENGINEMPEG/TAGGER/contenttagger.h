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
#pragma once

#include "..\\PipelineInterfaces.h"
#include "DvrInterfaces_private.h"
#include "..\\ContentRestriction.h"
#include "XDSCodecHelper.h"
#include "..\\Plumbing\\PipelineManagers.h"

namespace MSDvr
{
	class CContentTagger;

	// TODO:  Add IKsPropertySet
	class CContentTaggerInputPinHelper :
		public TRegisterableCOMInterface<IKsPropertySet>
	{
	public:
		CContentTaggerInputPinHelper(CDVRInputPin &rcDVRInputPin, CContentTagger &rcContentTagger);
		~CContentTaggerInputPinHelper();

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

protected:
		friend class CContentTagger;

		CContentTagger &m_rcContentTagger;
		CDVRInputPin &m_rcDVRInputPin;
		IKsPropertySet *m_piKsPropertySetNext;
	};

	class CContentTagger:
		public ICapturePipelineComponent,
		public TRegisterableCOMInterface<IXDSCodec>,
		public TRegisterableCOMInterface<IContentRestrictionCapture>,
		IXDSCodecCallbacks
	{
	public:
		CContentTagger(void);
		~CContentTagger(void);

		/* IPipelineComponent: */
		virtual void RemoveFromPipeline();
		virtual ROUTE ConfigurePipeline(
			UCHAR bNumPins,
			CMediaType cMediaTypes[],
			UINT uSizeCustom,
			BYTE bCustom[]);
		virtual ROUTE	NotifyFilterStateChange(
						// State being put into effect
						FILTER_STATE	eState);
		virtual ROUTE DispatchExtension(
						// @parm The extension to be dispatched.
						CExtendedRequest &rcExtendedRequest);

		/* ICapturePipelineComponent: */
		virtual unsigned char AddToPipeline(ICapturePipelineManager &rcManager);
		virtual ROUTE ProcessInputSample(
			IMediaSample &riMediaSample,
			CDVRInputPin &rcDVRInputPin);
		virtual ROUTE	NotifyEndFlush(
						CDVRInputPin&	rcInputPin);
		virtual ROUTE	NotifyNewSegment(
						CDVRInputPin&	rcInputPin,
						REFERENCE_TIME rtStart,
						REFERENCE_TIME rtEnd,
						double dblRate);

		/* IXDSCodec: */
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

		/* IContentRestrictionCapture: */
		STDMETHODIMP SetEncryptionEnabled(/* [in] */ BOOL fEncrypt);
		STDMETHODIMP GetEncryptionEnabled(/* [out] */ BOOL *pfEncrypt);
		STDMETHODIMP ExpireVBICopyProtection(/*[in] */ULONG uExpiredPolicy);

		/* IXDSCodecCallbacks: */
		virtual void OnNewXDSPacket();
		virtual void OnNewXDSRating();
		virtual void OnDuplicateXDSRating();

	protected:
		friend class CContentTaggerInputPinHelper;

		void SetMacrovisionPropertyPolicy(MacrovisionPolicy eMacrovisionPolicy);
		void SetCGMSAPolicy(CGMS_A_Policy eCGMSAPolicy, MacrovisionPolicy eMacrovisionPolicy);
		void SetMacrovisionPolicy();

		MacrovisionPolicy GetMacrovisionVBIPolicy();
		MacrovisionPolicy GetMacrovisionPropertyPolicy();
		CGMS_A_Policy GetCGMSAPolicy();

		void SyncPolicyWithTestHooks();

		void NotifyCopyProtectionPolicy();
		void NotifyNoCopyProtectionChange();


		void Cleanup();
		ISampleProducer *GetSampleProducer();
		void ComputeVBIPin();

		std::vector<CContentTaggerInputPinHelper *> m_vectorCContentTaggerInputPinHelper;
		CGMS_A_Policy m_eCGMSAPolicyCurrent;
		MacrovisionPolicy m_eMacrovisionPolicyVBICurrent;
		MacrovisionPolicy m_eMacrovisionPolicyPropertyCurrent;
		CGMS_A_Policy m_eCGMSAPolicy;
		MacrovisionPolicy m_eMacrovisionPolicyVBI;
		MacrovisionPolicy m_eMacrovisionPolicyProperty;
		bool m_fUsingTestHookOverride;
		ULONG m_uTestHookOverrideVersion;
        ISampleProducer *m_piSampleProducer;
		IMediaEventSink *m_piMediaEventSink;
		ICapturePipelineManager *m_pippmgr;
		IXDSCodec * m_piXDSCodecNext;
		IContentRestrictionCapture *m_piContentRestrictionCaptureNext;
		CCritSec m_cCritSecState;
		CDVRInputPin *m_pcDVRInputPinVBI;
		bool m_fConfiguredPins;
		bool m_fSawFlush;
		bool m_fSawNewSegment;
		bool m_fSawSample;
		BOOL m_fEncryptSamples;
		LONGLONG m_rtStartFlags, m_rtEndFlags;
		CXDSCodecHelper m_cXDSCodecHelper;
		CPipelineRouter m_cPipelineRouterForEvents;

		static ULONG s_uVBICopyProtectionEventMask;
		static ULONG s_uCopyProtectionEventMask;

	};

}
