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
*stdlib.h - declarations/definitions for commonly used library functions
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       This include file contains the function declarations for commonly
*       used library functions which either don't fit somewhere else, or,
*       cannot be declared in the normal place for other reasons.
*       [ANSI]
*
*       [Public]
*
*Revision History:
*       06-03-87  JMB   Added MSSDK_ONLY switch to OS2_MODE, DOS_MODE
*       06-30-87  SKS   Added MSSDK_ONLY switch to _osmode
*       08-17-87  PHG   Removed const from params to _makepath, _splitpath,
*                       _searchenv to conform with spec and documentation.
*       10-20-87  JCR   Removed "MSC40_ONLY" entries and "MSSDK_ONLY" comments
*       12-11-87  JCR   Added "_loadds" functionality
*       12-18-87  JCR   Added _FAR_ to declarations
*       01-04-88  WAJ   Increased _MAX_PATH and _MAX_DIR
*       01-21-88  JCR   Removed _LOAD_DS from search routine declarations
*       02-10-88  JCR   Cleaned up white space
*       05-31-88  SKS   Added EXIT_SUCCESS and EXIT_FAILURE
*       08-19-88  GJF   Modified to also work for the 386 (small model only)
*       09-29-88  JCR   onexit/atexit user routines must be _loadds in DLL
*       09-30-88  JCR   environ is a routine for DLL (update)
*       12-08-88  JCR   DLL environ is resolved directly (no __environ call)
*       12-15-88  GJF   Added definition of NULL (ANSI)
*       12-27-88  JCR   Added _fileinfo, also DLL support for _fmode entry
*       05-03-89  JCR   Corrected _osmajor/_osminor for 386
*       05-03-89  JCR   Added _INTERNAL_IFSTRIP for relinc usage
*       07-24-89  GJF   Gave names to the structs for div_t and ldiv_t types
*       08-01-89  GJF   Cleanup, now specific to OS/2 2.0 (i.e., 386 flat
*                       model). Also added parens to *_errno and *_doserrno
*                       definitions (same as 11-14-88 change to CRT version).
*       10-25-89  JCR   Upgraded _MAX values for long filename support
*       10-30-89  GJF   Fixed copyright
*       11-02-89  JCR   Changed "DLL" to "_DLL", removed superfluous _DLL defs
*       11-17-89  GJF   Moved _fullpath prototype here (from direct.h). Also,
*                       added const to appropriate arg types for _makepath(),
*                       putenv(), _searchenv() and _splitpath().
*       11-20-89  JCR   Routines are now _cdecl in both single and multi-thread
*       11-27-89  KRS   Fixed _MAX_PATH etc. to match current OS/2 limits.
*       03-02-90  GJF   Added #ifndef _INC_STDLIB and #include <cruntime.h>
*                       stuff. Also, removed some (now) useless preprocessor
*                       directives.
*       03-22-90  GJF   Replaced _cdecl with _CALLTYPE1 in prototypes and
*                       with _VARTYPE1 in variable declarations.
*       04-10-90  GJF   Made _errno() and __doserrno() _CALLTYPE1.
*       08-15-90  SBM   Made MTHREAD _errno() and __doserrno() return int *
*       10-31-90  JCR   Added WINR_MODE and WINP_MODE for consistency
*       11-12-90  GJF   Changed NULL to (void *)0.
*       11-30-90  GJF   Conditioned definition of _doserrno on _CRUISER_ or
*                       _WIN32_
*       01-21-91  GJF   ANSI naming.
*       02-12-91  GJF   Only #define NULL if it isn't #define-d.
*       03-21-91  KRS   Added wchar_t type, MB_CUR_MAX macro, and mblen,
*                       mbtowc, mbstowcs, wctomb, and wcstombs functions.
*       04-09-91  PNT   Added _MAC_ definitions
*       05-21-91  GJF   #define onexit_t to _onexit_t if __STDC__ is not
*                       not defined
*       08-08-91  GJF   Added prototypes for _atold and _strtold.
*       08-20-91  JCR   C++ and ANSI naming
*       08-26-91  BWM   Added prototypes for _beep, _sleep, _seterrormode.
*       09-28-91  JCR   ANSI names: DOSX32=prototypes, WIN32=#defines for now
*       11-15-91  GJF   Changed definitions of min and max to agree with
*                       windef.h
*       01-22-92  GJF   Fixed up definitions of global variables for build of,
*                       and users of, crtdll.dll. Also, deleted declaration
*                       of _psp (has no meaning outside of DOS).
*       01-30-92  GJF   Removed prototype for _strtold (no such function yet).
*       03-30-92  DJM   POSIX support.
*       04-29-92  GJF   Added _putenv_lk and _getenv_lk for Win32.
*       06-16-92  KRS   Added prototypes for wcstol and wcstod.
*       06-29-92  GJF   Removed bogus #define.
*       08-05-92  GJF   Function calling type and variable type macros. Also,
*                       replaced ref. to i386 with ref to _M_IX86.
*       08-18-92  KRS   Add __mblen_lk.
*       08-21-92  GJF   Conditionally removed _atold for Win32 (no long double
*                       in Win32).
*       08-21-92  GJF   Moved __mblen_lk into area that is stripped out by
*                       release scripts.
*       08-23-92  GJF   Exposed _itoa, _ltoa, _ultoa, mblen, mbtowc, mbstowcs
*                       for POSIX.
*       08-26-92  SKS   Add _osver, _winver, _winmajor, _winminor, _pgmptr
*       09-03-92  GJF   Merged changes from 8-5-92 on.
*       01-21-93  GJF   Removed support for C6-386's _cdecl.
*       03-01-93  SKS   Add __argc and __argv
*       03-20-93  SKS   Remove obsolete variables _cpumode, _osmajor, etc.
*                       Remove obsolete functions _beep, _sleep, _seterrormode
*       04-06-93  SKS   Replace _CRTAPI1/2 with __cdecl, _CRTVAR1 with nothing
*       04-07-93  SKS   Add _CRTIMP keyword for CRT DLL model
*                       Use link-time aliases for old names, not #define's
*       04-12-93  CFW   Add _MB_CUR_MAX_DEFINED protection.
*       04-14-93  CFW   Simplify MB_CUR_MAX def.
*       04-29-93  CFW   Add _mbslen prototype.
*       05-24-93  SKS   atexit and _onexit are no longner imported directly
*       06-03-93  CFW   Change _mbslen to _mbstrlen, returning type size_t.
*       09-13-93  CFW   Add _wtox and _xtow function prototypes.
*       09-13-93  GJF   Merged Cuda and NT SDK versions, with some cleanup.
*       10-21-93  GJF   Re-purged the obsolete version and mode variables.
*                       Changed _NTSDK definitions for _fmode slightly, to
*                       support setting it in dllstuff\crtexe.c.
*       11-19-93  CFW   Add _wargv, _wpgmptr.
*       11-29-93  CFW   Wide environment.
*       12-07-93  CFW   Move wide defs outside __STDC__ check.
*       12-23-93  GJF   __mb_cur_max must same type for _NTSDK (int) as for
*                       our own build.
*       04-27-94  GJF   Changed definitions/declarations of:
*                           MB_CUR_MAX
*                           __argc
*                           __argv
*                           __wargv
*                           _environ
*                           _wenviron
*                           _fmode
*                           _osver
*                           _pgmptr
*                           _wpgmptr
*                           _winver
*                           _winmajor
*                           _winminor
*                       for _DLL and/or DLL_FOR_WIN32S.
*       05-03-94  GJF   Made the changes above, for _DLL, conditional on
*                       _M_IX86 too.
*       06-06-94  SKS   Change if def(_MT) to if def(_MT) || def(_DLL)
*                       This will support single-thread apps using MSVCRT*.DLL
*       09-06-94  GJF   Added _set_error_mode() and related constants.
*       11-03-94  GJF   Ensure 8 byte alignment.
*       12-28-94  JCF   Merged with mac header.
*       02-11-95  CFW   Remove duplicate def.
*       02-11-95  CFW   Add _CRTBLD to avoid users getting wrong headers.
*       02-14-95  CFW   Clean up Mac merge.
*       03-10-95  BWT   Add _CRTIMP for mips intrinsics.
*       03-30-95  BWT   Fix _fmode definition for non-X86 CRTEXE. (second try)
*       10-20-95  GJF   #ifdef-ed out declarations of toupper/tolower for ANSI
*                       compilations (-Za). This was Olympus 1314. They cannot
*                       be removed completely (for now) because they are
*                       documented.
*       12-14-95  JWM   Add "#pragma once".
*       03-18-96  SKS   Add _fileinfo to variables implemented as functions.
*       04-01-96  BWT   Add _i64toa, _i64tow, _ui64toa, _ui64tow, _atoi64, _wtoi64
*       05-15-96  BWT   Fix POSIX definitions of environ
*       08-13-96  JWM   Add inline long abs(long), ifdef __cplusplus only.
*       08-19-96  JWM   Removed inline abs - entire header is being
*                       wrapped in 'extern "C"' by some users.
*       01-20-97  GJF   Cleaned out obsolete support for Win32s, _CRTAPI* and
*                       _NTSDK.
*       04-04-97  GJF   Restored some of the deleted support because it might
*                       be useful in planned startup improvements.
*       04-16-97  GJF   Restored some more for the same reason.
*       08-13-97  GJF   Strip out __p_* prototypes which aren't actually used
*                       from release version.
*       09-30-97  JWM   Restored not-so-obsolete _CRTAPI1 support.
*       09-10-98  GJF   Added support for per-thread locale information.
*       10-02-98  GJF   Added _osplatform.
*       12-15-98  GJF   Changes for 64-bit size_t.
*       05-13-99  PML   Remove _CRTAPI1
*       05-17-99  PML   Remove all Macintosh support.
*       10-06-99  PML   Add _W64 modifier to types which are 32 bits in Win32,
*                       64 bits in Win64.
*       02-11-00  GB    Add _strtoi64, wcstoi64, strtoui64 and wcstoui64
*       08-29-00  PML   Add _set_security_error_handler.
*       01-04-01  GB    Add _abs64, _rotl64, _rotr64, _byteswap and functions
*       01-29-01  GB    Added _func function version of data variable used in msvcprt.lib
*                       to work with STATIC_CPPLIB
*       07-15-01  PML   Remove all ALPHA, MIPS, and PPC code
*       11-12-01  GB    Added support for new locale implementation.
*       03-04-02  PML   Add __declspec(noalias, restrict) support
*       05-20-02  MSL   Added _set_purecall_handler
*       06-19-02  MSL   Added _CVTBUFSIZE as a public symbol for floating point conversion
*                       buffer size
*                       VS7 551701
*       08-14-02  CSP   Added AMD64 to security #ifdef's (_SECERR_BUFFER_OVERRUN) 
*       10-01-02  GB    Added ldiv_t div(long, long) and long abs(long)
*       03-10-03  EAN   Change argument names of div(long, long) to be _A1 and _A2 (VSWhidbey#65116)
*       08-01-03  AC    Added support for _dbg functions (a la _malloc_dbg)
*       08-04-03  GB    Exposing __clrcall only to MSVC compiler.
*       08-11-03  AC    Added safe versions (_fcvt_s, _ecvt_s, _gcvt_s).
*                       Added safe versions (_itoa_s, etc.)
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*       10-29-03  AC    Addded accessors for global variables
*       01-27-04  SJ    Remove _fileinfo variable
*       03-08-04  MSL   Added missing declarations of [a|w]to[f|l|i|i64]_l
*                       VSW#247255
*       03-09-04  AJS   added __clrcall and int versions of _set_invalid_parameter_handler
*       03-18-04  AC    Added _MAX_ENV constant. Changed _mbstrlen_s to _mbstrnlen.
*       04-07-04  MSL   Fixed onexit, purecall handler to work for managed
*                       VSW#216342
*                       VSW#226958
*       04-10-04  AJS   Disabled access to data exports under /clr:pure
*       04-28-04  AC    Readd _fileinfo variable, will remove in the next LKG.
*       05-11-04  AC    rand_s is declared only if _CRT_RAND_S is defined.
*                       VSW#279361
*       07-19-04  AC    Added _CRT_ALTERNATIVE_* defines for safecrt.h
*       07-30-04  AJS   Reremove _fileinfo variable for LKG9.
*       08-05-04  AC    Removed _CRT_ALTERNATIVE and introduced _CRTIMP_ALTERNATIVE.
*       08-10-04  AC    Added __cdecl to _set_errno and other functions
*       08-17-04  AJS   Removed _set_security_error_handler. VSWhidbey:281269
*       09-10-04  AC    Remove deprecation for _doserrno
*       09-08-04  AC    Added _dupenv_s family.
*                       VSW#355011
*                       Moved _get_fmode/_set_fmode from io.h
*                       VSW#265694
*                       Added _countof helper macro
*                       VSW#310001
*                       Added some internal _set_* functions for globals
*                       Removed some unused *_nolock functions.
*       09-14-04  MR    Add _wcstod_l (VSWhidbey: 304701)
*       09-24-04  MSL   Obsolete OS version functions
*       10-05-04  AC    Removed obsolete strlen_s family.
*                       VSW#379264
*       10-08-04  AC    Added __cdecl to _get_winmajor and other functions
*                       VSW#381819
*       10-10-04  ESC   Use _CRT_PACKING
*       10-31-04  MSL   Add missing __clrcall search/sort functions
*       11-02-04  AC    Add cpp overloads for secure functions.
*                       VSW#313364
*       11-07-04  MSL   Add ability to get purecall and invalid param functions; fix FMODE internal deprecation
*       11-08-04  JL    Added _CRT_NONSTDC_DEPRECATE to deprecate non-ANSI functions
*       11-18-04  ATC   SAL Annotations for CRT Headers
*       01-14-05  AC    Fix SAL annotations (using prefast espx plug-in)
*       01-20-05  AC    Added _CRT_INSECURE_DEPRECATE_GLOBALS
*                       VSW#438137
*       01-24-05  GR    Add _CRT_JIT_INTRINSIC annotations for JIT64 use
*       01-25-05  MSL   Add param names to overload macros
*       03-23-05  MSL   _P removal - not needed
*                       New deprecation warning with function name
*                       Packing fix
*       04-04-05  JL    Undeprecate _putenv and _wputenv VSW#425207
*       04-14-05  MSL   Intellisense locale name cleanup
*                       Compile clean under MIDL
*                       SAL cleanup for n functions
*       05-06-05  PML   add ___mb_cur_max_l_func
*       05-13-05  MSL   Switch _freea to inline
*       05-18-05  PML   Remove all traces of __set_security_error_handler
*       05-27-05  AGH   Clarify concept of "_m" functions in mixed mode. 
*       06-03-05  MSL   Revert 64 bit definition of NULL due to calling-convention fix
*       12-07-05  MSL   SAL fixes for prefast
*       01-18-05  ATC   Added push/pop_macro for _CRTDBG_MAP_ALLOC functions
*       11-08-06  PMB   Remove support for Win9x and _osplatform and relatives.
*                       DDB#11325
*       11-10-06  PMB   Removed most _INTEGRAL_MAX_BITS #ifdefs
*                       DDB#11174
*       04-11-07  WL    SAL fixes for wcstombs and mbstowcs. DDB#89957.
*       04-20-07  GM    Removed __Detect_msvcrt_and_setup().
*
****/

#pragma once

#ifndef _INC_STDLIB
#define _INC_STDLIB

#include <crtdefs.h>
#include <limits.h>

/*
 * Currently, all MS C compilers for Win32 platforms default to 8 byte
 * alignment.
 */
#pragma pack(push,_CRT_PACKING)

#ifdef  __cplusplus
extern "C" {
#endif

/* Define NULL pointer value */
#ifndef NULL
#ifdef __cplusplus
#define NULL    0
#else
#define NULL    ((void *)0)
#endif
#endif

/* Definition of the argument values for the exit() function */

#define EXIT_SUCCESS    0
#define EXIT_FAILURE    1


#ifndef _ONEXIT_T_DEFINED

#if !defined (_M_CEE_PURE)
typedef int (__cdecl * _onexit_t)(void);
#else
typedef int (__clrcall * _onexit_t)(void);
typedef _onexit_t _onexit_m_t;
#endif

#if defined (_M_CEE_MIXED)
typedef int (__clrcall * _onexit_m_t)(void);
#endif

#if     !__STDC__
/* Non-ANSI name for compatibility */
#define onexit_t _onexit_t
#endif

#define _ONEXIT_T_DEFINED
#endif


/* Data structure definitions for div and ldiv runtimes. */

#ifndef _DIV_T_DEFINED

typedef struct _div_t {
        int quot;
        int rem;
} div_t;

typedef struct _ldiv_t {
        long quot;
        long rem;
} ldiv_t;

typedef struct _lldiv_t {
        long long quot;
        long long rem;
} lldiv_t;

#define _DIV_T_DEFINED
#endif

/*
 * structs used to fool the compiler into not generating floating point
 * instructions when copying and pushing [long] double values
 */

#ifndef _CRT_DOUBLE_DEC

#ifndef _LDSUPPORT

#pragma pack(4)
typedef struct {
    unsigned char ld[10];
} _LDOUBLE;
#pragma pack()

#define _PTR_LD(x) ((unsigned char  *)(&(x)->ld))

#else

/* push and pop long, which is #defined as __int64 by a spec2k test */
#pragma push_macro("long")
#undef long
typedef long double _LDOUBLE;
#pragma pop_macro("long")

#define _PTR_LD(x) ((unsigned char  *)(x))

#endif

typedef struct {
        double x;
} _CRT_DOUBLE;

typedef struct {
    float f;
} _CRT_FLOAT;

/* push and pop long, which is #defined as __int64 by a spec2k test */
#pragma push_macro("long")
#undef long

typedef struct {
        /*
         * Assume there is a long double type
         */
        long double x;
} _LONGDOUBLE;

#pragma pop_macro("long")

#pragma pack(4)
typedef struct {
    unsigned char ld12[12];
} _LDBL12;
#pragma pack()

#define _CRT_DOUBLE_DEC
#endif

/* Maximum value that can be returned by the rand function. */

#define RAND_MAX 0x7fff

/*
 * Maximum number of bytes in multi-byte character in the current locale
 * (also defined in ctype.h).
 */
#ifndef MB_CUR_MAX
#ifndef _INTERNAL_IFSTRIP_
#if     defined(_DLL) && defined(_M_IX86)
/* Retained for compatibility with VC++ 5.0 and earlier versions */
_CRTIMP int * __cdecl __p___mb_cur_max(void);
#endif
#define __MB_CUR_MAX(ptloci) (ptloci)->mb_cur_max
#endif  /* _INTERNAL_IFSTRIP_ */
#define MB_CUR_MAX ___mb_cur_max_func()
#if !defined(_M_CEE_PURE)
_CRTIMP extern int __mb_cur_max;
#else
_CRTIMP int* __cdecl __p___mb_cur_max(void);
#define __mb_cur_max (*__p___mb_cur_max())
#endif /* !defined(_M_CEE_PURE) */
_CRTIMP int __cdecl ___mb_cur_max_func(void);
_CRTIMP int __cdecl ___mb_cur_max_l_func(_locale_t);
#endif  /* MB_CUR_MAX */

/* Minimum and maximum macros */

#define __max(a,b)  (((a) > (b)) ? (a) : (b))
#define __min(a,b)  (((a) < (b)) ? (a) : (b))

/*
 * Sizes for buffers used by the _makepath() and _splitpath() functions.
 * note that the sizes include space for 0-terminator
 */
#define _MAX_PATH   260 /* max. length of full pathname */
#define _MAX_DRIVE  3   /* max. length of drive component */
#define _MAX_DIR    256 /* max. length of path component */
#define _MAX_FNAME  256 /* max. length of file name component */
#define _MAX_EXT    256 /* max. length of extension component */

/*
 * Argument values for _set_error_mode().
 */
#define _OUT_TO_DEFAULT 0
#define _OUT_TO_STDERR  1
#define _OUT_TO_MSGBOX  2
#define _REPORT_ERRMODE 3

/*
 * Argument values for _set_abort_behavior().
 */
#define _WRITE_ABORT_MSG    0x1 /* debug only, has no effect in release */
#define _CALL_REPORTFAULT   0x2

/*
 * Sizes for buffers used by the getenv/putenv family of functions.
 */
#define _MAX_ENV 32767   

#if !defined(_M_CEE_PURE)
/* a purecall handler procedure. Never returns normally */
typedef void (__cdecl *_purecall_handler)(void); 

/* establishes a purecall handler for the process */
_CRTIMP _purecall_handler __cdecl _set_purecall_handler(_In_opt_ _purecall_handler _Handler);
_CRTIMP _purecall_handler __cdecl _get_purecall_handler(void);
#endif

#if defined(__cplusplus)
extern "C++"
{
#if defined(_M_CEE_PURE)
    typedef void (__clrcall *_purecall_handler)(void);
    typedef _purecall_handler _purecall_handler_m;
    _MRTIMP _purecall_handler __cdecl _set_purecall_handler(_In_opt_ _purecall_handler _Handler);
#endif
}
#endif

#if !defined(_M_CEE_PURE)
/* a invalid_arg handler procedure. */
typedef void (__cdecl *_invalid_parameter_handler)(const wchar_t *, const wchar_t *, const wchar_t *, unsigned int, uintptr_t); 

/* establishes a invalid_arg handler for the process */
_CRTIMP _invalid_parameter_handler __cdecl _set_invalid_parameter_handler(_In_opt_ _invalid_parameter_handler _Handler);
_CRTIMP _invalid_parameter_handler __cdecl _get_invalid_parameter_handler(void);
#endif

/* External variable declarations */
#ifndef _CRT_ERRNO_DEFINED
#define _CRT_ERRNO_DEFINED
_CRTIMP extern int * __cdecl _errno(void);
#define errno   (*_errno())

errno_t __cdecl _set_errno(_In_ int _Value);
errno_t __cdecl _get_errno(_Out_ int * _Value);
#endif

_CRTIMP unsigned long * __cdecl __doserrno(void);
#define _doserrno   (*__doserrno())

errno_t __cdecl _set_doserrno(_In_ unsigned long _Value);
errno_t __cdecl _get_doserrno(_Out_ unsigned long * _Value);

/* you can't modify this, but it is non-const for backcompat */
_CRTIMP _CRT_INSECURE_DEPRECATE(strerror) char ** __cdecl __sys_errlist(void);
#define _sys_errlist (__sys_errlist())

_CRTIMP _CRT_INSECURE_DEPRECATE(strerror) int * __cdecl __sys_nerr(void);
#define _sys_nerr (*__sys_nerr())

#if     defined(_DLL) && defined(_M_IX86)

#ifdef _CRT_USE_WINAPI_FAMILY_DESKTOP_APP
_CRTIMP int *          __cdecl __p___argc(void);
_CRTIMP char ***       __cdecl __p___argv(void);
_CRTIMP wchar_t ***    __cdecl __p___wargv(void);
_CRTIMP char ***       __cdecl __p__environ(void);
_CRTIMP wchar_t ***    __cdecl __p__wenviron(void);
#endif /* _CRT_USE_WINAPI_FAMILY_DESKTOP_APP */

_CRTIMP char **        __cdecl __p__pgmptr(void);
_CRTIMP wchar_t **     __cdecl __p__wpgmptr(void);

#ifndef _INTERNAL_IFSTRIP_
/* Retained for compatibility with VC++ 5.0 and earlier versions */
_CRTIMP int *          __cdecl __p__fmode(void);

#endif  /* _INTERNAL_IFSTRIP_ */

#endif  /* _M_IX86 && _DLL */

#ifdef _CRT_USE_WINAPI_FAMILY_DESKTOP_APP

#if !defined(_M_CEE_PURE)
_CRTIMP extern int __argc;          /* count of cmd line args */
_CRTIMP extern char ** __argv;      /* pointer to table of cmd line args */
_CRTIMP extern wchar_t ** __wargv;  /* pointer to table of wide cmd line args */
#else
_CRTIMP int* __cdecl __p___argc(void);
_CRTIMP char*** __cdecl __p___argv(void);
_CRTIMP wchar_t*** __cdecl __p___wargv(void);
#define __argv (*__p___argv())
#define __argc (*__p___argc())
#define __wargv (*__p___wargv())
#endif

#endif /* _CRT_USE_WINAPI_FAMILY_DESKTOP_APP */

#if !defined(_M_CEE_PURE)

#ifdef _CRT_USE_WINAPI_FAMILY_DESKTOP_APP

#ifdef  _POSIX_
extern char ** environ;             /* pointer to environment table */
#else
_CRTIMP extern char ** _environ;    /* pointer to environment table */
_CRTIMP extern wchar_t ** _wenviron;    /* pointer to wide environment table */
#endif  /* _POSIX_ */

#endif /* _CRT_USE_WINAPI_FAMILY_DESKTOP_APP */

_CRT_INSECURE_DEPRECATE_GLOBALS(_get_pgmptr) _CRTIMP extern char * _pgmptr;      /* points to the module (EXE) name */
_CRT_INSECURE_DEPRECATE_GLOBALS(_get_wpgmptr) _CRTIMP extern wchar_t * _wpgmptr;  /* points to the module (EXE) wide name */

#ifndef _INTERNAL_IFSTRIP_

_DEFINE_SET_FUNCTION(_set_pgmptr, char *, _pgmptr)
_DEFINE_SET_FUNCTION(_set_wpgmptr, wchar_t *, _wpgmptr)

#endif /* _INTERNAL_IFSTRIP_ */

#else

#ifdef _CRT_USE_WINAPI_FAMILY_DESKTOP_APP
_CRTIMP char*** __cdecl __p__environ(void);
_CRTIMP wchar_t*** __cdecl __p__wenviron(void);
#endif /* _CRT_USE_WINAPI_FAMILY_DESKTOP_APP */

_CRT_INSECURE_DEPRECATE_GLOBALS(_get_pgmptr) _CRTIMP char** __cdecl __p__pgmptr(void);
_CRT_INSECURE_DEPRECATE_GLOBALS(_get_wpgmptr) _CRTIMP wchar_t** __cdecl __p__wpgmptr(void);

#ifdef _CRT_USE_WINAPI_FAMILY_DESKTOP_APP
#define _environ   (*__p__environ())
#define _wenviron  (*__p__wenviron())
#endif /* _CRT_USE_WINAPI_FAMILY_DESKTOP_APP */

#define _pgmptr    (*__p__pgmptr())
#define _wpgmptr   (*__p__wpgmptr())

#endif /* !defined(_M_CEE_PURE) */

errno_t __cdecl _get_pgmptr(_Outptr_result_z_ char ** _Value);
errno_t __cdecl _get_wpgmptr(_Outptr_result_z_ wchar_t ** _Value);


#ifdef  SPECIAL_CRTEXE
extern int _fmode;          /* default file translation mode */
#else
#if !defined(_M_CEE_PURE)
_CRT_INSECURE_DEPRECATE_GLOBALS(_get_fmode) _CRTIMP extern int _fmode;          /* default file translation mode */
#else
_CRTIMP int* __cdecl __p__fmode(void);
#define _fmode (*__p__fmode())
#endif /* !defined(_M_CEE_PURE) */
#endif

_CRTIMP errno_t __cdecl _set_fmode(_In_ int _Mode);
_CRTIMP errno_t __cdecl _get_fmode(_Out_ int * _PMode);

/* _countof helper */
#if !defined(_countof)
#if !defined(__cplusplus)
#define _countof(_Array) (sizeof(_Array) / sizeof(_Array[0]))
#else
extern "C++"
{
template <typename _CountofType, size_t _SizeOfArray>
char (*__countof_helper(_UNALIGNED _CountofType (&_Array)[_SizeOfArray]))[_SizeOfArray];
#define _countof(_Array) (sizeof(*__countof_helper(_Array)) + 0)
}
#endif
#endif

/* function prototypes */

#ifndef _CRT_TERMINATE_DEFINED
#define _CRT_TERMINATE_DEFINED
_CRTIMP __declspec(noreturn) void __cdecl exit(_In_ int _Code);
_CRTIMP __declspec(noreturn) void __cdecl _exit(_In_ int _Code);
_CRTIMP __declspec(noreturn) void __cdecl abort(void);
#endif

_CRTIMP unsigned int __cdecl _set_abort_behavior(_In_ unsigned int _Flags, _In_ unsigned int _Mask);

#ifndef _CRT_ABS_DEFINED
#define _CRT_ABS_DEFINED
        int       __cdecl abs(_In_ int _X);
        long      __cdecl labs(_In_ long _X);
        long long __cdecl llabs(_In_ long long _X);
#endif

        __int64    __cdecl _abs64(__int64);
#if defined(_M_CEE) /*IFSTRIP=IGN*/
#pragma warning (push)
#pragma warning (disable: 4985)
        _Check_return_ int    __clrcall _atexit_m_appdomain(_In_opt_ void (__clrcall * _Func)(void));
#if defined(_M_CEE_MIXED)
#ifdef __cplusplus
        [System::Security::SecurityCritical] 
#endif /* __cplusplus */
#pragma warning (suppress: 4985)
        _Check_return_ int    __clrcall _atexit_m(_In_opt_ void (__clrcall * _Func)(void));
#else
#ifdef __cplusplus
        [System::Security::SecurityCritical] 
#endif /* __cplusplus */
        _Check_return_ inline int __clrcall _atexit_m(_In_opt_ void (__clrcall *_Function)(void))
        {
            return _atexit_m_appdomain(_Function);
        }
#endif
#pragma warning (pop)
#endif
#if defined(_M_CEE_PURE)
        /* In pure mode, atexit is the same as atexit_m_appdomain */
extern "C++"
{
#ifdef __cplusplus
        [System::Security::SecurityCritical] 
#endif /* __cplusplus */
inline  int    __clrcall atexit
(
    void (__clrcall *_Function)(void)
)
{
    return _atexit_m_appdomain(_Function);
}
}
#else
        int    __cdecl atexit(void (__cdecl *)(void));
#endif
#ifndef _CRT_ATOF_DEFINED
#define _CRT_ATOF_DEFINED
_Check_return_ _CRTIMP double  __cdecl atof(_In_z_ const char *_String);
_Check_return_ _CRTIMP double  __cdecl _atof_l(_In_z_ const char *_String, _In_opt_ _locale_t _Locale);
#endif
_Check_return_ _CRTIMP _CRT_JIT_INTRINSIC int    __cdecl atoi(_In_z_ const char *_Str);
_Check_return_ _CRTIMP int    __cdecl _atoi_l(_In_z_ const char *_Str, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP long   __cdecl atol(_In_z_ const char *_Str);
_Check_return_ _CRTIMP long   __cdecl _atol_l(_In_z_ const char *_Str, _In_opt_ _locale_t _Locale);
#ifndef _CRT_ALGO_DEFINED
#define _CRT_ALGO_DEFINED
#if __STDC_WANT_SECURE_LIB__
_Check_return_ _CRTIMP void * __cdecl bsearch_s(_In_ const void * _Key, _In_reads_bytes_(_NumOfElements * _SizeOfElements) const void * _Base, 
        _In_ rsize_t _NumOfElements, _In_ rsize_t _SizeOfElements,
        _In_ int (__cdecl * _PtFuncCompare)(void *, const void *, const void *), void * _Context);
#endif
_Check_return_ _CRTIMP void * __cdecl bsearch(_In_ const void * _Key, _In_reads_bytes_(_NumOfElements * _SizeOfElements) const void * _Base, 
        _In_ size_t _NumOfElements, _In_ size_t _SizeOfElements,
        _In_ int (__cdecl * _PtFuncCompare)(const void *, const void *));

#if __STDC_WANT_SECURE_LIB__
_CRTIMP void __cdecl qsort_s(_Inout_updates_bytes_(_NumOfElements* _SizeOfElements) void * _Base, 
        _In_ rsize_t _NumOfElements, _In_ rsize_t _SizeOfElements,
        _In_ int (__cdecl * _PtFuncCompare)(void *, const void *, const void *), void *_Context);
#endif
_CRTIMP void __cdecl qsort(_Inout_updates_bytes_(_NumOfElements * _SizeOfElements) void * _Base, 
	_In_ size_t _NumOfElements, _In_ size_t _SizeOfElements, 
        _In_ int (__cdecl * _PtFuncCompare)(const void *, const void *));
#endif
        _Check_return_ unsigned short __cdecl _byteswap_ushort(_In_ unsigned short _Short);
        _Check_return_ unsigned long  __cdecl _byteswap_ulong (_In_ unsigned long _Long);
        _Check_return_ unsigned __int64 __cdecl _byteswap_uint64(_In_ unsigned __int64 _Int64);
_Check_return_ _CRTIMP div_t  __cdecl div(_In_ int _Numerator, _In_ int _Denominator);

#ifdef _CRT_USE_WINAPI_FAMILY_DESKTOP_APP
_Check_return_ _CRTIMP _CRT_INSECURE_DEPRECATE(_dupenv_s) char * __cdecl getenv(_In_z_ const char * _VarName);
#if __STDC_WANT_SECURE_LIB__
_Check_return_opt_ _CRTIMP errno_t __cdecl getenv_s(_Out_ size_t * _ReturnSize, _Out_writes_opt_z_(_DstSize) char * _DstBuf, _In_ rsize_t _DstSize, _In_z_ const char * _VarName);
#endif
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_1_1(errno_t, getenv_s, _Out_ size_t *, _ReturnSize, char, _Dest, _In_z_ const char *, _VarName)
#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#pragma push_macro("_dupenv_s")
#undef _dupenv_s
#endif

_Check_return_opt_ _CRTIMP errno_t __cdecl _dupenv_s(_Outptr_result_buffer_maybenull_(*_PBufferSizeInBytes) _Outptr_result_z_ char **_PBuffer, _Out_opt_ size_t * _PBufferSizeInBytes, _In_z_ const char * _VarName);

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#pragma pop_macro("_dupenv_s")
#endif
#endif /* _CRT_USE_WINAPI_FAMILY_DESKTOP_APP */

_Check_return_opt_ _CRTIMP errno_t __cdecl _itoa_s(_In_ int _Value, _Out_writes_z_(_Size) char * _DstBuf, _In_ size_t _Size, _In_ int _Radix);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_1_1(errno_t, _itoa_s, _In_ int, _Value, char, _Dest, _In_ int, _Radix)
__DEFINE_CPP_OVERLOAD_STANDARD_FUNC_1_1(char *, __RETURN_POLICY_DST, _CRTIMP, _itoa, _In_ int, _Value, _Pre_notnull_ _Post_z_, char, _Dest, _In_ int, _Radix)
_Check_return_opt_ _CRTIMP errno_t __cdecl _i64toa_s(_In_ __int64 _Val, _Out_writes_z_(_Size) char * _DstBuf, _In_ size_t _Size, _In_ int _Radix);
_CRTIMP _CRT_INSECURE_DEPRECATE(_i64toa_s) char * __cdecl _i64toa(_In_ __int64 _Val, _Pre_notnull_ _Post_z_ char * _DstBuf, _In_ int _Radix);
_Check_return_opt_ _CRTIMP errno_t __cdecl _ui64toa_s(_In_ unsigned __int64 _Val, _Out_writes_z_(_Size) char * _DstBuf, _In_ size_t _Size, _In_ int _Radix);
_CRTIMP _CRT_INSECURE_DEPRECATE(_ui64toa_s) char * __cdecl _ui64toa(_In_ unsigned __int64 _Val, _Pre_notnull_ _Post_z_ char * _DstBuf, _In_ int _Radix);
_Check_return_ _CRTIMP __int64 __cdecl _atoi64(_In_z_ const char * _String);
_Check_return_ _CRTIMP __int64 __cdecl _atoi64_l(_In_z_ const char * _String, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP __int64 __cdecl _strtoi64(_In_z_ const char * _String, _Out_opt_ _Deref_post_z_ char ** _EndPtr, _In_ int _Radix);
_Check_return_ _CRTIMP __int64 __cdecl _strtoi64_l(_In_z_ const char * _String, _Out_opt_ _Deref_post_z_ char ** _EndPtr, _In_ int _Radix, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP unsigned __int64 __cdecl _strtoui64(_In_z_ const char * _String, _Out_opt_ _Deref_post_z_ char ** _EndPtr, _In_ int _Radix);
_Check_return_ _CRTIMP unsigned __int64 __cdecl _strtoui64_l(_In_z_ const char * _String, _Out_opt_ _Deref_post_z_ char ** _EndPtr, _In_ int  _Radix, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP ldiv_t __cdecl ldiv(_In_ long _Numerator, _In_ long _Denominator);
_Check_return_ _CRTIMP lldiv_t __cdecl lldiv(_In_ long long _Numerator, _In_ long long _Denominator);
#ifdef __cplusplus
extern "C++"
{
    inline long abs(long _X)
    {
        return labs(_X);
    }
    inline long long abs(long long _X)
    {
        return llabs(_X);
    }
    inline ldiv_t div(long _A1, long _A2)
    {
        return ldiv(_A1, _A2);
    }
    inline lldiv_t div(long long _A1, long long _A2)
    {
        return lldiv(_A1, _A2);
    }
}
#endif
_Check_return_opt_ _CRTIMP errno_t __cdecl _ltoa_s(_In_ long _Val, _Out_writes_z_(_Size) char * _DstBuf, _In_ size_t _Size, _In_ int _Radix);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_1_1(errno_t, _ltoa_s, _In_ long, _Value, char, _Dest, _In_ int, _Radix)
__DEFINE_CPP_OVERLOAD_STANDARD_FUNC_1_1(char *, __RETURN_POLICY_DST, _CRTIMP, _ltoa, _In_ long, _Value, _Pre_notnull_ _Post_z_, char, _Dest, _In_ int, _Radix)
_Check_return_ _CRTIMP int    __cdecl mblen(_In_reads_bytes_opt_(_MaxCount) _Pre_opt_z_ const char * _Ch, _In_ size_t _MaxCount);
_Check_return_ _CRTIMP int    __cdecl _mblen_l(_In_reads_bytes_opt_(_MaxCount) _Pre_opt_z_ const char * _Ch, _In_ size_t _MaxCount, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP size_t __cdecl _mbstrlen(_In_z_ const char * _Str);
_Check_return_ _CRTIMP size_t __cdecl _mbstrlen_l(_In_z_ const char *_Str, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP size_t __cdecl _mbstrnlen(_In_z_ const char *_Str, _In_ size_t _MaxCount);
_Check_return_ _CRTIMP size_t __cdecl _mbstrnlen_l(_In_z_ const char *_Str, _In_ size_t _MaxCount, _In_opt_ _locale_t _Locale);
_CRTIMP int    __cdecl mbtowc(_Pre_notnull_ _Post_z_ wchar_t * _DstCh, _In_reads_bytes_opt_(_SrcSizeInBytes) _Pre_opt_z_ const char * _SrcCh, _In_ size_t _SrcSizeInBytes);
_CRTIMP int    __cdecl _mbtowc_l(_Pre_notnull_ _Post_z_ wchar_t * _DstCh, _In_reads_bytes_opt_(_SrcSizeInBytes) _Pre_opt_z_ const char * _SrcCh, _In_ size_t _SrcSizeInBytes, _In_opt_ _locale_t _Locale);
_Check_return_opt_ _CRTIMP errno_t __cdecl mbstowcs_s(_Out_opt_ size_t * _PtNumOfCharConverted, _Out_writes_to_opt_(_SizeInWords, *_PtNumOfCharConverted) wchar_t * _DstBuf, _In_ size_t _SizeInWords, _In_reads_or_z_(_MaxCount) const char * _SrcBuf, _In_ size_t _MaxCount );
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_1_2(errno_t, mbstowcs_s, _Out_opt_ size_t *, _PtNumOfCharConverted, _Post_z_ wchar_t, _Dest, _In_z_ const char *, _Source, _In_ size_t, _MaxCount)
__DEFINE_CPP_OVERLOAD_STANDARD_NFUNC_0_2_SIZE(_CRTIMP, mbstowcs, _Out_writes_opt_z_(_MaxCount), wchar_t, _Dest, _In_z_ const char *, _Source, _In_ size_t, _MaxCount)

_Check_return_opt_ _CRTIMP errno_t __cdecl _mbstowcs_s_l(_Out_opt_ size_t * _PtNumOfCharConverted, _Out_writes_to_opt_(_SizeInWords, *_PtNumOfCharConverted) wchar_t * _DstBuf, _In_ size_t _SizeInWords, _In_reads_or_z_(_MaxCount) const char * _SrcBuf, _In_ size_t _MaxCount, _In_opt_ _locale_t _Locale);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_1_3(errno_t, _mbstowcs_s_l, _Out_opt_ size_t *, _PtNumOfCharConverted, wchar_t, _Dest, _In_z_ const char *, _Source, _In_ size_t, _MaxCount, _In_opt_ _locale_t, _Locale)
__DEFINE_CPP_OVERLOAD_STANDARD_NFUNC_0_3_SIZE_EX(_CRTIMP, _mbstowcs_l, _mbstowcs_s_l, _Out_writes_opt_z_(_Size) wchar_t, _Out_writes_z_(_MaxCount), wchar_t, _Dest, _In_z_ const char *, _Source, _In_ size_t, _MaxCount, _In_opt_ _locale_t, _Locale)

_Check_return_ _CRTIMP int    __cdecl rand(void);
#if defined(_CRT_RAND_S)
_CRTIMP errno_t __cdecl rand_s ( _Out_ unsigned int *_RandomValue);
#endif

_Check_return_opt_ _CRTIMP int    __cdecl _set_error_mode(_In_ int _Mode);

_CRTIMP void   __cdecl srand(_In_ unsigned int _Seed);
_Check_return_ _CRTIMP double __cdecl strtod(_In_z_ const char * _Str, _Out_opt_ _Deref_post_z_ char ** _EndPtr);
_Check_return_ _CRTIMP double __cdecl _strtod_l(_In_z_ const char * _Str, _Out_opt_ _Deref_post_z_ char ** _EndPtr, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP long   __cdecl strtol(_In_z_ const char * _Str, _Out_opt_ _Deref_post_z_ char ** _EndPtr, _In_ int _Radix );
_Check_return_ _CRTIMP long   __cdecl _strtol_l(_In_z_ const char *_Str, _Out_opt_ _Deref_post_z_ char **_EndPtr, _In_ int _Radix, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP unsigned long __cdecl strtoul(_In_z_ const char * _Str, _Out_opt_ _Deref_post_z_ char ** _EndPtr, _In_ int _Radix);
_Check_return_ _CRTIMP unsigned long __cdecl _strtoul_l(const char * _Str, _Out_opt_ _Deref_post_z_ char **_EndPtr, _In_ int _Radix, _In_opt_ _locale_t _Locale);

#ifdef _CRT_USE_WINAPI_FAMILY_DESKTOP_APP
#ifndef _CRT_SYSTEM_DEFINED
#define _CRT_SYSTEM_DEFINED
_CRTIMP int __cdecl system(_In_opt_z_ const char * _Command);
#endif
#endif /* _CRT_USE_WINAPI_FAMILY_DESKTOP_APP */

_Check_return_opt_ _CRTIMP errno_t __cdecl _ultoa_s(_In_ unsigned long _Val, _Out_writes_z_(_Size) char * _DstBuf, _In_ size_t _Size, _In_ int _Radix);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_1_1(errno_t, _ultoa_s, _In_ unsigned long, _Value, char, _Dest, _In_ int, _Radix)
__DEFINE_CPP_OVERLOAD_STANDARD_FUNC_1_1(char *, __RETURN_POLICY_DST, _CRTIMP, _ultoa, _In_ unsigned long, _Value, _Pre_notnull_ _Post_z_, char, _Dest, _In_ int, _Radix)
_CRTIMP _CRT_INSECURE_DEPRECATE(wctomb_s) int    __cdecl wctomb(_Out_writes_opt_z_(MB_LEN_MAX) char * _MbCh, _In_ wchar_t _WCh);
_CRTIMP _CRT_INSECURE_DEPRECATE(_wctomb_s_l) int    __cdecl _wctomb_l(_Pre_maybenull_ _Post_z_ char * _MbCh, _In_ wchar_t _WCh, _In_opt_ _locale_t _Locale);
#if __STDC_WANT_SECURE_LIB__
_Check_return_wat_ _CRTIMP errno_t __cdecl wctomb_s(_Out_opt_ int * _SizeConverted, _Out_writes_bytes_to_opt_(_SizeInBytes, *_SizeConverted) char * _MbCh, _In_ rsize_t _SizeInBytes, _In_ wchar_t _WCh);
#endif
_Check_return_wat_ _CRTIMP errno_t __cdecl _wctomb_s_l(_Out_opt_ int * _SizeConverted, _Out_writes_opt_z_(_SizeInBytes) char * _MbCh, _In_ size_t _SizeInBytes, _In_ wchar_t _WCh, _In_opt_ _locale_t _Locale);
_Check_return_wat_ _CRTIMP errno_t __cdecl wcstombs_s(_Out_opt_ size_t * _PtNumOfCharConverted, _Out_writes_bytes_to_opt_(_DstSizeInBytes, *_PtNumOfCharConverted) char * _Dst, _In_ size_t _DstSizeInBytes, _In_z_ const wchar_t * _Src, _In_ size_t _MaxCountInBytes);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_1_2(errno_t, wcstombs_s, _Out_opt_ size_t *, _PtNumOfCharConverted, _Out_writes_bytes_opt_(_Size) char, _Dest, _In_z_ const wchar_t *, _Source, _In_ size_t, _MaxCount)
__DEFINE_CPP_OVERLOAD_STANDARD_NFUNC_0_2_SIZE(_CRTIMP, wcstombs, _Out_writes_opt_z_(_MaxCount), char, _Dest, _In_z_ const wchar_t *, _Source, _In_ size_t, _MaxCount)
_Check_return_wat_ _CRTIMP errno_t __cdecl _wcstombs_s_l(_Out_opt_ size_t * _PtNumOfCharConverted, _Out_writes_bytes_to_opt_(_DstSizeInBytes, *_PtNumOfCharConverted) char * _Dst, _In_ size_t _DstSizeInBytes, _In_z_ const wchar_t * _Src, _In_ size_t _MaxCountInBytes, _In_opt_ _locale_t _Locale);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_1_3(errno_t, _wcstombs_s_l, _Out_opt_ size_t *,_PtNumOfCharConverted, _Out_writes_opt_(_Size) char, _Dest, _In_z_ const wchar_t *, _Source, _In_ size_t, _MaxCount, _In_opt_ _locale_t, _Locale)
__DEFINE_CPP_OVERLOAD_STANDARD_NFUNC_0_3_SIZE_EX(_CRTIMP, _wcstombs_l, _wcstombs_s_l, _Out_writes_opt_z_(_Size) char, _Out_writes_z_(_MaxCount), char, _Dest, _In_z_ const wchar_t *, _Source, _In_ size_t, _MaxCount, _In_opt_ _locale_t, _Locale)

#if defined(__cplusplus) && defined(_M_CEE) /*IFSTRIP=IGN*/
/*
 * Managed search routines. Note __cplusplus, this is because we only support
 * managed C++.
 */
extern "C++"
{
#if __STDC_WANT_SECURE_LIB__
_Check_return_ void * __clrcall bsearch_s(_In_ const void * _Key, _In_reads_bytes_(_NumOfElements*_SizeOfElements) const void * _Base, _In_ rsize_t _NumOfElements, _In_ rsize_t _SizeOfElements, 
        _In_ int (__clrcall * _PtFuncCompare)(void *, const void *, const void *), void * _Context);
#endif
_Check_return_ void * __clrcall bsearch  (_In_ const void * _Key, _In_reads_bytes_(_NumOfElements*_SizeOfElements) const void * _Base, _In_ size_t _NumOfElements, _In_ size_t _SizeOfElements,
        _In_ int (__clrcall * _PtFuncCompare)(const void *, const void *));

#if __STDC_WANT_SECURE_LIB__
void __clrcall qsort_s(_Inout_updates_bytes_(_NumOfElements*_SizeOfElements) void * _Base, 
        _In_ rsize_t _NumOfElements, _In_ rsize_t _SizeOfElements,
        _In_ int (__clrcall * _PtFuncCompare)(void *, const void *, const void *), void * _Context);
#endif
void __clrcall qsort(_Inout_updates_bytes_(_NumOfElements*_SizeOfElements) void * _Base, 
        _In_ size_t _NumOfElements, _In_ size_t _SizeOfElements, 
        _In_ int (__clrcall * _PtFuncCompare)(const void *, const void *));

}
#endif

#ifndef _CRT_ALLOCATION_DEFINED
#define _CRT_ALLOCATION_DEFINED

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)

#pragma push_macro("calloc")
#pragma push_macro("free")
#pragma push_macro("malloc")
#pragma push_macro("realloc")
#pragma push_macro("_recalloc")
#pragma push_macro("_aligned_free")
#pragma push_macro("_aligned_malloc")
#pragma push_macro("_aligned_offset_malloc")
#pragma push_macro("_aligned_realloc")
#pragma push_macro("_aligned_recalloc")
#pragma push_macro("_aligned_offset_realloc")
#pragma push_macro("_aligned_offset_recalloc")
#pragma push_macro("_aligned_msize")

#undef calloc
#undef free
#undef malloc
#undef realloc
#undef _recalloc
#undef _aligned_free
#undef _aligned_malloc
#undef _aligned_offset_malloc
#undef _aligned_realloc
#undef _aligned_recalloc
#undef _aligned_offset_realloc
#undef _aligned_offset_recalloc
#undef _aligned_msize

#endif
_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(_Count*_Size) _CRTIMP _CRT_JIT_INTRINSIC _CRTNOALIAS _CRTRESTRICT    void * __cdecl calloc(_In_ size_t _Count, _In_ size_t _Size);
_CRTIMP                     _CRTNOALIAS                                                                             void   __cdecl free(_Pre_maybenull_ _Post_invalid_ void * _Memory);
_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(_Size) _CRTIMP _CRT_JIT_INTRINSIC _CRTNOALIAS _CRTRESTRICT                              void * __cdecl malloc(_In_ size_t _Size);
_Success_(return!=0)
_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(_NewSize) _CRTIMP _CRTNOALIAS _CRTRESTRICT                           void * __cdecl realloc(_Pre_maybenull_ _Post_invalid_ void * _Memory, _In_ size_t _NewSize);
_Success_(return!=0)
_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(_Count*_Size) _CRTIMP _CRTNOALIAS _CRTRESTRICT                       void * __cdecl _recalloc(_Pre_maybenull_ _Post_invalid_ void * _Memory, _In_ size_t _Count, _In_ size_t _Size);
_CRTIMP                     _CRTNOALIAS                                                                             void   __cdecl _aligned_free(_Pre_maybenull_ _Post_invalid_ void * _Memory);
_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(_Size) _CRTIMP _CRTNOALIAS _CRTRESTRICT                              void * __cdecl _aligned_malloc(_In_ size_t _Size, _In_ size_t _Alignment);
_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(_Size) _CRTIMP _CRTNOALIAS _CRTRESTRICT                              void * __cdecl _aligned_offset_malloc(_In_ size_t _Size, _In_ size_t _Alignment, _In_ size_t _Offset);
_Success_(return!=0)
_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(_NewSize) _CRTIMP _CRTNOALIAS _CRTRESTRICT                              void * __cdecl _aligned_realloc(_Pre_maybenull_ _Post_invalid_ void * _Memory, _In_ size_t _NewSize, _In_ size_t _Alignment);
_Success_(return!=0)
_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(_Count*_Size) _CRTIMP _CRTNOALIAS _CRTRESTRICT                       void * __cdecl _aligned_recalloc(_Pre_maybenull_ _Post_invalid_ void * _Memory, _In_ size_t _Count, _In_ size_t _Size, _In_ size_t _Alignment);
_Success_(return!=0)
_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(_NewSize) _CRTIMP _CRTNOALIAS _CRTRESTRICT                              void * __cdecl _aligned_offset_realloc(_Pre_maybenull_ _Post_invalid_ void * _Memory, _In_ size_t _NewSize, _In_ size_t _Alignment, _In_ size_t _Offset);
_Success_(return!=0)
_Check_return_ _Ret_maybenull_ _Post_writable_byte_size_(_Count*_Size) _CRTIMP _CRTNOALIAS _CRTRESTRICT                       void * __cdecl _aligned_offset_recalloc(_Pre_maybenull_ _Post_invalid_ void * _Memory, _In_ size_t _Count, _In_ size_t _Size, _In_ size_t _Alignment, _In_ size_t _Offset);
_Check_return_ _CRTIMP                                                  size_t __cdecl _aligned_msize(_Pre_notnull_ void * _Memory, _In_ size_t _Alignment, _In_ size_t _Offset);


#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)

#pragma pop_macro("_aligned_msize")
#pragma pop_macro("_aligned_offset_recalloc")
#pragma pop_macro("_aligned_offset_realloc")
#pragma pop_macro("_aligned_recalloc")
#pragma pop_macro("_aligned_realloc")
#pragma pop_macro("_aligned_offset_malloc")
#pragma pop_macro("_aligned_malloc")
#pragma pop_macro("_aligned_free")
#pragma pop_macro("_recalloc")
#pragma pop_macro("realloc")
#pragma pop_macro("malloc")
#pragma pop_macro("free")
#pragma pop_macro("calloc")

#endif

#endif /*_CRT_ALLOCATION_DEFINED */

#ifndef _WSTDLIB_DEFINED

/* wide function prototypes, also declared in wchar.h  */

_Check_return_wat_ _CRTIMP errno_t __cdecl _itow_s (_In_ int _Val, _Out_writes_z_(_SizeInWords) wchar_t * _DstBuf, _In_ size_t _SizeInWords, _In_ int _Radix);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_1_1(errno_t, _itow_s, _In_ int, _Value, wchar_t, _Dest, _In_ int, _Radix)
__DEFINE_CPP_OVERLOAD_STANDARD_FUNC_1_1(wchar_t *, __RETURN_POLICY_DST, _CRTIMP, _itow, _In_ int, _Value, _Pre_notnull_ _Post_z_, wchar_t, _Dest, _In_ int, _Radix)
_Check_return_wat_ _CRTIMP errno_t __cdecl _ltow_s (_In_ long _Val, _Out_writes_z_(_SizeInWords) wchar_t * _DstBuf, _In_ size_t _SizeInWords, _In_ int _Radix);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_1_1(errno_t, _ltow_s, _In_ long, _Value, wchar_t, _Dest, _In_ int, _Radix)
__DEFINE_CPP_OVERLOAD_STANDARD_FUNC_1_1(wchar_t *, __RETURN_POLICY_DST, _CRTIMP, _ltow, _In_ long, _Value, _Pre_notnull_ _Post_z_, wchar_t, _Dest, _In_ int, _Radix)
_Check_return_wat_ _CRTIMP errno_t __cdecl _ultow_s (_In_ unsigned long _Val, _Out_writes_z_(_SizeInWords) wchar_t * _DstBuf, _In_ size_t _SizeInWords, _In_ int _Radix);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_1_1(errno_t, _ultow_s, _In_ unsigned long, _Value, wchar_t, _Dest, _In_ int, _Radix)
__DEFINE_CPP_OVERLOAD_STANDARD_FUNC_1_1(wchar_t *, __RETURN_POLICY_DST, _CRTIMP, _ultow, _In_ unsigned long, _Value, _Pre_notnull_ _Post_z_, wchar_t, _Dest, _In_ int, _Radix)
_Check_return_ _CRTIMP double __cdecl wcstod(_In_z_ const wchar_t * _Str, _Out_opt_ _Deref_post_z_ wchar_t ** _EndPtr);
_Check_return_ _CRTIMP double __cdecl _wcstod_l(_In_z_ const wchar_t *_Str, _Out_opt_ _Deref_post_z_ wchar_t ** _EndPtr, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP long   __cdecl wcstol(_In_z_ const wchar_t *_Str, _Out_opt_ _Deref_post_z_ wchar_t ** _EndPtr, int _Radix);
_Check_return_ _CRTIMP long   __cdecl _wcstol_l(_In_z_ const wchar_t *_Str, _Out_opt_ _Deref_post_z_ wchar_t **_EndPtr, int _Radix, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP unsigned long __cdecl wcstoul(_In_z_ const wchar_t *_Str, _Out_opt_ _Deref_post_z_ wchar_t ** _EndPtr, int _Radix);
_Check_return_ _CRTIMP unsigned long __cdecl _wcstoul_l(_In_z_ const wchar_t *_Str, _Out_opt_ _Deref_post_z_ wchar_t **_EndPtr, int _Radix, _In_opt_ _locale_t _Locale);

#ifdef _CRT_USE_WINAPI_FAMILY_DESKTOP_APP

_Check_return_ _CRTIMP _CRT_INSECURE_DEPRECATE(_wdupenv_s) wchar_t * __cdecl _wgetenv(_In_z_ const wchar_t * _VarName);
_Check_return_wat_ _CRTIMP errno_t __cdecl _wgetenv_s(_Out_ size_t * _ReturnSize, _Out_writes_opt_z_(_DstSizeInWords) wchar_t * _DstBuf, _In_ size_t _DstSizeInWords, _In_z_ const wchar_t * _VarName);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_1_1(errno_t, _wgetenv_s, _Out_ size_t *, _ReturnSize, wchar_t, _Dest, _In_z_ const wchar_t *, _VarName)

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#pragma push_macro("_wdupenv_s")
#undef _wdupenv_s
#endif

_Check_return_wat_ _CRTIMP errno_t __cdecl _wdupenv_s(_Outptr_result_buffer_maybenull_(*_BufferSizeInWords) _Outptr_result_z_ wchar_t **_Buffer, _Out_opt_ size_t *_BufferSizeInWords, _In_z_ const wchar_t *_VarName);

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#pragma pop_macro("_wdupenv_s")
#endif

#ifndef _CRT_WSYSTEM_DEFINED
#define _CRT_WSYSTEM_DEFINED
_CRTIMP int __cdecl _wsystem(_In_opt_z_ const wchar_t * _Command);
#endif /* _CRT_WSYSTEM_DEFINED */

#endif /* _CRT_USE_WINAPI_FAMILY_DESKTOP_APP */

_Check_return_ _CRTIMP double __cdecl _wtof(_In_z_ const wchar_t *_Str);
_Check_return_ _CRTIMP double __cdecl _wtof_l(_In_z_ const wchar_t *_Str, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl _wtoi(_In_z_ const wchar_t *_Str);
_Check_return_ _CRTIMP int __cdecl _wtoi_l(_In_z_ const wchar_t *_Str, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP long __cdecl _wtol(_In_z_ const wchar_t *_Str);
_Check_return_ _CRTIMP long __cdecl _wtol_l(_In_z_ const wchar_t *_Str, _In_opt_ _locale_t _Locale);

_Check_return_wat_ _CRTIMP errno_t __cdecl _i64tow_s(_In_ __int64 _Val, _Out_writes_z_(_SizeInWords) wchar_t * _DstBuf, _In_ size_t _SizeInWords, _In_ int _Radix);
_CRTIMP _CRT_INSECURE_DEPRECATE(_i64tow_s) wchar_t * __cdecl _i64tow(_In_ __int64 _Val, _Pre_notnull_ _Post_z_ wchar_t * _DstBuf, _In_ int _Radix);
_Check_return_wat_ _CRTIMP errno_t __cdecl _ui64tow_s(_In_ unsigned __int64 _Val, _Out_writes_z_(_SizeInWords) wchar_t * _DstBuf, _In_ size_t _SizeInWords, _In_ int _Radix);
_CRTIMP _CRT_INSECURE_DEPRECATE(_ui64tow_s) wchar_t * __cdecl _ui64tow(_In_ unsigned __int64 _Val, _Pre_notnull_ _Post_z_ wchar_t * _DstBuf, _In_ int _Radix);
_Check_return_ _CRTIMP __int64   __cdecl _wtoi64(_In_z_ const wchar_t *_Str);
_Check_return_ _CRTIMP __int64   __cdecl _wtoi64_l(_In_z_ const wchar_t *_Str, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP __int64   __cdecl _wcstoi64(_In_z_ const wchar_t * _Str, _Out_opt_ _Deref_post_z_ wchar_t ** _EndPtr, _In_ int _Radix);
_Check_return_ _CRTIMP __int64   __cdecl _wcstoi64_l(_In_z_ const wchar_t * _Str, _Out_opt_ _Deref_post_z_ wchar_t ** _EndPtr, _In_ int _Radix, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP unsigned __int64  __cdecl _wcstoui64(_In_z_ const wchar_t * _Str, _Out_opt_ _Deref_post_z_ wchar_t ** _EndPtr, _In_ int _Radix);
_Check_return_ _CRTIMP unsigned __int64  __cdecl _wcstoui64_l(_In_z_ const wchar_t *_Str , _Out_opt_ _Deref_post_z_ wchar_t ** _EndPtr, _In_ int _Radix, _In_opt_ _locale_t _Locale);

#define _WSTDLIB_DEFINED
#endif


#ifndef _POSIX_

/* 
Buffer size required to be passed to _gcvt, fcvt and other fp conversion routines
*/
#define _CVTBUFSIZE (309+40) /* # of digits in max. dp value + slop */

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)

#pragma push_macro("_fullpath")
#undef _fullpath

#endif

_Check_return_ _CRTIMP char * __cdecl _fullpath(_Out_writes_opt_z_(_SizeInBytes) char * _FullPath, _In_z_ const char * _Path, _In_ size_t _SizeInBytes);

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)

#pragma pop_macro("_fullpath")

#endif

_Check_return_wat_ _CRTIMP errno_t __cdecl _ecvt_s(_Out_writes_z_(_Size) char * _DstBuf, _In_ size_t _Size, _In_ double _Val, _In_ int _NumOfDights, _Out_ int * _PtDec, _Out_ int * _PtSign);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_4(errno_t, _ecvt_s, char, _Dest, _In_ double, _Value, _In_ int, _NumOfDigits, _Out_ int *, _PtDec, _Out_ int *, _PtSign)
_Check_return_ _CRTIMP _CRT_INSECURE_DEPRECATE(_ecvt_s) char * __cdecl _ecvt(_In_ double _Val, _In_ int _NumOfDigits, _Out_ int * _PtDec, _Out_ int * _PtSign);
_Check_return_wat_ _CRTIMP errno_t __cdecl _fcvt_s(_Out_writes_z_(_Size) char * _DstBuf, _In_ size_t _Size, _In_ double _Val, _In_ int _NumOfDec, _Out_ int * _PtDec, _Out_ int * _PtSign);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_4(errno_t, _fcvt_s, char, _Dest, _In_ double, _Value, _In_ int, _NumOfDigits, _Out_ int *, _PtDec, _Out_ int *, _PtSign)
_Check_return_ _CRTIMP _CRT_INSECURE_DEPRECATE(_fcvt_s) char * __cdecl _fcvt(_In_ double _Val, _In_ int _NumOfDec, _Out_ int * _PtDec, _Out_ int * _PtSign);
_CRTIMP errno_t __cdecl _gcvt_s(_Out_writes_z_(_Size) char * _DstBuf, _In_ size_t _Size, _In_ double _Val, _In_ int _NumOfDigits);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_2(errno_t, _gcvt_s, char, _Dest, _In_ double, _Value, _In_ int, _NumOfDigits)
_CRTIMP _CRT_INSECURE_DEPRECATE(_gcvt_s) char * __cdecl _gcvt(_In_ double _Val, _In_ int _NumOfDigits, _Pre_notnull_ _Post_z_ char * _DstBuf);

_Check_return_ _CRTIMP int __cdecl _atodbl(_Out_ _CRT_DOUBLE * _Result, _In_z_ char * _Str);
_Check_return_ _CRTIMP int __cdecl _atoldbl(_Out_ _LDOUBLE * _Result, _In_z_ char * _Str);
_Check_return_ _CRTIMP int __cdecl _atoflt(_Out_ _CRT_FLOAT * _Result, _In_z_ char * _Str);
_Check_return_ _CRTIMP int __cdecl _atodbl_l(_Out_ _CRT_DOUBLE * _Result, _In_z_ char * _Str, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl _atoldbl_l(_Out_ _LDOUBLE * _Result, _In_z_ char * _Str, _In_opt_ _locale_t _Locale);
_Check_return_ _CRTIMP int __cdecl _atoflt_l(_Out_ _CRT_FLOAT * _Result, _In_z_ char * _Str, _In_opt_ _locale_t _Locale);
        _Check_return_ unsigned long __cdecl _lrotl(_In_ unsigned long _Val, _In_ int _Shift);
        _Check_return_ unsigned long __cdecl _lrotr(_In_ unsigned long _Val, _In_ int _Shift);
_Check_return_wat_ _CRTIMP_ALTERNATIVE errno_t   __cdecl _makepath_s(_Out_writes_z_(_SizeInWords) char * _PathResult, _In_ size_t _SizeInWords, _In_opt_z_ const char * _Drive, _In_opt_z_ const char * _Dir, _In_opt_z_ const char * _Filename,
        _In_opt_z_ const char * _Ext);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_4(errno_t, _makepath_s, char, _Path, _In_opt_z_ const char *, _Drive, _In_opt_z_ const char *, _Dir, _In_opt_z_ const char *, _Filename, _In_opt_z_ const char *, _Ext)
__DEFINE_CPP_OVERLOAD_STANDARD_FUNC_0_4(void, __RETURN_POLICY_VOID, _CRTIMP, _makepath, _Pre_notnull_ _Post_z_, char, _Path, _In_opt_z_ const char *, _Drive, _In_opt_z_ const char *, _Dir, _In_opt_z_ const char *, _Filename, _In_opt_z_ const char *, _Ext)

#if defined(_M_CEE) /*IFSTRIP=IGN*/
		_onexit_m_t    __clrcall _onexit_m_appdomain(_onexit_m_t _Function);
	#if defined(_M_CEE_MIXED)
		_onexit_m_t    __clrcall _onexit_m(_onexit_m_t _Function);
	#else
		inline _onexit_m_t    __clrcall _onexit_m(_onexit_t _Function)
		{
			return _onexit_m_appdomain(_Function);
		}
	#endif
        
#endif
#if defined(_M_CEE_PURE)
        /* In pure mode, _onexit is the same as _onexit_m_appdomain */
extern "C++"
{
inline  _onexit_t    __clrcall _onexit
(
    _onexit_t _Function
)
{
    return _onexit_m_appdomain(_Function);
}
}
#else
        _onexit_t __cdecl _onexit(_In_opt_ _onexit_t _Func);
#endif
        
#ifndef _CRT_PERROR_DEFINED
#define _CRT_PERROR_DEFINED
_CRTIMP void __cdecl perror(_In_opt_z_ const char * _ErrMsg);
#endif

#pragma warning (push)
#pragma warning (disable:6540) // the functions below have declspecs in their declarations in the windows headers, causing PREfast to fire 6540 here

#ifdef _CRT_USE_WINAPI_FAMILY_DESKTOP_APP
_Check_return_ _CRTIMP int    __cdecl _putenv(_In_z_ const char * _EnvString);
_Check_return_wat_ _CRTIMP errno_t __cdecl _putenv_s(_In_z_ const char * _Name, _In_z_ const char * _Value);
#endif /* _CRT_USE_WINAPI_FAMILY_DESKTOP_APP */

        unsigned int __cdecl _rotl(_In_ unsigned int _Val, _In_ int _Shift);
        unsigned __int64 __cdecl _rotl64(_In_ unsigned __int64 _Val, _In_ int _Shift);
        unsigned int __cdecl _rotr(_In_ unsigned int _Val, _In_ int _Shift);
        unsigned __int64 __cdecl _rotr64(_In_ unsigned __int64 _Val, _In_ int _Shift);
#pragma warning (pop)

#ifdef _CRT_USE_WINAPI_FAMILY_DESKTOP_APP
_CRTIMP errno_t __cdecl _searchenv_s(_In_z_ const char * _Filename, _In_z_ const char * _EnvVar, _Out_writes_z_(_SizeInBytes) char * _ResultPath, _In_ size_t _SizeInBytes);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_2_0(errno_t, _searchenv_s, _In_z_ const char *, _Filename, _In_z_ const char *, _EnvVar, char, _ResultPath)
__DEFINE_CPP_OVERLOAD_STANDARD_FUNC_2_0(void, __RETURN_POLICY_VOID, _CRTIMP, _searchenv, _In_z_ const char *, _Filename, _In_z_ const char *, _EnvVar, _Pre_notnull_ _Post_z_, char, _ResultPath)
#endif /* _CRT_USE_WINAPI_FAMILY_DESKTOP_APP */

_CRT_INSECURE_DEPRECATE(_splitpath_s) _CRTIMP void   __cdecl _splitpath(_In_z_ const char * _FullPath, _Pre_maybenull_ _Post_z_ char * _Drive, _Pre_maybenull_ _Post_z_ char * _Dir, _Pre_maybenull_ _Post_z_ char * _Filename, _Pre_maybenull_ _Post_z_ char * _Ext);
_Check_return_wat_ _CRTIMP_ALTERNATIVE errno_t  __cdecl _splitpath_s(_In_z_ const char * _FullPath, 
		_Out_writes_opt_z_(_DriveSize) char * _Drive, _In_ size_t _DriveSize, 
		_Out_writes_opt_z_(_DirSize) char * _Dir, _In_ size_t _DirSize, 
		_Out_writes_opt_z_(_FilenameSize) char * _Filename, _In_ size_t _FilenameSize, 
		_Out_writes_opt_z_(_ExtSize) char * _Ext, _In_ size_t _ExtSize);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_SPLITPATH(errno_t, _splitpath_s,  char, _Dest)

_CRTIMP void   __cdecl _swab(_Inout_updates_(_SizeInBytes) _Post_readable_size_(_SizeInBytes) char * _Buf1, _Inout_updates_(_SizeInBytes) _Post_readable_size_(_SizeInBytes) char * _Buf2, int _SizeInBytes);

#ifndef _WSTDLIBP_DEFINED

/* wide function prototypes, also declared in wchar.h  */

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#pragma push_macro("_wfullpath")
#undef _wfullpath
#endif

_Check_return_ _CRTIMP wchar_t * __cdecl _wfullpath(_Out_writes_opt_z_(_SizeInWords) wchar_t * _FullPath, _In_z_ const wchar_t * _Path, _In_ size_t _SizeInWords);

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#pragma pop_macro("_wfullpath")
#endif

_Check_return_wat_ _CRTIMP_ALTERNATIVE errno_t __cdecl _wmakepath_s(_Out_writes_z_(_SIZE) wchar_t * _PathResult, _In_ size_t _SIZE, _In_opt_z_ const wchar_t * _Drive, _In_opt_z_ const wchar_t * _Dir, _In_opt_z_ const wchar_t * _Filename,
        _In_opt_z_ const wchar_t * _Ext);        
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_4(errno_t, _wmakepath_s, wchar_t, _ResultPath, _In_opt_z_ const wchar_t *, _Drive, _In_opt_z_ const wchar_t *, _Dir, _In_opt_z_ const wchar_t *, _Filename, _In_opt_z_ const wchar_t *, _Ext)
__DEFINE_CPP_OVERLOAD_STANDARD_FUNC_0_4(void, __RETURN_POLICY_VOID, _CRTIMP, _wmakepath, _Pre_notnull_ _Post_z_, wchar_t, _ResultPath, _In_opt_z_ const wchar_t *, _Drive, _In_opt_z_ const wchar_t *, _Dir, _In_opt_z_ const wchar_t *, _Filename, _In_opt_z_ const wchar_t *, _Ext)
#ifndef _CRT_WPERROR_DEFINED
#define _CRT_WPERROR_DEFINED
_CRTIMP void __cdecl _wperror(_In_opt_z_ const wchar_t * _ErrMsg);
#endif 

#ifdef _CRT_USE_WINAPI_FAMILY_DESKTOP_APP
_Check_return_ _CRTIMP int    __cdecl _wputenv(_In_z_ const wchar_t * _EnvString);
_Check_return_wat_ _CRTIMP errno_t __cdecl _wputenv_s(_In_z_ const wchar_t * _Name, _In_z_ const wchar_t * _Value);
_CRTIMP errno_t __cdecl _wsearchenv_s(_In_z_ const wchar_t * _Filename, _In_z_ const wchar_t * _EnvVar, _Out_writes_z_(_SizeInWords) wchar_t * _ResultPath, _In_ size_t _SizeInWords);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_2_0(errno_t, _wsearchenv_s, _In_z_ const wchar_t *, _Filename, _In_z_ const wchar_t *, _EnvVar, wchar_t, _ResultPath)
__DEFINE_CPP_OVERLOAD_STANDARD_FUNC_2_0(void, __RETURN_POLICY_VOID, _CRTIMP, _wsearchenv, _In_z_ const wchar_t *, _Filename, _In_z_ const wchar_t *, _EnvVar, _Pre_notnull_ _Post_z_, wchar_t, _ResultPath)
#endif /* _CRT_USE_WINAPI_FAMILY_DESKTOP_APP */

_CRT_INSECURE_DEPRECATE(_wsplitpath_s) _CRTIMP void   __cdecl _wsplitpath(_In_z_ const wchar_t * _FullPath, _Pre_maybenull_ _Post_z_ wchar_t * _Drive, _Pre_maybenull_ _Post_z_ wchar_t * _Dir, _Pre_maybenull_ _Post_z_ wchar_t * _Filename, _Pre_maybenull_ _Post_z_ wchar_t * _Ext);
_CRTIMP_ALTERNATIVE errno_t __cdecl _wsplitpath_s(_In_z_ const wchar_t * _FullPath, 
		_Out_writes_opt_z_(_DriveSize) wchar_t * _Drive, _In_ size_t _DriveSize, 
		_Out_writes_opt_z_(_DirSize) wchar_t * _Dir, _In_ size_t _DirSize, 
		_Out_writes_opt_z_(_FilenameSize) wchar_t * _Filename, _In_ size_t _FilenameSize, 
		_Out_writes_opt_z_(_ExtSize) wchar_t * _Ext, _In_ size_t _ExtSize);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_SPLITPATH(errno_t, _wsplitpath_s, wchar_t, _Path)

#define _WSTDLIBP_DEFINED
#endif

#ifdef _CRT_USE_WINAPI_FAMILY_DESKTOP_APP
/* The Win32 API SetErrorMode, Beep and Sleep should be used instead. */
_CRT_OBSOLETE(SetErrorMode) _CRTIMP void __cdecl _seterrormode(_In_ int _Mode);
_CRT_OBSOLETE(Beep) _CRTIMP void __cdecl _beep(_In_ unsigned _Frequency, _In_ unsigned _Duration);
_CRT_OBSOLETE(Sleep) _CRTIMP void __cdecl _sleep(_In_ unsigned long _Duration);
#endif /* _CRT_USE_WINAPI_FAMILY_DESKTOP_APP */

#endif  /* _POSIX_ */

#if     !__STDC__

#ifndef _POSIX_

/* Non-ANSI names for compatibility */

#ifndef __cplusplus
#define max(a,b)    (((a) > (b)) ? (a) : (b))
#define min(a,b)    (((a) < (b)) ? (a) : (b))
#endif

#define sys_errlist _sys_errlist
#define sys_nerr    _sys_nerr

#ifdef _CRT_USE_WINAPI_FAMILY_DESKTOP_APP
#define environ     _environ
#endif /* _CRT_USE_WINAPI_FAMILY_DESKTOP_APP */

#pragma warning(push)
#pragma warning(disable: 4141) /* Using deprecated twice */ 
_Check_return_ _CRT_NONSTDC_DEPRECATE(_ecvt) _CRT_INSECURE_DEPRECATE(_ecvt_s) _CRTIMP char * __cdecl ecvt(_In_ double _Val, _In_ int _NumOfDigits, _Out_ int * _PtDec, _Out_ int * _PtSign);
_Check_return_ _CRT_NONSTDC_DEPRECATE(_fcvt) _CRT_INSECURE_DEPRECATE(_fcvt_s) _CRTIMP char * __cdecl fcvt(_In_ double _Val, _In_ int _NumOfDec, _Out_ int * _PtDec, _Out_ int * _PtSign);
_CRT_NONSTDC_DEPRECATE(_gcvt) _CRT_INSECURE_DEPRECATE(_fcvt_s)		_CRTIMP char * __cdecl gcvt(_In_ double _Val, _In_ int _NumOfDigits, _Pre_notnull_ _Post_z_ char * _DstBuf);
_CRT_NONSTDC_DEPRECATE(_itoa) _CRT_INSECURE_DEPRECATE(_itoa_s)		_CRTIMP char * __cdecl itoa(_In_ int _Val, _Pre_notnull_ _Post_z_ char * _DstBuf, _In_ int _Radix);
_CRT_NONSTDC_DEPRECATE(_ltoa) _CRT_INSECURE_DEPRECATE(_ltoa_s)		_CRTIMP char * __cdecl ltoa(_In_ long _Val, _Pre_notnull_ _Post_z_ char * _DstBuf, _In_ int _Radix);

#ifdef _CRT_USE_WINAPI_FAMILY_DESKTOP_APP
_Check_return_ _CRT_NONSTDC_DEPRECATE(_putenv) _CRTIMP int    __cdecl putenv(_In_z_ const char * _EnvString);
#endif /* _CRT_USE_WINAPI_FAMILY_DESKTOP_APP */

_CRT_NONSTDC_DEPRECATE(_swab)										_CRTIMP void   __cdecl swab(_Inout_updates_z_(_SizeInBytes) char * _Buf1,_Inout_updates_z_(_SizeInBytes) char * _Buf2, _In_ int _SizeInBytes);
_CRT_NONSTDC_DEPRECATE(_ultoa) _CRT_INSECURE_DEPRECATE(_ultoa_s)	_CRTIMP char * __cdecl ultoa(_In_ unsigned long _Val, _Pre_notnull_ _Post_z_ char * _Dstbuf, _In_ int _Radix);
#pragma warning(pop)
onexit_t __cdecl onexit(_In_opt_ onexit_t _Func);

#endif  /* _POSIX_ */

#endif  /* __STDC__ */

#ifdef  __cplusplus
}

#endif

#pragma pack(pop)

#endif  /* _INC_STDLIB */
