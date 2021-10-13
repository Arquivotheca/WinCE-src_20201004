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
////////////////////////////////////////////////////////////////////////////////
//
//  TUXTEST TUX DLL
//
//  Module: StreamLogger.h
//          Contains function declarations, globals, and macros common to all WMT end to end tests.
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////

#ifndef __STREAMLOGGER_H__
#define __STREAMLOGGER_H__

////////////////////////////////////////////////////////////////////////////////
// Include files for global declarations

#include "Streams.h"

////////////////////////////////////////////////////////////////////////////////
// Type declarations

////////////////////////////////////////////////////////////////////////////////
// Global Constants

////////////////////////////////////////////////////////////////////////////////
// Utility macros
#define SAFE_CLOSEHANDLE(h)		if(NULL != h && INVALID_HANDLE_VALUE != h) \
								{ \
									CloseHandle(h); \
									h = NULL; \
								}

#define SAFE_DELETE(ptr)			if(NULL != ptr) \
								{ \
									delete ptr; \
									ptr = NULL; \
								}

#define SAFE_DELETEARRAY(ptr)	if(NULL != ptr) \
								{ \
									delete[] ptr; \
									ptr = NULL; \
								}

#define SAFE_RELEASE(ptr)		if(NULL != ptr) \
								{ \
									((IUnknown *)(ptr))->Release(); \
									ptr = NULL; \
								}

////////////////////////////////////////////////////////////////////////////////
// Global variables
								   
////////////////////////////////////////////////////////////////////////////////
// Utility func declarations

#endif


