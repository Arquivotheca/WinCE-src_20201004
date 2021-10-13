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
//  Module: main.h
//          Header for all files in the project.
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////

#ifndef __MAIN_DEFAULT_H__
#define __MAIN_DEFAULT_H__

////////////////////////////////////////////////////////////////////////////////
// Included files

#include <windows.h>
#include <tchar.h>

#define FAIL(x, ...)     Debug( \
                        TEXT("FAIL in %s at line %d: ") ## x, \
                        TEXT(__FILE__), __LINE__, __VA_ARGS__ )

// same as FAIL, but also logs GetLastError value
#define ERRFAIL(x, ...)  Debug( \
                        TEXT("FAIL in %s at line %d: ") ## x ## TEXT("; error %u"), \
                        TEXT(__FILE__), __LINE__, __VA_ARGS__, GetLastError() )

// same as FAIL, but also logs hresult value
#define HRFAIL(hr, x, ...)  Debug( \
                        TEXT("FAIL in %s at line %d: ") ## x ## TEXT("; error 0x%08x"), \
                        TEXT(__FILE__), __LINE__, __VA_ARGS__, hr )


#define ABORT(x, ...)     Debug( \
                        TEXT("ABORT in %s at line %d: ") ## x, \
                        TEXT(__FILE__), __LINE__, __VA_ARGS__ )

// same as ABORT, but also logs GetLastError value
#define ERRABORT(x, ...)  Debug( \
                        TEXT("ABORT in %s at line %d: ") ## x ## TEXT("; error %u"), \
                        TEXT(__FILE__), __LINE__, __VA_ARGS__, GetLastError() )


// same as ABORT, but also logs GetLastError value
#define HRABORT(hr, x, ...)  Debug( \
                        TEXT("ABORT in %s at line %d: ") ## x ## TEXT("; error 0x%08x"), \
                        TEXT(__FILE__), __LINE__, __VA_ARGS__, hr)


#define WARN(x, ...)     Debug( \
                        TEXT("WARNING in %s at line %d ") ## x, \
                        TEXT(__FILE__), __LINE__, __VA_ARGS__ )

// same as ABORT, but also logs GetLastError value
#define ERRWARN(x, ...)  Debug( \
                        TEXT("WARNING in %s at line %d: ") ## x ## TEXT("; error %u"), \
                        TEXT(__FILE__), __LINE__, __VA_ARGS__, GetLastError() )

// same as ABORT, but also logs GetLastError value
#define HRWARN(hr, x, ...)  Debug( \
                        TEXT("WARNING in %s at line %d: ") ## x ## TEXT("; error 0x%08x"), \
                        TEXT(__FILE__), __LINE__, __VA_ARGS__, hr)


// log's an error, but doesn't log a failure
#define ERR(x, ...)      Debug( \
                        TEXT("ERROR in %s at line %d: ") ## x, \
                        TEXT(__FILE__), __LINE__, __VA_ARGS__ )

#define DETAIL(x, ...)   Debug( x, __VA_ARGS__ )

#define PASS(x, ...)     Debug( x, __VA_ARGS__ )

#define COMMENT(x, ...)  Debug( x, __VA_ARGS__ )

#define SKIP(x, ...)     Debug( x, __VA_ARGS__ )

#define UNIMPL(x, ...)   Debug( x, __VA_ARGS__ )


#define Log Debug
void Debug(LPCTSTR szFormat, ...) ;


#endif // __MAIN_DEFAULT_H__
