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
#pragma once

#ifndef __DDRAWUTY_MACROS_H__
#define __DDRAWUTY_MACROS_H__

// Some global, useful macros
#define countof(x) (sizeof(x)/sizeof(*(x)))

// used in STL container construction
#define ARRAYSIZE(A) (sizeof(A)/sizeof(A[0]))
#define BEGIN(A) &(A[0])
#define END(A) (A+ARRAYSIZE(A))
#define VECTOR(A) BEGIN(A),END(A)

#ifndef min
template<class T> inline const T& min(const T& x, const T& y)
{
    return (y<x) ? y : x;
}
#endif

#ifndef max
template<class T> inline const T& max(const T& x, const T& y)
{
    return (y>x) ? y : x;
}
#endif

#endif // !defined(__DDRAWUTY_MACROS_H__)

