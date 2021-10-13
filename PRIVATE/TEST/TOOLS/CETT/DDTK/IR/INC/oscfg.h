//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
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
