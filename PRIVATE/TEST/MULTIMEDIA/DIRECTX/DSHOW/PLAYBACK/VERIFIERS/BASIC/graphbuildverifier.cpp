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
////////////////////////////////////////////////////////////////////////////////
//
//  Verifier objects
//
//
////////////////////////////////////////////////////////////////////////////////

#include "logging.h"
#include "globals.h"
#include "TestDesc.h"
#include "TestGraph.h"
#include "PinEnumerator.h"
#include "StringConversions.h"
#include "utility.h"
#include "BasicVerifiers.h"

HRESULT CreateGraphBuildVerifier(VerificationType type, void* pVerificationData, TestGraph* pTestGraph, IGraphVerifier** ppGraphVerifier)
{
	HRESULT hr = S_OK;
	IGraphVerifier* pVerifier = NULL;

	switch(type)
	{
	case CorrectGraph:
		pVerifier = new GraphBuildVerifier();
		if (!pVerifier)
			hr = E_OUTOFMEMORY;
		break;
	
	default:
		hr = E_NOTIMPL;
		break;
	};
	
	if (SUCCEEDED(hr))
	{
		hr = pVerifier->Init(pTestGraph, type, pVerificationData);
		if (FAILED(hr))
			delete pVerifier;
		else 
			*ppGraphVerifier = pVerifier;
	}
	
	return hr;
}

GraphBuildVerifier::GraphBuildVerifier()
{
	// Set the type to something not valid
	m_VerificationType = VerificationEndMarker;
	m_pTestGraph = NULL;
	m_hr = S_OK;
}

GraphBuildVerifier::~GraphBuildVerifier()
{
}

HRESULT GraphBuildVerifier::Init(TestGraph* pTestGraph, VerificationType type, void* pVerificationData)
{
	// Store the verification requested
	m_VerificationType = type;

	// Store the test graph
	m_pTestGraph = pTestGraph;

	// Store the filter list passed in
	m_FilterList = ((GraphDesc*)pVerificationData)->GetFilterList();
	return S_OK;
}

HRESULT GraphBuildVerifier::GetResult(VerificationType type, void *pVerificationData, TCHAR* pResultStr)
{
	m_hr = m_pTestGraph->AreFiltersInGraph(m_FilterList);
	// There is no verification data to pass back 
	return m_hr;
}

HRESULT GraphBuildVerifier::ProcessEvent(GraphEvent event, void* pEventData)
{
	return S_OK;
}

HRESULT GraphBuildVerifier::Start()
{
	return S_OK;
}

void GraphBuildVerifier::Stop()
{
}

void GraphBuildVerifier::Reset()
{
	m_hr = S_OK;
}
