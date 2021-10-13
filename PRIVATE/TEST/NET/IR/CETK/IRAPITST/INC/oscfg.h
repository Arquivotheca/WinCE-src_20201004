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

#include <windows.h>
#include <tchar.h>
#include <winbase.h>

#ifdef UNDER_CE
#include "types.h"

#else // UNDER_CE

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <stddef.h>
#include <limits.h>


#define ASSERT assert

#define RETAILMSG(exp,p)	((exp)?OutStr p,1:0)

#ifdef DEBUG
#define DEBUGMSG(exp,p)	((exp)?OutStr p,1:0)
#undef NDEBUG
#define DEBUGZONE(x)        1<<x
#else // DEBUG

#define DEBUGMSG(exp,p) (0)
#define NDEBUG

#endif // DEBUG

#endif // UNDER_CE

#define countof(a) (sizeof(a)/sizeof(*(a)))

extern void
OutStr(TCHAR *format, ...);
