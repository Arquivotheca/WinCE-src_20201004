//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

#define BEGIN_CONST_SECTION     data_seg(".rdata")
#define END_CONST_SECTION       data_seg()

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
