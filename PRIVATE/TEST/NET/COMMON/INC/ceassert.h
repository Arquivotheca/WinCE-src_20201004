//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


Module Name:

    CeAssert.h

Abstract:

    CeVersion of the Assert Macros'



    YOUR-FULL-NAME (YOUR-EMAIL-NAME) **creation-date-dd-mmm-yyyy**

-------------------------------------------------------------------*/
#ifndef __CEASSERT_H__
#define __CEASSERT_H__

#include <windows.h>

#ifdef UNDER_CE

#ifdef _DEBUG

#define _ASSERT(f)	  		\
	if( !(f) )					\
	{						\
		NKDbgPrintfW( L"\n\rAssertion failed: %hs line %u\n\r", __FILE__, __LINE__ );	\
		DebugBreak();	\
	}
#else

#define _ASSERT( f ) 

#endif // DEBUG

#endif // UNDER_CE

#endif // __CEASSERT_H__

