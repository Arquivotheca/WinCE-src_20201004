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
// --------------------------------------------------------------------
//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
//                                                                     
// --------------------------------------------------------------------

#ifndef __DEBUG_H__
#define __DEBUG_H__

extern CKato* g_pKato;

// --------------------------------------------------------------------
//
// Debugging Stuff
//
// use: #pragma message(BUGBUG "message")
#define __BUGBUG(n) __FILE__ "(" #n "): BUGBUG - "
#define _BUGBUG(n) __BUGBUG(n)
#define BUGBUG _BUGBUG(__LINE__)

// use: #pragma message(TODO "message")
#define __TODO(n) __FILE__ "(" #n "): TODO - "
#define _TODO(n) __TODO(n)
#define TODO _TODO(__LINE__)


#define FAIL(x)		g_pKato->Log( LOG_FAIL, \
						TEXT("FAIL in %s at line %d: %s"), \
						__FILE_NAME__, __LINE__, TEXT(x) )
						
#define SUCCESS(x)	g_pKato->Log( LOG_PASS, \
						TEXT("SUCCESS: %s"), TEXT(x) )

#define WARN(x)		g_pKato->Log( LOG_DETAIL, \
						TEXT("WARNING in %s at line %d: %s"), \
						__FILE_NAME__, __LINE__, TEXT(x) )

#define DETAIL(x)	g_pKato->Log( LOG_DETAIL, TEXT(x) )

#endif
