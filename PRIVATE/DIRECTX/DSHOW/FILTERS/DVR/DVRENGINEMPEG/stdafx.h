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
// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

// Macro for unused-in-release-build warning suppression (stolen from MFC)
#ifdef _DEBUG
#define UNUSED(x)
#else
#define UNUSED(x) x
#endif

// Suppress warning about long symbol names being truncated to 255 characters.
// This warning is disabled by default on VS.NET but enabled in our CE build.
#pragma warning(disable : 4786)

#include <DShow.h>
#include <Streams.h>
#include <tchar.h>
#include <Dvdmedia.h>
#include <EncDec.h>
#include <ks.h>
#include <ksmedia.h>
#include <wincrypt.h>

#include <math.h>

// STL header section
#include <algorithm>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <deque>
// end of STL header section

// ATL headers
#include <atlbase.h>

#ifndef DEBUGMSG
#define DEBUGMSG(x,y)
#endif

#ifndef RETAILMSG
#define RETAILMSG(x,y)
#endif

#ifndef UNDER_CE
#define DEBUGZONE	
#endif

// Our Win/CE override of debug zones:
#include "debug.h"
