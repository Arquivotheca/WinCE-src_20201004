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
*process.h - definition and declarations for process control functions
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       This file defines the modeflag values for spawnxx calls.
*       Also contains the function argument declarations for all
*       process control related routines.
*
*       [Public]
*
*Revision History:
*       08/24/87  JCR   Added P_NOWAITO
*       10/20/87  JCR   Removed "MSC40_ONLY" entries and "MSSDK_ONLY" comments
*       12-11-87  JCR   Added "_loadds" functionality
*       12-18-87  JCR   Added _FAR_ to declarations
*       01-11-88  JCR   Added _beginthread/_endthread
*       01-15-88  JCR   Got rid of _p_overlay for MTRHEAD/DLL
*       02-10-88  JCR   Cleaned up white space
*       05-08-88  SKS   Removed bogus comment about "DOS 4"; Added "P_DETACH"
*       08-22-88  GJF   Modified to also work for the 386 (small model only)
*       09-14-88  JCR   Added _cexit and _c_exit declarations
*       05-03-89  JCR   Added _INTERNAL_IFSTRIP for relinc usage
*       06-08-89  JCR   386 _beginthread does NOT take a stackpointer arg
*       08-01-89  GJF   Cleanup, now specific to OS/2 2.0 (i.e., 386 flat model)
*       10-30-89  GJF   Fixed copyright
*       11-02-89  JCR   Changed "DLL" to "_DLL"
*       11-17-89  GJF   Added const attribute to appropriate arg types
*       03-01-90  GJF   Added #ifndef _INC_PROCESS and #include <cruntime.h>
*                       stuff. Also, removed some (now) useless preprocessor
*                       directives.
*       03-21-90  GJF   Replaced _cdecl with _CALLTYPE1 or _CALLTYPE2 in
*                       prototypes.
*       04-10-90  GJF   Replaced remaining instances of _cdecl (with _CALLTYPE1
*                       or _VARTYPE1, as appropriate).
*       10-12-90  GJF   Changed return type of _beginthread() to unsigned long.
*       01-17-91  GJF   ANSI naming.
*       08-20-91  JCR   C++ and ANSI naming
*       08-26-91  BWM   Added prototypes for _loaddll, unloaddll, and
*                       _getdllprocaddr.
*       09-28-91  JCR   ANSI names: DOSX32=prototypes, WIN32=#defines for now
*       07-22-92  GJF   Deleted references to _wait for Win32.
*       08-05-92  GJF   Function calling type and variable type macros.
*       08-28-92  GJF   #ifdef-ed out for POSIX.
*       09-03-92  GJF   Merged two changes above.
*       01-21-93  GJF   Removed support for C6-386's _cdecl.
*       03-20-93  SKS   Remove obsolete _loaddll, unloaddll, _getdllprocaddr.
*       04-07-93  SKS   Add _CRTIMP keyword for CRT DLL model
*                       Use link-time aliases for old names, not #define's
*       10-11-93  GJF   Merged NT and Cuda versions.
*       12-06-93  CFW   Add wCRT_INIT.
*       12-07-93  CFW   Add wide exec/spawn protos.
*       02-16-94  SKS   Add _beginthreadex(), _endthreadex()
*       12-28-94  JCF   Merged with mac header
*       02-11-95  CFW   Add _CRTBLD to avoid users getting wrong headers.
*       02-13-95  CFW   Fixed Mac merge.
*       02-14-95  CFW   Clean up Mac merge.
*       05-24-95  CFW   "spawn" not a mac oldames.
*       12-14-95  JWM   Add "#pragma once".
*       02-20-97  GJF   Cleaned out obsolete support for _CRTAPI* and _NTSDK.
*                       Also, detab-ed.
*       09-30-97  JWM   Restored not-so-obsolete _CRTAPI1 support.
*       02-06-98  GJF   Changes for Win64: changed return and argument types to
*                       to intptr_t and uintptr_t where appropriate.
*       02-10-98  GJF   Changes for Win64: fixed a couple of prototypes
*       05-13-99  PML   Remove _CRTAPI1
*       05-17-99  PML   Remove all Macintosh support.
*       06-05-99  PML   Win64: int -> intptr_t for !__STDC__ variants.
*       10-06-99  PML   Add _W64 modifier to types which are 32 bits in Win32,
*                       64 bits in Win64.
*       10-25-02  SJ    Fixed /clr /W4 Warnings VSWhidbey-2445
*       10-28-02  PK    Marked _loaddll, _unloaddll and _getdllprocaddr as 
*                       #pragma deprecated.
*       02-16-04  SJ    VSW#243523 - moved the lines preventing deprecation from
*                       the header to the sources.
*       04-07-04  GB    Added support for beginthread et. al. for /clr:pure.
*       11-08-04  JL    Added _CRT_NONSTDC_DEPRECATE to deprecate non-ANSI functions
*       11-18-04  ATC   SAL Annotations for CRT Headers
*       01-14-05  AC    Fix SAL annotations (using prefast espx plug-in)
*       03-23-05  MSL   _P removal - not needed
*                       New deprecation warning with function name
*                       Packing fix
*       04-14-05  MSL   Compile clean under MIDL
*       05-24-05  PML   Make sure unencoded global func-ptrs are in .rdata
*                       (vsw#191114)
*       06-06-05  PML   Add /GS helpers as part of _except_handler4 work.
*
****/

#pragma once

#ifndef _INC_PROCESS
#define _INC_PROCESS

#include <crtdefs.h>

#ifndef _POSIX_

#ifdef __cplusplus
extern "C" {
#endif

/* modeflag values for _spawnxx routines */

#define _P_WAIT         0
#define _P_NOWAIT       1
#define _OLD_P_OVERLAY  2
#define _P_NOWAITO      3
#define _P_DETACH       4

#define _P_OVERLAY      2

/* Action codes for _cwait(). The action code argument to _cwait is ignored
   on Win32 though it is accepted for compatibilty with old MS CRT libs */
#define _WAIT_CHILD      0
#define _WAIT_GRANDCHILD 1


/* function prototypes */

#ifdef _CRT_USE_WINAPI_FAMILY_DESKTOP_APP
_CRTIMP uintptr_t __cdecl _beginthread (_In_ void (__cdecl * _StartAddress) (void *),
        _In_ unsigned _StackSize, _In_opt_ void * _ArgList);
_CRTIMP void __cdecl _endthread(void);
_CRTIMP uintptr_t __cdecl _beginthreadex(_In_opt_ void * _Security, _In_ unsigned _StackSize,
        _In_ unsigned (__stdcall * _StartAddress) (void *), _In_opt_ void * _ArgList, 
        _In_ unsigned _InitFlag, _Out_opt_ unsigned * _ThrdAddr);
_CRTIMP void __cdecl _endthreadex(_In_ unsigned _Retval);
#endif /* _CRT_USE_WINAPI_FAMILY_DESKTOP_APP */

#ifndef _CRT_TERMINATE_DEFINED
#define _CRT_TERMINATE_DEFINED
_CRTIMP __declspec(noreturn) void __cdecl exit(_In_ int _Code);
_CRTIMP __declspec(noreturn) void __cdecl _exit(_In_ int _Code);
_CRTIMP __declspec(noreturn) void __cdecl abort(void);
#endif

_CRTIMP void __cdecl _cexit(void);
_CRTIMP void __cdecl _c_exit(void);

#ifdef _CRT_USE_WINAPI_FAMILY_DESKTOP_APP
_CRTIMP int __cdecl _getpid(void);
_CRTIMP intptr_t __cdecl _cwait(_Out_opt_ int * _TermStat, _In_ intptr_t _ProcHandle, _In_ int _Action);
_CRTIMP intptr_t __cdecl _execl(_In_z_ const char * _Filename, _In_z_ const char * _ArgList, ...);
_CRTIMP intptr_t __cdecl _execle(_In_z_ const char * _Filename, _In_z_ const char * _ArgList, ...);
_CRTIMP intptr_t __cdecl _execlp(_In_z_ const char * _Filename, _In_z_ const char * _ArgList, ...);
_CRTIMP intptr_t __cdecl _execlpe(_In_z_ const char * _Filename, _In_z_ const char * _ArgList, ...);
_CRTIMP intptr_t __cdecl _execv(_In_z_ const char * _Filename, _In_z_ const char * const * _ArgList);
_CRTIMP intptr_t __cdecl _execve(_In_z_ const char * _Filename, _In_z_ const char * const * _ArgList, _In_opt_z_ const char * const * _Env);
_CRTIMP intptr_t __cdecl _execvp(_In_z_ const char * _Filename, _In_z_ const char * const * _ArgList);
_CRTIMP intptr_t __cdecl _execvpe(_In_z_ const char * _Filename, _In_z_ const char * const * _ArgList, _In_opt_z_ const char * const * _Env);
_CRTIMP intptr_t __cdecl _spawnl(_In_ int _Mode, _In_z_ const char * _Filename, _In_z_ const char * _ArgList, ...);
_CRTIMP intptr_t __cdecl _spawnle(_In_ int _Mode, _In_z_ const char * _Filename, _In_z_ const char * _ArgList, ...);
_CRTIMP intptr_t __cdecl _spawnlp(_In_ int _Mode, _In_z_ const char * _Filename, _In_z_ const char * _ArgList, ...);
_CRTIMP intptr_t __cdecl _spawnlpe(_In_ int _Mode, _In_z_ const char * _Filename, _In_z_ const char * _ArgList, ...);
_CRTIMP intptr_t __cdecl _spawnv(_In_ int _Mode, _In_z_ const char * _Filename, _In_z_ const char * const * _ArgList);
_CRTIMP intptr_t __cdecl _spawnve(_In_ int _Mode, _In_z_ const char * _Filename, _In_z_ const char * const * _ArgList,
        _In_opt_z_ const char * const * _Env);
_CRTIMP intptr_t __cdecl _spawnvp(_In_ int _Mode, _In_z_ const char * _Filename, _In_z_ const char * const * _ArgList);
_CRTIMP intptr_t __cdecl _spawnvpe(_In_ int _Mode, _In_z_ const char * _Filename, _In_z_ const char * const * _ArgList,
        _In_opt_z_ const char * const * _Env);

#ifndef _CRT_SYSTEM_DEFINED
#define _CRT_SYSTEM_DEFINED
_CRTIMP int __cdecl system(_In_opt_z_ const char * _Command);
#endif

#ifndef _WPROCESS_DEFINED
/* wide function prototypes, also declared in wchar.h  */
_CRTIMP intptr_t __cdecl _wexecl(_In_z_ const wchar_t * _Filename, _In_z_ const wchar_t * _ArgList, ...);
_CRTIMP intptr_t __cdecl _wexecle(_In_z_ const wchar_t * _Filename, _In_z_ const wchar_t * _ArgList, ...);
_CRTIMP intptr_t __cdecl _wexeclp(_In_z_ const wchar_t * _Filename, _In_z_ const wchar_t * _ArgList, ...);
_CRTIMP intptr_t __cdecl _wexeclpe(_In_z_ const wchar_t * _Filename, _In_z_ const wchar_t * _ArgList, ...);
_CRTIMP intptr_t __cdecl _wexecv(_In_z_ const wchar_t * _Filename, _In_z_ const wchar_t * const * _ArgList);
_CRTIMP intptr_t __cdecl _wexecve(_In_z_ const wchar_t * _Filename, _In_z_ const wchar_t * const * _ArgList,
        _In_opt_z_ const wchar_t * const * _Env);
_CRTIMP intptr_t __cdecl _wexecvp(_In_z_ const wchar_t * _Filename, _In_z_ const wchar_t * const * _ArgList);
_CRTIMP intptr_t __cdecl _wexecvpe(_In_z_ const wchar_t * _Filename, _In_z_ const wchar_t * const * _ArgList, 
        _In_opt_z_ const wchar_t * const * _Env);
_CRTIMP intptr_t __cdecl _wspawnl(_In_ int _Mode, _In_z_ const wchar_t * _Filename, _In_z_ const wchar_t * _ArgList, ...);
_CRTIMP intptr_t __cdecl _wspawnle(_In_ int _Mode, _In_z_ const wchar_t * _Filename, _In_z_ const wchar_t * _ArgList, ...);
_CRTIMP intptr_t __cdecl _wspawnlp(_In_ int _Mode, _In_z_ const wchar_t * _Filename, _In_z_ const wchar_t * _ArgList, ...);
_CRTIMP intptr_t __cdecl _wspawnlpe(_In_ int _Mode, _In_z_ const wchar_t * _Filename, _In_z_ const wchar_t * _ArgList, ...);
_CRTIMP intptr_t __cdecl _wspawnv(_In_ int _Mode, _In_z_ const wchar_t * _Filename, _In_z_ const wchar_t * const * _ArgList);
_CRTIMP intptr_t __cdecl _wspawnve(_In_ int _Mode, _In_z_ const wchar_t * _Filename, _In_z_ const wchar_t * const * _ArgList,
        _In_opt_z_ const wchar_t * const * _Env);
_CRTIMP intptr_t __cdecl _wspawnvp(_In_ int _Mode, _In_z_ const wchar_t * _Filename, _In_z_ const wchar_t * const * _ArgList);
_CRTIMP intptr_t __cdecl _wspawnvpe(_In_ int _Mode, _In_z_ const wchar_t * _Filename, _In_z_ const wchar_t * const * _ArgList,
        _In_opt_z_ const wchar_t * const * _Env);
#ifndef _CRT_WSYSTEM_DEFINED
#define _CRT_WSYSTEM_DEFINED
_CRTIMP int __cdecl _wsystem(_In_opt_z_ const wchar_t * _Command);
#endif

#define _WPROCESS_DEFINED
#endif

#endif /* _CRT_USE_WINAPI_FAMILY_DESKTOP_APP */

/*
 * Security check initialization and failure reporting used by /GS security
 * checks.
 */
#if !defined(_M_CEE)
void __cdecl __security_init_cookie(void);
#ifdef  _M_IX86
void __fastcall __security_check_cookie(_In_ uintptr_t _StackCookie);
__declspec(noreturn) void __cdecl __report_gsfailure(void);
#else
void __cdecl __security_check_cookie(_In_ uintptr_t _StackCookie);
__declspec(noreturn) void __cdecl __report_gsfailure(_In_ uintptr_t _StackCookie);
#endif
#endif
extern uintptr_t __security_cookie;

#ifdef _CRT_USE_WINAPI_FAMILY_DESKTOP_APP
/* --------- The following functions are OBSOLETE --------- */
/*
 * The Win32 API LoadLibrary, FreeLibrary and GetProcAddress should be used
 * instead.
 */

_CRT_OBSOLETE(LoadLibrary) intptr_t __cdecl _loaddll(_In_z_ char * _Filename);
_CRT_OBSOLETE(FreeLibrary) int __cdecl _unloaddll(_In_ intptr_t _Handle);
_CRT_OBSOLETE(GetProcAddress) int (__cdecl * __cdecl _getdllprocaddr(_In_ intptr_t _Handle, _In_opt_z_ char * _ProcedureName, _In_ intptr_t _Ordinal))(void);

/* --------- The preceding functions are OBSOLETE --------- */
#endif /* _CRT_USE_WINAPI_FAMILY_DESKTOP_APP */


#ifdef  _DECL_DLLMAIN
/*
 * Declare DLL notification (initialization/termination) routines
 *      The preferred method is for the user to provide DllMain() which will
 *      be called automatically by the DLL entry point defined by the C run-
 *      time library code.  If the user wants to define the DLL entry point
 *      routine, the user's entry point must call _CRT_INIT on all types of
 *      notifications, as the very first thing on attach notifications and
 *      as the very last thing on detach notifications.
 */
// CE's windows.h define as __WINDOWS__ and NT's windows.h define as _WINDOWS_
#ifdef  __WINDOWS__       /* Use types from WINDOWS.H */
#if defined (MRTDLL)
BOOL __clrcall DllMain(_In_ HANDLE _HDllHandle, _In_ DWORD _Reason, _In_opt_ LPVOID _Reserved);
#else
BOOL WINAPI DllMain(_In_ HANDLE _HDllHandle, _In_ DWORD _Reason, _In_opt_ LPVOID _Reserved);
#endif
#if defined (MRTDLL)
BOOL _CRT_INIT(_In_ HANDLE _HDllHandle, _In_ DWORD _Reason, _In_opt_ LPVOID _Reserved);
#else
BOOL WINAPI _CRT_INIT(_In_ HANDLE _HDllHandle, _In_ DWORD _Reason, _In_opt_ LPVOID _Reserved);
#endif
BOOL WINAPI _wCRT_INIT(_In_ HANDLE _HDllHandle, _In_ DWORD _Reason, _In_opt_ LPVOID _Reserved);
extern BOOL (WINAPI * const _pRawDllMain)(HANDLE, DWORD, LPVOID);
#else
int __stdcall DllMain(_In_ void * _HDllHandle, _In_ unsigned _Reason, _In_opt_ void * _Reserved);
int __stdcall _CRT_INIT(_In_ void * _HDllHandle, _In_ unsigned _Reason, _In_opt_ void * _Reserved);
int __stdcall _wCRT_INIT(_In_ void * _HDllHandle, _In_ unsigned _Reason, _In_opt_ void * _Reserved);
extern int (__stdcall * const _pRawDllMain)(void *, unsigned, void *);
#endif  /* _WINDOWS_ */
#endif

#if     !__STDC__

/* Non-ANSI names for compatibility */

#define P_WAIT          _P_WAIT
#define P_NOWAIT        _P_NOWAIT
#define P_OVERLAY       _P_OVERLAY
#define OLD_P_OVERLAY   _OLD_P_OVERLAY
#define P_NOWAITO       _P_NOWAITO
#define P_DETACH        _P_DETACH
#define WAIT_CHILD      _WAIT_CHILD
#define WAIT_GRANDCHILD _WAIT_GRANDCHILD

#ifdef _CRT_USE_WINAPI_FAMILY_DESKTOP_APP

/* current declarations */
_CRT_NONSTDC_DEPRECATE(_cwait) _CRTIMP intptr_t __cdecl cwait(_Out_opt_ int * _TermStat, _In_ intptr_t _ProcHandle, _In_ int _Action);
_CRT_NONSTDC_DEPRECATE(_execl) _CRTIMP intptr_t __cdecl execl(_In_z_ const char * _Filename, _In_z_ const char * _ArgList, ...);
_CRT_NONSTDC_DEPRECATE(_execle) _CRTIMP intptr_t __cdecl execle(_In_z_ const char * _Filename, _In_z_ const char * _ArgList, ...);
_CRT_NONSTDC_DEPRECATE(_execlp) _CRTIMP intptr_t __cdecl execlp(_In_z_ const char * _Filename, _In_z_ const char * _ArgList, ...);
_CRT_NONSTDC_DEPRECATE(_execlpe) _CRTIMP intptr_t __cdecl execlpe(_In_z_ const char * _Filename, _In_z_ const char * _ArgList, ...);
_CRT_NONSTDC_DEPRECATE(_execv) _CRTIMP intptr_t __cdecl execv(_In_z_ const char * _Filename, _In_z_ const char * const * _ArgList);
_CRT_NONSTDC_DEPRECATE(_execve) _CRTIMP intptr_t __cdecl execve(_In_z_ const char * _Filename, _In_z_ const char * const * _ArgList, _In_opt_z_ const char * const * _Env);
_CRT_NONSTDC_DEPRECATE(_execvp) _CRTIMP intptr_t __cdecl execvp(_In_z_ const char * _Filename, _In_z_ const char * const * _ArgList);
_CRT_NONSTDC_DEPRECATE(_execvpe) _CRTIMP intptr_t __cdecl execvpe(_In_z_ const char * _Filename, _In_z_ const char * const * _ArgList, _In_opt_z_ const char * const * _Env);
_CRT_NONSTDC_DEPRECATE(_spawnl) _CRTIMP intptr_t __cdecl spawnl(_In_ int, _In_z_ const char * _Filename, _In_z_ const char * _ArgList, ...);
_CRT_NONSTDC_DEPRECATE(_spawnle) _CRTIMP intptr_t __cdecl spawnle(_In_ int, _In_z_ const char * _Filename, _In_z_ const char * _ArgList, ...);
_CRT_NONSTDC_DEPRECATE(_spawnlp) _CRTIMP intptr_t __cdecl spawnlp(_In_ int, _In_z_ const char * _Filename, _In_z_ const char * _ArgList, ...);
_CRT_NONSTDC_DEPRECATE(_spawnlpe) _CRTIMP intptr_t __cdecl spawnlpe(_In_ int, _In_z_ const char * _Filename, _In_z_ const char * _ArgList, ...);
_CRT_NONSTDC_DEPRECATE(_spawnv) _CRTIMP intptr_t __cdecl spawnv(_In_ int, _In_z_ const char * _Filename, _In_z_ const char * const * _ArgList);
_CRT_NONSTDC_DEPRECATE(_spawnve) _CRTIMP intptr_t __cdecl spawnve(_In_ int, _In_z_ const char * _Filename, _In_z_ const char * const * _ArgList,
        _In_opt_z_ const char * const * _Env);
_CRT_NONSTDC_DEPRECATE(_spawnvp) _CRTIMP intptr_t __cdecl spawnvp(_In_ int, _In_z_ const char * _Filename, _In_z_ const char * const * _ArgList);
_CRT_NONSTDC_DEPRECATE(_spawnvpe) _CRTIMP intptr_t __cdecl spawnvpe(_In_ int, _In_z_ const char * _Filename, _In_z_ const char * const * _ArgList,
        _In_opt_z_ const char * const * _Env);

_CRT_NONSTDC_DEPRECATE(_getpid) _CRTIMP int __cdecl getpid(void);

#endif /* _CRT_USE_WINAPI_FAMILY_DESKTOP_APP */

#endif  /* __STDC__ */

#ifdef  __cplusplus
}
#endif

#endif  /* _POSIX_ */

#endif  /* _INC_PROCESS */
