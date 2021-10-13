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
*wsplitpath_s.c - break down path name into components
*
*   Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*   To provide support for accessing the individual components of an
*   arbitrary path name
*
*Revision History:
*   08-05-04  AC    Created from splitpath.c and tsplitpath_s.inl
*   08-12-04  AC    Clean up
*
*******************************************************************************/

#include <stdlib.h>
#include <mbstring.h>
#include <internal_securecrt.h>

#define _FUNC_PROLOGUE
#define _FUNC_NAME _wsplitpath_s
#define _CHAR wchar_t
#define _TCSNCPY_S wcsncpy_s
#ifdef _WIN32_WCE // CE's Windows.h includes tchar.h 
#undef _T
#endif //_WIN32_WCE
#define _T(_Character) L##_Character
#define _MBS_SUPPORT 0

#include <tsplitpath_s.inl>
