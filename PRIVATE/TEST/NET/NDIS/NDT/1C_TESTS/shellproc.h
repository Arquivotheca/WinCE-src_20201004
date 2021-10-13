//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __SHELLPROC_H
#define __SHELLPROC_H

//------------------------------------------------------------------------------

#include "tux.h"
#include "kato.h"

//------------------------------------------------------------------------------

// Our function table that we pass to Tux
extern FUNCTION_TABLE_ENTRY g_lpFTE[];

// Macros for common test constructs
#define TEST_FUNCTION(name) \
   TESTPROCAPI name( \
      UINT uMsg, \
      TPPARAM tpParam, \
      LPFUNCTION_TABLE_ENTRY lpFTE \
   )

// Check our message value to see why we have been called.	
#define TEST_ENTRY \
if (uMsg == TPM_QUERY_THREAD_COUNT) { \
   ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = lpFTE->dwUserData; \
   return SPR_HANDLED; \
} else if (uMsg != TPM_EXECUTE) { \
   return TPR_NOT_HANDLED; \
}

//------------------------------------------------------------------------------

extern CKato* g_pKato;

//------------------------------------------------------------------------------

#endif // __SHELLPROC_H
