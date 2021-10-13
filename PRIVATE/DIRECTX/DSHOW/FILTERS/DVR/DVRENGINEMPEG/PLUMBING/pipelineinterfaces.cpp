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
// PipelineInterfaces.cpp : Default implementations of internal interfaces
// exposed by sink and source filter pipeline components.
//

#include "stdafx.h"
#include "..\\PipelineInterfaces.h"

namespace MSDvr
{

CPipelineUnknownBase::CPipelineUnknownBase() :
	m_pcUnknown			( NULL )
{
}

IPipelineManager::~IPipelineManager()
{
}

IPipelineComponent::~IPipelineComponent()
{
}

ROUTE IPipelineComponent::GetPrivateInterface(REFIID, void*&)
{
	return UNHANDLED_CONTINUE;
}

ROUTE IPipelineComponent::NotifyFilterStateChange(FILTER_STATE)
{
	return UNHANDLED_CONTINUE;
}

ROUTE IPipelineComponent::ConfigurePipeline(UCHAR, CMediaType[], UINT, BYTE[])
{
	return UNHANDLED_CONTINUE;
}

ROUTE IPipelineComponent::DispatchExtension(
						// @parm The extension to be dispatched.
						CExtendedRequest &rcExtendedRequest)
{
	return UNHANDLED_CONTINUE;
}

ROUTE ICapturePipelineComponent::ProcessInputSample(IMediaSample&, CDVRInputPin&)
{
	return UNHANDLED_CONTINUE;
}

ROUTE ICapturePipelineComponent::NotifyBeginFlush(CDVRInputPin&)
{
	return UNHANDLED_CONTINUE;
}

ROUTE ICapturePipelineComponent::NotifyEndFlush(CDVRInputPin&)
{
	return UNHANDLED_CONTINUE;
}

ROUTE ICapturePipelineComponent::GetAllocatorRequirements(ALLOCATOR_PROPERTIES&, CDVRInputPin&)
{
	return UNHANDLED_CONTINUE;
}

ROUTE ICapturePipelineComponent::NotifyAllocator(IMemAllocator&, bool, CDVRInputPin&)
{
	return UNHANDLED_CONTINUE;
}
ROUTE ICapturePipelineComponent::NotifyNewSegment(
						CDVRInputPin&	rcInputPin,
						REFERENCE_TIME rtStart,
						REFERENCE_TIME rtEnd,
						double dblRate)
{
	return UNHANDLED_CONTINUE;
}

ROUTE ICapturePipelineComponent::CheckMediaType(const CMediaType&, CDVRInputPin&, HRESULT&)
{
	return UNHANDLED_CONTINUE;
}

ROUTE ICapturePipelineComponent::CompleteConnect(IPin& riReceivePin, CDVRInputPin& rcInputPin)
{
	return UNHANDLED_CONTINUE;
}

ROUTE IPlaybackPipelineComponent::ProcessOutputSample(IMediaSample&, CDVROutputPin&)
{
	return UNHANDLED_CONTINUE;
}

ROUTE IPlaybackPipelineComponent::DecideBufferSize(IMemAllocator&, ALLOCATOR_PROPERTIES&, CDVROutputPin&)
{
	return UNHANDLED_CONTINUE;
}

ROUTE IPlaybackPipelineComponent::CheckMediaType(const CMediaType&, CDVROutputPin&, HRESULT&)
{
	return UNHANDLED_CONTINUE;
}

ROUTE IPlaybackPipelineComponent::EndOfStream()
{
	return UNHANDLED_CONTINUE;
}

}
