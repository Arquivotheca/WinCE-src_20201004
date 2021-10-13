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
#include "ContentDetagger.h"
#include "..\\DVREngine.h"
#include "..\\Plumbing\\PipelineManagers.h"
#include "..\\Plumbing\\Source.h"
#include "..\\HResultError.h"

using namespace MSDvr;

#define OLESTRLEN(x)        wcslen(x)
#define OLESTRCMP(s1,s2)    wcscmp(s1,s2)

///////////////////////////////////////////////////////////////////////
//
//  Class CSetCopyProtection -- helper class
//
///////////////////////////////////////////////////////////////////////

CSetCopyProtection::CSetCopyProtection(MacrovisionPolicy eMacrovisionPolicy, IFilterGraph *piFilterGraph)
	: CAppThreadAction()
	, m_eMacrovisionPolicy(eMacrovisionPolicy)
	, m_piFilterGraph(piFilterGraph)
{
} // CSetCopyProtection::CSetCopyProtection

void CSetCopyProtection::Do()
{
	bool fSetMacrovision = false;

	KS_COPY_MACROVISION sKsCopyMacrovision;
	switch (m_eMacrovisionPolicy)
	{
	case Macrovision_NotApplicable:
	case Macrovision_Disabled:
		sKsCopyMacrovision.MACROVISIONLevel =KS_MACROVISION_DISABLED;
		break;

	case Macrovision_CopyNever_AGC_01:
		sKsCopyMacrovision.MACROVISIONLevel =KS_MACROVISION_LEVEL1;
		break;

	case Macrovision_CopyNever_AGC_ColorStripe2:
		sKsCopyMacrovision.MACROVISIONLevel =KS_MACROVISION_LEVEL2;
		break;

	case Macrovision_CopyNever_AGC_ColorStripe4:
		sKsCopyMacrovision.MACROVISIONLevel =KS_MACROVISION_LEVEL3;
		break;
	}

	CComPtr<IEnumFilters> pFilterIter;

	if (FAILED(m_piFilterGraph->EnumFilters(&pFilterIter)))
	{
		DbgLog((ZONE_COPY_PROTECTION, 3, _T("CSetCopyProtection()::Do() -- unable to enumerate filters, giving up\n"), this));
		return;
	}

	CComPtr<IBaseFilter> pFilter;
	ULONG iFiltersFound;

	for ( ; SUCCEEDED(pFilterIter->Next(1, &pFilter.p, &iFiltersFound)) &&
			(iFiltersFound > 0); pFilter.Release())
	{
		CComPtr<IEnumPins> piEnumPins;
		bool fPinSupportsMacrovision = false;
		if (SUCCEEDED(pFilter->EnumPins(&piEnumPins)) && piEnumPins)
		{
			CComPtr<IPin> piPin;
			ULONG cPinCount;

			for ( ;
				 SUCCEEDED(piEnumPins->Next(1, &piPin.p, &cPinCount)) && (cPinCount > 0);
				 piPin.Release())
			{
				// We've found a pin. We're only interested in it if it is an input pin
				// that is connected to something:

				PIN_INFO sPinInfo;

				if (FAILED(piPin->QueryPinInfo(&sPinInfo)))
					continue;
				if (sPinInfo.pFilter)
					sPinInfo.pFilter->Release();
				if (sPinInfo.dir != PINDIR_INPUT)
					continue;

				CComPtr<IPin> piConnectedPin;
				if (FAILED(piPin->ConnectedTo(&piConnectedPin)) ||
					!piConnectedPin)
					continue;

				// We do have a connected pin.  If it supports the IKsPropertySet interface
				// and that interface says it supports the Macrovision property of the
				// copy protection property set, we want to notify it of the new Macrovision
				// setting:

				CComPtr<IKsPropertySet> piKsPropertySet;

				if (FAILED(piPin->QueryInterface(IID_IKsPropertySet, (void **) &piKsPropertySet.p)))
					continue;

				DWORD dwSupportType = 0;

				if ((S_OK != piKsPropertySet->QuerySupported(AM_KSPROPSETID_CopyProt,
							AM_PROPERTY_COPY_MACROVISION, &dwSupportType)) ||
					!(dwSupportType & KSPROPERTY_SUPPORT_SET))
					continue;

				fPinSupportsMacrovision = true;
				HRESULT hr = piKsPropertySet->Set(AM_KSPROPSETID_CopyProt, AM_PROPERTY_COPY_MACROVISION,
												  NULL, 0,
												  &sKsCopyMacrovision, sizeof(sKsCopyMacrovision));
				if (FAILED(hr))
				{
					DbgLog((LOG_ERROR, 3, _T("CSetCopyProtection::Do() input pin promising Macrovision support rejected new setting (error %d)\n"),
						this, hr));
				}
				else
				{
					fSetMacrovision = true;
				}
			}
		}
		
		if (!fPinSupportsMacrovision)
		{
			CComPtr<IKsPropertySet> piKsPropertySet;

			if (SUCCEEDED(pFilter->QueryInterface(IID_IKsPropertySet, (void **) &piKsPropertySet.p)))
			{
				DWORD dwSupportType = 0;

				if ((S_OK == piKsPropertySet->QuerySupported(AM_KSPROPSETID_CopyProt,
							AM_PROPERTY_COPY_MACROVISION, &dwSupportType)) &&
					(dwSupportType & KSPROPERTY_SUPPORT_SET))
				{
					HRESULT hr = piKsPropertySet->Set(AM_KSPROPSETID_CopyProt, AM_PROPERTY_COPY_MACROVISION,
													NULL, 0,
													&sKsCopyMacrovision, sizeof(sKsCopyMacrovision));
					if (FAILED(hr))
					{
						DbgLog((LOG_ERROR, 3, _T("CSetCopyProtection::Do() input pin promising Macrovision support rejected new setting (error %d)\n"),
							this, hr));
					}
					else
					{
						fSetMacrovision = true;
					}
				}
			}
		}
	}

	if (fSetMacrovision)
	{
		DbgLog((ZONE_COPY_PROTECTION, 1, _T("DVR Source Filter told 1+ downstream filters about Macrovision level %d\n"), sKsCopyMacrovision));
	}
	else
	{
		DbgLog((ZONE_COPY_PROTECTION, 1, _T("DVR Source Filter found no downstream filter interested in knowing about Macrovision level %d\n"), sKsCopyMacrovision));
	}
} // CSetCopyProtection::Do


///////////////////////////////////////////////////////////////////////
//
//  Class CContentDetagger -- constructor and destructor
//
///////////////////////////////////////////////////////////////////////

CContentDetagger::CContentDetagger(void)
	: m_pippmgr(0)
	, m_piDecoderDriver(0)
	, m_piFilterGraph(0)
	, m_cCritSecState()
	, m_pcDVROutputPinPrimary(0)
	, m_eMacrovisionPolicyEnforced(Macrovision_NotApplicable)

{
	DbgLog((ZONE_COPY_PROTECTION, 2, _T("CContentDetagger: constructed %p\n"), this));
	m_sContentRestrictionStamp.dwSize = sizeof(m_sContentRestrictionStamp);
	m_sContentRestrictionStamp.dwBfEnShowAttributes = 0;
	m_sContentRestrictionStamp.dwProgramCounter = 0;
	m_sContentRestrictionStamp.dwTuningCounter = 0;
	m_sContentRestrictionStamp.eCGMSAPolicy = CGMS_A_NotApplicable;
	m_sContentRestrictionStamp.eEnTvRatGeneric = TvRat_LevelDontKnow;
	m_sContentRestrictionStamp.eEnTvRatSystem = TvRat_SystemDontKnow;
	m_sContentRestrictionStamp.eMacrovisionPolicyVBI = Macrovision_NotApplicable;
	m_sContentRestrictionStamp.eMacrovisionPolicyProperty = Macrovision_NotApplicable;
	m_sContentRestrictionStamp.fTuneComplete = true;
	m_sContentRestrictionStamp.fEncrypted = false;
#ifdef DVR_ENGINE_FEATURE_IN_PROGRESS
	m_sContentRestrictionStamp.fShouldBeEncrypted = false;
#endif // DVR_ENGINE_FEATURE_IN_PROGRESS
} // CContentDetagger::CContentDetagger

CContentDetagger::~CContentDetagger(void)
{
	DbgLog((ZONE_COPY_PROTECTION, 2, _T("CContentDetagger: destroying %p\n"), this));

	{
		CAutoLock cAutoLock(&m_cCritSecState);

		Cleanup();
	}
} // CContentDetagger::~CContentDetagger

///////////////////////////////////////////////////////////////////////
//
//  Class CContentDetagger -- IPipelineComponent
//
///////////////////////////////////////////////////////////////////////

void CContentDetagger::RemoveFromPipeline()
{
	DbgLog((ZONE_COPY_PROTECTION, 3, _T("CContentDetagger(%p)::RemoveFromPipeline\n"), this));

	CAutoLock cAutoLock(&m_cCritSecState);

	Cleanup();
} // CContentDetagger::RemoveFromPipeline

ROUTE CContentDetagger::ConfigurePipeline(
	UCHAR iNumPins,
	CMediaType cMediaTypes[],
	UINT iSizeCustom,
	BYTE Custom[])
{
	DbgLog((ZONE_COPY_PROTECTION, 3, _T("CContentDetagger(%p)::ConfigurePipeline()\n"), this));

	// This is the point that we grab references to other pipeline and
	// filter components -- the pipeline has been fully configured and we're
	// still safely running on an app thread.
	//
	CAutoLock cAutoLock(&m_cCritSecState);

	if (!GetDecoderDriver())
	{
		DbgLog((LOG_ERROR, 2, _T("CContentDetagger(%p)::ConfigurePipeline() -- no decoder driver\n"), this));
		throw CHResultError(E_FAIL, "No decoder driver found");
	}
	// Pins are not yet valid during the initial call.
	if (m_pcDVROutputPinPrimary)
		m_pcDVROutputPinPrimary = &m_pippmgr->GetPrimaryOutput();

	return HANDLED_CONTINUE;
} // CContentDetagger::ConfigurePipeline

ROUTE CContentDetagger::NotifyFilterStateChange(
						// State being put into effect
						FILTER_STATE	eState)
{
	if (!m_pcDVROutputPinPrimary)
		m_pcDVROutputPinPrimary = &m_pippmgr->GetPrimaryOutput();
	if (!m_piFilterGraph)
		m_piFilterGraph = m_pippmgr->GetFilter().GetFilterGraph();

	return HANDLED_CONTINUE;
} // CContentDetagger::NotifyFilterStateChange

ROUTE CContentDetagger::DispatchExtension(
						CExtendedRequest &rcExtendedRequest)
{
	return UNHANDLED_CONTINUE;
} // CContentDetagger::DispatchExtension

///////////////////////////////////////////////////////////////////////
//
//  Class CContentDetagger -- IPlaybackPipelineComponent
//
///////////////////////////////////////////////////////////////////////

unsigned char CContentDetagger::AddToPipeline(
	IPlaybackPipelineManager &rcManager)
{
	DbgLog((ZONE_COPY_PROTECTION, 3, _T("CContentDetagger(%p)::AddToPipeline()\n"), this));
	CAutoLock cAutoLock(&m_cCritSecState);

	IPipelineManager *rpManager = (IPipelineManager *) &rcManager;
	m_pippmgr = &rcManager;

	return 0;	// no streaming threads
} // CContentDetagger::AddToPipeline

ROUTE CContentDetagger::ProcessOutputSample(
	IMediaSample &riSample,
	CDVROutputPin &rcOutputPin)
{
	ROUTE eRoute = UNHANDLED_CONTINUE;

	if (&rcOutputPin == m_pcDVROutputPinPrimary)
	{
		CAutoLock cAutoLock(&m_cCritSecState);
		bool fNotifyCopyProtectionChange = false;

#ifdef DVR_ENGINE_FEATURE_IN_PROGRESS
		m_sContentRestrictionStamp.fEncrypted = CCopyProtectionExtractor::IsEncrypted(riSample);
		m_sContentRestrictionStamp.fShouldBeEncrypted = CCopyProtectionExtractor::ShouldBeEncrypted(riSample);
#endif // DVR_ENGINE_FEATURE_IN_PROGRESS

		CGMS_A_Policy eCGMSAPolicyNew = CCopyProtectionExtractor::GetCGMSA(riSample);
		if (eCGMSAPolicyNew != m_sContentRestrictionStamp.eCGMSAPolicy)
		{
			m_sContentRestrictionStamp.eCGMSAPolicy = eCGMSAPolicyNew;
			fNotifyCopyProtectionChange = true;
		}

		MacrovisionPolicy eMacrovisionPolicyNew = CCopyProtectionExtractor::GetMacrovisionVBI(riSample);
		if (eMacrovisionPolicyNew != m_sContentRestrictionStamp.eMacrovisionPolicyVBI)
		{
			m_sContentRestrictionStamp.eMacrovisionPolicyVBI = eMacrovisionPolicyNew;
			fNotifyCopyProtectionChange = true;
		}

		eMacrovisionPolicyNew = CCopyProtectionExtractor::GetMacrovisionProperty(riSample);
		if (eMacrovisionPolicyNew != m_sContentRestrictionStamp.eMacrovisionPolicyProperty)
		{
			m_sContentRestrictionStamp.eMacrovisionPolicyProperty = eMacrovisionPolicyNew;
			fNotifyCopyProtectionChange = true;
		}

		if (fNotifyCopyProtectionChange && m_piDecoderDriver)
		{
			// Enforce an change in the Macrovision policy:

			ASSERT(m_piFilterGraph);

			MacrovisionPolicy eMacrovisionPolicyEnforced = CCopyProtectionExtractor::CombineMacrovision(
						m_sContentRestrictionStamp.eMacrovisionPolicyProperty,
						m_sContentRestrictionStamp.eMacrovisionPolicyVBI);
			if (m_eMacrovisionPolicyEnforced != eMacrovisionPolicyEnforced)
			{
				m_eMacrovisionPolicyEnforced = eMacrovisionPolicyEnforced;

				DbgLog((ZONE_EVENT_DETECTED, 3, _T("CContentDetagger(%p)::ProcessOutputSample() -- Macrovision policy changed to %d\n"),
					this, m_eMacrovisionPolicyEnforced));

				try {
					CSetCopyProtection *pcSetCopyProtection =
						new CSetCopyProtection(eMacrovisionPolicyEnforced, m_piFilterGraph);
					try {
						m_piDecoderDriver->DeferToAppThread(pcSetCopyProtection);
					} catch (const std::exception &) {
						delete pcSetCopyProtection;
						throw;
					};
				} catch (const std::exception &) {
					// Yikes!  We failed to enforce the Macrovision policy!

					if (eMacrovisionPolicyEnforced >= Macrovision_CopyNever_AGC_01)
					{
						// This is really, really nasty.  Stop the filter graph.

						DbgLog((LOG_ERROR, 2,
							_T("CContentDetagger(%p)::ProcessOutputSample() -- cannot enforce Macrovision\n"),
							this));
						m_eMacrovisionPolicyEnforced = Macrovision_Disabled;
						m_piDecoderDriver->ImplementGraphConfused(E_FAIL);
					}
				}
			}
		}

		// Notify IMediaEventSink:

		if (m_piDecoderDriver && fNotifyCopyProtectionChange)
		{
			LONGLONG hyStart, hyEnd;
			REFERENCE_TIME rtStart, rtEnd;

			if (SUCCEEDED(riSample.GetTime(&rtStart, &rtEnd)) &&
				SUCCEEDED(riSample.GetMediaTime(&hyStart, &hyEnd)))
			{
				long lCopyProtectionBits =
					(long) ((rtStart >> SAMPLE_FIELD_OFFSET_CGMSA_PRESENT) & 0xff);  // 8 bits
				m_piDecoderDriver->IssueNotification(this, DVRENGINE_EVENT_COPYPROTECTION_CHANGE_DETECTED,
					lCopyProtectionBits, (long) (hyStart / ((LONGLONG)10000)), true);
			}
			else
			{
				DbgLog((LOG_ERROR, 3, _T("CContentDetagger(%p)::ProcessOutputSample() failed to get time values\n"), this));
			}
		}
	}

	return eRoute;
} // CContentDetagger::ProcessOutputSample

///////////////////////////////////////////////////////////////////////
//
//  Class CContentDetagger -- Utilities
//
///////////////////////////////////////////////////////////////////////

void CContentDetagger::Cleanup()
{
	DbgLog((ZONE_COPY_PROTECTION, 3, _T("CContentDetagger(%p)::Cleanup()\n"), this));

	m_pippmgr = 0;
	m_piDecoderDriver = 0;
	m_piFilterGraph = 0;
	m_pcDVROutputPinPrimary = 0;
	m_eMacrovisionPolicyEnforced = Macrovision_NotApplicable;
	m_sContentRestrictionStamp.dwSize = sizeof(m_sContentRestrictionStamp);
	m_sContentRestrictionStamp.dwBfEnShowAttributes = 0;
	m_sContentRestrictionStamp.dwProgramCounter = 0;
	m_sContentRestrictionStamp.dwTuningCounter = 0;
	m_sContentRestrictionStamp.eCGMSAPolicy = CGMS_A_NotApplicable;
	m_sContentRestrictionStamp.eEnTvRatGeneric = TvRat_LevelDontKnow;
	m_sContentRestrictionStamp.eEnTvRatSystem = TvRat_SystemDontKnow;
	m_sContentRestrictionStamp.eMacrovisionPolicyVBI = Macrovision_NotApplicable;
	m_sContentRestrictionStamp.eMacrovisionPolicyProperty = Macrovision_NotApplicable;
	m_sContentRestrictionStamp.fTuneComplete = true;
} // CContentDetagger::Cleanup

IDecoderDriver *CContentDetagger::GetDecoderDriver(void)
{
	if (!m_piDecoderDriver && m_pippmgr)
	{
		CPipelineRouter cPipelineRouter = m_pippmgr->GetRouter(NULL, false, false);
		void *pvDecoderDriver = NULL;
		cPipelineRouter.GetPrivateInterface(IID_IDecoderDriver, pvDecoderDriver);
		m_piDecoderDriver = static_cast<IDecoderDriver*>(pvDecoderDriver);
	}
	return m_piDecoderDriver;
} // CContentDetagger::GetDecoderDriver
