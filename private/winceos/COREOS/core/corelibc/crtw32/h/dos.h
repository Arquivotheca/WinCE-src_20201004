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
*dos.h - definitions for MS-DOS interface routines
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Defines the structs and unions used for the direct DOS interface
*       routines; includes macros to access the segment and offset
*       values of far pointers, so that they may be used by the routines; and
*       provides function prototypes for direct DOS interface functions.
*
*       [Public]
*
*Revision History:
*       06-11-87  JCR   Added find_t
*       06-15-87  JCR   Added O_NOINHERIT
*       06-18-87  JCR   Added some DOS function prototypes
*       06-19-87  JCR   Moved O_NOINHERIT to fcntl.h
*       06-25-87  JMB   Added _HARDERR_* constants
*       06-25-87  SKS   Added diskfree_t, dosdate_t, dostime_t structures
*       06-25-87  JCR   Added _A_NORMAL, etc. constants
*       07-17-87  JCR   Added _chain_intr, also the "interrupt" type to
*                       _dos_setvec and _dos_getvec.
*       07-27-87  SKS   Added several _dos_*() functions, _disable()/_enable()
*       08-17-87  PHG   Fixed bad prototype for _dos_getdiskfree()
*       10-08-87  JCR   Added _CDECL to prototypes with "interrupt" declaration
*                       (needed for compiling with -Gc switch).
*       09-27-88  JCR   386 version
*       10-03-88  GJF   Use M_I386, not I386
*       05-03-89  JCR   Added _INTERNAL_IFSTRIP for relinc usage
*       07-25-89  GJF   Major cleanup. Alignment of struct fields is now
*                       protected by pack pragmas. Now specific to 386.
*       10-30-89  GJF   Fixed copyright
*       11-02-89  JCR   Changed "DLL" to "_DLL"
*       02-28-90  GJF   Added #ifndef _INC_DOS and #include <cruntime.h>
*                       stuff. Also, removed some (now) useless preprocessor
*                       directives.
*       03-22-90  GJF   Replaced _cdecl with _CALLTYPE1 in prototype and with
*                       _VARTYPE1 in variable declaration.
*       12-28-90  SRW   Added _CRUISER_ conditional around pack pragmas
*       01-23-91  GJF   Removed segread() prototype.
*       04-04-91  GJF   Added version info variables (_WIN32_).
*       08-20-91  JCR   C++ and ANSI naming
*       08-26-91  BWM   Added _peek, poke, and _getvideoaddr (_DOSX32_).
*       08-26-91  BWM   Removed _harderr constants, replaced by _seterrormode.
*       08-26-91  BWM   Removed datetime prototypes, replaced by systime functions.
*       09-05-91  JCR   Added missing #endif (update), removed obsolete stuff
*       09-16-91  BWM   Fixed reversed #ifdef on screen address constants.
*       01-22-92  GJF   Fixed up definitions of global variables for build of,
*                       and users of, crtdll.dll.
*       03-30-92  DJM   POSIX support.
*       06-02-92  SKS   Fix typo in DLL declaration of _osmajor
*                       Add declartion of _pgmptr
*       08-07-92  GJF   Function calling type and variable type macros.
*       01-21-93  GJF   Removed support for C6-386's _cdecl.
*       04-06-93  SKS   Replace _CRTAPI1/2 with __cdecl, _CRTVAR1 with nothing
*                       Remove support for DOSX32, OS/2, etc.
*                       Remove obsolete vars and vars moved to STDLIB.H
*                       Remove duplicate references to _osver, etc.
*                       Remove obsolete functions peek, poke, etc.
*       04-07-93  SKS   Add _CRTIMP keyword for CRT DLL model
*       06-15-93  GJF   Restored prototypes for _enable, _disable.
*       09-01-93  GJF   Merged Cuda and NT SDK versions.
*       11-19-93  CFW   Add _wpgmptr.
*       11-03-94  GJF   Ensure 8 byte alignment.
*       12-15-94  XY    merged with mac header
*       02-11-95  CFW   Add _CRTBLD to avoid users getting wrong headers.
*       02-14-95  CFW   Clean up Mac merge.
*       12-14-95  JWM   Add "#pragma once".
*       01-23-97  GJF   Cleaned out obsolete support for _NTSDK and _CRTAPI*.
*                       Also, detab-ed.
*       09-30-97  JWM   Restored not-so-obsolete _CRTAPI1 support.
*       05-13-99  PML   Remove _CRTAPI1
*       05-17-99  PML   Remove all Macintosh support.
*       10-10-04  ESC   Use _CRT_PACKING
*       11-18-04  ATC   SAL Annotations for CRT Headers
*       03-23-05  MSL   _P removal - not needed
*                       New deprecation warning with function name
*                       Packing fix
*
****/

#pragma once

#ifndef _INC_DOS
#define _INC_DOS

#include <crtdefs.h>

/*
 * Currently, all MS C compilers for Win32 platforms default to 8 byte
 * alignment.
 */
#pragma pack(push,_CRT_PACKING)

#ifdef  __cplusplus
extern "C" {
#endif

#ifndef _DISKFREE_T_DEFINED
/* _getdiskfree structure (duplicated in DIRECT.H) */
struct _diskfree_t {
        unsigned total_clusters;
        unsigned avail_clusters;
        unsigned sectors_per_cluster;
        unsigned bytes_per_sector;
        };

#define _DISKFREE_T_DEFINED
#endif

/* File attribute constants */

#define _A_NORMAL       0x00    /* Normal file - No read/write restrictions */
#define _A_RDONLY       0x01    /* Read only file */
#define _A_HIDDEN       0x02    /* Hidden file */
#define _A_SYSTEM       0x04    /* System file */
#define _A_SUBDIR       0x10    /* Subdirectory */
#define _A_ARCH         0x20    /* Archive file */

#ifdef _CRT_USE_WINAPI_FAMILY_DESKTOP_APP
/* Function prototypes */
#ifndef _GETDISKFREE_DEFINED
_Check_return_ _CRTIMP unsigned __cdecl _getdiskfree(_In_ unsigned _Drive, _Out_ struct _diskfree_t * _DiskFree);
#define _GETDISKFREE_DEFINED
#endif
#endif /* _CRT_USE_WINAPI_FAMILY_DESKTOP_APP */

#ifdef  _M_IX86
void __cdecl _disable(void);
void __cdecl _enable(void);
#endif  /* _M_IX86 */

#if     !__STDC__
/* Non-ANSI name for compatibility */
#define diskfree_t  _diskfree_t
#endif  /* __STDC__ */

#ifdef  __cplusplus
}
#endif

#pragma pack(pop)

#endif  /* _INC_DOS */
