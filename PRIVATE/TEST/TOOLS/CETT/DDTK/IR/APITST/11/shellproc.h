//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//

#ifndef __SHELLPROC_H__
#define __SHELLPROC_H__


// Macros for common test constructs

#define TEST_FUNCTION(name) \
	TESTPROCAPI name( \
		UINT uMsg, \
		TPPARAM tpParam, \
		LPFUNCTION_TABLE_ENTRY lpFTE) 

// Check our message value to see why we have been called.	

#define TEST_ENTRY \
	if (uMsg == TPM_QUERY_THREAD_COUNT) \
	{ \
		((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = lpFTE->dwUserData; \
		return SPR_HANDLED; \
	} \
	else if (uMsg != TPM_EXECUTE) \
	{ \
		return TPR_NOT_HANDLED; \
	}

extern int g_af;
extern char *g_szServerName;
extern FUNCTION_TABLE_ENTRY g_lpFTE[];


#endif // __SHELLPROC_H__
