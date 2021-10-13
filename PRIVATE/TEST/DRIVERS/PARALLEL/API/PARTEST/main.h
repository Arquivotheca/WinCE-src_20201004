//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#if 0
-------------------------------------------------------------------------------


Module Name:

	main.h

Description:

	Header for all files in the project

-------------------------------------------------------------------------------
#endif


#ifndef __MAIN_H__
#define __MAIN_H__

// ----------------------------------------------------------------------------
// Included files
// ----------------------------------------------------------------------------

#include <windows.h>
#include <stdlib.h>
#include <tchar.h>
#include <tux.h>
#include <tuxslct.h>
#include <kato.h>

// ----------------------------------------------------------------------------
// Suggested log verbosities
// ----------------------------------------------------------------------------

#define LOG_EXCEPTION          0
#define LOG_FAIL               2
#define LOG_ABORT              4
#define LOG_SKIP               6
#define LOG_NOT_IMPLEMENTED    8
#define LOG_PASS              10
#define LOG_DETAIL            12
#define LOG_COMMENT           14

#endif // __MAIN_H__


