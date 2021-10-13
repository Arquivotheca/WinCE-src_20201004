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
#include "DvrInterfaces.h"
#include "..\\ContentRestriction.h"

namespace MSDvr
{

	class CSetCopyProtection : public CAppThreadAction
	{
	public:
		CSetCopyProtection(MacrovisionPolicy eMacrovisionPolicy, IFilterGraph *piFilterGraph);

		virtual void Do();

	protected:
		MacrovisionPolicy m_eMacrovisionPolicy;
		CComPtr<IFilterGraph> m_piFilterGraph;
	};

	class CContentDetagger:
		public IPlaybackPipelineComponent
	{
	public:
		CContentDetagger(void);
		~CContentDetagger(void);

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

		/* IPlaybackPipelineComponent: */
		virtual unsigned char AddToPipeline(IPlaybackPipelineManager &rcManager);
		virtual ROUTE ProcessOutputSample(
			IMediaSample &riMediaSample,
			CDVROutputPin &rcDVROutputPin);

	protected:
		void Cleanup();
		IDecoderDriver *GetDecoderDriver();

		IPlaybackPipelineManager *m_pippmgr;
		IDecoderDriver *m_piDecoderDriver;
		CCritSec m_cCritSecState;
		SContentRestrictionStamp m_sContentRestrictionStamp;
		MacrovisionPolicy m_eMacrovisionPolicyEnforced;
		CDVROutputPin *m_pcDVROutputPinPrimary;
		IFilterGraph *m_piFilterGraph;

	};

}
