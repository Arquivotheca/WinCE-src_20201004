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
#include "SampleProducer.h"

using namespace MSDvr;

static std::list<ISampleProducer*> g_listpisampleproducer;
static CCritSec g_cCritSec;

typedef std::list<ISampleProducer *>::iterator ListCSampleProducerIterator;

HRESULT CSampleProducerLocator::BindToSampleProducer(
		/* [in] */ ISampleConsumer *piSampleConsumer,
		/* [in] */ LPCOLESTR pszRecordingName)
{
	ISampleProducer *piSampleProducer = NULL;

	{
		CAutoLock cAutoLock(&g_cCritSec);

		for (ListCSampleProducerIterator iter = g_listpisampleproducer.begin();
			iter != g_listpisampleproducer.end();
			++iter)
		{
			piSampleProducer = *iter;
			if (piSampleProducer->IsHandlingFile(pszRecordingName))
				goto BindTheConsumer;
		}
	}
	DbgLog((LOG_SOURCE_STATE, 2, _T("CSampleProducerLocator: unable to bind consumer %p"),
			piSampleConsumer));
	return E_FAIL;

BindTheConsumer:
	HRESULT hr = piSampleProducer->BindConsumer(piSampleConsumer, pszRecordingName);
	DbgLog((LOG_SOURCE_STATE, 2, _T("CSampleProducerLocator: bound producer %p to consumer %p -- status %d"),
							piSampleProducer, piSampleConsumer, hr));
	return hr;
}

HRESULT CSampleProducerLocator::RegisterSampleProducer(
		ISampleProducer *piSampleProducer)
{
	CAutoLock cAutoLock(&g_cCritSec);

	g_listpisampleproducer.push_back(piSampleProducer);
	DbgLog((LOG_ENGINE_OTHER, 2, _T("CSampleProducerLocator: registered producer %p"),
			piSampleProducer));
	return S_OK;
}
	
void CSampleProducerLocator::UnregisterSampleProducer(
		const ISampleProducer *piSampleProducer)
{
	CAutoLock cAutoLock(&g_cCritSec);

	for (ListCSampleProducerIterator iter = g_listpisampleproducer.begin();
		iter != g_listpisampleproducer.end();
		++iter)
	{
		if (*iter == piSampleProducer)
		{
			g_listpisampleproducer.erase(iter);
			DbgLog((LOG_ENGINE_OTHER, 2, _T("CSampleProducerLocator: un registered producer %p"),
						piSampleProducer));
			return;
		}
	}
}

HRESULT CSampleProducerLocator::GetSourceGraphClock(LPCOLESTR pszLiveTvToken, IReferenceClock** ppIReferenceClock)
{
	CAutoLock cAutoLock(&g_cCritSec);
	// Validate input
	if (ppIReferenceClock == NULL)
	{
		return E_INVALIDARG;
	}
	*ppIReferenceClock = NULL;

	for (ListCSampleProducerIterator iter = g_listpisampleproducer.begin();
		iter != g_listpisampleproducer.end();
		++iter)
	{
		ISampleProducer *piSampleProducer = *iter;
#if 0
		if (piSampleProducer->IsLiveTvToken(pszLiveTvToken))
#else
		if (piSampleProducer->IsHandlingFile(pszLiveTvToken))
#endif
		{
			return piSampleProducer->GetGraphClock(ppIReferenceClock);
		}
	}
	return E_FAIL;
}

bool CSampleProducerLocator::IsLiveTvToken(LPCOLESTR pszLiveTvToken)
{
	CAutoLock cAutoLock(&g_cCritSec);

	for (ListCSampleProducerIterator iter = g_listpisampleproducer.begin();
		iter != g_listpisampleproducer.end();
		++iter)
	{
		const ISampleProducer *piSampleProducer = *iter;
		if (piSampleProducer->IsLiveTvToken(pszLiveTvToken))
			return true;
	}
	return false;
}
