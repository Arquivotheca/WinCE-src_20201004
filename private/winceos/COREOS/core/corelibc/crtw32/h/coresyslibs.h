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
/***
*coresyslibs.h - contains declarations of internal routines and variables
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Reference libraries specific to platforms built from coresystem.
*
****/

#pragma once

#ifndef _CRTBLD
/*
 * This is an internal C runtime header file. It is used when building
 * the C runtimes only. It is not to be used as a public header file.
 */
#error ERROR: Use of C runtime library internal header file.
#endif  /* _CRTBLD */

#ifndef _CORESYS
#error ERROR: This header should only be included when _CORESYS is defined
#endif /* _CORESYS */

#ifdef _WIN32_WCE
#pragma comment(linker, "/defaultlib:coredll.lib")
#else
// Link with windowsphonecore.LIB to resolve Platform APIs such as IsProcessorFeaturePresent.
// Note that msvcr*.dll targets coresystem. However the header targets the Windows Phone SDK
// Windowsphonecore.lib includes a subset of the APIs from mincore.lib (coresystem) that is 
// sufficient for the platform APIs that are currently used by the CRT's import lib.
#pragma comment(linker, "/defaultlib:windowsphonecore.lib")
#endif // _WIN32_WCE

