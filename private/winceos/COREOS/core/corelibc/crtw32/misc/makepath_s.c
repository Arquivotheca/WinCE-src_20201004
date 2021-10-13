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
/***
*makepath_s.c - create path name from components
*
*   Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*   To provide support for creation of full path names from components
*
*Revision History:
*   08-05-04  AC    Created from makepath.c and tmakepath_s.inl
*
*******************************************************************************/

#include <stdlib.h>
#include <mbstring.h>
#include <internal_securecrt.h>

#define _FUNC_PROLOGUE
#define _FUNC_NAME _makepath_s
#define _CHAR char
#define _DEST _Dst
#define _SIZE _SizeInBytes
#ifdef _WIN32_WCE // CE's Windows.h includes tchar.h 
#undef _T
#endif // _WIN32_WCE
#define _T(_Character) _Character

#if defined(_NTSUBSET_)
#define _MBS_SUPPORT 0
#else
#define _MBS_SUPPORT 1
#endif

#include <tmakepath_s.inl>
