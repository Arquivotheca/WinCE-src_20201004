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

#ifndef __MAIN_TUX_H__
#define __MAIN_TUX_H__

////////////////////////////////////////////////////////////////////////////////
// Included files

#include <windows.h>
#include <stdlib.h>
#include <tchar.h>
#include <stressutils.h>

#define FAIL LogFail
#define ERRFAIL LogFail
#define ABORT LogFail
#define ERRABORT LogFail
#define WARN LogWarn1
#define ERRWARN LogWarn1
#define ERR LogFail
#define DETAIL LogVerbose
#define PASS LogVerbose
#define COMMENT LogVerbose
#define SKIP LogVerbose
#define UNIMPL LogVerbose

#define Log LogVerbose
void Debug(LPCTSTR szFormat, ...) ;

#endif // __MAIN_TUX_H__
