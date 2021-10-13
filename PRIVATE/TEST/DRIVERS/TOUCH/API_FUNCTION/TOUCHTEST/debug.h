//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
