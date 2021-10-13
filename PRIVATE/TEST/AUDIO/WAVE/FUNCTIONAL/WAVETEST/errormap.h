//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef _errormap_h
#define _errormap_h
#include "MAP.H"

#undef QueryMMRESULT
#undef CheckMMRESULT
#undef CheckCondition
#undef QueryCondition


#define QueryMMRESULT(_retval,_string,_result, _cause) \
	if(_retval!=MMSYSERR_NOERROR) { \
		LOG(TEXT("%s returned %s(%u) [%s:%u]\n"),TEXT(#_string),g_ErrorMap[_retval],_retval,TEXT(__FILE__),__LINE__); \
		LOG(TEXT("Possible Cause:  %s\n"),TEXT(#_cause)); \
		tr|=_result; \
	}

#define CheckMMRESULT(_retval,_string,_result, _cause) \
	if(_retval!=MMSYSERR_NOERROR) { \
		LOG(TEXT("%s returned %s(%u) [%s:%u]\n"),TEXT(#_string),g_ErrorMap[_retval],_retval,TEXT(__FILE__),__LINE__); \
		LOG(TEXT("Possible Cause:  %s\n"),TEXT(#_cause)); \
		tr|=_result; \
		goto Error; \
	}


#define CheckCondition(_cond,_string,_result,_cause) \
	if(_cond) { \
		LOG(TEXT("%s (%s) [%s:%u]\n"),TEXT(#_string),TEXT(#_cond),TEXT(__FILE__),__LINE__); \
		LOG(TEXT("Possible Cause:  %s\n"),TEXT(#_cause)); \
		tr|=_result; \
		goto Error; \
	}

#define QueryCondition(_cond,_string,_result,_cause) \
	if(_cond) { \
		LOG(TEXT("%s (%s) [%s:%u]\n"),TEXT(#_string),TEXT(#_cond),TEXT(__FILE__),__LINE__); \
		LOG(TEXT("Possible Cause:  %s\n"),TEXT(#_cause)); \
 		tr|=_result; \
	}

extern Map<MMRESULT> g_ErrorMap;

#endif
