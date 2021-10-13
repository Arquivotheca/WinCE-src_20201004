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
// MPEGPipelineManagers.h : Classes for managing MPEG capture and playback
// pipelines.
//

#pragma once

#include "PipelineManagers.h"

namespace MSDvr
{

//////////////////////////////////////////////////////////////////////////////
// @class CMPEGCapturePipelineManager |
// The MPEG capture pipeline manager maintains writer and sample producer
// pipeline components for the management of a single, sync-flagged
// multimedia stream.
//////////////////////////////////////////////////////////////////////////////
class CMPEGCapturePipelineManager : public CCapturePipelineManager
{
// @access Public Interface
public:
	// @cmember Constructor
	CMPEGCapturePipelineManager(CDVRSinkFilter& rcFilter,
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

		// @method ROUTE | IPipelineComponent | NotifyFilterStateChange |
		// Pipeline components are notified of filter state changes
		virtual ROUTE	NotifyFilterStateChange(
							// State being put into effect
							FILTER_STATE	eState);

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

	private:
		inline CMPEGCapturePipelineManager& Owner();

	}						m_sComponent;

	friend struct SComponent;

	// @mdata ICapturePipelineManager*& | CMPEGCapturePipelineManager | m_rpiPipelineManager |
	ICapturePipelineManager*& m_rpiPipelineManager;
};

//////////////////////////////////////////////////////////////////////////////
// @class CMPEGPlaybackPipelineManager |
// The MPEG playback pipeline manager maintains reader and sample consumer
// pipeline components for the management of a single, sync-flagged
// multimedia stream.
//////////////////////////////////////////////////////////////////////////////
class CMPEGPlaybackPipelineManager : public CPlaybackPipelineManager
{
// @access Public Interface
public:
	// @cmember Constructor
	CMPEGPlaybackPipelineManager(CDVRSourceFilter& rcFilter,
								IPlaybackPipelineManager*& rpiPipelineManager,
								IReader& riReader,
								LPCOLESTR pszFileName,
								const AM_MEDIA_TYPE* pmt);

	// @cmember Identification of the primary stream
	virtual CBasePin&		GetPrimary();

	// @cmember Type-safe identification of the primary stream
	virtual CDVROutputPin&	GetPrimaryOutput();

	// @method HRESULT | IPlaybackPipelineManager | GetMediaType |
	// Per-output pin media type(s) access
	virtual HRESULT			GetMediaType(
								// @parm type index
								int					iPosition,
								// @parm media type object to be copied into
								CMediaType*			pMediaType,
								// @parm output pin
								CDVROutputPin&		rcOutputPin);

// @access Private Members
private:
	// @struct SComponent |
	// Pipeline commands are routed to and intercepted by this type
	struct SComponent : IPlaybackPipelineComponent
	{
		// @cmember Invoked at time of pipeline removal
		// Not used with the pipeline manager's own component
		virtual void	RemoveFromPipeline();

		// @method ROUTE | IPipelineComponent | NotifyFilterStateChange |
		// Pipeline components are notified of filter state changes
		virtual ROUTE	NotifyFilterStateChange(
							// State being put into effect
							FILTER_STATE	eState);

		// @method ROUTE | IPipelineComponent | ConfigurePipeline |
		// Pipeline configuration <nl>
		// Sent by reader/deserializer after being added to the
		// pipeline; queued by reader/deserializer in-line with
		// media samples for mid-stream changes; handled by
		// playback pipeline manager
		virtual ROUTE	ConfigurePipeline(
							// @parm Number of streams
							UCHAR			iNumPins,
							// @parm Ordered array of media types, of the
							// number indicated
							CMediaType		cMediaTypes[],
							// @parm Size of custom pipeline manager
							// information
							UINT			iSizeCustom,
							// @parm Custom pipeline manager information,
							// of the size indicated
							BYTE			Custom[]);

		// @method unsigned char | IPlaybackPipelineComponent | AddToPipeline |
		// Invoked at time of pipeline adding; returns number of streaming
		// threads to manage
		// Not used with the pipeline manager's own component
		virtual unsigned char AddToPipeline(
							// @parm Gives pipeline component access to the pipeline
							// manager, and thus the entire filter infrastructure
							IPlaybackPipelineManager& rcManager);

		// @method ROUTE | IPlaybackPipelineComponent | ProcessOutputSample |
		// Sample processing <nl>
		// Handler:	output pin - may block
		virtual ROUTE	ProcessOutputSample(
							// @parm Media sample to be delivered; must have been
							// acquired from target pin's allocator
							IMediaSample&	riSample,
							// @parm Sending pin
							CDVROutputPin&	rcOutputPin);

		// @method ROUTE | IPlaybackPipelineComponent | CheckMediaType |
		// Pin type negotiation <nl>
		// Invoker:	output pin - synchronous
		virtual ROUTE	CheckMediaType(
							// @parm Proposed media type
							const CMediaType& rcMediaType,
							// @parm Sending pin
							CDVROutputPin&	rcOutputPin,
							// @parm Format acceptance indicator
							HRESULT&		rhResult);

		// @method ROUTE | IPlaybackPipelineComponent | EndOfStream |
		// Deliver end of stream notifications via all output pins <nl>
		// Invocation: asynchronous
		virtual ROUTE	EndOfStream();

	private:
		inline CMPEGPlaybackPipelineManager& Owner();

	}						m_sComponent;

	friend struct SComponent;

	// @mdata IPlaybackPipelineManager*& | CMPEGPlaybackPipelineManager | m_rpiPipelineManager |
	IPlaybackPipelineManager*& m_rpiPipelineManager;

	// @mdata CMediaType | CMPEGPlaybackPipelineManager | m_cMediaType |
	CMediaType				m_cMediaType;

	// @mdata CCritSec | CMPEGPlaybackPipelineManager | m_cMediaTypeLeafLock |
	CCritSec				m_cMediaTypeLeafLock;
};

}
