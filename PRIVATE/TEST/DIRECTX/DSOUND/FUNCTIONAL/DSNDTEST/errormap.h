//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//
#ifndef _errormap_h
#define _errormap_h
#include "MAP.H"

#undef QueryMMRESULT
#undef CheckMMRESULT
#undef CheckCondition
#undef QueryCondition

#define QueryMMRESULT(_retval,_string,_result, _cause) \
	if(_retval!=DS_OK) { \
		LOG(TEXT("%s returned %s(%u) [%s:%u]\n"),TEXT(#_string),g_ErrorMap[_retval],_retval,TEXT(__FILE__),__LINE__); \
		LOG(TEXT("Possible Cause:  %s\n"),TEXT(#_cause)); \
		tr|=_result; \
	}

#define CheckMMRESULT(_retval,_string,_result, _cause) \
	if(_retval!=DS_OK) { \
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

extern Map<HRESULT> g_ErrorMap;

#endif

