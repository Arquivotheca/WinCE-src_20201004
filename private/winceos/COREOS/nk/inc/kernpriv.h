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
//
// This source code is licensed under Microsoft Shared Source License
// Version 6.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

//
//    kernpriv.h - internal kernel process header
//

#ifndef _NK_PRIV_H_
#define _NK_PRIV_H_

#ifndef offsetof
  #define offsetof(s,m) ((unsigned)&(((s *)0)->m))
#endif

#if defined(XREF_CPP_FILE)
#define ERRFALSE(exp)
#elif !defined(ERRFALSE)
// This macro is used to trigger a compile error if exp is false.
// If exp is false, i.e. 0, then the array is declared with size 0, triggering a compile error.
// If exp is true, the array is declared correctly.
// There is no actual array however.  The declaration is extern and the array is never actually referenced.
#define ERRFALSE(exp)           extern char __ERRXX[(exp)!=0]
#endif



#endif // _NK_PRIV_H_

