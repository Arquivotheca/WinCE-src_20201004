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

#ifndef _SNS_MACROS_
#define _SNS_MACROS_

#include <windows.h>
#include <types.h>
#include <ceddk.h>
#include <tux.h>
#include <dbgapi.h>

#include "snsDebug.h"


//-----------------------------------------------------------------------------
// Constants
//
#define ONE_SECOND 1000
#define ONE_MINUTE ONE_SECOND*60




//-----------------------------------------------------------------------------
// Functional Macros
//
#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define MIN(x, y) ((x) > (y) ? (y) : (x))

__inline DWORD RANGE_RAND ( DWORD range_min, DWORD range_max ) {
    UINT uiRand = 0;
    rand_s(&uiRand);
    return ((DWORD)((double)uiRand / (UINT_MAX) * (range_max - range_min) + range_min));
}

#define _F_ _T(__FUNCTION__)


// Useful Macros
#define Debug NKDbgPrintfW
#ifndef VALID_HANDLE
#define VALID_HANDLE(X) (X != INVALID_HANDLE_VALUE && X != NULL)
#endif

#ifndef INVALID_HANDLE
#define INVALID_HANDLE(X) (X == INVALID_HANDLE_VALUE || X == NULL)
#endif

#ifndef VALID_POINTER
#define VALID_POINTER(X) (X != NULL && 0xcccccccc != (int) X)
#endif

#ifndef CHECK_CLOSE_KEY
#define CHECK_CLOSE_KEY(X) if (VALID_POINTER(X)) { RegCloseKey(X); X = NULL; }
#endif

#ifndef CHECK_LOCAL_FREE
#define CHECK_LOCAL_FREE(X) if (VALID_POINTER(X)) { LocalFree(X); X = NULL; }
#endif

#ifndef CHECK_FREE
#define CHECK_FREE(X) if (VALID_POINTER(X)) { free(X); X = NULL; }
#endif

#ifndef CHECK_CLOSE_HANDLE
#define CHECK_CLOSE_HANDLE(X) if (VALID_HANDLE(X)) { CloseHandle(X); X = INVALID_HANDLE_VALUE; }
#endif

#ifndef CHECK_DELETE
#define CHECK_DELETE( ptr ){if( ptr != NULL ){delete ptr; ptr = NULL;}}
#endif

#ifndef CHECK_CLOSE_Q_HANDLE
#define CHECK_CLOSE_Q_HANDLE( X ){if( X != NULL && X != INVALID_HANDLE_VALUE){CloseMsgQueue(X); X = INVALID_HANDLE_VALUE;}}
#endif

#ifndef CHECK_CLOSE_FIND_HANDLE
#define CHECK_CLOSE_FIND_HANDLE( X ){if( X != INVALID_HANDLE_VALUE){FindClose(X); X = INVALID_HANDLE_VALUE;}}
#endif

#endif //_SNS_MACROS_
