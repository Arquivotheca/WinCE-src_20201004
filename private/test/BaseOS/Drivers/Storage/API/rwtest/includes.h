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
/*
-------------------------------------------------------------------------------

Module Name:

    includes.h

Description:

    Header for all files in the project

-------------------------------------------------------------------------------
*/

#ifndef __INCLUDES_H__
#define __INCLUDES_H__

// ----------------------------------------------------------------------------
// Make sure _WIN32_WCE is set when building for Windows CE
// ----------------------------------------------------------------------------

#if defined(UNDER_CE) && !defined(_WIN32_WCE)
#define _WIN32_WCE
#endif


// ----------------------------------------------------------------------------
// Includes
// ----------------------------------------------------------------------------

#include <windows.h>
#include <tchar.h>
#include <kato.h>
#include <tux.h>
#include <diskio.h>
#include <stdlib.h>

#ifndef _WIN32_WCE
#include <stdio.h>
#endif

// ----------------------------------------------------------------------------
// Kato Logging Constants
// ----------------------------------------------------------------------------

#define LOG_EXCEPTION            0
#define LOG_FAIL                 2
#define LOG_ABORT                4
#define LOG_SKIP                 6
#define LOG_NOT_IMPLEMENTED      8
#define LOG_PASS                10
#define LOG_DETAIL              12
#define LOG_COMMENT             14

// ----------------------------------------------------------------------------
// Windows CE specific code
// ----------------------------------------------------------------------------

#ifdef _WIN32_WCE

#ifndef STARTF_USESIZE
#define STARTF_USESIZE      0x00000002
#endif

#ifndef STARTF_USEPOSITION
#define STARTF_USEPOSITION 0x00000004
#endif

#ifndef ZeroMemory
#define ZeroMemory(Destination,Length) memset(Destination, 0, Length)
#endif

#ifndef _vsntprintf
#define _vsntprintf(d,c,f,a) wvsprintf(d,f,a)
#endif

#endif

// ----------------------------------------------------------------------------
// Macros
// ----------------------------------------------------------------------------

#define countof(a) (sizeof(a)/sizeof(*(a)))

#endif // __INCLUDES_H__
