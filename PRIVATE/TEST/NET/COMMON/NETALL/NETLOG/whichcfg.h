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

	whichcfg.h

Description:

	This file contains declarations of variables used in the whichcfg utility.

-------------------------------------------------------------------------------
*/

#ifndef _WHICHCFG_H_
#define _WHICHCFG_H_

#if defined(__cplusplus)
extern "C" {
#endif // __cplusplus

// ----------------------------------------------------------------------------
// Constants
// ----------------------------------------------------------------------------

#define MAX_CONFIG_STRING_LENGTH 64

// ----------------------------------------------------------------------------
// Function prototypes
// ----------------------------------------------------------------------------

BOOL WhichVersion( LPTSTR lpVersionString );
BOOL WhichConfig( LPTSTR lpConfigString );
DWORD WhichBuild( void );

#if defined(__cplusplus)
}
#endif // __cplusplus

#endif // _WHICHCFG_H_