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