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
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


Module Name:

    CeAssert.h

Abstract:

    CeVersion of the Assert Macros'

Author:

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

