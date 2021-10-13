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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#ifndef _errormap_h
#define _errormap_h
#include "MAP.H"

#undef QueryMMRESULT
#undef CheckMMRESULT
#undef CheckCondition
#undef QueryCondition
#undef QueryMMRESULT_mmRtn
#undef CheckMMRESULT_mmRtn
#undef CheckCondition_mmRtn
#undef QueryCondition_mmRtn


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

#define QueryMMRESULT_mmRtn(_retval,_string,_result, _cause) \
	if(_retval!=MMSYSERR_NOERROR) { \
		LOG(TEXT("%s returned %s(%u) [%s:%u]\n"),TEXT(#_string),g_ErrorMap[_retval],_retval,TEXT(__FILE__),__LINE__); \
		LOG(TEXT("Possible Cause:  %s\n"),TEXT(#_cause)); \
		mmRtn = GetReturnCode(mmRtn, _result); \
	}


#define CheckMMRESULT_mmRtn(_retval,_string,_result, _cause) \
	if(_retval!=MMSYSERR_NOERROR) { \
		LOG(TEXT("%s returned %s(%u) [%s:%u]\n"),TEXT(#_string),g_ErrorMap[_retval],_retval,TEXT(__FILE__),__LINE__); \
		LOG(TEXT("Possible Cause:  %s\n"),TEXT(#_cause)); \
		mmRtn = GetReturnCode(mmRtn, _result); \
		goto Error; \
	}


#define CheckCondition_mmRtn(_cond,_string,_result,_cause) \
	if(_cond) { \
		LOG(TEXT("%s (%s) [%s:%u]\n"),TEXT(#_string),TEXT(#_cond),TEXT(__FILE__),__LINE__); \
		LOG(TEXT("Possible Cause:  %s\n"),TEXT(#_cause)); \
		mmRtn = GetReturnCode(mmRtn, _result); \
		goto Error; \
	}

#define QueryCondition_mmRtn(_cond,_string,_result,_cause) \
	if(_cond) { \
		LOG(TEXT("%s (%s) [%s:%u]\n"),TEXT(#_string),TEXT(#_cond),TEXT(__FILE__),__LINE__); \
		LOG(TEXT("Possible Cause:  %s\n"),TEXT(#_cause)); \
 		mmRtn = GetReturnCode(mmRtn, _result); \
	}

extern Map<MMRESULT> g_ErrorMap;

#endif
