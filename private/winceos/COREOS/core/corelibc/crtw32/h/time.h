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
*time.h - definitions/declarations for time routines
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       This file has declarations of time routines and defines
*       the structure returned by the localtime and gmtime routines and
*       used by asctime.
*       [ANSI/System V]
*
*       [Public]
*
*Revision History:
*       07-27-87  SKS   Added _strdate(), _strtime()
*       10-20-87  JCR   Removed "MSC40_ONLY" entries
*       12-11-87  JCR   Added "_loadds" functionality
*       12-18-87  JCR   Added _FAR_ to declarations
*       01-16-88  JCR   Added function versions of daylight/timezone/tzset
*       01-20-88  SKS   Change _timezone(n) to _timezone(), _daylight()
*       02-10-88  JCR   Cleaned up white space
*       12-07-88  JCR   DLL timezone/daylight/tzname now directly refers to data
*       03-14-89  JCR   Added strftime() prototype and size_t definition
*       05-03-89  JCR   Added _INTERNAL_IFSTRIP for relinc usage
*       08-15-89  GJF   Cleanup, now specific to OS/2 2.0 (i.e., 386 flat model)
*       10-30-89  GJF   Fixed copyright, removed dummy args from prototypes
*       11-02-89  JCR   Changed "DLL" to "_DLL"
*       11-20-89  JCR   difftime() always _cdecl (not pascal even under mthread)
*       03-02-90  GJF   Added #ifndef _INC_TIME and #include <cruntime.h>
*                       stuff. Also, removed some (now) useless preprocessor
*                       directives.
*       03-29-90  GJF   Replaced _cdecl with _CALLTYPE1 in prototypes and with
*                       _VARTYPE1 in variable declarations.
*       08-16-90  SBM   Added NULL definition for ANSI compliance
*       11-12-90  GJF   Changed NULL to (void *)0.
*       01-21-91  GJF   ANSI naming.
*       02-12-91  GJF   Only #define NULL if it isn't #define-d.
*       08-20-91  JCR   C++ and ANSI naming
*       08-26-91  BWM   Added prototypes for _getsystime and _setsystem.
*       09-28-91  JCR   ANSI names: DOSX32=prototypes, WIN32=#defines for now
*       01-22-92  GJF   Fixed up definitions of global variables for build of,
*                       and users of, crtdll.dll.
*       03-25-92  DJM   POSIX support.
*       08-05-92  GJF   Function calling type and variable type macros.
*       08-24-92  PBS   Support for Posix TZ variable.
*       01-21-93  GJF   Removed support for C6-386's _cdecl.
*       03-10-93  MJB   Fixes for Posix TZ stuff.
*       03-20-93  SKS   Remove obsolete functions _getsystime/_setsystime
*       04-06-93  SKS   Replace _CRTAPI1/2 with __cdecl, _CRTVAR1 with nothing
*       04-07-93  SKS   Add _CRTIMP keyword for CRT DLL model
*                       Use link-time aliases for old names, not #define's
*                       Remove POSIX #ifdef's
*       05-05-93  CFW   Add wcsftime proto.
*       06-08-93  SKS   Cannot #define the old name "timezone" to "_timezone"
*                       because of conflict conflict with the timezone field
*                       in struct timeb in <sys/timeb.h>.
*       09-13-93  GJF   Merged NT SDK and Cuda versions.
*       11-15-93  GJF   Enclosed _getlocaltime, _setlocaltime prototypes with
*                       a warning noting they are obsolete.
*       12-07-93  CFW   Add wide char version protos.
*       12-07-93  CFW   Move wide defs outside __STDC__ check.
*       04-13-94  GJF   Made _daylight, _timezone and _tzname into deferences
*                       of function return values (for compatibility with the
*                       Win32s version of msvcrt*.dll). Also, added
*                       conditional include for win32s.h.
*       05-04-94  GJF   Made definitions of _daylight, _timezone and _tzname
*                       for _DLL conditional on _M_IX86 also.
*       11-03-94  GJF   Ensure 8 byte alignment.
*       12-16-94  CFW   Wcsftime format must be wchar_t!
*       02-11-95  CFW   Add _CRTBLD to avoid users getting wrong headers.
*       02-14-95  CFW   Clean up Mac merge.
*       06-21-95  CFW   Oldnames daylight, timezone, and tzname for Win32s.
*       06-23-95  CFW   Remove timezone oldname support for Win32 DLL
*                       conflicts with timeb.h.
*       08-30-95  GJF   Added _dstbias.
*       12-14-95  JWM   Add "#pragma once".
*       01-22-97  GJF   Cleaned out obsolete support for Win32s, _NTSDK and
*                       _CRTAPI*.
*       08-13-97  GJF   Strip __p_* prototypes from release version.
*       09-30-97  JWM   Restored not-so-obsolete _CRTAPI1 support.
*       02-06-98  GJF   Changes for Win64: made time_t __int64.
*       05-04-98  GJF   Added __time64_t support.
*       01-06-99  GJF   Changes for 64-bit size_t.
*       05-13-99  PML   Remove _CRTAPI1
*       05-17-99  PML   Remove all Macintosh support.
*       10-06-99  PML   Add _W64 modifier to types which are 32 bits in Win32,
*                       64 bits in Win64.
*       11-12-99  PML   Wrap __time64_t in its own #ifndef.
*       10-09-02  GB    Exporting mkgmtime.. functions
*       12-18-02  EAN   changed time_t to use a 64-bit int if available.
*                       VSWhidbey 6851
*       03-06-03  SSM   Secure time functions and deprecate old functions
*                       Secure versions return errno_t.
*                       Secure versions have _s suffix.
*       03-08-04  MSL   Added missing [str|wcs]ftime_l
*                       VSW#253081
*       03-08-04  MSL   Added missing global accessors
*                       VSW#241420
*       04-17-04  AC    Added accessors for pure mode
*                       VSW#216331
*       04-19-04  AC    Changed _get_tzname to copy the string instead of
*                       returning a pointer to the internal buffers. VSW#287265
*       05-05-04  GB    Added error message for the case when using 32 bit
*                       time_t in with win64
*       06-30-04  SJ    Don't need the _USE_32BIT_TIME_T block here because 
*                       time.h #includes crtdefs.h which contains the same.
*       09-24-04  MSL   Conformant comments
*       10-10-04  ESC   Use _CRT_PACKING
*       11-02-04  AC    Add cpp overloads for secure functions.
*                       VSW#313364
*       11-08-04  JL    Added _CRT_NONSTDC_DEPRECATE to deprecate non-ANSI functions
*       11-18-04  ATC   SAL Annotations for CRT Headers
*       01-14-05  AC    Fix SAL annotations (using prefast espx plug-in)
*       01-20-05  AC    Added _CRT_INSECURE_DEPRECATE_GLOBALS
*                       VSW#438137
*       01-21-05  MSL   Add param names to overload macros
*       03-23-05  MSL   _P removal - not needed
*                       New deprecation warning with function name
*                       Packing fix
*       04-14-05  MSL   Intellisense locale name cleanup
*       06-03-05  MSL   Revert 64 bit definition of NULL due to calling-convention fix
*       11-10-06  PMB   Removed most _INTEGRAL_MAX_BITS #ifdefs
*                       DDB#11174
*
****/

#pragma once

#ifndef _INC_TIME
#define _INC_TIME

#include <crtdefs.h>

#if     !defined(_WIN32)
#error ERROR: Only Win32 target supported!
#endif

#ifndef _CRTBLD
/* This version of the header files is NOT for user programs.
 * It is intended for use when building the C runtimes ONLY.
 * The version intended for public use will not have this message.
 */
#error ERROR: Use of C runtime library internal header file.
#endif  /* _CRTBLD */

/*
 * Currently, all MS C compilers for Win32 platforms default to 8 byte
 * alignment.
 */
#pragma pack(push,_CRT_PACKING)

#ifdef  __cplusplus
extern "C" {
#endif

#ifndef _INTERNAL_IFSTRIP_
#include <cruntime.h>
#endif  /* _INTERNAL_IFSTRIP_ */

#if !defined(_W64)
#if !defined(__midl) && (defined(_X86_) || defined(_M_IX86)) /*IFSTRIP=IGN*/
#define _W64 __w64
#else
#define _W64
#endif
#endif

/* Define _CRTIMP */

#ifndef _CRTIMP
#ifdef  CRTDLL
#define _CRTIMP __declspec(dllexport)
#else   /* ndef CRTDLL */
#ifdef  _DLL
#define _CRTIMP __declspec(dllimport)
#else   /* ndef _DLL */
#define _CRTIMP
#endif  /* _DLL */
#endif  /* CRTDLL */
#endif  /* _CRTIMP */

#ifndef _WCHAR_T_DEFINED
typedef unsigned short wchar_t;
#define _WCHAR_T_DEFINED
#endif

#ifndef _TIME32_T_DEFINED
typedef _W64 long __time32_t;   /* 32-bit time value */
#define _TIME32_T_DEFINED
#endif

#ifndef _TIME64_T_DEFINED
typedef __int64 __time64_t;     /* 64-bit time value */
#define _TIME64_T_DEFINED
#endif

#ifndef _TIME_T_DEFINED
#ifdef _USE_32BIT_TIME_T
typedef __time32_t time_t;      /* time value */
#else
typedef __time64_t time_t;      /* time value */
#endif
#define _TIME_T_DEFINED         /* avoid multiple def's of time_t */
#endif

#ifndef _CLOCK_T_DEFINED
typedef long clock_t;
#define _CLOCK_T_DEFINED
#endif

#ifndef _SIZE_T_DEFINED
#ifdef  _WIN64
typedef unsigned __int64    size_t;
#else
typedef _W64 unsigned int   size_t;
#endif
#define _SIZE_T_DEFINED
#endif

/* Define NULL pointer value */
#ifndef NULL
#ifdef __cplusplus
#define NULL    0
#else
#define NULL    ((void *)0)
#endif
#endif


#ifndef _TM_DEFINED
struct tm {
        int tm_sec;     /* seconds after the minute - [0,59] */
        int tm_min;     /* minutes after the hour - [0,59] */
        int tm_hour;    /* hours since midnight - [0,23] */
        int tm_mday;    /* day of the month - [1,31] */
        int tm_mon;     /* months since January - [0,11] */
        int tm_year;    /* years since 1900 */
        int tm_wday;    /* days since Sunday - [0,6] */
        int tm_yday;    /* days since January 1 - [0,365] */
        int tm_isdst;   /* daylight savings time flag */
        };
#define _TM_DEFINED
#endif


/* Clock ticks macro - ANSI version */

#define CLOCKS_PER_SEC  1000


/* Extern declarations for the global variables used by the ctime family of
 * routines.
 */
#ifndef _INTERNAL_IFSTRIP_
#if     defined(_DLL) && defined(_M_IX86)
/* Retained for compatibility with VC++ 5.0 and earlier versions */
_CRTIMP int * __cdecl __p__daylight(void);
_CRTIMP long * __cdecl __p__dstbias(void);
_CRTIMP long * __cdecl __p__timezone(void);
_CRTIMP char ** __cdecl __p__tzname(void);
#endif
#endif  /* _INTERNAL_IFSTRIP_ */

/* non-zero if daylight savings time is used */
_Check_return_ _CRT_INSECURE_DEPRECATE_GLOBALS(_get_daylight) _CRTIMP int* __cdecl __daylight(void);
#define _daylight (*__daylight())

/* offset for Daylight Saving Time */
_Check_return_ _CRT_INSECURE_DEPRECATE_GLOBALS(_get_dstbias) _CRTIMP long* __cdecl __dstbias(void);
#define _dstbias (*__dstbias())

/* difference in seconds between GMT and local time */
_Check_return_ _CRT_INSECURE_DEPRECATE_GLOBALS(_get_timezone) _CRTIMP long* __cdecl __timezone(void);
#define _timezone (*__timezone())

/* standard/daylight savings time zone names */
_Check_return_ _Deref_ret_z_ _CRT_INSECURE_DEPRECATE_GLOBALS(_get_tzname) _CRTIMP char ** __cdecl __tzname(void);
#define _tzname (__tzname())

_CRTIMP errno_t __cdecl _get_daylight(_Out_ int * _Daylight);
_CRTIMP errno_t __cdecl _get_dstbias(_Out_ long * _Daylight_savings_bias);
_CRTIMP errno_t __cdecl _get_timezone(_Out_ long * _Timezone);
_CRTIMP errno_t __cdecl _get_tzname(_Out_ size_t *_ReturnValue, _Out_writes_z_(_SizeInBytes) char *_Buffer, _In_ size_t _SizeInBytes, _In_ int _Index);

#ifndef _INTERNAL_IFSTRIP_

_DEFINE_SET_FUNCTION(_set_daylight, int, _daylight)
_DEFINE_SET_FUNCTION(_set_dstbias, long, _dstbias)
_DEFINE_SET_FUNCTION(_set_timezone, long, _timezone)

#endif /* _INTERNAL_IFSTRIP_ */

/* Function prototypes */
_Check_return_ _CRT_INSECURE_DEPRECATE(asctime_s) _CRTIMP char * __cdecl asctime(_In_ const struct tm * _Tm);
#if __STDC_WANT_SECURE_LIB__
_Check_return_wat_ _CRTIMP errno_t __cdecl asctime_s(_Out_writes_(_SizeInBytes) _Post_readable_size_(26) char *_Buf, _In_ size_t _SizeInBytes, _In_ const struct tm * _Tm);
#endif
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_1(errno_t, asctime_s, _Post_readable_size_(26) char, _Buffer, _In_ const struct tm *, _Time)

_CRT_INSECURE_DEPRECATE(_ctime32_s) _CRTIMP char * __cdecl _ctime32(_In_ const __time32_t * _Time);
_CRTIMP errno_t __cdecl _ctime32_s(_Out_writes_(_SizeInBytes) _Post_readable_size_(26) char *_Buf, _In_ size_t _SizeInBytes, _In_ const __time32_t *_Time);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_1(errno_t, _ctime32_s, _Post_readable_size_(26) char, _Buffer, _In_ const __time32_t *, _Time)

_Check_return_ _CRTIMP clock_t __cdecl clock(void);
_CRTIMP double __cdecl _difftime32(_In_ __time32_t _Time1, _In_ __time32_t _Time2);

_Check_return_ _CRT_INSECURE_DEPRECATE(_gmtime32_s) _CRTIMP struct tm * __cdecl _gmtime32(_In_ const __time32_t * _Time);
_Check_return_wat_ _CRTIMP errno_t __cdecl _gmtime32_s(_In_ struct tm *_Tm, _In_ const __time32_t * _Time);

_CRT_INSECURE_DEPRECATE(_localtime32_s) _CRTIMP struct tm * __cdecl _localtime32(_In_ const __time32_t * _Time);
_CRTIMP errno_t __cdecl _localtime32_s(_Out_ struct tm *_Tm, _In_ const __time32_t * _Time);

_CRTIMP size_t __cdecl strftime(_Out_writes_z_(_SizeInBytes) char * _Buf, _In_ size_t _SizeInBytes, _In_z_ _Printf_format_string_ const char * _Format, _In_ const struct tm * _Tm);
_CRTIMP size_t __cdecl _strftime_l(_Pre_notnull_ _Post_z_ char *_Buf, _In_ size_t _Max_size, _In_z_ _Printf_format_string_ const char * _Format, _In_ const struct tm *_Tm, _In_opt_ _locale_t _Locale);

_Check_return_wat_ _CRTIMP errno_t __cdecl _strdate_s(_Out_writes_(_SizeInBytes) _Post_readable_size_(9) char *_Buf, _In_ size_t _SizeInBytes);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_0(errno_t, _strdate_s, _Post_readable_size_(9) char, _Buffer)
__DEFINE_CPP_OVERLOAD_STANDARD_FUNC_0_0(char *, __RETURN_POLICY_DST, _CRTIMP, _strdate, _Out_writes_z_(9), char, _Buffer)

_Check_return_wat_ _CRTIMP errno_t __cdecl _strtime_s(_Out_writes_(_SizeInBytes) _Post_readable_size_(9) char *_Buf , _In_ size_t _SizeInBytes);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_0(errno_t, _strtime_s, _Post_readable_size_(9) char, _Buffer)
__DEFINE_CPP_OVERLOAD_STANDARD_FUNC_0_0(char *, __RETURN_POLICY_DST, _CRTIMP, _strtime, _Out_writes_z_(9), char, _Buffer)

_CRTIMP __time32_t __cdecl _time32(_Out_opt_ __time32_t * _Time);
_CRTIMP __time32_t __cdecl _mktime32(_Inout_ struct tm * _Tm);
_CRTIMP __time32_t __cdecl _mkgmtime32(_Inout_ struct tm * _Tm);

#ifdef _CRT_USE_WINAPI_FAMILY_DESKTOP_APP

#ifdef  _POSIX_
_CRTIMP void __cdecl tzset(void);
#else
_CRTIMP void __cdecl _tzset(void);
#endif

#endif /* _CRT_USE_WINAPI_FAMILY_DESKTOP_APP */

_Check_return_ _CRTIMP double __cdecl _difftime64(_In_ __time64_t _Time1, _In_ __time64_t _Time2);
_CRT_INSECURE_DEPRECATE(_ctime64_s) _CRTIMP char * __cdecl _ctime64(_In_ const __time64_t * _Time);
_CRTIMP errno_t __cdecl _ctime64_s(_Out_writes_z_(_SizeInBytes) char *_Buf, _In_ size_t _SizeInBytes, _In_ const __time64_t * _Time);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_1(errno_t, _ctime64_s, char, _Buffer, _In_ const __time64_t *, _Time)

_CRT_INSECURE_DEPRECATE(_gmtime64_s) _CRTIMP struct tm * __cdecl _gmtime64(_In_ const __time64_t * _Time);
_CRTIMP errno_t __cdecl _gmtime64_s(_Out_ struct tm *_Tm, _In_ const __time64_t *_Time);

_CRT_INSECURE_DEPRECATE(_localtime64_s) _CRTIMP struct tm * __cdecl _localtime64(_In_ const __time64_t * _Time);
_CRTIMP errno_t __cdecl _localtime64_s(_Out_ struct tm *_Tm, _In_ const __time64_t *_Time);

_CRTIMP __time64_t __cdecl _mktime64(_Inout_ struct tm * _Tm);
_CRTIMP __time64_t __cdecl _mkgmtime64(_Inout_ struct tm * _Tm);
_CRTIMP __time64_t __cdecl _time64(_Out_opt_ __time64_t * _Time);

#ifdef _CRT_USE_WINAPI_FAMILY_DESKTOP_APP
/* The Win32 API GetLocalTime and SetLocalTime should be used instead. */
_CRT_OBSOLETE(GetLocalTime) unsigned __cdecl _getsystime(_Out_ struct tm * _Tm);
_CRT_OBSOLETE(SetLocalTime) unsigned __cdecl _setsystime(_In_ struct tm * _Tm, unsigned _MilliSec);
#endif /* _CRT_USE_WINAPI_FAMILY_DESKTOP_APP */


#ifndef _SIZE_T_DEFINED
typedef unsigned int size_t;
#define _SIZE_T_DEFINED
#endif

#ifndef _WTIME_DEFINED

/* wide function prototypes, also declared in wchar.h */
 
_CRT_INSECURE_DEPRECATE(_wasctime_s) _CRTIMP wchar_t * __cdecl _wasctime(_In_ const struct tm * _Tm);
_CRTIMP errno_t __cdecl _wasctime_s(_Out_writes_(_SizeInWords) _Post_readable_size_(26) wchar_t *_Buf, _In_ size_t _SizeInWords, _In_ const struct tm * _Tm);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_1(errno_t, _wasctime_s, _Post_readable_size_(26) wchar_t, _Buffer, _In_ const struct tm *, _Time)

_CRT_INSECURE_DEPRECATE(_wctime32_s) _CRTIMP wchar_t * __cdecl _wctime32(_In_ const __time32_t *_Time);
_CRTIMP errno_t __cdecl _wctime32_s(_Out_writes_(_SizeInWords) _Post_readable_size_(26) wchar_t* _Buf, _In_ size_t _SizeInWords, _In_ const __time32_t * _Time);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_1(errno_t, _wctime32_s, _Post_readable_size_(26) wchar_t, _Buffer, _In_ const __time32_t *, _Time)

_CRTIMP size_t __cdecl wcsftime(_Out_writes_z_(_SizeInWords) wchar_t * _Buf, _In_ size_t _SizeInWords, _In_z_ _Printf_format_string_ const wchar_t * _Format,  _In_ const struct tm * _Tm);
_CRTIMP size_t __cdecl _wcsftime_l(_Out_writes_z_(_SizeInWords) wchar_t * _Buf, _In_ size_t _SizeInWords, _In_z_ _Printf_format_string_ const wchar_t *_Format, _In_ const struct tm *_Tm, _In_opt_ _locale_t _Locale);

_CRTIMP errno_t __cdecl _wstrdate_s(_Out_writes_(_SizeInWords) _Post_readable_size_(9) wchar_t * _Buf, _In_ size_t _SizeInWords);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_0(errno_t, _wstrdate_s, _Post_readable_size_(9) wchar_t, _Buffer)
__DEFINE_CPP_OVERLOAD_STANDARD_FUNC_0_0(wchar_t *, __RETURN_POLICY_DST, _CRTIMP, _wstrdate, _Out_writes_z_(9), wchar_t, _Buffer)

_CRTIMP errno_t __cdecl _wstrtime_s(_Out_writes_(_SizeInWords) _Post_readable_size_(9) wchar_t * _Buf, _In_ size_t _SizeInWords);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_0(errno_t, _wstrtime_s, _Post_readable_size_(9) wchar_t, _Buffer)
__DEFINE_CPP_OVERLOAD_STANDARD_FUNC_0_0(wchar_t *, __RETURN_POLICY_DST, _CRTIMP, _wstrtime, _Out_writes_z_(9), wchar_t, _Buffer)

_CRT_INSECURE_DEPRECATE(_wctime64_s) _CRTIMP wchar_t * __cdecl _wctime64(_In_ const __time64_t * _Time);
_CRTIMP errno_t __cdecl _wctime64_s(_Out_writes_(_SizeInWords) _Post_readable_size_(26) wchar_t* _Buf, _In_ size_t _SizeInWords, _In_ const __time64_t *_Time);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_1(errno_t, _wctime64_s, _Post_readable_size_(26) wchar_t, _Buffer, _In_ const __time64_t *, _Time)

#if !defined(RC_INVOKED) && !defined(__midl)
#include <wtime.inl>
#endif

#define _WTIME_DEFINED
#endif

#if !defined(RC_INVOKED) && !defined(__midl)
#include <time.inl>
#endif

#if     !__STDC__ || defined(_POSIX_)

/* Non-ANSI names for compatibility */

#define CLK_TCK  CLOCKS_PER_SEC

/*
daylight, timezone, and tzname are not available under /clr:pure.
Please use _daylight, _timezone, and _tzname or 
_get_daylight, _get_timezone, and _get_tzname instead.
*/
#if !defined(_M_CEE_PURE)
_CRT_INSECURE_DEPRECATE_GLOBALS(_get_daylight) _CRTIMP extern int daylight;
_CRT_INSECURE_DEPRECATE_GLOBALS(_get_timezone) _CRTIMP extern long timezone;
_CRT_INSECURE_DEPRECATE_GLOBALS(_get_tzname) _CRTIMP extern char * tzname[2];
#endif /* !defined(_M_CEE_PURE) */

#ifdef _CRT_USE_WINAPI_FAMILY_DESKTOP_APP
_CRT_NONSTDC_DEPRECATE(_tzset) _CRTIMP void __cdecl tzset(void);
#endif /* _CRT_USE_WINAPI_FAMILY_DESKTOP_APP */

#endif  /* __STDC__ */


#ifdef  __cplusplus
}
#endif

#pragma pack(pop)

#endif  /* _INC_TIME */
