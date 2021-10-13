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
// @doc
// BootstrapPipelineManagers.h : Classes for bootstrapping capture and playback
// pipelines and selection of final pipeline managers.
//

#pragma once

#include "PipelineManagers.h"

namespace MSDvr
{

//////////////////////////////////////////////////////////////////////////////
// @class CBootstrapCapturePipelineManager |
// The bootstrap capture pipeline manager selects the appropriate final
// pipeline manager on the basis of the pin connection media types.
//////////////////////////////////////////////////////////////////////////////
class CBootstrapCapturePipelineManager : public CCapturePipelineManager
{
// @access Public Interface
public:
	// @cmember Constructor
	CBootstrapCapturePipelineManager(CDVRSinkFilter& rcFilter,
									 ICapturePipelineManager*& rpiPipelineManager);

	// @cmember Identification of the primary stream
	virtual CBasePin&		GetPrimary();

	// @cmember Type-safe identification of the primary stream
	virtual CDVRInputPin&	GetPrimaryInput();

// @access Private Members
private:
	// @struct SComponent |
	// Pipeline commands are routed to and intercepted by this type
	struct SComponent : ICapturePipelineComponent
	{
		// @cmember Invoked at time of pipeline removal
		// Not used with the pipeline manager's own component
		virtual void	RemoveFromPipeline();

		// @method unsigned char | ICapturePipelineComponent | AddToPipeline |
		// Invoked at time of pipeline adding; returns number of streaming
		// threads to manage
		// Not used with the pipeline manager's own component
		virtual unsigned char AddToPipeline(
							// @parm Gives pipeline component access to the pipeline
							// manager, and thus the entire filter infrastructure
							ICapturePipelineManager& rcManager);

		// @method ROUTE | ICapturePipelineComponent | CheckMediaType |
		// Pin type negotiation <nl>
		// Invoker:	input pin - synchronous
		virtual ROUTE	CheckMediaType(
							// @parm Proposed media type
							const CMediaType& rcMediaType,
							// @parm Receiving pin
							CDVRInputPin&	rcInputPin,
							// @parm Format acceptance indicator
							HRESULT&		rhResult);

		// @method ROUTE | ICapturePipelineComponent | CompleteConnect |
		// Pin connection establishment <nl>
		// Invoker: input pin - synchronous <nl>
		// Handler: pipeline manager, post routing
		virtual ROUTE	CompleteConnect(
							// @parm Pointer to the output pin's interface
							IPin&			riReceivePin,
							// @parm Receiving pin
							CDVRInputPin&	rcInputPin);

	private:
		inline CBootstrapCapturePipelineManager& Owner();

	}						m_sComponent;

	friend struct SComponent;

	// @mdata ICapturePipelineManager*& | CBootstrapCapturePipelineManager | m_rpiPipelineManager |
	ICapturePipelineManager*& m_rpiPipelineManager;
};

//////////////////////////////////////////////////////////////////////////////
// @class CBootstrapPlaybackPipelineManager |
// The bootstrap playback pipeline manager selects the appropriate final
// pipeline manager on the basis of which reader was able to consume the
// file passed in to IFileSourceFilter::Load.
//////////////////////////////////////////////////////////////////////////////
class CBootstrapPlaybackPipelineManager : public CPlaybackPipelineManager,
										  public TRegisterableCOMInterface<IFileSourceFilter>
{
// @access Public Interface
public:
	// @cmember Constructor
	CBootstrapPlaybackPipelineManager(CDVRSourceFilter& rcFilter,
									  IPlaybackPipelineManager*& rpiPipelineManager);

	// @cmember Identification of the primary stream
	virtual CBasePin&		GetPrimary();

	// @cmember Type-safe identification of the primary stream
	virtual CDVROutputPin&	GetPrimaryOutput();

	// IFileSourceFilter
	STDMETHOD(Load)(LPCOLESTR pszFileName, const AM_MEDIA_TYPE* pmt);
	STDMETHOD(GetCurFile)(LPOLESTR* ppszFileName, AM_MEDIA_TYPE* pmt);

// @access Private Members
private:
	// @mdata IPlaybackPipelineManager*& | CBootstrapPlaybackPipelineManager | m_rpiPipelineManager |
	IPlaybackPipelineManager*& m_rpiPipelineManager;
};

}
