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
//  Verifier list class
//
//
////////////////////////////////////////////////////////////////////////////////

#include "Verification.h"

//basic verifiers
extern int g_BasicVerifierObjTableSize;
extern "C" {VerifierObj g_BasicVerifierObjTable[];}


//audio verifiers
extern int g_AudioVerifierObjTableSize;
extern "C" {VerifierObj g_AudioVerifierObjTable[];}


//latency verifiers
extern int g_LatencyVerifierObjTableSize;
extern "C" {VerifierObj g_LatencyVerifierObjTable[];}


//Misc Verifiers
extern int g_MiscVerifierObjTableSize;
extern "C" {VerifierObj g_MiscVerifierObjTable[];}


//Seek Verifiers
extern int g_SeekVerifierObjTableSize;
extern "C" {VerifierObj g_SeekVerifierObjTable[];}


//Video Verifiers
extern int g_VideoVerifierObjTableSize;
extern "C" {VerifierObj g_VideoVerifierObjTable[];}

//currently we have the following  6 categories of verifiers
//this helps us abstract from the categories

int VerifierMgr::GetNumVerifiers()
{
	int NumVerifiers = 0;

#ifdef AUDIO_VERIFIERS
	
	NumVerifiers = NumVerifiers + g_AudioVerifierObjTableSize;

#endif

#ifdef BASIC_VERIFIERS
	
	NumVerifiers = NumVerifiers + g_BasicVerifierObjTableSize;

#endif

#ifdef LATENCY_VERIFIERS

	NumVerifiers = NumVerifiers + g_LatencyVerifierObjTableSize;

#endif

#ifdef MISC_VERIFIERS
	
	NumVerifiers = NumVerifiers + g_MiscVerifierObjTableSize;

#endif

#ifdef SEEK_VERIFIERS
	
	NumVerifiers = NumVerifiers + g_SeekVerifierObjTableSize;

#endif

#ifdef VIDEO_VERIFIERS
	
	NumVerifiers = NumVerifiers + g_VideoVerifierObjTableSize;

#endif

	return NumVerifiers;

}

VerifierObj* VerifierMgr::GetVerifier(int index)
{
	VerifierObj* verifier = NULL;
	int count = 0;

#ifdef AUDIO_VERIFIERS
	
	if((index < g_AudioVerifierObjTableSize) && (index >-1))
		verifier = &(g_AudioVerifierObjTable[index]);
	count = g_AudioVerifierObjTableSize;

#endif

#ifdef BASIC_VERIFIERS
	
	if((index >= count) && (index <(count + g_BasicVerifierObjTableSize)))
		verifier = &(g_BasicVerifierObjTable[index - count]);
	count = count + g_BasicVerifierObjTableSize;

#endif

#ifdef LATENCY_VERIFIERS
	
	if((index >= count) && (index <(count + g_LatencyVerifierObjTableSize)))
		verifier = &(g_LatencyVerifierObjTable[index - count]);
	count = count + g_LatencyVerifierObjTableSize;

#endif

#ifdef MISC_VERIFIERS
	
	if((index >= count) && (index <(count + g_MiscVerifierObjTableSize)))
		verifier = &(g_MiscVerifierObjTable[index - count]);
	count = count + g_MiscVerifierObjTableSize;


#endif

#ifdef SEEK_VERIFIERS
	
	if((index >= count) && (index <(count + g_SeekVerifierObjTableSize)))
		verifier = &(g_SeekVerifierObjTable[index - count]);
	count = count + g_SeekVerifierObjTableSize;


#endif

#ifdef VIDEO_VERIFIERS
	
	if((index >= count) && (index <(count + g_VideoVerifierObjTableSize)))
		verifier = &(g_VideoVerifierObjTable[index - count]);
	count = count + g_VideoVerifierObjTableSize;


#endif

	return verifier;

}



