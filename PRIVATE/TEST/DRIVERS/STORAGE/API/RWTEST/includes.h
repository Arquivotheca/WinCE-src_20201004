//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
