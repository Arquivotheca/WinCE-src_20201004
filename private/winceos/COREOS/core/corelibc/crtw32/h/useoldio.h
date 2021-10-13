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
*useoldio.h - force the use of the Microsoft "classic" iostream libraries.
*
*       Copyright (c) 1996-2001, Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Generates default library directives for the old ("classic") IOSTREAM
*       libraries.  The exact name of the library specified in the directive
*       depends on the compiler switches (-ML, -MT, -MD, -MLd, -MTd, and -MDd).
*
*       This header file is only included by other header files.
*
*       [Public]
*
*Revision History:
*       04-16-96  JWM   new file
*       02-24-97  GJF   Detab-ed.
*       05-31-00  GB    Added warning for old iostream are deprected
*
****/

#ifndef _USE_OLD_IOSTREAMS
#define _USE_OLD_IOSTREAMS
#ifndef _INTERNAL_IFSTRIP_
#pragma warning(disable : 4995)
#endif  /* _INTERNAL_IFSTRIP_ */
#if defined(DEPRECATE_IOSTREAMS)
#ifndef _M_IA64
/*
 * Warning C4995, '_OLD_IOSTREAMS_ARE_DEPRECATED' is a deprecated name, is 
 * being issued because the old I/O Streams headers iostreams.h et al will no
 * longer be supported from VC8.  Replace references such as #include 
 * <iostreams.h> with #include <iostreams>, using the new, more conformant, I/O
 * Streams headers.
 */

#pragma deprecated(_OLD_IOSTREAMS_ARE_DEPRECATED)
extern void _OLD_IOSTREAMS_ARE_DEPRECATED();
#endif  /* _M_IA64 */
#endif  /* DEPRECATE_IOSTREAMS */
#ifdef  _MT
#ifdef  _DLL
#ifdef  _DEBUG
#pragma comment(lib,"msvcirtd")
#else   /* _DEBUG */
#pragma comment(lib,"msvcirt")
#endif  /* _DEBUG */

#else   /* _DLL */
#ifdef  _DEBUG
#pragma comment(lib,"libcimtd")
#else   /* _DEBUG */
#pragma comment(lib,"libcimt")
#endif  /* _DEBUG */
#endif  /* _DLL */

#else   /* _MT */
#ifdef  _DEBUG
#pragma comment(lib,"libcid")
#else   /* _DEBUG */
#pragma comment(lib,"libci")
#endif  /* _DEBUG */
#endif

#endif  /* _USE_OLD_IOSTREAMS */
