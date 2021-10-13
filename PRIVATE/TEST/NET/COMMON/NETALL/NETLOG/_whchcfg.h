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