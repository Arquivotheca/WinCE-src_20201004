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
#include "Verification.h"

//singleton verifier manager class. Everyone uses this class
VerifierMgr g_verifierMgr;

const CHAR* GetVerificationString(VerificationType type)
{
	VerifierObj* vTemp = NULL;
	int iSize = g_verifierMgr.GetNumVerifiers();
	
	for(int i = 0; i < iSize; i++)
	{
		vTemp = g_verifierMgr.GetVerifier(i);
		if ((vTemp->verificationType== type) && vTemp->szVerificationType)
		{
			return vTemp->szVerificationType;
		}
	}

	return NULL;
}

bool IsVerifiable(VerificationType type)
{

	VerifierObj* vTemp = NULL;
	int iSize = g_verifierMgr.GetNumVerifiers();
	
	for(int i = 0; i < iSize; i++)
	{
		vTemp = g_verifierMgr.GetVerifier(i);
		if ((vTemp->verificationType== type) && vTemp->fn)
		{
			return true;
		}
	}

	return false;
}

HRESULT CreateGraphVerifier(VerificationType type, void* pVerificationData, TestGraph* pTestGraph, IGraphVerifier** ppGraphVerifier)
{
	HRESULT hr = E_NOTIMPL;
	VerifierObj* vTemp = NULL;
	int iSize = g_verifierMgr.GetNumVerifiers();	
	
	for(int i = 0; i < iSize; i++)
	{
		vTemp = g_verifierMgr.GetVerifier(i);
		if ((vTemp->verificationType == type) && vTemp->fn)
		{
			return vTemp->fn(type, pVerificationData, pTestGraph, ppGraphVerifier);
		}
	}
	
	return hr;
}

// This is the call back method from the tap filter. This call needs to be relayed to the verifier
HRESULT GenericTapFilterCallback(GraphEvent event, void* pGraphEventData, void* pTapCallbackData, void* pObject)
{
	IGraphVerifier *pVerifier = (IGraphVerifier*)pObject;
	return pVerifier->ProcessEvent(event, pGraphEventData);
}
