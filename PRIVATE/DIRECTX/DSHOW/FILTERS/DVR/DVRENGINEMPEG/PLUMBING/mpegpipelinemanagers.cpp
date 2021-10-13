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
// MPEGPipelineManagers.h : Classes for managing MPEG capture and playback
// pipelines.
//

#include "stdafx.h"
#include "MPEGPipelineManagers.h"

#include "Sink.h"
#include "Source.h"
#include "..\\SampleProducer\\MPEGSampleProducer.h"
#include "..\\SampleConsumer\\SampleConsumer.h"
#include "..\\DecoderDriver\\MPEGDecoderDriver.h"
#include "..\\Writer\\Writer.h"
#include "..\\Encrypter\\Encrypter.h"
#include "..\\Encrypter\\Decrypter.h"
#include "..\\Tagger\\ContentTagger.h"
#include "..\\Detagger\\ContentDetagger.h"
#include "MpegEncoderReceiver.h"

namespace MSDvr
{

//////////////////////////////////////////////////////////////////////////////
//	MPEGCapturePipelineManager.												//
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//	Construction/Destruction.												//
//////////////////////////////////////////////////////////////////////////////

CMPEGCapturePipelineManager::CMPEGCapturePipelineManager(
		CDVRSinkFilter& rcFilter,
		ICapturePipelineManager*& rpiPipelineManager) :
	CCapturePipelineManager	(rcFilter),
	m_rpiPipelineManager	(rpiPipelineManager)
{
	try
	{
		// Prepare mpeg analyzer
		std::auto_ptr<ICapturePipelineComponent> spEncoderReceiver(new CMpegEncoderReceiver);
		m_cBasePipelineManager.AppendPipelineComponent(*spEncoderReceiver, true);
		spEncoderReceiver.release();

		// Prepare tagger
		std::auto_ptr<ICapturePipelineComponent> spTagger(new CContentTagger);
		m_cBasePipelineManager.AppendPipelineComponent(*spTagger, true);
		spTagger.release();

		// Prepare encrypter
		CEncrypter *pEncrypter = new CEncrypter;
		std::auto_ptr<ICapturePipelineComponent> spEncrypter(pEncrypter);
		m_cBasePipelineManager.AppendPipelineComponent(*spEncrypter, true);
		spEncrypter.release();

		// Prepare sample producer
		std::auto_ptr<ICapturePipelineComponent> spProducer(new CMPEGSampleProducer);
		m_cBasePipelineManager.AppendPipelineComponent(*spProducer, true);
		spProducer.release();

		// Prepare writer
		std::auto_ptr<ICapturePipelineComponent> spWriter(new CWriter);
		m_cBasePipelineManager.AppendPipelineComponent(*spWriter, true);
		spWriter.release();

		// Run add notification across components
		m_cBasePipelineManager.NotifyAllAddedComponents(true, *this);

		// Route all pipeline calls to this instance
		m_cBasePipelineManager.AppendPipelineComponent(m_sComponent, false);

		// Create a VBI pin
		rcFilter.AddPin(L"VBI");

		// Prepare for sample processing by initializing the pipeline with
		// configuration information
		CMediaType rgPinMediaTypes[2];
		EXECUTE_ASSERT(SUCCEEDED(rcFilter.GetPin(0)->ConnectionMediaType(&rgPinMediaTypes[0])));
		rgPinMediaTypes[1].SetType(&MEDIATYPE_AUXLine21Data);
		rgPinMediaTypes[1].SetSubtype(&MEDIASUBTYPE_Line21_BytePair);

		GetRouter(NULL, false, false).ConfigurePipeline(2,
														rgPinMediaTypes,
														pEncrypter->GetCustomDataLen(),
														pEncrypter->GetCustomData());
	}
	catch (...)
	{
		// Notify components of pipeline removal and delete them
		m_cBasePipelineManager.ClearPipeline();
		throw;
	}

	// Since everything that could possibly fail has succeeded, install this
	// pipeline manager as the active one
	rpiPipelineManager = this;
}

//////////////////////////////////////////////////////////////////////////////
//	Capture pipeline manager pure virtuals.									//
//////////////////////////////////////////////////////////////////////////////

CBasePin& CMPEGCapturePipelineManager::GetPrimary()
{
	// This manager supports a multiple pins
	return *GetSinkFilter().GetPin(0);
}

CDVRInputPin& CMPEGCapturePipelineManager::GetPrimaryInput()
{
	return static_cast <CDVRInputPin&> (GetPrimary());
}

//////////////////////////////////////////////////////////////////////////////
//	Capture pipeline manager pipeline component hooks.						//
//////////////////////////////////////////////////////////////////////////////

unsigned char CMPEGCapturePipelineManager::SComponent::
AddToPipeline(ICapturePipelineManager& rcManager)
{
	// The MPEG manager will not call this method on its own component
	ASSERT (FALSE);
	return 0;
}

void CMPEGCapturePipelineManager::SComponent::RemoveFromPipeline()
{
	// The MPEG manager will not call this method on its own component
	ASSERT (FALSE);
}

ROUTE CMPEGCapturePipelineManager::SComponent::
NotifyFilterStateChange(FILTER_STATE eState)
{
	if (State_Stopped == eState)
		// When stopping, the worker thread needs to be cleaned up
		Owner().m_cBasePipelineManager.WaitStreamingThread();
	else if (State_Paused == eState)
	{
		// Determine if we're transitioning from stopped
		FILTER_STATE State;
		EXECUTE_ASSERT (SUCCEEDED(Owner().GetSinkFilter().GetState(0, &State)));

		if (State_Stopped == State)
			Owner().m_cBasePipelineManager.StartStreamingThread();
	}

	return HANDLED_CONTINUE;
}

ROUTE CMPEGCapturePipelineManager::SComponent::
CheckMediaType(const CMediaType& rcMediaType, CDVRInputPin& rcInputPin,
			   HRESULT& rhResult)
{
	rhResult = S_FALSE;

	if ((rcInputPin.GetPinNum() == 0) &&
		IsEqualGUID(*rcMediaType.Type(), MEDIATYPE_Stream)	&&
		IsEqualGUID(*rcMediaType.Subtype(), MEDIASUBTYPE_MPEG2_PROGRAM))
	{
		rhResult = S_OK;
	}
	else if ((rcInputPin.GetPinNum() == 1) &&
			IsEqualGUID(*rcMediaType.Type(), MEDIATYPE_AUXLine21Data)	&&
			IsEqualGUID(*rcMediaType.Subtype(), MEDIASUBTYPE_Line21_BytePair))
	{
		rhResult = S_OK;
	}

	return HANDLED_CONTINUE;
}

//////////////////////////////////////////////////////////////////////////////
//	Capture pipeline manager private implementation.						//
//////////////////////////////////////////////////////////////////////////////

CMPEGCapturePipelineManager&
CMPEGCapturePipelineManager::SComponent::Owner()
{
	return *reinterpret_cast <CMPEGCapturePipelineManager*>
		(reinterpret_cast <char*> (this) -
		 offsetof(CMPEGCapturePipelineManager, m_sComponent));
}

//////////////////////////////////////////////////////////////////////////////
//	MPEGPlaybackPipelineManager.											//
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//	Construction/Destruction.												//
//////////////////////////////////////////////////////////////////////////////

CMPEGPlaybackPipelineManager::CMPEGPlaybackPipelineManager(
		CDVRSourceFilter& rcFilter,
		IPlaybackPipelineManager*& rpiPipelineManager,
		IReader& riReader,
		LPCOLESTR pszFileName,
		const AM_MEDIA_TYPE* pmt) :
	CPlaybackPipelineManager	(rcFilter),
	m_rpiPipelineManager		(rpiPipelineManager)
{
	try
	{
		// Add the reader (which has reported that it can read the indicated
		// file) to the pipeline
		m_cBasePipelineManager.AppendPipelineComponent(riReader, false);
		  // take ownership only once we know that we will not throw an
		  // exception from this constructor

		// Prepare sample consumer
		std::auto_ptr<IPlaybackPipelineComponent> spConsumer(new CSampleConsumer);
		m_cBasePipelineManager.AppendPipelineComponent(*spConsumer, true);
		spConsumer.release();  // list now owns component

		// Prepare decrypter
		std::auto_ptr<IPlaybackPipelineComponent> spDecrypter(new CDecrypter);
		m_cBasePipelineManager.AppendPipelineComponent(*spDecrypter, true);
		spDecrypter.release();

		// Prepare detagger
		std::auto_ptr<IPlaybackPipelineComponent> spDetagger(new CContentDetagger);
		m_cBasePipelineManager.AppendPipelineComponent(*spDetagger, true);
		spDetagger.release();  // list now owns component

		// Prepare decoder driver
		std::auto_ptr<IPlaybackPipelineComponent> spDecoderDriver(new CMPEGDecoderDriver);
		m_cBasePipelineManager.AppendPipelineComponent(*spDecoderDriver, true);
		spDecoderDriver.release();  // list now owns component

		// Run add notification across components
		m_cBasePipelineManager.NotifyAllAddedComponents(false, *this);

		// Route all pipeline calls to this instance
		m_cBasePipelineManager.AppendPipelineComponent(m_sComponent, false);

		// Route Load call to the components
		IFileSourceFilter* piFileSourceFilter;
		m_cBasePipelineManager.NonDelegatingQueryInterface(IID_IFileSourceFilter,
			reinterpret_cast <LPVOID&> (piFileSourceFilter));
		ASSERT (piFileSourceFilter);
		piFileSourceFilter->Release();
		piFileSourceFilter->Load(pszFileName, pmt);
	}
	catch (...)
	{
		// Notify components of pipeline removal and delete them
		m_cBasePipelineManager.ClearPipeline();
		throw;
	}

	// Since everything that could possibly fail has succeeded, install this
	// pipeline manager as the active one, and take ownership over the reader
	rpiPipelineManager = this;
	m_cBasePipelineManager.TakeHeadOwnership();
}

//////////////////////////////////////////////////////////////////////////////
//	Playback pipeline manager pure virtuals.								//
//////////////////////////////////////////////////////////////////////////////

CBasePin& CMPEGPlaybackPipelineManager::GetPrimary()
{
	return *GetSourceFilter().GetPin(0);
}

CDVROutputPin& CMPEGPlaybackPipelineManager::GetPrimaryOutput()
{
	return static_cast <CDVROutputPin&> (GetPrimary());
}

HRESULT CMPEGPlaybackPipelineManager::GetMediaType(
			int iPosition, CMediaType* pMediaType,
			CDVROutputPin& /*rcOutputPin*/)
{
	if (! pMediaType)
		return E_POINTER;

	if (iPosition < 0)
		return E_INVALIDARG;

	if (iPosition > 0)
		return VFW_S_NO_MORE_ITEMS;

	// Guard with leaf lock
	CAutoLock cLock(&m_cMediaTypeLeafLock);
	*pMediaType = m_cMediaType;

	return S_OK;
}

//////////////////////////////////////////////////////////////////////////////
//	Playback pipeline manager pipeline component hooks.						//
//////////////////////////////////////////////////////////////////////////////

unsigned char CMPEGPlaybackPipelineManager::SComponent::
AddToPipeline(IPlaybackPipelineManager& rcManager)
{
	// The MPEG manager will not call this method on its own component
	ASSERT (FALSE);
	return 0;
}

void CMPEGPlaybackPipelineManager::SComponent::RemoveFromPipeline()
{
	// The MPEG manager will not call this method on its own component
	ASSERT (FALSE);
}

ROUTE CMPEGPlaybackPipelineManager::SComponent::
NotifyFilterStateChange(FILTER_STATE eState)
{
	if (State_Stopped == eState)
		// When stopping, the worker thread needs to be cleaned up
		Owner().m_cBasePipelineManager.WaitStreamingThread();
	else if (State_Paused == eState)
	{
		// Determine if we're transitioning from stopped
		FILTER_STATE State;
		EXECUTE_ASSERT (SUCCEEDED(Owner().GetSourceFilter().GetState(0, &State)));

		if (State_Stopped == State)
			Owner().m_cBasePipelineManager.StartStreamingThread();
	}

	return HANDLED_CONTINUE;
}

ROUTE CMPEGPlaybackPipelineManager::SComponent::
ConfigurePipeline(UCHAR iNumPins, CMediaType cMediaTypes[], UINT iSizeCustom,
				  BYTE /*Custom*/[])
{
	DbgLog((ZONE_SOURCE_DISPATCH, 3, _T("DVR source filter: pipeline configuration request received")));

	UNUSED (iNumPins);
	UNUSED (cMediaTypes);
	UNUSED (iSizeCustom);

	ASSERT(iNumPins == 1);
	ASSERT (IsEqualGUID(*cMediaTypes[0].Type(), MEDIATYPE_Stream)
		&&	IsEqualGUID(*cMediaTypes[0].Subtype(), MEDIASUBTYPE_MPEG2_PROGRAM));

	// Add the output pin only on the first call to this method
	if (!Owner().GetSourceFilter().GetPinCount())
	{
		Owner().GetSourceFilter().AddPin(L"MPEGStream");

		// Make media type information available to the pin - and recall that
		// ConfigurePipeline could be called by an app or streaming thread
		CAutoLock cLock(&Owner().m_cMediaTypeLeafLock);
		Owner().m_cMediaType = cMediaTypes[0];
	}

	return HANDLED_CONTINUE;
}

ROUTE CMPEGPlaybackPipelineManager::SComponent::
ProcessOutputSample(IMediaSample& riSample, CDVROutputPin& rcOutputPin)
{
	DbgLog((ZONE_SOURCE_DISPATCH, 3, _T("DVR source filter: sample delivery request received")));

	const HRESULT hr = rcOutputPin.Deliver(&riSample);

	UNUSED (hr);
	DbgLog((ZONE_SOURCE_DISPATCH, 3, _T("DVR source filter: sample delivered: %X"), hr));
	return HANDLED_CONTINUE;
}

ROUTE CMPEGPlaybackPipelineManager::SComponent::
CheckMediaType(const CMediaType& rcMediaType, CDVROutputPin& /*rcOutputPin*/,
			   HRESULT& rhResult)
{
	// For now, once connected with the MPEG format, all further connections
	// to the filter must be made with this same format
	if (IsEqualGUID(*rcMediaType.Type(), MEDIATYPE_Stream)
	&&	IsEqualGUID(*rcMediaType.Subtype(), MEDIASUBTYPE_MPEG2_PROGRAM))

		rhResult = S_OK;
	else
		rhResult = S_FALSE;

	return HANDLED_CONTINUE;
}

ROUTE CMPEGPlaybackPipelineManager::SComponent::EndOfStream()
{
	DbgLog((ZONE_EVENT_DETECTED, 3, _T("DVR source filter: end of stream delivery request received")));

	// This method must route to all output pins; for this pipeline manager,
	// this means one
	const HRESULT hr = Owner().GetPrimaryOutput().DeliverEndOfStream();

	UNUSED (hr);
	DbgLog((ZONE_SOURCE_DISPATCH, 3, _T("DVR source filter: end of stream delivered: %X"), hr));
	return HANDLED_CONTINUE;
}

//////////////////////////////////////////////////////////////////////////////
//	Playback pipeline manager private implementation.						//
//////////////////////////////////////////////////////////////////////////////

CMPEGPlaybackPipelineManager&
CMPEGPlaybackPipelineManager::SComponent::Owner()
{
	return *reinterpret_cast <CMPEGPlaybackPipelineManager*>
		(reinterpret_cast <char*> (this) -
		 offsetof(CMPEGPlaybackPipelineManager, m_sComponent));
}

}
