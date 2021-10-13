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
// BootstrapPipelineManagers.h : Classes for bootstrapping capture and playback
// pipelines and selection of final pipeline managers.
//

#include "stdafx.h"
#include "BootstrapPipelineManagers.h"

#include "Sink.h"
#include "MPEGPipelineManagers.h"
#include "..\\Reader\\Reader.h"

namespace MSDvr
{

//////////////////////////////////////////////////////////////////////////////
//	BootstrapCapturePipelineManager.										//
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//	Construction.															//
//////////////////////////////////////////////////////////////////////////////

CBootstrapCapturePipelineManager::CBootstrapCapturePipelineManager(
		CDVRSinkFilter& rcFilter,
		ICapturePipelineManager*& rpiPipelineManager) :
	CCapturePipelineManager	(rcFilter),
	m_rpiPipelineManager	(rpiPipelineManager)
{
	// Route all pipeline calls to this instance
	m_cBasePipelineManager.AppendPipelineComponent(m_sComponent, false);

	// Install this pipeline manager as the active one
	rpiPipelineManager = this;

	// Create the first pin
	rcFilter.AddPin(L"Sensor");
}

//////////////////////////////////////////////////////////////////////////////
//	Capture pipeline manager pure virtuals.									//
//////////////////////////////////////////////////////////////////////////////

CBasePin& CBootstrapCapturePipelineManager::GetPrimary()
{
	// This method should not be called on the boostrap manager
	ASSERT (FALSE);
	throw std::logic_error("Must not GetPrimary when bootstrapping");
}

CDVRInputPin& CBootstrapCapturePipelineManager::GetPrimaryInput()
{
	// This method should not be called on the boostrap manager
	ASSERT (FALSE);
	throw std::logic_error("Must not GetPrimaryInput when bootstrapping");
}

//////////////////////////////////////////////////////////////////////////////
//	Capture pipeline manager pipeline component hooks.						//
//////////////////////////////////////////////////////////////////////////////

unsigned char CBootstrapCapturePipelineManager::SComponent::
AddToPipeline(ICapturePipelineManager& rcManager)
{
	// The bootstrap manager will not call this method on its own component
	ASSERT (FALSE);
	return 0;
}

void CBootstrapCapturePipelineManager::SComponent::RemoveFromPipeline()
{
	// The bootstrap manager will not call this method on its own component
	ASSERT (FALSE);
}

ROUTE CBootstrapCapturePipelineManager::SComponent::
CheckMediaType(const CMediaType& rcMediaType,
			   CDVRInputPin& /*rcInputPin*/,
			   HRESULT& rhResult)
{
	// Determine whether this format can be supported by some pipeline manager
	if (IsEqualGUID(*rcMediaType.Type(), MEDIATYPE_Stream) &&
			(IsEqualGUID(*rcMediaType.Subtype(), MEDIASUBTYPE_Asf) ||
			IsEqualGUID(*rcMediaType.Subtype(), MEDIASUBTYPE_MPEG2_PROGRAM)))
	{
		rhResult = S_OK;
	}
	else
		rhResult = S_FALSE;

	return HANDLED_CONTINUE;
}

ROUTE CBootstrapCapturePipelineManager::SComponent::
CompleteConnect(IPin& /*riReceivePin*/, CDVRInputPin& rcInputPin)
{
	// Determine which new pipeline manager to substitute
	CMediaType cSinglePinMediaType;
	EXECUTE_ASSERT (SUCCEEDED(rcInputPin.
							  ConnectionMediaType(&cSinglePinMediaType)));

	if (IsEqualGUID(*cSinglePinMediaType.Type(), MEDIATYPE_Stream))
	{
		if (IsEqualGUID(*cSinglePinMediaType.Subtype(), MEDIASUBTYPE_MPEG2_PROGRAM))
		{
			new CMPEGCapturePipelineManager(Owner().GetSinkFilter(),
											Owner().m_rpiPipelineManager);
		}
		
		// add else clauses for additional media types here
	}

	// Since a new pipeline manager has been installed, delete this one now
	delete &Owner();

	return HANDLED_STOP;
}

//////////////////////////////////////////////////////////////////////////////
//	Capture pipeline manager private implementation.						//
//////////////////////////////////////////////////////////////////////////////

CBootstrapCapturePipelineManager&
CBootstrapCapturePipelineManager::SComponent::Owner()
{
	return *reinterpret_cast <CBootstrapCapturePipelineManager*>
		(reinterpret_cast <char*> (this) -
		 offsetof(CBootstrapCapturePipelineManager, m_sComponent));
}

//////////////////////////////////////////////////////////////////////////////
//	BootstrapPlaybackPipelineManager.										//
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//	Construction.															//
//////////////////////////////////////////////////////////////////////////////

CBootstrapPlaybackPipelineManager::CBootstrapPlaybackPipelineManager(
		CDVRSourceFilter& rcFilter,
		IPlaybackPipelineManager*& rpiPipelineManager) :
	CPlaybackPipelineManager	(rcFilter),
	m_rpiPipelineManager		(rpiPipelineManager)
{
	// Register the COM interface implementation
	RegisterCOMInterface(*this);

	// Install this pipeline manager as the active one
	rpiPipelineManager = this;
}

//////////////////////////////////////////////////////////////////////////////
//	Playback pipeline manager pure virtuals.								//
//////////////////////////////////////////////////////////////////////////////

CBasePin& CBootstrapPlaybackPipelineManager::GetPrimary()
{
	// This method should not be called on the boostrap manager
	ASSERT (FALSE);
	throw std::logic_error("Must not GetPrimary when bootstrapping");
}

CDVROutputPin& CBootstrapPlaybackPipelineManager::GetPrimaryOutput()
{
	// This method should not be called on the boostrap manager
	ASSERT (FALSE);
	throw std::logic_error("Must not GetPrimaryInput when bootstrapping");
}

//////////////////////////////////////////////////////////////////////////////
//	IFileSourceFilter Implementation.										//
//////////////////////////////////////////////////////////////////////////////

STDMETHODIMP CBootstrapPlaybackPipelineManager::
Load(LPCOLESTR pszFileName, const AM_MEDIA_TYPE* pmt)
{
	if (! pszFileName)
		return E_POINTER;

	typedef std::auto_ptr<IReader> t_spReader;
	t_spReader spReader;

	// If a media type is indicated, try that reader only
	if (pmt)
	{
		if (IsEqualGUID(pmt->majortype, MEDIATYPE_Stream)
		&&	IsEqualGUID(pmt->subtype, MEDIASUBTYPE_Asf))
		{
			spReader = t_spReader(new CReader);

			if (! spReader->CanReadFile(pszFileName))
				spReader = t_spReader(NULL);
		}
		else if (IsEqualGUID(pmt->majortype, MEDIATYPE_Stream)
		&&	IsEqualGUID(pmt->subtype, MEDIASUBTYPE_MPEG2_PROGRAM))
		{
			spReader = t_spReader(new CReader);

			if (! spReader->CanReadFile(pszFileName))
				spReader = t_spReader(NULL);
		}
	}
	else // try all readers in sequence
	{
		spReader = t_spReader(new CReader);

		if (! spReader->CanReadFile(pszFileName))
			spReader = t_spReader(NULL);
	}

	if (! spReader.get())
		return E_FAIL;

	// BUGBUG: This assumes the pipeline type.  todo - we need a mechanism
	// for creating different pipeline manager types.

	// Roll to the new pipeline manager
	new CMPEGPlaybackPipelineManager(GetSourceFilter(), m_rpiPipelineManager,
									*spReader, pszFileName, pmt);
	spReader.release();

	// Since a new pipeline manager has been installed, delete this one now
	delete this;
	return S_OK;
}

STDMETHODIMP CBootstrapPlaybackPipelineManager::
GetCurFile(LPOLESTR* /*ppszFileName*/, AM_MEDIA_TYPE* /*pmt*/)
{
	return E_NOTIMPL;
}

}
