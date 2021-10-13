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
/* wrapper to suppress warnings in windows.h
 */
#ifndef _WRAPWIN_H
#define _WRAPWIN_H

#include <string.h>

#ifdef _WIN32_WCE
 #include <stdlib.h>
 #include <time.h>
#endif /* _WIN32_WCE */

#pragma warning(disable: 4005 4052 4115 4201 4214)

#include <windows.h>
#endif /* _WRAPWIN_H */

/*
 * Copyright (c) 1992-2012 by P.J. Plauger.  ALL RIGHTS RESERVED.
 * Consult your license regarding permissions and restrictions.
V6.00:0009 */
