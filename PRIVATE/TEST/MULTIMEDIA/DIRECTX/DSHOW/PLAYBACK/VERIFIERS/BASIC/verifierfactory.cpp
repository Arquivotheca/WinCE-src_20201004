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

#include "BasicVerifiers.h"
#include "TestDescParser.h"

extern "C" {VerifierObj g_BasicVerifierObjTable[] = 
{
	{CorrectGraph, STR_CorrectGraph, ParseVerifyGraphDesc, CreateGraphBuildVerifier},
	{VerifyPlaybackDuration, STR_PlaybackDuration, ParseVerifyPlaybackDuration, CreatePlaybackDurationVerifier},
	{VerifySampleDelivered, STR_VerifySampleDelivered, ParseVerifySampleDelivered, CreateSampleDeliveryVerifier}
};}

int g_BasicVerifierObjTableSize = sizeof(g_BasicVerifierObjTable)/sizeof(VerifierObj);
