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
/*++

Module Name:  
    tuxbvt.h

Description:  
    Smart Card BVT Test
    
Notes: 

--*/
#ifndef __TUXBVT_H_
#define __TUXBVT_H_
#include <tux.h>
#include <kato.h>

extern CKato            *g_pKato;
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
// Global Macro
#define countof(a) (sizeof(a)/sizeof(*(a)))
///////////////////////////////////////////////////////////////////////////////

extern SPS_SHELL_INFO   *g_pShellInfo;

#endif



