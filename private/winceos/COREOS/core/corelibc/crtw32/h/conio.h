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
*conio.h - console and port I/O declarations
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       This include file contains the function declarations for
*       the MS C V2.03 compatible console I/O routines.
*
*       [Public]
*
*Revision History:
*       07-27-87  SKS   Added inpw(), outpw()
*       08-05-87  SKS   Change outpw() from "int" return to "unsigned"
*       11-16-87  JCR   Multi-thread support
*       12-11-87  JCR   Added "_loadds" functionality
*       12-17-87  JCR   Added _MTHREAD_ONLY
*       12-18-87  JCR   Added _FAR_ to declarations
*       02-10-88  JCR   Cleaned up white space
*       08-19-88  GJF   Modified to also work for the 386 (small model only)
*       05-03-89  JCR   Added _INTERNAL_IFSTRIP for relinc usage
*       07-27-89  GJF   Cleanup, now specific to the 386
*       10-30-89  GJF   Fixed copyright
*       11-02-89  JCR   Changed "DLL" to "_DLL"
*       11-17-89  GJF   Added const to appropriate arg types of cprintf(),
*                       cputs() and cscanf().
*       02-27-90  GJF   Added #ifndef _INC_CONIO and #include <cruntime.h>
*                       stuff. Also, removed some (now) useless preprocessor
*                       directives.
*       03-21-90  GJF   Replaced _cdecl with _CALLTYPE1 or _CALLTYPE2 in
*                       prototypes.
*       07-23-90  SBM   Added _getch_nolock() prototype/macro
*       01-16-91  GJF   ANSI support. Also, removed prototypes for port i/o
*                       functions (not supported in 32-bit).
*       08-20-91  JCR   C++ and ANSI naming
*       09-28-91  JCR   ANSI names: DOSX32=prototypes, WIN32=#defines for now
*       08-26-92  GJF   Function calling type and variable type macros.
*       01-21-93  GJF   Removed support for C6-386's _cdecl.
*       04-06-93  SKS   Replace _CRTAPI1/2 with __cdecl, _CRTVAR1 with nothing
*       04-07-93  SKS   Add _CRTIMP keyword for CRT DLL model
*                       Use link-time aliases for old names, not #define's
*       04-09-93  GJF   Restored prototypes for port i/o.
*       04-13-93  GJF   Change port i/o prototypes per ChuckM.
*       09-01-93  GJF   Merged Cuda and NT SDK versions.
*       10-27-93  GJF   Made port i/o prototypes conditional on _M_IX86.
*       02-11-95  CFW   Add _CRTBLD to avoid users getting wrong headers.
*       02-14-95  CFW   Clean up Mac merge.
*       05-24-95  CFW   Header not for use with Mac.
*       12-14-95  JWM   Add "#pragma once".
*       01-23-97  GJF   Cleaned out obsolete support for _NTSDK and _CRTAPI*.
*                       Also, detab-ed.
*       09-30-97  JWM   Restored not-so-obsolete _CRTAPI1 support.
*       05-13-99  PML   Remove _CRTAPI1
*       05-17-99  PML   Remove all Macintosh support.
*       02-11-00  GB    Added support for unicode console output functions.
*       04-25-00  GB    Added support for unicode console input functions.
*       07-20-00  GB    typedefed wint_t to unsigned short
*       11-22-00  PML   Wide-char *putwc* functions take a wchar_t, not wint_t.
*       06-13-01  PML   Compile clean -Za -W4 -Tc (vs7#267063)
*       03-13-03  SSM   Expose _vcprintf & _vcwprintf. VSWhidbey:2439
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*       10-23-03  SJ    Secure Version for scanf family which takes an extra 
*                       size parameter from the var arg list.
*       10-28-03  SJ    Secure CRT - printf_s : positional args & validations
*       01-30-04  SJ    VSW#228233 - splitting printf_s into 2 functions.
*       02-13-04  SJ    VSW#242637 - removing _printf_p from the headers
*       03-18-04  SJ    Putting _printf_p back in the headers
*       03-24-04  MSL   Fixed declarations to work with newsyntax
*                       VSW#257801
*       09-07-04  AC    Removed deprecation for cprintf family.
*                       VSW#344608
*       11-02-04  AC    Add cpp overloads for secure functions.
*                       VSW#313364
*       11-08-04  JL    Added _CRT_NONSTDC_DEPRECATE to deprecate non-ANSI functions
*       11-18-04  ATC   SAL Annotations for CRT Headers
*       11-23-04  AC    Fix cpp overload for cgets family.
*                       VSW#408697
*       01-14-05  AC    Fix SAL annotations (using prefast espx plug-in)
*       01-21-05  MSL   Add param names to overload macros
*       03-23-05  MSL   _P removal - not needed
*                       New deprecation warning with function name
*       04-14-05  MSL   Intellisense locale name cleanup
*       06-10-05  AC    Cleanup tchar.h and add some missing _nolock function exports
*                       VSW#502000
*       03-31-08  HG    Fix cgets_s overloads DDB#391750
*
****/

#pragma once

#ifndef _INC_CONIO
#define _INC_CONIO

#include <crtdefs.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Function prototypes */

#ifdef _CRT_USE_WINAPI_FAMILY_DESKTOP_APP

_Check_return_wat_ _CRTIMP errno_t __cdecl _cgets_s(_Out_writes_z_(_Size)                char * _Buffer, size_t _Size, _Out_ size_t * _SizeRead);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_1(errno_t, _cgets_s, _Out_writes_z_(*_Buffer) char, _Buffer, _Out_ size_t *, _SizeRead)
__DEFINE_CPP_OVERLOAD_STANDARD_FUNC_0_0_CGETS(char *, _CRTIMP, _cgets, _Pre_notnull_ _Post_z_, char, _Buffer)
_Check_return_opt_ _CRTIMP int __cdecl _cprintf(_In_z_ _Printf_format_string_ const char * _Format, ...);
_Check_return_opt_ _CRTIMP int __cdecl _cprintf_s(_In_z_ _Printf_format_string_ const char * _Format, ...);
_Check_return_opt_ _CRTIMP int __cdecl _cputs(_In_z_ const char * _Str);
_Check_return_opt_ _CRT_INSECURE_DEPRECATE(_cscanf_s) _CRTIMP int __cdecl _cscanf(_In_z_ _Scanf_format_string_ const char * _Format, ...);
_Check_return_opt_ _CRT_INSECURE_DEPRECATE(_cscanf_s_l) _CRTIMP int __cdecl _cscanf_l(_In_z_ _Scanf_format_string_params_(0) const char * _Format, _In_opt_ _locale_t _Locale, ...);
_Check_return_opt_ _CRTIMP int __cdecl _cscanf_s(_In_z_ _Scanf_format_string_ const char * _Format, ...);
_Check_return_opt_ _CRTIMP int __cdecl _cscanf_s_l(_In_z_ _Scanf_format_string_params_(0) const char * _Format, _In_opt_ _locale_t _Locale, ...);
_Check_return_ _CRTIMP int __cdecl _getch(void);
_Check_return_ _CRTIMP int __cdecl _getche(void);
_Check_return_opt_ _CRTIMP int __cdecl _vcprintf(_In_z_ _Printf_format_string_ const char * _Format, va_list _ArgList);
_Check_return_opt_ _CRTIMP int __cdecl _vcprintf_s(_In_z_ _Printf_format_string_ const char * _Format, va_list _ArgList);

_Check_return_opt_ _CRTIMP int __cdecl _cprintf_p(_In_z_ _Printf_format_string_ const char * _Format, ...);
_Check_return_opt_ _CRTIMP int __cdecl _vcprintf_p(_In_z_ const char * _Format, va_list _ArgList);

_Check_return_opt_ _CRTIMP int __cdecl _cprintf_l(_In_z_ _Printf_format_string_params_(0) const char * _Format, _In_opt_ _locale_t _Locale, ...);
_Check_return_opt_ _CRTIMP int __cdecl _cprintf_s_l(_In_z_ _Printf_format_string_params_(0) const char * _Format, _In_opt_ _locale_t _Locale, ...);
_Check_return_opt_ _CRTIMP int __cdecl _vcprintf_l(_In_z_ _Printf_format_string_params_(2) const char * _Format, _In_opt_ _locale_t _Locale, va_list _ArgList);
_Check_return_opt_ _CRTIMP int __cdecl _vcprintf_s_l(_In_z_ _Printf_format_string_params_(2) const char * _Format, _In_opt_ _locale_t _Locale, va_list _ArgList);
_Check_return_opt_ _CRTIMP int __cdecl _cprintf_p_l(_In_z_ _Printf_format_string_params_(0) const char * _Format, _In_opt_ _locale_t _Locale, ...);
_Check_return_opt_ _CRTIMP int __cdecl _vcprintf_p_l(_In_z_ _Printf_format_string_params_(2) const char * _Format, _In_opt_ _locale_t _Locale, va_list _ArgList);

#if defined(_M_IX86) || defined(_M_X64)
int __cdecl _inp(unsigned short);
unsigned short __cdecl _inpw(unsigned short);
unsigned long __cdecl _inpd(unsigned short);
#endif /* _M_IX86 || _M_X64 */
_CRTIMP int __cdecl _kbhit(void);
#if defined(_M_IX86) || defined(_M_X64)
int __cdecl _outp(unsigned short, int);
unsigned short __cdecl _outpw(unsigned short, unsigned short);
unsigned long __cdecl _outpd(unsigned short, unsigned long);
#endif /* _M_IX86 || _M_X64 */
_CRTIMP int __cdecl _putch(_In_ int _Ch);
_CRTIMP int __cdecl _ungetch(_In_ int _Ch);

_Check_return_ _CRTIMP int __cdecl _getch_nolock(void);
_Check_return_ _CRTIMP int __cdecl _getche_nolock(void);
_CRTIMP int __cdecl _putch_nolock(_In_ int _Ch);
_CRTIMP int __cdecl _ungetch_nolock(_In_ int _Ch);

#ifndef _WCONIO_DEFINED

/* wide function prototypes, also declared in wchar.h */

#ifndef WEOF
#define WEOF (wint_t)(0xFFFF)
#endif

_Check_return_wat_ _CRTIMP errno_t __cdecl _cgetws_s(_Out_writes_to_(_SizeInWords, *_SizeRead) wchar_t * _Buffer, size_t _SizeInWords, _Out_ size_t * _SizeRead);
__DEFINE_CPP_OVERLOAD_SECURE_FUNC_0_1(errno_t, _cgetws_s, _Out_writes_z_(*_Buffer) wchar_t, _Buffer, size_t *, _SizeRead)
__DEFINE_CPP_OVERLOAD_STANDARD_FUNC_0_0_CGETS(wchar_t *, _CRTIMP, _cgetws, _Pre_notnull_ _Post_z_, wchar_t, _Buffer)
_Check_return_ _CRTIMP wint_t __cdecl _getwch(void);
_Check_return_ _CRTIMP wint_t __cdecl _getwche(void);
_Check_return_ _CRTIMP wint_t __cdecl _putwch(wchar_t _WCh);
_Check_return_ _CRTIMP wint_t __cdecl _ungetwch(wint_t _WCh);
_Check_return_opt_ _CRTIMP int __cdecl _cputws(_In_z_ const wchar_t * _String);
_Check_return_opt_ _CRTIMP int __cdecl _cwprintf(_In_z_ _Printf_format_string_ const wchar_t * _Format, ...);
_Check_return_opt_ _CRTIMP int __cdecl _cwprintf_s(_In_z_ _Printf_format_string_ const wchar_t * _Format, ...);
_Check_return_opt_ _CRT_INSECURE_DEPRECATE(_cwscanf_s) _CRTIMP int __cdecl _cwscanf(_In_z_ _Scanf_format_string_ const wchar_t * _Format, ...);
_Check_return_opt_ _CRT_INSECURE_DEPRECATE(_cwscanf_s_l) _CRTIMP int __cdecl _cwscanf_l(_In_z_ _Scanf_format_string_params_(0) const wchar_t * _Format, _In_opt_ _locale_t _Locale, ...);
_Check_return_opt_ _CRTIMP int __cdecl _cwscanf_s(_In_z_ _Scanf_format_string_ const wchar_t * _Format, ...);
_Check_return_opt_ _CRTIMP int __cdecl _cwscanf_s_l(_In_z_ _Scanf_format_string_params_(0) const wchar_t * _Format, _In_opt_ _locale_t _Locale, ...);
_Check_return_opt_ _CRTIMP int __cdecl _vcwprintf(_In_z_ _Printf_format_string_ const wchar_t *_Format, va_list _ArgList);
_Check_return_opt_ _CRTIMP int __cdecl _vcwprintf_s(_In_z_ _Printf_format_string_ const wchar_t *_Format, va_list _ArgList);

_Check_return_opt_ _CRTIMP int __cdecl _cwprintf_p(_In_z_ _Printf_format_string_ const wchar_t * _Format, ...);
_Check_return_opt_ _CRTIMP int __cdecl _vcwprintf_p(_In_z_ _Printf_format_string_ const wchar_t*  _Format, va_list _ArgList);

_CRTIMP int __cdecl _cwprintf_l(_In_z_ _Printf_format_string_params_(0) const wchar_t * _Format, _In_opt_ _locale_t _Locale, ...);
_CRTIMP int __cdecl _cwprintf_s_l(_In_z_ _Printf_format_string_params_(0) const wchar_t * _Format, _In_opt_ _locale_t _Locale, ...);
_CRTIMP int __cdecl _vcwprintf_l(_In_z_ _Printf_format_string_params_(2) const wchar_t *_Format, _In_opt_ _locale_t _Locale, va_list _ArgList);
_CRTIMP int __cdecl _vcwprintf_s_l(_In_z_ _Printf_format_string_params_(2) const wchar_t * _Format, _In_opt_ _locale_t _Locale, va_list _ArgList);
_CRTIMP int __cdecl _cwprintf_p_l(_In_z_ _Printf_format_string_params_(0) const wchar_t * _Format, _In_opt_ _locale_t _Locale, ...);
_CRTIMP int __cdecl _vcwprintf_p_l(_In_z_ _Printf_format_string_params_(2) const wchar_t * _Format, _In_opt_ _locale_t _Locale, va_list _ArgList);

_Check_return_opt_ _CRTIMP wint_t __cdecl _putwch_nolock(wchar_t _WCh);
_Check_return_ _CRTIMP wint_t __cdecl _getwch_nolock(void);
_Check_return_ _CRTIMP wint_t __cdecl _getwche_nolock(void);
_Check_return_opt_ _CRTIMP wint_t __cdecl _ungetwch_nolock(wint_t _WCh);

#define _WCONIO_DEFINED
#endif  /* _WCONIO_DEFINED */

#if     !__STDC__

/* Non-ANSI names for compatibility */

#pragma warning(push)
#pragma warning(disable: 4141) /* Using deprecated twice */ 
_Check_return_opt_ _CRT_NONSTDC_DEPRECATE(_cgets) _CRT_INSECURE_DEPRECATE(_cgets_s) _CRTIMP char * __cdecl cgets(_Out_writes_z_(_Inexpressible_(*_Buffer+2)) char * _Buffer);
#pragma warning(pop)
_Check_return_opt_ _CRT_NONSTDC_DEPRECATE(_cprintf) _CRTIMP int __cdecl cprintf(_In_z_ _Printf_format_string_ const char * _Format, ...);
_Check_return_opt_ _CRT_NONSTDC_DEPRECATE(_cputs) _CRTIMP int __cdecl cputs(_In_z_ const char * _Str);
_Check_return_opt_ _CRT_NONSTDC_DEPRECATE(_cscanf) _CRTIMP int __cdecl cscanf(_In_z_ _Scanf_format_string_ const char * _Format, ...);
#if defined(_M_IX86) || defined(_M_X64)
_CRT_NONSTDC_DEPRECATE(_inp) int __cdecl inp(unsigned short);
_CRT_NONSTDC_DEPRECATE(_inpd) unsigned long __cdecl inpd(unsigned short);
_CRT_NONSTDC_DEPRECATE(_inpw) unsigned short __cdecl inpw(unsigned short);
#endif /* _M_IX86 || _M_X64 */
_Check_return_ _CRT_NONSTDC_DEPRECATE(_getch) _CRTIMP int __cdecl getch(void);
_Check_return_ _CRT_NONSTDC_DEPRECATE(_getche) _CRTIMP int __cdecl getche(void);
_Check_return_ _CRT_NONSTDC_DEPRECATE(_kbhit) _CRTIMP int __cdecl kbhit(void);
#if defined(_M_IX86) || defined(_M_X64)
_CRT_NONSTDC_DEPRECATE(_outp) int __cdecl outp(unsigned short, int);
_CRT_NONSTDC_DEPRECATE(_outpd) unsigned long __cdecl outpd(unsigned short, unsigned long);
_CRT_NONSTDC_DEPRECATE(_outpw) unsigned short __cdecl outpw(unsigned short, unsigned short);
#endif /* _M_IX86 || _M_X64 */
_Check_return_opt_ _CRT_NONSTDC_DEPRECATE(_putch) _CRTIMP int __cdecl putch(int _Ch);
_Check_return_opt_ _CRT_NONSTDC_DEPRECATE(_ungetch) _CRTIMP int __cdecl ungetch(int _Ch);

#endif  /* __STDC__ */

#endif /* _CRT_USE_WINAPI_FAMILY_DESKTOP_APP */

#ifdef  __cplusplus
}
#endif

#endif  /* _INC_CONIO */
