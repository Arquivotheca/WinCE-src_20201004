//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

/*
-------------------------------------------------------------------------------

Module Name:

	_whchcfg.h

Description:

	This file contains declarations of variables used in the whichcfg utility.

-------------------------------------------------------------------------------
*/

#ifndef __WHCHCFG_H_
#define __WHCHCFG_H_

// ----------------------------------------------------------------------------
// Variable declarations
// ----------------------------------------------------------------------------

typedef enum _CONFIGS
{
	UNKNOWN,
	MINKERN,
	MININPUT,
	MINCOMM,
	MINGDI,
	MINWMGR,
	MINSHELL,
	MAXALL,
	MAXIMUM_CONFIGS
} CONFIGS;

TCHAR cszKeyValues[][MAX_CONFIG_STRING_LENGTH] =
{
	TEXT("UNKNOWN"),
	TEXT("WinCE MinKern Device"),		// Not implemented
	TEXT("WinCE MinInput Device"),		// Not implemented
	TEXT("WinCE MinComm Device"),
	TEXT("WinCE MinGdi Device"),		// Not implemented
	TEXT("WinCE MinWMgr Device"),
	TEXT("WinCE MinShell Device"),
	TEXT("WinCE MaxAll Device")
};

TCHAR cszFriendlyNames[][MAX_CONFIG_STRING_LENGTH] =
{
	TEXT("UNKNOWN"),
	TEXT("MinKern"),
	TEXT("MinInput"),
	TEXT("MinComm"),
	TEXT("MinGdi"),	
	TEXT("MinWMgr"),
	TEXT("MinShell"),
	TEXT("MaxAll")
};

#endif // __WHCHCFG_H_