//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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



