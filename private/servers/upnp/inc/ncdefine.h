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
//  File:       U P D E F I N E . H
//
//  Contents:   Very generic defines. Don't throw non-generic stuff
//              in here!
//
//
//----------------------------------------------------------------------------

#pragma once
#ifndef _UPDEFINE_H_
#define _UPDEFINE_H_

#define celems(_x)          (sizeof(_x) / sizeof(_x[0]))


#if defined(_M_IA64) || defined(_M_ALPHA) || defined(_M_MRX000) || defined(_M_PPC) || defined(UNDER_CE)

#ifdef NOTHROW
#undef NOTHROW
#endif
#define NOTHROW

#else

#ifdef NOTHROW
#undef NOTHROW
#endif
#define NOTHROW __declspec(nothrow)

#endif

#ifndef NOP_FUNCTION
#if (_MSC_VER >= 1210)
#define NOP_FUNCTION __noop
#else
#define NOP_FUNCTION (void)0
#endif
#endif

// Defines for C source files including us.
//
#ifndef __cplusplus
#ifndef inline
#define inline  __inline
#endif
#endif

#endif // _NCDEFINE_H_
