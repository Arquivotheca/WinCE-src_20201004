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
//  Copyright (c) Microsoft Corporation
//
//  Module: main.h
//          Header for all files in the project.
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////

#pragma once

////////////////////////////////////////////////////////////////////////////////
// Included files

#include <windows.h>
#include <stdlib.h>
#include <tchar.h>
#include <tux.h>
#include <kato.h>

////////////////////////////////////////////////////////////////////////////////
// Suggested log verbosities

#define LOG_EXCEPTION          0
#define LOG_FAIL               2
#define LOG_ABORT              4
#define LOG_SKIP               6
#define LOG_NOT_IMPLEMENTED    8
#define LOG_PASS              10
#define LOG_DETAIL            12
#define LOG_COMMENT           14

////////////////////////////////////////////////////////////////////////////////
// Macros for Logging with different Log Verbosities

#define LOGF_EXCEPTION(szText) \
	g_pKato->Log (LOG_EXCEPTION, szText TEXT(" at %s:%d\r\n"), TEXT(__FILE__), __LINE__)
#define LOGF_FAIL(szText) \
	g_pKato->Log (LOG_FAIL, szText TEXT(" at %s:%d\r\n"), TEXT(__FILE__), __LINE__)
#define LOGF_ABORT(szText) \
	g_pKato->Log (LOG_ABORT, szText TEXT(" at %s:%d\r\n"), TEXT(__FILE__), __LINE__)
#define LOGF_SKIP(szText) \
	g_pKato->Log (LOG_SKIP, szText TEXT(" at %s:%d\r\n"), TEXT(__FILE__), __LINE__)
#define LOGF_NOT_IMPLEMENTED(szText) \
	g_pKato->Log (LOG_NOT_IMPLEMENTED, szText TEXT(" at %s:%d\r\n"), TEXT(__FILE__), __LINE__)
#define LOGF_PASS(szText) \
	g_pKato->Log (LOG_PASS, szText TEXT(" at %s:%d\r\n"), TEXT(__FILE__), __LINE__)
#define LOGF_DETAIL(szText) \
	g_pKato->Log (LOG_DETAIL, szText TEXT(" at %s:%d\r\n"), TEXT(__FILE__), __LINE__)
#define LOGF_COMMENT(szText) \
	g_pKato->Log (LOG_COMMENT, szText TEXT(" at %s:%d\r\n"), TEXT(__FILE__), __LINE__)





