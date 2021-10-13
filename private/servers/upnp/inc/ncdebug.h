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
//+---------------------------------------------------------------------------
//
//  Microsoft Windows
//
//  File:       N C D E B U G . H
//
//  Contents:   Debug routines.
//
//  Notes:
//
//  Author:     danielwe   24 Mar 1997
//
//----------------------------------------------------------------------------

#pragma once
#ifndef _NCDEBUG_H_
#define _NCDEBUG_H_

#include "trace.h"


//
//  "Normal" assertion checking.  Provided for compatibility with
//  imported code.
//
//      Assert(a)       Displays a message indicating the file and line number
//                      of this Assert() if a == 0.
//      AssertSz(a,b)   As Assert(); also displays the message b (which should
//                      be a string literal.)
//      SideAssert(a)   As Assert(); the expression a is evaluated even if
//                      asserts are disabled.
//
#undef AssertSz
#undef Assert


//+---------------------------------------------------------------------------
//
// DBG (checked) build
//
#ifdef DEBUG

VOID    WINAPI   AssertSzFn        (PCSTR pszaMsg, PCSTR pszaFile, int nLine);

#define Assert(a)                   AssertSz(a, "Assert(" #a ")\r\n")
#define AssertSz(a,b)               if (!(a)) { AssertSzFn(b, __FILE__, __LINE__); }

// Setinel Checking
inline void SetSentinelx(void *buf) { ((DWORD *)buf)[0] = 0xdeadbeef; }
inline bool SentinelSetx(void *buf) { return ((DWORD) 0xdeadbeef == ((DWORD *)buf)[0]); }
#define SetSentinel(x)              SetSentinelx(x)
#define SentinelSet(x)              SentinelSetx(x)

//+---------------------------------------------------------------------------
//
// !DBG (retail) build
//
#else

#define Assert(a)
#define AssertSz(a,b)

// Setinel Checking
#define SetSentinel(x)               NOP_FUNCTION
#define SentinelSet(x)               NOP_FUNCTION 


#endif  // DBG


#endif // _NCDEBUG_H_

