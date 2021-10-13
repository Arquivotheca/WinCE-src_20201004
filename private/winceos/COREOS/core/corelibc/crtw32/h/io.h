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
*io.h - declarations for low-level file handling and I/O functions
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       This file contains the function declarations for the low-level
*       file handling and I/O functions.
*
*       [Public]
*
*Revision History:
*       10/20/87  JCR   Removed "MSC40_ONLY" entries
*       11/09/87  JCR   Multi-thread support
*       12-11-87  JCR   Added "_loadds" functionality
*       12-17-87  JCR   Added _MTHREAD_ONLY comments
*       12-18-87  JCR   Added _FAR_ to declarations
*       02-10-88  JCR   Cleaned up white space
*       08-19-88  GJF   Modified to also work for the 386 (small model only)
*       05-03-89  JCR   Added _INTERNAL_IFSTRIP for relinc usage
*       08-03-89  GJF   Cleanup, now specific to OS/2 2.0 (i.e., 386 flat model)
*       08-14-89  GJF   Added prototype for _pipe()
*       10-30-89  GJF   Fixed copyright
*       11-02-89  JCR   Changed "DLL" to "_DLL"
*       11-17-89  GJF   read() should take "void *" not "char *", write()
*                       should take "const void *" not "char *". Also,
*                       added const to appropriate arg types for access(),
*                       chmod(), creat(), open() and sopen()
*       03-01-90  GJF   Added #ifndef _INC_IO and #include <cruntime.h>
*                       stuff. Also, removed some (now) useless preprocessor
*                       directives.
*       03-21-90  GJF   Replaced _cdecl with _CALLTYPE1 or _CALLTYPE2 in
*                       prototypes.
*       05-28-90  SBM   Added _commit()
*       01-18-91  GJF   ANSI naming.
*       02-25-91  SRW   Exposed _get_osfhandle and _open_osfhandle [_WIN32_]
*       08-01-91  GJF   No _pipe for Dosx32.
*       08-20-91  JCR   C++ and ANSI naming
*       08-26-91  BWM   Added _findfirst, etc.
*       09-16-91  BWM   Changed find handle type to long.
*       09-28-91  JCR   ANSI names: DOSX32=prototypes, WIN32=#defines for now
*       03-30-92  DJM   POSIX support.
*       06-23-92  GJF   double slash is non-ANSI comment delimiter.
*       08-06-92  GJF   Function calling type and variable type macros.
*       08-25-92  GJF   For POSIX build, #ifdef-ed out all but some internally
*                       used macros (and these are stripped out on release).
*       09-03-92  GJF   Merge two changes above.
*       01-21-93  GJF   Removed support for C6-386's _cdecl.
*       03-29-93  JWM   Increased name buffer in finddata structure to 260 bytes.
*       04-06-93  SKS   Replace _CRTAPI1/2 with __cdecl, _CRTVAR1 with nothing
*       04-07-93  SKS   Add _CRTIMP keyword for CRT DLL model
*                       Use link-time aliases for old names, not #define's
*       05-17-93  SKS   #if for old names no longer checks for __cplusplus.
*                       It used to do so past because #define-ing names like
*                       open, read, write, etc. created problems for users
*       09-01-93  GJF   Merged NT SDK and Cuda versions.
*       12-07-93  CFW   Add wide char version protos.
*       11-03-94  GJF   Ensure 8 byte alignment.
*       11-18-94  GJF   Added prototypes for _lseeki64, _filelengthi64 and
*                       _telli64.
*       12-07-94  SKS   Add comment for ifstrip utility (src release process)
*       12-15-94  XY    merged with mac header
*       12-29-94  GJF   Added _[w]findfilei64 and _[w]findnexti64. Also removed
*                       obsolete _CALLTYPE* macro.
*       02-11-95  CFW   Add _CRTBLD to avoid users getting wrong headers.
*       02-14-95  CFW   Clean up Mac merge.
*       02-24-95  SKS   Replace _MTHREAD_ONLY comments (stripped by source
*                       cleansing) with #ifdef _NOT_CRTL_BUILD_
*       10-06-95  SKS   Add "const" to "char *" in prototypes for *findfirst().
*       12-14-95  JWM   Add "#pragma once".
*       02-20-97  GJF   Cleaned out obsolete support for _CRTAPI* and _NTSDK.
*                       Also, detab-ed.
*       09-30-97  JWM   Restored not-so-obsolete _CRTAPI1 support.
*       02-10-98  GJF   Changes for Win64: made time_t __int64, and changed
*                       arg and return types to intptr_t where appropriate
*       05-04-98  GJF   Added __time64_t support.
*       05-13-99  PML   Remove _CRTAPI1
*       05-17-99  PML   Remove all Macintosh support.
*       10-06-99  PML   Add _W64 modifier to types which are 32 bits in Win32,
*                       64 bits in Win64.
*       11-12-99  PML   Wrap __time64_t in its own #ifndef.
*       12-18-02  EAN   changed time_t to use a 64-bit int if available.
*                       VSWhidbey 6851
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*       02-26-04  SSM   Make 64 bit lseek available always. read uses it to 
*                       handle files > 4GB in text mode
*       03-24-04  MSL   Fixed declarations to work with /clr:pure
*                       VSW#258403
*       04-07-04  MSL   Double slash removal
*       06-07-04  SJ    Removed the temporary switch provided to revert to 
*                       the old insecure (s)open VSW#172434
*       09-10-04  AC    Moved _set_fmode/_get_fmode to stdlib.h
*                       VSW#265694
*       09-15-04  AGH   Changed _ST_MODEL to _CRT_DISABLE_PERFCRITICAL_THREADLOCKING
*       09-16-04  AC    Changed the signature of _sopen_s family
*                       VSW#362611
*       10-06-04  AC    Deprecate C++ of _open and _sopen family
*                       VSW#380319
*       10-10-04  ESC   Use _CRT_PACKING
*       10-31-04  MSL   Add missing 64 bit open overloads
*       11-02-04  AC    Add cpp overloads for secure functions.
*                       VSW#313364
*       11-08-04  JL    Added _CRT_NONSTDC_DEPRECATE to deprecate non-ANSI functions
*       11-18-04  ATC   SAL Annotations for CRT Headers
*       11-22-04  AC    Remove deprecation from access.
*                       VSW#415339
*       12-14-04  AC    Replaced _CRT_DISABLE_PERFCRITICAL_THREADLOCKING with _CRT_DISABLE_PERFCRIT_LOCKS
*                       VSW#300574
*       01-14-05  AC    Fix SAL annotations (using prefast espx plug-in)
*       01-21-05  MSL   Add param names to overload macros
*       03-23-05  MSL   _P removal - not needed
*                       New deprecation warning with function name
*                       Packing fix
*       04-14-05  MSL   Intellisense locale name cleanup
*                       Compile under MIDL
*                       SAL fixes for n functions
*       11-10-06  PMB   Removed most _INTEGRAL_MAX_BITS #ifdefs
*                       DDB#11174
*
****/

#pragma once

#ifndef _INC_IO
#define _INC_IO

#include <crtdefs.h>

/*
 * Currently, all MS C compilers for Win32 platforms default to 8 byte
 * alignment.
 */
#pragma pack(push,_CRT_PACKING)

#ifndef _POSIX_

#ifdef  __cplusplus
extern "C" {
#endif

#ifndef _FSIZE_T_DEFINED
typedef unsigned long _fsize_t; /* Could be 64 bits for Win32 */
#define _FSIZE_T_DEFINED
#endif

#ifndef _FINDDATA_T_DEFINED

struct _finddata32_t {
        unsigned    attrib;
        __time32_t  time_create;    /* -1 for FAT file systems */
        __time32_t  time_access;    /* -1 for FAT file systems */
        __time32_t  time_write;
        _fsize_t    size;
        char        name[260];
};

struct _finddata32i64_t {
        unsigned    attrib;
        __time32_t  time_create;    /* -1 for FAT file systems */
        __time32_t  time_access;    /* -1 for FAT file systems */
        __time32_t  time_write;
        __int64     size;
        char        name[260];
};

struct _finddata64i32_t {
        unsigned    attrib;
        __time64_t  time_create;    /* -1 for FAT file systems */
        __time64_t  time_access;    /* -1 for FAT file systems */
        __time64_t  time_write;
        _fsize_t    size;
        char        name[260];
};

struct __finddata64_t {
        unsigned    attrib;
        __time64_t  time_create;    /* -1 for FAT file systems */
        __time64_t  time_access;    /* -1 for FAT file systems */
        __time64_t  time_write;
        __int64     size;
        char        name[260];
};

#ifdef _USE_32BIT_TIME_T
#define _finddata_t     _finddata32_t
#define _finddatai64_t  _finddata32i64_t

#define _findfirst      _findfirst32
#define _findnext       _findnext32
#define _findfirsti64   _findfirst32i64
#define _findnexti64     _findnext32i64

#else
#define _finddata_t     _finddata64i32_t
#define _finddatai64_t  __finddata64_t

#define _findfirst      _findfirst64i32
#define _findnext       _findnext64i32
#define _findfirsti64   _findfirst64
#define _findnexti64    _findnext64

#endif


#define _FINDDATA_T_DEFINED
#endif

#ifndef _WFINDDATA_T_DEFINED

struct _wfinddata32_t {
        unsigned    attrib;
        __time32_t  time_create;    /* -1 for FAT file systems */
        __time32_t  time_access;    /* -1 for FAT file systems */
        __time32_t  time_write;
        _fsize_t    size;
        wchar_t     name[260];
};

struct _wfinddata32i64_t {
        unsigned    attrib;
        __time32_t  time_create;    /* -1 for FAT file systems */
        __time32_t  time_access;    /* -1 for FAT file systems */
        __time32_t  time_write;
        __int64     size;
        wchar_t     name[260];
};

struct _wfinddata64i32_t {
        unsigned    attrib;
        __time64_t  time_create;    /* -1 for FAT file systems */
        __time64_t  time_access;    /* -1 for FAT file systems */
        __time64_t  time_write;
        _fsize_t    size;
        wchar_t     name[260];
};

struct _wfinddata64_t {
        unsigned    attrib;
        __time64_t  time_create;    /* -1 for FAT file systems */
        __time64_t  time_access;    /* -1 for FAT file systems */
        __time64_t  time_write;
        __int64     size;
        wchar_t     name[260];
};

#ifdef _USE_32BIT_TIME_T
#define _wfinddata_t    _wfinddata32_t
#define _wfinddatai64_t _wfinddata32i64_t

#define _wfindfirst     _wfindfirst32
#define _wfindnext      _wfindnext32
#define _wfindfirsti64  _wfindfirst32i64
#define _wfindnexti64   _wfindnext32i64

#else                  
#define _wfinddata_t    _wfinddata64i32_t
#define _wfinddatai64_t _wfinddata64_t

#define _wfindfirst     _wfindfirst64i32
#define _wfindnext      _wfindnext64i32
#define _wfindfirsti64  _wfindfirst64
#define _wfindnexti64   _wfindnext64

#endif

#define _WFINDDATA_T_DEFINED
#endif

/* File attribute constants for _findfirst() */

#define _A_NORMAL       0x00    /* Normal file - No read/write restrictions */
#define _A_RDONLY       0x01    /* Read only file */
#define _A_HIDDEN       0x02    /* Hidden file */
#define _A_SYSTEM       0x04    /* System file */
#define _A_SUBDIR       0x10    /* Subdirectory */
#define _A_ARCH         0x20    /* Archive file */

/* function prototypes */

#ifndef _SIZE_T_DEFINED
#ifdef  _WIN64
typedef unsigned __int64    size_t;
#else
typedef _W64 unsigned int   size_t;
#endif
#define _SIZE_T_DEFINED
#endif

_Check_return_ _CRTIMP int __cdecl _access(_In_z_ const char * _Filename, _In_ int _AccessMode);
_Check_return_wat_ _CRTIMP errno_t __cdecl _access_s(_In_z_ const char * _Filename, _In_ int _AccessMode);
_Check_return_ _CRTIMP int __cdecl _chmod(_In_z_ const char * _Filename, _In_ int _Mode);
/* note that the newly added _chsize_s takes a 64 bit value */
_Check_return_ _CRTIMP int __cdecl _chsize(_In_ int _FileHandle, _In_ long _Size);
_Check_return_wat_ _CRTIMP errno_t __cdecl _chsize_s(_In_ int _FileHandle,_In_ __int64 _Size);
_Check_return_opt_ _CRTIMP int __cdecl _close(_In_ int _FileHandle);
_Check_return_opt_ _CRTIMP int __cdecl _commit(_In_ int _FileHandle);
_Check_return_ _CRT_INSECURE_DEPRECATE(_sopen_s) _CRTIMP int __cdecl _creat(_In_z_ const char * _Filename, _In_ int _PermissionMode);
_Check_return_ _CRTIMP int __cdecl _dup(_In_ int _FileHandle);
_Check_return_ _CRTIMP int __cdecl _dup2(_In_ int _FileHandleSrc, _In_ int _FileHandleDst);
_Check_return_ _CRTIMP int __cdecl _eof(_In_ int _FileHandle);
_Check_return_ _CRTIMP long __cdecl _filelength(_In_ int _FileHandle);
_Check_return_ _CRTIMP intptr_t __cdecl _findfirst32(_In_z_ const char * _Filename, _Out_ struct _finddata32_t * _FindData);
_Check_return_ _CRTIMP int __cdecl _findnext32(_In_ intptr_t _FindHandle, _Out_ struct _finddata32_t * _FindData);
_Check_return_opt_ _CRTIMP int __cdecl _findclose(_In_ intptr_t _FindHandle);
_Check_return_ _CRTIMP int __cdecl _isatty(_In_ int _FileHandle);
_CRTIMP int __cdecl _locking(_In_ int _FileHandle, _In_ int _LockMode, _In_ long _NumOfBytes);
_Check_return_opt_ _CRTIMP long __cdecl _lseek(_In_ int _FileHandle, _In_ long _Offset, _In_ int _Origin);
_Check_return_wat_ _CRTIMP errno_t __cdecl _mktemp_s(_Inout_updates_z_(_Size) char * _TemplateName, _In_ size_t _Size);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_0(errno_t, _mktemp_s, _Prepost_z_ char, _TemplateName)
_Check_return_ __DEFINE_CPP_OVERLOAD_STANDARD_FUNC_0_0(char *, __RETURN_POLICY_DST, _CRTIMP, _mktemp, _Inout_z_, char, _TemplateName)
#ifdef _CRT_USE_WINAPI_FAMILY_DESKTOP_APP
_Check_return_ _CRTIMP int __cdecl _pipe(_Inout_updates_(2) int * _PtHandles, _In_ unsigned int _PipeSize, _In_ int _TextMode);
#endif /* _CRT_USE_WINAPI_FAMILY_DESKTOP_APP */
_Check_return_ _CRTIMP int __cdecl _read(_In_ int _FileHandle, _Out_writes_bytes_(_MaxCharCount) void * _DstBuf, _In_ unsigned int _MaxCharCount);

#ifndef _CRT_DIRECTORY_DEFINED
#define _CRT_DIRECTORY_DEFINED
_CRTIMP int __cdecl remove(_In_z_ const char * _Filename);
_Check_return_ _CRTIMP int __cdecl rename(_In_z_ const char * _OldFilename, _In_z_ const char * _NewFilename);
_CRTIMP int __cdecl _unlink(_In_z_ const char * _Filename);
#if !__STDC__
_CRT_NONSTDC_DEPRECATE(_unlink) _CRTIMP int __cdecl unlink(_In_z_ const char * _Filename);
#endif
#endif

_Check_return_ _CRTIMP int __cdecl _setmode(_In_ int _FileHandle, _In_ int _Mode);
_Check_return_ _CRTIMP long __cdecl _tell(_In_ int _FileHandle);
_CRT_INSECURE_DEPRECATE(_umask_s) _CRTIMP int __cdecl _umask(_In_ int _Mode);
_Check_return_wat_ _CRTIMP errno_t __cdecl _umask_s(_In_ int _NewMode, _Out_ int * _OldMode);
_CRTIMP int __cdecl _write(_In_ int _FileHandle, _In_reads_bytes_(_MaxCharCount) const void * _Buf, _In_ unsigned int _MaxCharCount);

_Check_return_ _CRTIMP __int64 __cdecl _filelengthi64(_In_ int _FileHandle);
_Check_return_ _CRTIMP intptr_t __cdecl _findfirst32i64(_In_z_ const char * _Filename, _Out_ struct _finddata32i64_t * _FindData);
_Check_return_ _CRTIMP intptr_t __cdecl _findfirst64i32(_In_z_ const char * _Filename, _Out_ struct _finddata64i32_t * _FindData);
_Check_return_ _CRTIMP intptr_t __cdecl _findfirst64(_In_z_ const char * _Filename, _Out_ struct __finddata64_t * _FindData);
_Check_return_ _CRTIMP int __cdecl _findnext32i64(_In_ intptr_t _FindHandle, _Out_ struct _finddata32i64_t * _FindData);
_Check_return_ _CRTIMP int __cdecl _findnext64i32(_In_ intptr_t _FindHandle, _Out_ struct _finddata64i32_t * _FindData);
_Check_return_ _CRTIMP int __cdecl _findnext64(_In_ intptr_t _FindHandle, _Out_ struct __finddata64_t * _FindData);
_Check_return_opt_ _CRTIMP __int64 __cdecl _lseeki64(_In_ int _FileHandle, _In_ __int64 _Offset, _In_ int _Origin);
_Check_return_ _CRTIMP __int64 __cdecl _telli64(_In_ int _FileHandle);

_Check_return_wat_ _CRTIMP errno_t __cdecl _sopen_s(_Out_ int * _FileHandle, _In_z_ const char * _Filename,_In_ int _OpenFlag, _In_ int _ShareFlag, _In_ int _PermissionMode);
_Check_return_ errno_t __cdecl _sopen_s_nolock(_Out_ int * _FileHandle, _In_z_ const char * _Filename,_In_ int _OpenFlag, _In_ int _ShareFlag, _In_ int _PermissionMode);

#if !defined(__cplusplus)
_Check_return_ _CRT_INSECURE_DEPRECATE(_sopen_s) _CRTIMP int __cdecl _open(_In_z_ const char * _Filename, _In_ int _OpenFlag, ...);
_Check_return_ _CRT_INSECURE_DEPRECATE(_sopen_s) _CRTIMP int __cdecl _sopen(_In_z_ const char * _Filename, _In_ int _OpenFlag, int _ShareFlag, ...);
#else

/* these function do not validate pmode; use _sopen_s */
extern "C++" _Check_return_ _CRT_INSECURE_DEPRECATE(_sopen_s) _CRTIMP int __cdecl _open(_In_z_ const char * _Filename, _In_ int _Openflag, _In_ int _PermissionMode = 0);
extern "C++" _Check_return_ _CRT_INSECURE_DEPRECATE(_sopen_s) _CRTIMP int __cdecl _sopen(_In_z_ const char * _Filename, _In_ int _Openflag, _In_ int _ShareFlag, _In_ int _PermissionMode = 0);

#endif

#ifndef _WIO_DEFINED

/* wide function prototypes, also declared in wchar.h  */

_Check_return_ _CRTIMP int __cdecl _waccess(_In_z_ const wchar_t * _Filename, _In_ int _AccessMode);
_Check_return_wat_ _CRTIMP errno_t __cdecl _waccess_s(_In_z_ const wchar_t * _Filename, _In_ int _AccessMode);
_Check_return_ _CRTIMP int __cdecl _wchmod(_In_z_ const wchar_t * _Filename, _In_ int _Mode);
_Check_return_ _CRT_INSECURE_DEPRECATE(_wsopen_s) _CRTIMP int __cdecl _wcreat(_In_z_ const wchar_t * _Filename, _In_ int _PermissionMode);
_Check_return_ _CRTIMP intptr_t __cdecl _wfindfirst32(_In_z_ const wchar_t * _Filename, _Out_ struct _wfinddata32_t * _FindData);
_Check_return_ _CRTIMP int __cdecl _wfindnext32(_In_ intptr_t _FindHandle, _Out_ struct _wfinddata32_t * _FindData);
_CRTIMP int __cdecl _wunlink(_In_z_ const wchar_t * _Filename);
_Check_return_ _CRTIMP int __cdecl _wrename(_In_z_ const wchar_t * _OldFilename, _In_z_ const wchar_t * _NewFilename);
_CRTIMP errno_t __cdecl _wmktemp_s(_Inout_updates_z_(_SizeInWords) wchar_t * _TemplateName, _In_ size_t _SizeInWords);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_0(errno_t, _wmktemp_s, _Prepost_z_ wchar_t, _TemplateName)
__DEFINE_CPP_OVERLOAD_STANDARD_FUNC_0_0(wchar_t *, __RETURN_POLICY_DST, _CRTIMP, _wmktemp, _Inout_z_, wchar_t, _TemplateName)

_Check_return_ _CRTIMP intptr_t __cdecl _wfindfirst32i64(_In_z_ const wchar_t * _Filename, _Out_ struct _wfinddata32i64_t * _FindData);
_Check_return_ _CRTIMP intptr_t __cdecl _wfindfirst64i32(_In_z_ const wchar_t * _Filename, _Out_ struct _wfinddata64i32_t * _FindData);
_Check_return_ _CRTIMP intptr_t __cdecl _wfindfirst64(_In_z_ const wchar_t * _Filename, _Out_ struct _wfinddata64_t * _FindData);
_Check_return_ _CRTIMP int __cdecl _wfindnext32i64(_In_ intptr_t _FindHandle, _Out_ struct _wfinddata32i64_t * _FindData);
_Check_return_ _CRTIMP int __cdecl _wfindnext64i32(_In_ intptr_t _FindHandle, _Out_ struct _wfinddata64i32_t * _FindData);
_Check_return_ _CRTIMP int __cdecl _wfindnext64(_In_ intptr_t _FindHandle, _Out_ struct _wfinddata64_t * _FindData);

_Check_return_wat_ _CRTIMP errno_t __cdecl _wsopen_s(_Out_ int * _FileHandle, _In_z_ const wchar_t * _Filename, _In_ int _OpenFlag, _In_ int _ShareFlag, _In_ int _PermissionFlag);

#if !defined(__cplusplus) || !defined(_M_IX86)

_Check_return_ _CRT_INSECURE_DEPRECATE(_wsopen_s) _CRTIMP int __cdecl _wopen(_In_z_ const wchar_t * _Filename, _In_ int _OpenFlag, ...);
_Check_return_ _CRT_INSECURE_DEPRECATE(_wsopen_s) _CRTIMP int __cdecl _wsopen(_In_z_ const wchar_t * _Filename, _In_ int _OpenFlag, int _ShareFlag, ...);

#else

/* these function do not validate pmode; use _sopen_s */
extern "C++" _CRT_INSECURE_DEPRECATE(_wsopen_s) _CRTIMP int __cdecl _wopen(_In_z_ const wchar_t * _Filename, _In_ int _OpenFlag, _In_ int _PermissionMode = 0);
extern "C++" _CRT_INSECURE_DEPRECATE(_wsopen_s) _CRTIMP int __cdecl _wsopen(_In_z_ const wchar_t * _Filename, _In_ int _OpenFlag, _In_ int _ShareFlag, int _PermissionMode = 0);

#endif

#define _WIO_DEFINED
#endif

int  __cdecl __lock_fhandle(_In_ int _Filehandle);
void __cdecl _unlock_fhandle(_In_ int _Filehandle);

#ifndef _NOT_CRTL_BUILD_
_Check_return_ int __cdecl _chsize_nolock(_In_ int _FileHandle,_In_ __int64 _Size);
_Check_return_opt_ int __cdecl _close_nolock(_In_ int _FileHandle);
_Check_return_opt_ long __cdecl _lseek_nolock(_In_ int _FileHandle, _In_ long _Offset, _In_ int _Origin);
_Check_return_ int __cdecl _setmode_nolock(_In_ int _FileHandle, _In_ int _Mode);
_Check_return_ int __cdecl _read_nolock(_In_ int _FileHandle, _Out_writes_bytes_(_MaxCharCount) void * _DstBuf, _In_ unsigned int _MaxCharCount);
_Check_return_ int __cdecl _write_nolock(_In_ int _FileHandle, _In_reads_bytes_(_MaxCharCount) const void * _Buf, _In_ unsigned int _MaxCharCount);
_Check_return_opt_ __int64 __cdecl _lseeki64_nolock(_In_ int _FileHandle, _In_ __int64 _Offset, _In_ int _Origin);

#if defined(_CRT_DISABLE_PERFCRIT_LOCKS) && !defined(_DLL)
#define _chsize(fh, size)               _chsize_nolock(fh, size)
#define _close(fh)                      _close_nolock(fh)
#define _lseek(fh, offset, origin)      _lseek_nolock(fh, offset, origin)
#define _setmode(fh, mode)              _setmode_nolock(fh, mode)
#define _read(fh, buff, count)          _read_nolock(fh, buff, count)
#define _write(fh, buff, count)         _write_nolock(fh, buff, count)
#define _lseeki64(fh,offset,origin)     _lseeki64_nolock(fh,offset,origin)
#endif

#endif  /* _NOT_CRTL_BUILD_ */

_CRTIMP intptr_t __cdecl _get_osfhandle(_In_ int _FileHandle);
_CRTIMP int __cdecl _open_osfhandle(_In_ intptr_t _OSFileHandle, _In_ int _Flags);

#if     !__STDC__

/* Non-ANSI names for compatibility */

#pragma warning(push)
#pragma warning(disable: 4141) /* Using deprecated twice */ 
_Check_return_ _CRT_NONSTDC_DEPRECATE(_access) _CRTIMP int __cdecl access(_In_z_ const char * _Filename, _In_ int _AccessMode);
_Check_return_ _CRT_NONSTDC_DEPRECATE(_chmod) _CRTIMP int __cdecl chmod(_In_z_ const char * _Filename, int _AccessMode);
_Check_return_ _CRT_NONSTDC_DEPRECATE(_chsize) _CRTIMP int __cdecl chsize(_In_ int _FileHandle, _In_ long _Size);
_Check_return_opt_ _CRT_NONSTDC_DEPRECATE(_close) _CRTIMP int __cdecl close(_In_ int _FileHandle);
_Check_return_ _CRT_NONSTDC_DEPRECATE(_creat) _CRT_INSECURE_DEPRECATE(_sopen_s) _CRTIMP int __cdecl creat(_In_z_ const char * _Filename, _In_ int _PermissionMode);
_Check_return_ _CRT_NONSTDC_DEPRECATE(_dup) _CRTIMP int __cdecl dup(_In_ int _FileHandle);
_Check_return_ _CRT_NONSTDC_DEPRECATE(_dup2) _CRTIMP int __cdecl dup2(_In_ int _FileHandleSrc, _In_ int _FileHandleDst);
_Check_return_ _CRT_NONSTDC_DEPRECATE(_eof) _CRTIMP int __cdecl eof(_In_ int _FileHandle);
_Check_return_ _CRT_NONSTDC_DEPRECATE(_filelength) _CRTIMP long __cdecl filelength(_In_ int _FileHandle);
_Check_return_ _CRT_NONSTDC_DEPRECATE(_isatty) _CRTIMP int __cdecl isatty(_In_ int _FileHandle);
_Check_return_ _CRT_NONSTDC_DEPRECATE(_locking) _CRTIMP int __cdecl locking(_In_ int _FileHandle, _In_ int _LockMode, _In_ long _NumOfBytes);
_Check_return_opt_ _CRT_NONSTDC_DEPRECATE(_lseek) _CRTIMP long __cdecl lseek(_In_ int _FileHandle, _In_ long _Offset, _In_ int _Origin);
_CRT_NONSTDC_DEPRECATE(_mktemp) _CRT_INSECURE_DEPRECATE(_mktemp_s) _CRTIMP char * __cdecl mktemp(_Inout_z_ char * _TemplateName);
_Check_return_ _CRT_NONSTDC_DEPRECATE(_open) _CRT_INSECURE_DEPRECATE(_sopen_s) _CRTIMP int __cdecl open(_In_z_ const char * _Filename, _In_ int _OpenFlag, ...);
_CRT_NONSTDC_DEPRECATE(_read) _CRTIMP int __cdecl read(int _FileHandle, _Out_writes_bytes_(_MaxCharCount) void * _DstBuf, _In_ unsigned int _MaxCharCount);
_Check_return_ _CRT_NONSTDC_DEPRECATE(_setmode) _CRTIMP int __cdecl setmode(_In_ int _FileHandle, _In_ int _Mode);
_CRT_NONSTDC_DEPRECATE(_sopen) _CRT_INSECURE_DEPRECATE(_sopen_s) _CRTIMP int __cdecl sopen(const char * _Filename, _In_ int _OpenFlag, _In_ int _ShareFlag, ...);
_Check_return_ _CRT_NONSTDC_DEPRECATE(_tell) _CRTIMP long __cdecl tell(_In_ int _FileHandle);
_CRT_NONSTDC_DEPRECATE(_umask) _CRT_INSECURE_DEPRECATE(_umask_s) _CRTIMP int __cdecl umask(_In_ int _Mode);
_Check_return_ _CRT_NONSTDC_DEPRECATE(_write) _CRTIMP int __cdecl write(_In_ int _Filehandle, _In_reads_bytes_(_MaxCharCount) const void * _Buf, _In_ unsigned int _MaxCharCount);
#pragma warning(pop)

#endif  /* __STDC__ */

#ifdef  __cplusplus
}
#endif

#endif  /* _POSIX_ */

#pragma pack(pop)

#endif  /* _INC_IO */
