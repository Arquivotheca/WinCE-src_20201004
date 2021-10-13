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
//  TUXTEST TUX DLL
//
//  Module: main.h
//          Header for all files in the project.
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////

#ifndef __MAIN_DEFAULT_H__
#define __MAIN_DEFAULT_H__

////////////////////////////////////////////////////////////////////////////////
// Included files

#include <windows.h>
#include <tchar.h>

#define FAIL Debug
#define ERRFAIL Debug
#define ABORT Debug
#define ERRABORT Debug
#define WARN Debug
#define ERRWARN Debug
#define ERR Debug
#define DETAIL Debug
#define PASS Debug
#define COMMENT Debug
#define SKIP Debug
#define UNIMPL Debug

#define Log Debug
void Debug(LPCTSTR szFormat, ...) ;


#endif // __MAIN_DEFAULT_H__
