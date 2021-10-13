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

#ifndef __BREAK_FOR_DEBUG_HH__
#define __BREAK_FOR_DEBUG_HH__

#include <windows.h>

/*
* break on failure if the user has requested this.
* calls DebugBreak()
*/
void breakOnFailureIfRequested ();

/*
* sets whether we break on failure or not.
*
* default at the onset is not to break on failure.
*/
void setBreakOnFailure (BOOL bSetTo);


#endif



