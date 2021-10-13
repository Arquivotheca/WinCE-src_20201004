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
*internal.h - contains declarations of internal routines and variables
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Declares routines and variables used internally by the C run-time.
*
*       [Internal]
*
*Revision History:
*       05-18-87  SKS   Module created
*       07-15-87  JCR   Added _old_pfxlen and _tempoff
*       08-05-87  JCR   Added _getbuf (corrected by SKS)
*       11-05-87  JCR   Added _buferr
*       11-18-87  SKS   Add __tzset(), made _isindst() near, remove _dtoxmode
*       01-26-88  SKS   Make __tzset, _isindst, _dtoxtime near/far for QC
*       02-10-88  JCR   Cleaned up white space
*       06-22-88  SKS   _canonic/_getcdrv are now used by all models
*       06-29-88  JCR   Removed static buffers _bufout and _buferr
*       08-18-88  GJF   Revised to also work for the 386 (small model only).
*       09-22-88  GJF   Added declarations for _freebuf, _stbuf and _ftbuf.
*       01-31-89  JCR   Removed _canonic, _getcdrv, _getcdwd (see direct.h)
*       06-07-89  PHG   Added _dosret for i860 (N10) version of libs
*       07-05-89  PHG   Changed above to _dosmaperr, added startup variables
*       08-17-89  GJF   Cleanup, removed stuff not needed for 386
*       10-25-89  JCR   Added prototype for _getpath()
*       10-30-89  GJF   Fixed copyright
*       11-02-89  JCR   Changed "DLL" to "_DLL"
*       03-01-90  GJF   Added #ifndef _INC_INTERNAL and #include <cruntime.h>
*                       stuff. Also, removed some (now) useless preprocessing
*                       directives.
*       03-21-90  GJF   Put _CALLTYPE1 into prototypes.
*       03-26-90  GJF   Added prototypes for _output() and _input(). Filled
*                       out the prototype for _openfile
*       04-05-90  GJF   Added prototype for __NMSG_WRITE() (C source build
*                       only).
*       04-10-90  GJF   Added prototypes for startup functions.
*       05-28-90  SBM   Added _flush()
*       07-11-90  SBM   Added _commode, removed execload()
*       07-20-90  SBM   Changes supporting clean -W3 compiles (added _cftoe
*                       and _cftof prototypes)
*       08-01-90  SBM   Moved _cftoe() and _cftof() to new header
*                       <fltintrn.h>, formerly named <struct.h>
*       08-21-90  GJF   Changed prototypes for _amsg_exit() and _NMSG_WRITE().
*       11-29-90  GJF   Added some defs/decls for lowio under Win32.
*       12-04-90  SRW   Added _osfile back for win32.  Changed _osfinfo from
*                       an array of structures to an array of 32-bit handles
*                       (_osfhnd)
*       04-06-91  GJF   Changed _heapinit to _heap_init.
*       08-19-91  JCR   Added _exitflag
*       08-20-91  JCR   C++ and ANSI naming
*       01-05-92  GJF   Added declaration for termination done flag [_WIN32_]
*       01-08-92  GJF   Added prototype for _GetMainArgs.
*       01-18-92  GJF   Added _aexit_rtn.
*       01-22-92  GJF   Fixed definitions of _acmdln and _aexit_rtn for the
*                       of crtdll.dll, crtdll.lib.
*       01-29-92  GJF   Added support for linked-in options equivalent to
*                       commode.obj and setargv.obj (i.e., special declarations
*                       for _commode and _dowildcard).
*       02-14-92  GJF   Replace _nfile with _nhandle for Win32. Also, added
*                       #define-s for _NHANDLE_.
*       03-17-92  GJF   Removed declaration of _tmpoff for Win32.
*       03-30-92  DJM   POSIX support.
*       04-27-92  GJF   Added prototypes for _ValidDrive (in stat.c).
*       05-28-92  GJF   Added prototype for _mtdeletelocks() for Win32.
*       06-02-92  SKS   Move prototype for _pgmptr to <DOS.H>
*       06-02-92  KRS   Added prototype for _woutput().
*       08-06-92  GJF   Function calling type and variable type macros.
*       08-17-92  KRS   Added prototype for _winput().
*       08-21-92  GJF   Merged last two changes above.
*       08-24-92  PBS   Added _dstoffset for posix TZ
*       10-24-92  SKS   Add a fourth parameter to _GetMainArgs: wildcard flag
*                       _GetMainArgs => __GetMainArgs: 2 leading _'s = internal
*       10-24-92  SKS   Remove two unnecessary parameters from _cenvarg()
*       01-21-93  GJF   Removed support for C6-386's _cdecl.
*       03-30-93  GJF   __gmtotime_t supercedes _dtoxtime.
*       04-06-93  SKS   Replace _CRTAPI1/2 with __cdecl, _CRTVAR1 with nothing
*                       Change _ValidDrive to _validdrive
*       04-07-93  SKS   Add _CRTIMP keyword for CRT DLL model
*                       Use link-time aliases for old names, not #define's
*       04-13-93  SKS   Add _mtterm (complement of _mtinit)
*       04-26-93  SKS   _mtinit now returns success (1) or failure (0)
*       05-06-93  SKS   Add _heap_term() - frees up memory upon DLL detach
*       07-21-93  GJF   __loctotime_t supercedes _gmtotime_t.
*       09-15-93  CFW   Added mbc init function prototypes.
*       09-17-93  GJF   Merged NT SDK and Cuda versions, added prototype for
*                       _heap_abort.
*       10-13-93  GJF   Replaced _ALPHA_ with _M_ALPHA.
*       10-21-93  GJF   Changed _NTSDK definition of _commode slightly to
*                       work with dllsuff\crtexe.c.
*       10-22-93  CFW   Test for invalid MB chars using global preset flag.
*       10-26-93  GJF   Added typedef for _PVFV.
*       11-19-93  CFW   Add _wcmdln, wmain, _wsetargv.
*       11-23-93  CFW   Undef GetEnvironmentStrings (TEMPORARY).
*       11-29-93  CFW   Remove GetEnvironmentStrings undef, NT 540 has fix.
*       12-01-93  CFW   Add _wenvptr and protos for wide environ functions.
*       12-07-93  CFW   Add _wcenvarg, _wcapture_argv, and wdospawn protos.
*       01-11-94  GJF   __GetMainArgs() instead of __getmainargs for NT SDK.
*       03-04-94  SKS   Add declarations of _newmode and _dowildcard.
*                       Adjust decl of _[w]getmainargs for 4th parameter.
*       03-25-94  GJF   Added declaration of __[w]initenv
*       03-25-94  GJF   Made declarations of:
*                           _acmdln,    _wcmdln,
*                           _aenvptr,   _wenvptr
*                           _C_Termination_Flag,
*                           _exitflag,
*                           __initenv,  __winitenv,
*                           __invalid_mb_chars
*                           _lastiob,
*                           _old_pfxlen,
*                           _osfhnd[]
*                           _osfile[],
*                           _pipech[],
*                           _tempoff,
*                           _umaskval
*                       conditional on DLL_FOR_WIN32S. Made declaration of
*                       _newmode conditional on DLL_FOR_WIN32S and CRTDLL.
*                       Made declaration of _cflush conditional on CRTDLL.
*                       Defined _commode to be a dereferenced function return
*                       for _DLL. Conditionally included win32s.h.
*       04-14-94  GJF   Added definition for FILE.
*       05-03-94  GJF   Made declarations of _commode, __initenv, __winitenv
*                       _acmdln and _wcmdln also conditional on _M_IX86.
*       05-09-94  CFW   Add __fcntrlcomp, remove DLL_FOR_WIN32S protection
*                       on __invalid_mb_chars.
*       09-06-94  GJF   Added declarations for __app_type, __set_app_type()
*                       and related constants, and __error_mode.
*       09-06-94  CFW   Remove _MBCS_OS switch.
*       12-14-94  SKS   Increase file handle limit for MSVCRT30.DLL
*       12-15-94  XY    merged with mac header
*       12-21-94  CFW   Remove fcntrlcomp & invalid_mb NT 3.1 hacks.
*       12-23-94  GJF   Added prototypes for _fseeki64, __fseeki64_lk,
*                       _ftelli64 and _ftelli64_nolock.
*       12-28-94  JCF   Changed _osfhnd from long to int in _MAC_.
*       01-17-95  BWT   Don't define main/wmain for POSIX
*       02-11-95  CFW   Don't define __argc, __argv, _pgmptr for Mac.
*       02-14-95  GJF   Made __dnames[] and __mnames[] const.
*       02-14-95  CFW   Clean up Mac merge.
*       03-03-95  GJF   Changes to manage streams via __piob[], rather than
*                       _iob[].
*       03-29-95  BWT   Define _commode properly for RISC _DLL CRTEXE case.
*       03-29-95  CFW   Add error message to internal headers.
*       04-06-95  CFW   Add parameter to _setenvp().
*       05-08-95  CFW   Official ANSI C++ new handler added.
*       06-15-95  GJF   Revised for ioinfo arrays.
*       07-04-95  GJF   Removed additional parameter from _setenvp().
*       06-23-95  CFW   ANSI new handler removed from build.
*       07-26-95  GJF   Added safe versions of ioinfo access macros.
*       09-25-95  GJF   Added parameter to __loctotime_t.
*       12-08-95  SKS   Add __initconin()/__initconout() for non-MAC platforms.
*       12-14-95  JWM   Add "#pragma once".
*       04-12-96  SKS   __badioinfo and __pioinfo must be exported for the
*                       Old Iostreams DLLs (msvcirt.dll and msvcirtd.dll).
*       04-22-96  GJF   Return type of _heap_init changed.
*       05-10-96  SKS   Add definition of _CRTIMP1 -- needed by mtlock/mtunlock
*       08-01-96  RDK   For PMac, add extern for _osfileflags for extra byte of
*                       file flags.
*       08-22-96  SKS   Add definition of _CRTIMP2
*       02-03-97  GJF   Cleaned out obsolete support for Win32s, _CRTAPI* and 
*                       _NTSDK. Replaced defined(_M_MPPC) || defined(_M_M68K)
*                       with defined(_MAC). Also, detab-ed.
*       04-16-97  GJF   Restored the macros for _[w]initenv in the DLL model
*                       because they prove useful in something else.
*       07-23-97  GJF   _heap_init changed slightly.
*       02-07-98  GJF   Changes for Win64: use intptr_t where appropriate, 
*                       and made time_t __int64.
*       05-04-98  GJF   Added __time64_t support.
*       12-15-98  GJF   Changes for 64-bit size_t.
*       05-17-99  PML   Remove all Macintosh support.
*       05-28-99  GJF   Changed prototype for __crt[w]setenv.
*       06-01-99  PML   Minor cleanup for 5/3/99 Plauger STL drop.
*       10-06-99  PML   Add _W64 modifier to types which are 32 bits in Win32,
*                       64 bits in Win64.
*       10-14-99  PML   Add __crtInitCritSecAndSpinCount and _CRT_SPINCOUNT.
*       11-03-99  PML   Add va_list definition for _M_CEE.
*       11-12-99  PML   Wrap __time64_t in its own #ifndef.
*       03-06-00  PML   Add __crtExitProcess.
*       09-07-00  PML   Remove va_list definition for _M_CEE (vs7#159777)
*       02-20-01  PML   vs7#172586 Avoid _RT_LOCK by preallocating all locks
*                       that will be required, and returning failure back on
*                       inability to allocate a lock.
*       03-19-01  BWT   Stop calling through functions on X86 to get environment
*       03-27-01  PML   Return success/failure code from several startup
*                       routines instead of calling _amsg_exit (vs7#231220)
*       07-15-01  PML   Remove all ALPHA, MIPS, and PPC code
*       03-07-02  PML   Added DEFAULT_SECURITY_COOKIE
*       05-09-02  MSL   Changed _crt[w]setenv to allow more explicit
*                       failure and memory ownership indication 
*                       VS7 527539
*       06-14-02  MSL   _cinit now takes an int to indicate FP initialisation
*                       VS7 550455
*       11-18-02  GB    Added __crtdll_callstaticterminators which is used in
*                       dllmain of CRTDLL.
*       12-18-02  EAN   changed time_t to use a 64-bit int if available.
*                       VSWhidbey 6851
*       03-06-03  SSM   Move IS_LEAP_YEAR to this file. Used for validation
*                       in multiple files.
*                       Added macros for validation.
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*       10-23-03  SJ    Secure Version for scanf family which takes an extra 
*                       size parameter from the var arg list.
*       10-28-03  SJ    Secure CRT - printf_s : positional args & validations
*       11-13-03  SJ    File Handle checks made consistent - VSW#199372
*       12-02-03  SJ    Reroute Unicode I/O
*       01-30-04  SJ    VSW#228233 - splitting printf_s into 2 functions.
*       02-03-04  SJ    /GS Improvements in the CRT - VSW#222320
*       03-13-04  MSL   Added new non-returning validation macros to avoid 
*                       forcing jumps out of __try blocks
*       03-20-04  SSM   Move _fseeki64 and _ftelli64 and related functions
*                       to public header - stdio.h.
*       04-07-04  MSL   Double slash removal
*       04-08-04  AC    Added STATUS_FATAL_APP_EXIT, used in abort.
*                       Added a lookup function for sigabrt.
*                       VSW#172535
*       07-19-04  AC    Added _SAFECRT_IMPL for safecrt.lib
*       08-05-04  AC    Added some helper macros for string padding and mbs function redirection.
*       08-19-04  AJS   Changed values of DEFAULT_SECURITY_COOKIE to match Windows. VSWhidbey:237200.
*       09-03-04  AC    Added _tsopen_helper functions.
*       09-15-04  PL    VSW#250680: Added unicode bit to ioinfo, check the bit in 
*                       _VALIDATE_STREAM_ANSI_*.
*       09-23-04  DJT   Added _encode_pointer and related functions.
*       09-24-04  MSL   Single evaluation in validation macros
*       10-08-04  ESC   Protect against -Zp with pragma pack
*       10-18-04  MSL   Managed new handler
*       11-02-04  AC    Readd _invoke_watson
*                       VSW#320460
*       11-06-04  AC    Add _SECURECRT_FILL_BUFFER_PATTERN
*                       VSW#2459
*       11-07-04  MSL   Cleanup to avoid pure data imports and enable compiler warning
*       11-11-04  MR    Add support for special handles in C# - VSW#272079
*       11-18-04  ATC   SAL Annotations for CRT Headers
*       01-12-05  DJT   Use OS-encoded global function pointers
*       01-14-05  AC    Fix SAL annotations (using prefast espx plug-in)
*       03-08-05  AC    Make locking for native initialisation fiber-safe
*       03-11-05  PML   Move DEFAULT_SECURITY_COOKIE to gs_cookie.c and
*                       gs_support.c, remove STATUS_STACK_BUFFER_OVERRUN (VSW#448466).
*       03-24-05  AC    Added encode/decode function which do not take the loader lock
*                       VSW#473724
*       03-23-05  MSL   _P removal - not needed
*                       New deprecation warning with function name
*                       Packing fix
*       04-05-05  AC    exit() needs to call CorExitProcess before shutting down native
*                       VSW#467254
*       04-01-05  MSL   Added _calloca function to support integer overflow fixes
*       04-14-05  MSL   Intellisense locale name cleanup
*       04-20-05  MSL   CLS compliance
*       05-06-05  AC    Add _invalid_parameter_noinfo, to have better code generation with Secure SCL
*       05-24-05  PML   Make sure unencoded global func-ptrs are in .rdata
*                       (vsw#191114)
*       06-15-05  AEI   Added macro _VALIDATE_RETURN_ERRCODE_NOEXC, which returns the error code
*                       if the expression is not validated, but without making an assertion and without
*                       calling _INVALID_PARAMETER
*                       VSW#503686
*       03-03-06  SSM   Moved URT8 related and Common file related macros to header
*                       Added fields to ioinfo to store file pos that corresponds to 
*                       beginning of buffer for buffered io
*       03-14-06  AEI   VSW#584673: __native_startup_lock has to be void * volatile 
*                       instead of volatile void *
*       08-10-06  ATC   DevDivBugs#12336: unexpected behavior for locale with wcout.
*                       Adding an extra 2 character buffer to each ioinfo object
*       02-06-07  AC    DDB#63283: cache value of fileno instead of calling it lots of times
*       04-20-07  GM    Removed __Detect_msvcrt_and_setup().
*       06-01-07  AEI   Defined CRT_DISABLE_PUSH and CRT_POP macros as clones to PREFAST_DISABLE_PUSH and PREFAST_POP
*                       from prefast.h. Crt user build should not be aware of prefast but the internal build needs the macros.
*
****/

#pragma once

#ifndef _INC_INTERNAL
#define _INC_INTERNAL

#include <crtdefs.h>

#ifdef  __cplusplus
extern "C" {
#endif

#include <cruntime.h>
#include <limits.h>

/*
 * Conditionally include windows.h to pick up the definition of 
 * CRITICAL_SECTION.
 */
#include <windows.h>

#include <mtdll.h>

#pragma pack(push,_CRT_PACKING)

/* Define function types used in several startup sources */

typedef void (__cdecl *_PVFV)(void);
typedef int  (__cdecl *_PIFV)(void);
typedef void (__cdecl *_PVFI)(int);

#if defined(_M_CEE) /*IFSTRIP=IGN*/
typedef const void* (__clrcall *_PVFVM)(void);
typedef int (__clrcall *_PIFVM)(void);
typedef void (__clrcall *_CPVFV)(void);
#endif

#if defined(_M_CEE_PURE) || (defined(_DLL) && defined(_M_IX86))
/* Retained for compatibility with VC++ 5.0 and earlier versions */
_CRTIMP int * __cdecl __p__commode(void);
#endif
#if     defined(SPECIAL_CRTEXE) && defined(_DLL)
        extern int _commode;
#else
#ifndef _M_CEE_PURE
_CRTIMP extern int _commode;
#else
#define _commode (*__p___commode())
#endif
#endif  /* defined(_DLL) && defined(SPECIAL_CRTEXE) */

#define __IOINFO_TM_ANSI    0   /* Regular Text */
#define __IOINFO_TM_UTF8    1   /* UTF8 Encoded */
#define __IOINFO_TM_UTF16LE 2   /* UTF16 Little Endian Encoded */

#define LF 10           /* line feed */
#define CR 13           /* carriage return */
#define CTRLZ 26        /* ctrl-z means eof for text */

extern char _lookuptrailbytes[256];

/* Most significant Bit */
#define _msbit(c) ((c) & 0x80)

/* Independent byte has most significant bit set to 0 */
#define  _utf8_is_independent(c)    (_msbit(c) == 0)

/* Any leadbyte will have the patterns 11000xxx 11100xxx or 11110xxx */
#define  _utf8_is_leadbyte(c)       (_lookuptrailbytes[(unsigned char)c] != 0)

/* Get no of trailing bytes from the lookup table */
#define  _utf8_no_of_trailbytes(c)  _lookuptrailbytes[(unsigned char)c]

/*
 * Control structure for lowio file handles
 */
typedef struct {
        intptr_t osfhnd;    /* underlying OS file HANDLE */
        char osfile;        /* attributes of file (e.g., open in text mode?) */
        char pipech;        /* one char buffer for handles opened on pipes */
        int lockinitflag;
        CRITICAL_SECTION lock;
#ifndef _SAFECRT_IMPL
        /* Not used in the safecrt downlevel. We do not define them, so we cannot use them accidentally */
        char textmode : 7;     /* __IOINFO_TM_ANSI or __IOINFO_TM_UTF8 or __IOINFO_TM_UTF16LE */
        char unicode : 1;      /* Was the file opened as unicode? */
        char pipech2[2];       /* 2 more peak ahead chars for UNICODE mode */
        __int64 startpos;      /* File position that matches buffer start */
        BOOL utf8translations; /* Buffer contains translations other than CRLF*/
        char dbcsBuffer;       /* Buffer for the lead byte of dbcs when converting from dbcs to unicode */
        BOOL dbcsBufferUsed;   /* Bool for the lead byte buffer is used or not */
#endif
    }   ioinfo;

/*
 * Definition of IOINFO_L2E, the log base 2 of the number of elements in each
 * array of ioinfo structs.
 */
#define IOINFO_L2E          5

/*
 * Definition of IOINFO_ARRAY_ELTS, the number of elements in ioinfo array
 */
#define IOINFO_ARRAY_ELTS   (1 << IOINFO_L2E)

/*
 * Definition of IOINFO_ARRAYS, maximum number of supported ioinfo arrays.
 */
#define IOINFO_ARRAYS       64

#define _NHANDLE_           (IOINFO_ARRAYS * IOINFO_ARRAY_ELTS)

#define _TZ_STRINGS_SIZE    64

#define STDIO_HANDLES_COUNT       3
/*
 * Access macros for getting at an ioinfo struct and its fields from a
 * file handle
 */
#define _pioinfo(i) ( __pioinfo[(i) >> IOINFO_L2E] + ((i) & (IOINFO_ARRAY_ELTS - 1)) )

#define _osfhnd(i)  ( _pioinfo(i)->osfhnd )

#define _osfile(i)  ( _pioinfo(i)->osfile )

#define _pipech(i)  ( _pioinfo(i)->pipech )

#define _pipech2(i)  ( _pioinfo(i)->pipech2 )

#define _textmode(i) ( _pioinfo(i)->textmode )

#define _tm_unicode(i) ( _pioinfo(i)->unicode )

#define _startpos(i) ( _pioinfo(i)->startpos )

#define _utf8translations(i) ( _pioinfo(i)->utf8translations )

#define _dbcsBuffer(i) ( _pioinfo(i)->dbcsBuffer )

#define _dbcsBufferUsed(i) ( _pioinfo(i)->dbcsBufferUsed )

/*
 * Safer versions of the above macros. Currently, only _osfile_safe is 
 * used.
 */
#define _pioinfo_safe(i)    ( (((i) != -1) && ((i) != -2)) ? _pioinfo(i) : &__badioinfo )

#define _osfhnd_safe(i)     ( _pioinfo_safe(i)->osfhnd )

#define _osfile_safe(i)     ( _pioinfo_safe(i)->osfile )

#define _pipech_safe(i)     ( _pioinfo_safe(i)->pipech )

#define _pipech2_safe(i)    ( _pioinfo_safe(i)->pipech2 )

#ifdef _SAFECRT_IMPL
/* safecrt does not have support for textmode, so we always return __IOINFO_TM_ANSI */
#define _textmode_safe(i)   __IOINFO_TM_ANSI
#define _tm_unicode_safe(i)  0
#define _startpos_safe(i)   ( 0 )
#define _utf8translations_safe(i)  ( FALSE )
#else
#define _textmode_safe(i)   ( _pioinfo_safe(i)->textmode )
#define _tm_unicode_safe(i) ( _pioinfo_safe(i)->unicode )
#define _startpos_safe(i)   ( _pioinfo_safe(i)->startpos )
#define _utf8translations_safe(i)  ( _pioinfo_safe(i)->utf8translations )
#endif

#ifndef _M_CEE_PURE
#ifdef _SAFECRT_IMPL
/* We need to get this from the downlevel DLL, even when we build safecrt.lib */
extern __declspec(dllimport) ioinfo __badioinfo;
extern __declspec(dllimport) ioinfo * __pioinfo[];
#else
/*
 * Special, static ioinfo structure used only for more graceful handling
 * of a C file handle value of -1 (results from common errors at the stdio 
 * level). 
 */
extern _CRTIMP ioinfo __badioinfo;

/*
 * Array of arrays of control structures for lowio files.
 */
extern _CRTIMP ioinfo * __pioinfo[];

#endif
#endif

/*
 * Current number of allocated ioinfo structures (_NHANDLE_ is the upper
 * limit).
 */
extern int _nhandle;

int __cdecl _alloc_osfhnd(void);
int __cdecl _free_osfhnd(int);
int __cdecl _set_osfhnd(int, intptr_t);

/* 
    fileno for stdout, stdin & stderr when there is no console
*/
#define _NO_CONSOLE_FILENO (intptr_t)-2

#ifdef  _POSIX_
extern long _dstoffset;
#endif  /* _POSIX_ */

extern const char __dnames[];
extern const char __mnames[];

extern int _days[];
extern int _lpdays[];

extern __time32_t __cdecl __loctotime32_t(int, int, int, int, int, int, int);
extern __time64_t __cdecl __loctotime64_t(int, int, int, int, int, int, int);

#ifdef  _TM_DEFINED
extern int __cdecl _isindst(_In_ struct tm * _Time);
#endif

extern void __cdecl __tzset(void);

extern int __cdecl _validdrive(unsigned);

/*
 * If we are only interested in years between 1901 and 2099, we could use this:
 *
 *      #define IS_LEAP_YEAR(y)  (y % 4 == 0)
 */

#define IS_LEAP_YEAR(y)  (((y) % 4 == 0 && (y) % 100 != 0) || (y) % 400 == 0)

/*
 *      get the buffer used by gmtime
 */
struct tm * __cdecl __getgmtimebuf ();

/*
 * This variable is in the C start-up; the length must be kept synchronized
 * It is used by the *cenvarg.c modules
 */

extern char _acfinfo[]; /* "_C_FILE_INFO=" */

#define CFI_LENGTH  12  /* "_C_FILE_INFO" is 12 bytes long */

/*
 * latest instruction set available for use
 */

#ifdef _WIN32_WCE
#include "..\..\langapi\include\isa_availability.h"
#else
#include <isa_availability.h>
#endif // _WIN32_WCE

extern int __isa_available;

#if defined(_M_IX86) || defined(_M_X64)
extern int __isa_enabled;
extern int __favor;
#endif

/*
 * stdio internals
 */
#ifndef _FILE_DEFINED
struct _iobuf {
        char *_ptr;
        int   _cnt;
        char *_base;
        int   _flag;
        int   _file;
        int   _charbuf;
        int   _bufsiz;
        char *_tmpfname;
        };
typedef struct _iobuf FILE;
#define _FILE_DEFINED
#endif  /* _FILE_DEFINED */

//CE's windows.h define __WINDOWS__ and it is different than NT's _WINDOWS_ define
#if !defined(_FILEX_DEFINED) && defined(__WINDOWS__)
/*
 * Variation of FILE type used for the dynamically allocated portion of
 * __piob[]. For single thread, _FILEX is the same as FILE. For multithread
 * models, _FILEX has two fields: the FILE struct and the CRITICAL_SECTION
 * struct used to serialize access to the FILE.
 */
  
typedef struct {  
        FILE f;  
        CRITICAL_SECTION lock;  
        }   _FILEX;  

#define _FILEX_DEFINED
#endif  /* _FILEX_DEFINED */

/*
 * Number of entries supported in the array pointed to by __piob[]. That is,
 * the number of stdio-level files which may be open simultaneously. This
 * is normally set to _NSTREAM_ by the stdio initialization code.
 */
extern int _nstream;
#ifdef _WIN32_WCE 

// Special value interpreted by output functions to redirect to OutputDebugStringW
#define _CRT_DBGOUT_HANDLE ((HANDLE)(-2))
#endif // _WIN32_WCE

#ifdef _WIN32_WCE
// for supporting redirect to COM
// As hCOMPort code is moved to coremain.lib and cannot be accessed any more,
// Hence removing this  hCOMPort extern declaration 
// extern HANDLE hCOMPort;

// Implemented a  new function GetCOMPortHandle() to get COM Port handle 
HANDLE __cdecl GetCOMPortHandle(void);

typedef BOOL (*tGetCommState) (HANDLE hFile, LPDCB lpDCB);
typedef BOOL (*tSetCommState) (HANDLE hFile, LPDCB lpDCB);
typedef BOOL (*tSetCommTimeouts) (HANDLE hFile, LPCOMMTIMEOUTS lpCommTimeouts);
typedef BOOL (*tGetCommTimeouts) (HANDLE hFile, LPCOMMTIMEOUTS lpCommTimeouts);

extern tGetCommState g_pfnGetCommState;
extern tSetCommState g_pfnSetCommState;
extern tSetCommTimeouts g_pfnSetCommTimeouts;
extern tGetCommTimeouts g_pfnGetCommTimeouts;

typedef HANDLE (*tActivateDeviceEx)(LPCWSTR lpszDevKey, LPCVOID lpRegEnts, DWORD cRegEnts, LPVOID lpvParam);
typedef BOOL (*tDeactivateDevice)(HANDLE hDevice);
typedef BOOL (*tGetDeviceInformationByDeviceHandle)(HANDLE hDevice, PDEVMGR_DEVICE_INFORMATION pdi);

// Control whether CRT APIs use pre-CE6 input validation and terminate
// on invalid parameters
extern BOOL g_fCrtLegacyInputValidation;

// Activate STDIO
HANDLE __cdecl OpenStdConsole(int fh, PBOOL is_dev);
#endif // _WIN32_WCE


/*
 * Pointer to the array of pointers to FILE/_FILEX structures that are used
 * to manage stdio-level files.
 */
//#ifdef _WIN32_WCE
//extern FILEX** __piob;
//#else
extern void **__piob;
//#endif // _WIN32_WCE

#ifdef _WIN32_WCE
// For componentization
//typedef int (__cdecl *PFNFILBUF)(FILEX *str, int fWide);
//typedef int (__cdecl *PFNFLSBUF)(int ch, FILEX *str, int fWide);
//extern PFNFILBUF _pfnfilbuf2;
//extern PFNFLSBUF _pfnflsbuf2;

// Cleanup of startup & componentize
//extern int fStdioInited;
//int __cdecl InitStdio(void);
//#define CheckStdioInit() (fStdioInited ? TRUE : InitStdio())
#endif // _WIN32_WCE

FILE * __cdecl _getstream(void);
#ifdef  _POSIX_
FILE * __cdecl _openfile(_In_z_ const char * _Filename, _In_z_ const char * _Mode, _Out_ FILE * _File);
#else
FILE * __cdecl _openfile(_In_z_ const char * _Filename, _In_z_ const char * _Mode, _In_ int _ShFlag, _Out_ FILE * _File);
#endif
FILE * __cdecl _wopenfile(_In_z_ const wchar_t * _Filename, _In_z_ const wchar_t * _Mode, _In_ int _ShFlag, _Out_ FILE * _File);
void __cdecl _getbuf(_Out_ FILE * _File);
int __cdecl _filwbuf (_Inout_ FILE * _File);
int __cdecl _flswbuf(_In_ int _Ch, _Inout_ FILE * _File);
void __cdecl _freebuf(_Inout_ FILE * _File);
int __cdecl _stbuf(_Inout_ FILE * _File);
void __cdecl _ftbuf(int _Flag, _Inout_ FILE * _File);

#ifdef _SAFECRT_IMPL

int __cdecl _output(_Inout_ FILE * _File, _In_z_ _Printf_format_string_ const char *_Format, va_list _ArgList);
int __cdecl _woutput(_Inout_ FILE * _File, _In_z_ _Printf_format_string_ const wchar_t *_Format, va_list _ArgList);
int __cdecl _output_s(_Inout_ FILE * _File, _In_z_ _Printf_format_string_ const char *_Format, va_list _ArgList);
int __cdecl _output_p(_Inout_ FILE * _File, _In_z_ _Printf_format_string_ const char *_Format, va_list _ArgList);
int __cdecl _woutput_s(_Inout_ FILE * _File, _In_z_ _Printf_format_string_ const wchar_t *_Format, va_list _ArgList);
int __cdecl _woutput_p(_Inout_ FILE * _File, _In_z_ _Printf_format_string_ const wchar_t *_Format, va_list _ArgList);
typedef int (*OUTPUTFN)(FILE *, const char *, va_list);
typedef int (*WOUTPUTFN)(FILE *, const wchar_t *, va_list);

#else

int __cdecl _output_l(_Inout_ FILE * _File, _In_z_ _Printf_format_string_params_(2) const char *_Format, _In_opt_ _locale_t _Locale, va_list _ArgList);
int __cdecl _woutput_l(_Inout_ FILE * _File, _In_z_ _Printf_format_string_params_(2) const wchar_t *_Format, _In_opt_ _locale_t _Locale, va_list _ArgList);
int __cdecl _output_s_l(_Inout_ FILE * _File, _In_z_ _Printf_format_string_params_(2) const char *_Format, _In_opt_ _locale_t _Locale, va_list _ArgList);
int __cdecl _output_p_l(_Inout_ FILE * _File, _In_z_ _Printf_format_string_params_(2) const char *_Format, _In_opt_ _locale_t _Locale, va_list _ArgList);
int __cdecl _woutput_s_l(_Inout_ FILE * _File, _In_z_ _Printf_format_string_params_(2) const wchar_t *_Format, _In_opt_ _locale_t _Locale, va_list _ArgList);
int __cdecl _woutput_p_l(_Inout_ FILE * _File, _In_z_ _Printf_format_string_params_(2) const wchar_t *_Format, _In_opt_ _locale_t _Locale, va_list _ArgList);
typedef int (*OUTPUTFN)(_Inout_ FILE * _File, const char *, _locale_t, va_list);
typedef int (*WOUTPUTFN)(_Inout_ FILE * _File, const wchar_t *, _locale_t, va_list);

#endif

#ifdef _SAFECRT_IMPL

int __cdecl _input(_In_ FILE * _File, _In_z_ _Scanf_format_string_ const unsigned char * _Format, va_list _ArgList);
int __cdecl _winput(_In_ FILE * _File, _In_z_ _Scanf_format_string_ const wchar_t * _Format, va_list _ArgList);
int __cdecl _input_s(_In_ FILE * _File, _In_z_ _Scanf_s_format_string_ const unsigned char * _Format, va_list _ArgList);
int __cdecl _winput_s(_In_ FILE * _File, _In_z_ _Scanf_s_format_string_ const wchar_t * _Format, va_list _ArgList);
typedef int (*INPUTFN)(FILE *, const unsigned char *, va_list);
typedef int (*WINPUTFN)(FILE *, const wchar_t *, va_list);

#else

int __cdecl _input_l(_Inout_ FILE * _File, _In_z_ _Scanf_format_string_params_(2) const unsigned char *, _In_opt_ _locale_t _Locale, va_list _ArgList);
int __cdecl _winput_l(_Inout_ FILE * _File, _In_z_ _Scanf_format_string_params_(2) const wchar_t *, _In_opt_ _locale_t _Locale, va_list _ArgList);
int __cdecl _input_s_l(_Inout_ FILE * _File, _In_z_ _Scanf_s_format_string_params_(2) const unsigned char *, _In_opt_ _locale_t _Locale, va_list _ArgList);
int __cdecl _winput_s_l(_Inout_ FILE * _File, _In_z_ _Scanf_s_format_string_params_(2) const wchar_t *, _In_opt_ _locale_t _Locale, va_list _ArgList);
typedef int (*INPUTFN)(FILE *, const unsigned char *, _locale_t, va_list);
typedef int (*WINPUTFN)(FILE *, const wchar_t *, _locale_t, va_list);

#ifdef _UNICODE
#define TINPUTFN WINPUTFN
#else
#define TINPUTFN INPUTFN
#endif

#endif

int __cdecl _flush(_Inout_ FILE * _File);
void __cdecl _endstdio(void);

errno_t __cdecl _sopen_helper(_In_z_ const char * _Filename, 
    _In_ int _OFlag, _In_ int _ShFlag, _In_ int _PMode, 
    _Out_ int * _PFileHandle, int _BSecure);
errno_t __cdecl _wsopen_helper(_In_z_ const wchar_t * _Filename, 
    _In_ int _OFlag, _In_ int _ShFlag, _In_ int _PMode,
    _Out_ int * _PFileHandle, int _BSecure);
errno_t __cdecl _sopen_nolock (_Out_ int * UnlockFlag, 
    _Out_ int * _FileHandle, _In_z_ const char * _Filename, 
    _In_ int _OpenFlag, _In_ int _ShareFlag, 
    _In_ int _PermissionFlag, _In_ int _SecureFlag);
errno_t __cdecl _wsopen_nolock (_Out_ int * UnlockFlag, 
    _Out_ int * _FileHandle, _In_z_ const wchar_t * _Filename, 
    _In_ int _OpenFlag, _In_ int _ShareFlag, 
    _In_ int _PermissionFlag, _In_ int _SecureFlag);


#ifndef CRTDLL
extern int _cflush;
#endif  /* CRTDLL */

extern unsigned int _tempoff;

extern unsigned int _old_pfxlen;

extern int _umaskval;       /* the umask value */

extern char _pipech[];      /* pipe lookahead */

extern char _exitflag;      /* callable termination flag */

extern int _C_Termination_Done; /* termination done flag */

char * __cdecl _getpath(_In_z_ const char * _Src, _Out_writes_z_(_SizeInChars) char * _Dst, _In_ size_t _SizeInChars);
wchar_t * __cdecl _wgetpath(_In_z_ const wchar_t * _Src, _Out_writes_z_(_SizeInWords) wchar_t * _Dst, _In_ size_t _SizeInWords);

extern int _dowildcard;     /* flag to enable argv[] wildcard expansion */

#ifndef _PNH_DEFINED
typedef int (__cdecl * _PNH)( size_t );
#define _PNH_DEFINED
#endif

#if defined(_M_CEE) /*IFSTRIP=IGN*/
#ifndef __MPNH_DEFINED
typedef int (__clrcall * __MPNH)( size_t );
#define __MPNH_DEFINED
#endif
#endif

/* calls the currently installed new handler */
int __cdecl _callnewh(_In_ size_t _Size);

extern int _newmode;    /* malloc new() handler mode */

/* pointer to initial environment block that is passed to [w]main */
#ifndef _M_CEE_PURE
extern _CRTIMP wchar_t **__winitenv;
extern _CRTIMP char **__initenv;
#endif

/* _calloca helper */
#define _calloca(count, size)  ((count<=0 || size<=0 || ((((size_t)_HEAP_MAXREQ) / ((size_t)count)) < ((size_t)size)))? NULL : _malloca(count * size))

/* startup set values */
extern char *_aenvptr;      /* environment ptr */
extern wchar_t *_wenvptr;   /* wide environment ptr */

/* command line */

#if defined(_DLL)
_CRTIMP char ** __cdecl __p__acmdln(void);
_CRTIMP wchar_t ** __cdecl __p__wcmdln(void);
#endif
#ifndef _M_CEE_PURE
_CRTIMP extern char *_acmdln;
_CRTIMP extern wchar_t *_wcmdln;
#else
#define _acmdln (*__p__acmdln())
#define _wcmdln (*__p__wcmdln())
#endif

/*
 * prototypes for internal startup functions
 */
int __cdecl _cwild(void);           /* wild.c */
int __cdecl _wcwild(void);          /* wwild.c */
int  __cdecl _mtinit(void);         /* tidtable.c */
void __cdecl _mtterm(void);         /* tidtable.c */
int  __cdecl _mtinitlocks(void);    /* mlock.c */
void __cdecl _mtdeletelocks(void);  /* mlock.c */
int  __cdecl _mtinitlocknum(_In_ int);   /* mlock.c */

#define _CRT_SPINCOUNT  4000

#ifdef _WIN32_WCE // re-map dekstop stuff as CE implementations   
#define FLS_OUT_OF_INDEXES TLS_OUT_OF_INDEXES
#define FlsGetValue TlsGetValue
#define FlsSetValue TlsSetValue
#define FlsFree TlsFree
#define PCRITICAL_SECTION LPCRITICAL_SECTION 
#endif 
/*
 * C source build only!!!!
 *
 * more prototypes for internal startup functions
 */
void __cdecl _amsg_exit(int);           /* crt0.c */
#ifdef _WIN32_WCE
DECLSPEC_NORETURN
VOID
__crt_ExitProcess(
    DWORD uExitCode
    );
#define __crtExitProcess __crt_ExitProcess

VOID
__crtRaiseException(
    DWORD         dwExceptionCode,
    DWORD         dwExceptionFlags,
    DWORD         nNumberOfArguments,
    CONST DWORD * lpArguments
    );

BOOL
__crtIsDebuggerPresent(void);

#else
void __cdecl __crtExitProcess(int);     /* crt0dat.c */
#endif 
void __cdecl __crtCorExitProcess(int);  /* crt0dat.c */
void __cdecl __crtdll_callstaticterminators(void); /* crt0dat.c */

/*  
_cinit now allows the caller to suppress floating point precision init
This allows the DLLs that use the CRT to not initialise FP precision,
allowing the EXE's setting to persist even when a DLL is loaded
*/
int  __cdecl _cinit(int /* initFloatingPrecision */);   /* crt0dat.c */
void __cdecl __doinits(void);           /* astart.asm */
void __cdecl __doterms(void);           /* astart.asm */
void __cdecl __dopreterms(void);        /* astart.asm */
void __cdecl _FF_MSGBANNER(void);
void __cdecl _fpmath(int /*initPrecision*/);
void __cdecl _fptrap(void);             /* crt0fp.c */
void __cdecl __freeCrtMemory();         /* crt0dat.c */
int  __cdecl _heap_init(void);
void __cdecl _heap_term(void);
void __cdecl _heap_abort(void);
void __cdecl __initconin(void);         /* initcon.c */
void __cdecl __initconout(void);        /* initcon.c */
void  __cdecl _ioinit0(void);             /* crt0.c, crtlib.c */
int  __cdecl _ioinit(void);             /* crt0.c, crtlib.c */
void __cdecl _ioterm(void);             /* crt0.c, crtlib.c */
const wchar_t * __cdecl _GET_RTERRMSG(int);
void __cdecl _NMSG_WRITE(int);
int  __CRTDECL _setargv(void);            /* setargv.c, stdargv.c */
int  __CRTDECL __setargv(void);           /* stdargv.c */
int  __CRTDECL _wsetargv(void);           /* wsetargv.c, wstdargv.c */
int  __CRTDECL __wsetargv(void);          /* wstdargv.c */
int  __cdecl _setenvp(void);            /* stdenvp.c */
int  __cdecl _wsetenvp(void);           /* wstdenvp.c */
void __cdecl __setmbctable(unsigned int);   /* mbctype.c */

#ifdef  _MBCS
int  __cdecl __initmbctable(void);      /* mbctype.c */
#endif

#ifndef _POSIX_
#ifndef _MANAGED_MAIN
int __CRTDECL main(_In_ int _Argc, _In_reads_(_Argc) _Pre_z_ char ** _Argv, _In_z_ char ** _Env);
int __CRTDECL wmain(_In_ int _Argc, _In_reads_(_Argc) _Pre_z_ wchar_t ** _Argv, _In_z_ wchar_t ** _Env);
#endif
#endif

/* helper functions for wide/multibyte environment conversion */
int __cdecl __mbtow_environ (void);
int __cdecl __wtomb_environ (void);

/* These two functions take a char ** for the environment option
   At some point during their execution, they take ownership of the
   memory block passed in using option. At this point, they 
   NULL out the incoming char * / wchar_t * to ensure there is no
   double-free
*/
int __cdecl __crtsetenv (_Inout_ _Deref_prepost_opt_valid_ char ** _POption, _In_ const int _Primary);
int __cdecl __crtwsetenv (_Inout_ _Deref_prepost_opt_valid_ wchar_t ** _POption, _In_ const int _Primary);

#if     defined(_DLL) || defined(CRTDLL)

#ifndef _STARTUP_INFO_DEFINED
typedef struct
{
        int newmode;
} _startupinfo;
#define _STARTUP_INFO_DEFINED
#endif  /* _STARTUP_INFO_DEFINED */

_CRTIMP int __cdecl __getmainargs(_Out_ int * _Argc, _Outptr_result_buffer_(*_Argc) char *** _Argv, 
                                  _Outptr_result_maybenull_ char *** _Env, _In_ int _DoWildCard,
                                  _In_ _startupinfo * _StartInfo);

_CRTIMP int __cdecl __wgetmainargs(_Out_ int * _Argc, _Outptr_result_buffer_(*_Argc) wchar_t *** _Argv,
                                   _Outptr_result_maybenull_ wchar_t *** _Env, _In_ int _DoWildCard,
                                   _In_ _startupinfo * _StartInfo);

#endif  /* defined(_DLL) || defined(CRTDLL) */

/*
 * Prototype, variables and constants which determine how error messages are
 * written out.
 */
#define _UNKNOWN_APP    0
#define _CONSOLE_APP    1
#define _GUI_APP        2

extern int __app_type;

#if !defined(_M_CEE_PURE)

typedef enum {
    __uninitialized,
    __initializing,
    __initialized
} __enative_startup_state;

extern volatile __enative_startup_state __native_startup_state;
extern void * volatile __native_startup_lock;

#define __NO_REASON UINT_MAX
extern volatile unsigned int __native_dllmain_reason;

#if defined(__cplusplus)

#pragma warning(push)
#pragma warning(disable: 4483)
#define _NATIVE_STARTUP_NAMESPACE  __identifier("<CrtImplementationDetails>")

namespace _NATIVE_STARTUP_NAMESPACE
{
    class NativeDll
    {
    private:
        static const unsigned int ProcessDetach   = 0;
        static const unsigned int ProcessAttach   = 1;
        static const unsigned int ThreadAttach    = 2;
        static const unsigned int ThreadDetach    = 3;
        static const unsigned int ProcessVerifier = 4;

    public:

        inline static bool IsInDllMain()
        {
            return (__native_dllmain_reason != __NO_REASON);
        }

        inline static bool IsInProcessAttach()
        {
            return (__native_dllmain_reason == ProcessAttach);
        }

        inline static bool IsInProcessDetach()
        {
            return (__native_dllmain_reason == ProcessDetach);
        }

        inline static bool IsSafeForManagedCode()
        {
            if (!IsInDllMain())
            {
                return true;
            }

            return !IsInProcessAttach() && !IsInProcessDetach();
        }
    };
}
#pragma warning(pop)

#endif

#endif

extern int __error_mode;

_CRTIMP void __cdecl __set_app_type(int);

/*
 * C source build only!!!!
 *
 * map Win32 errors into Xenix errno values -- for modules written in C
 */
_CRTIMP void __cdecl _dosmaperr(unsigned long);
extern int __cdecl _get_errno_from_oserr(unsigned long);

/*
 * internal routines used by the exec/spawn functions
 */

extern intptr_t __cdecl _dospawn(_In_ int _Mode, _In_opt_z_ const char * _Name, _Inout_z_ char * _Cmd, _In_opt_z_ char * _Env);
extern intptr_t __cdecl _wdospawn(_In_ int _Mode, _In_opt_z_ const wchar_t * _Name, _Inout_z_ wchar_t * _Cmd, _In_opt_z_ wchar_t * _Env);
extern int __cdecl _cenvarg(_In_z_ const char * const * _Argv, _In_opt_z_ const char * const * _Env,
        _Outptr_result_maybenull_ char ** _ArgBlk, _Outptr_result_maybenull_ char ** _EnvBlk, _In_z_ const char *_Name);
extern int __cdecl _wcenvarg(_In_z_ const wchar_t * const * _Argv, _In_opt_z_ const wchar_t * const * _Env,
        _Outptr_result_maybenull_ wchar_t ** _ArgBlk, _Outptr_result_maybenull_ wchar_t ** _EnvBlk, _In_z_ const wchar_t * _Name);
#ifndef _M_IX86
extern char ** _capture_argv(_In_ va_list *, _In_z_ const char * _FirstArg, _Out_writes_z_(_MaxCount) char ** _Static_argv, _In_ size_t _MaxCount);
extern wchar_t ** _wcapture_argv(_In_ va_list *, _In_z_ const wchar_t * _FirstArg, _Out_writes_z_(_MaxCount) wchar_t ** _Static_argv, _In_ size_t _MaxCount);
#endif

/*
 * internal routine used by the abort
 */

extern _PHNDLR __cdecl __get_sigabrt(void);

/*
 * Type from ntdef.h
 */

typedef LONG NTSTATUS;

/*
 * Exception code used in _invalid_parameter
 */

#ifndef STATUS_INVALID_PARAMETER
#define STATUS_INVALID_PARAMETER         ((NTSTATUS)0xC000000DL)
#endif

/*
 * Exception code used for abort and _CALL_REPORTFAULT
 */

#ifndef STATUS_FATAL_APP_EXIT
#define STATUS_FATAL_APP_EXIT            ((NTSTATUS)0x40000015L)
#endif

/*
 * Validate functions
 */
#include <crtdbg.h> /* _ASSERTE */
#include <errno.h>

#define __STR2WSTR(str)    L##str

#define _STR2WSTR(str)     __STR2WSTR(str)

#define __FILEW__          _STR2WSTR(__FILE__)
#define __FUNCTIONW__      _STR2WSTR(__FUNCTION__)

/* We completely fill the buffer only in debug (see _SECURECRT__FILL_STRING 
 * and _SECURECRT__FILL_BYTE macros).
 */
#if !defined(_SECURECRT_FILL_BUFFER)
#ifdef _DEBUG
#define _SECURECRT_FILL_BUFFER 1
#else
#define _SECURECRT_FILL_BUFFER 0
#endif
#endif

/* In ARM and Win8 we promptly call __fastfail intrinsic instead */
#if defined(_M_IX86) || defined(_M_X64)
void __cdecl _call_reportfault(int nDbgHookCode, DWORD dwExceptionCode, DWORD dwExceptionFlags);
#endif

/* Invoke Watson if _ExpressionError is not 0; otherwise simply return _EspressionError */
SECURITYCRITICAL_ATTRIBUTE
__forceinline 
void _invoke_watson_if_error(
    errno_t _ExpressionError,
    _In_opt_z_ const wchar_t *_Expression,
    _In_opt_z_ const wchar_t *_Function,
    _In_opt_z_ const wchar_t *_File,
    unsigned int _Line,
    uintptr_t _Reserved
    )
{
    if (_ExpressionError == 0)
    {
        return;
    }
    _invoke_watson(_Expression, _Function, _File, _Line, _Reserved);
}

/* Invoke Watson if _ExpressionError is not 0 and equal to _ErrorValue1 or _ErrorValue2; otherwise simply return _EspressionError */
__forceinline 
errno_t _invoke_watson_if_oneof(
    errno_t _ExpressionError,
    errno_t _ErrorValue1,
    errno_t _ErrorValue2,
    _In_opt_z_ const wchar_t *_Expression,
    _In_opt_z_ const wchar_t *_Function,
    _In_opt_z_ const wchar_t *_File,
    unsigned int _Line,
    uintptr_t _Reserved
    )
{
    if (_ExpressionError == 0 || (_ExpressionError != _ErrorValue1 && _ExpressionError != _ErrorValue2))
    {
        return _ExpressionError;
    }
    _invoke_watson(_Expression, _Function, _File, _Line, _Reserved);
    return _ExpressionError;
}

/*
 * Assert in debug builds.
 * set errno and return
 *
 */
#ifdef _DEBUG 
#define _CALL_INVALID_PARAMETER(expr) _invalid_parameter(expr, __FUNCTIONW__, __FILEW__, __LINE__, 0)
#define _INVOKE_WATSON_IF_ERROR(expr) _invoke_watson_if_error((expr), __STR2WSTR(#expr), __FUNCTIONW__, __FILEW__, __LINE__, 0)
#define _INVOKE_WATSON_IF_ONEOF(expr, errvalue1, errvalue2) _invoke_watson_if_oneof(expr, (errvalue1), (errvalue2), __STR2WSTR(#expr), __FUNCTIONW__, __FILEW__, __LINE__, 0)
#else
#define _CALL_INVALID_PARAMETER(expr) _invalid_parameter_noinfo()
#define _INVOKE_WATSON_IF_ERROR(expr) _invoke_watson_if_error(expr, NULL, NULL, NULL, 0, 0)
#define _INVOKE_WATSON_IF_ONEOF(expr, errvalue1, errvalue2) _invoke_watson_if_oneof((expr), (errvalue1), (errvalue2), NULL, NULL, NULL, 0, 0)
#endif

#ifndef _WCE_BOOTCRT
#define _INVALID_PARAMETER(expr) _CALL_INVALID_PARAMETER(expr)
#else
#define _INVALID_PARAMETER(expr)
#endif

#define _VALIDATE_RETURN_VOID( expr, errorcode )                               \
    {                                                                          \
        int _Expr_val=!!(expr);                                                \
        _ASSERT_EXPR( ( _Expr_val ), _CRT_WIDE(#expr) );                       \
        if ( !( _Expr_val ) )                                                  \
        {                                                                      \
            errno = errorcode;                                                 \
            _INVALID_PARAMETER(_CRT_WIDE(#expr));                              \
            return;                                                            \
        }                                                                      \
    }

/*
 * Assert in debug builds.
 * set errno and return value
 */
// FOR bootcrt, errno is not supported
#ifndef _WCE_BOOTCRT
#ifndef _VALIDATE_RETURN
#define _VALIDATE_RETURN( expr, errorcode, retexpr )                           \
    {                                                                          \
        int _Expr_val=!!(expr);                                                \
        _ASSERT_EXPR( ( _Expr_val ), _CRT_WIDE(#expr) );                       \
        if ( !( _Expr_val ) )                                                  \
        {                                                                      \
            errno = errorcode;                                                 \
            _INVALID_PARAMETER(_CRT_WIDE(#expr) );                             \
            return ( retexpr );                                                \
        }                                                                      \
    }
#endif

#ifndef _VALIDATE_RETURN_NOEXC
#define _VALIDATE_RETURN_NOEXC( expr, errorcode, retexpr )                     \
    {                                                                          \
        if ( !(expr) )                                                         \
        {                                                                      \
            errno = errorcode;                                                 \
            return ( retexpr );                                                \
        }                                                                      \
    }
#endif
#else //_FULLIBC_WCE
#ifndef _VALIDATE_RETURN
#define _VALIDATE_RETURN( expr, errorcode, retexpr )                           \
    {                                                                          \
        int _Expr_val=!!(expr);                                                \
        _ASSERT_EXPR( ( _Expr_val ), _CRT_WIDE(#expr) );                       \
        if ( !( _Expr_val ) )                                                  \
        {                                                                      \
            _INVALID_PARAMETER(_CRT_WIDE(#expr) );                             \
            return ( retexpr );                                                \
        }                                                                      \
    }
#endif 

#ifndef _VALIDATE_RETURN_NOEXC
#define _VALIDATE_RETURN_NOEXC( expr, errorcode, retexpr )                     \
    {                                                                          \
        if ( !(expr) )                                                         \
        {                                                                      \
            return ( retexpr );                                                \
        }                                                                      \
    }
#endif
#endif //_WCE_BOOTCRT

/*
 * Assert in debug builds.
 * set errno and set retval for later usage
 */

#define _VALIDATE_SETRET( expr, errorcode, retval, retexpr )                   \
    {                                                                          \
        int _Expr_val=!!(expr);                                                \
        _ASSERT_EXPR( ( _Expr_val ), _CRT_WIDE(#expr) );                       \
        if ( !( _Expr_val ) )                                                  \
        {                                                                      \
            errno = errorcode;                                                 \
            _INVALID_PARAMETER(_CRT_WIDE(#expr));                              \
            retval=( retexpr );                                                \
        }                                                                      \
    }
    
#define _CHECK_FH_RETURN( handle, errorcode, retexpr )                         \
    {                                                                          \
        if(handle == _NO_CONSOLE_FILENO)                                       \
        {                                                                      \
            errno = errorcode;                                                 \
            return ( retexpr );                                                \
        }                                                                      \
    }

#define _CHECK_IO_INIT( retexpr )                                             \
    {                                                                         \
        if (_ioinit() < 0)                                                    \
            return (retexpr);                                                 \
    }

#define _CHECK_IO_INIT_NOERRNO( )                                             \
    {                                                                         \
        if (_ioinit() < 0)                                                    \
            return;                                                           \
    }

/* 
    We use _VALIDATE_STREAM_ANSI_RETURN to ensure that ANSI file operations(
    fprintf etc) aren't called on files opened as UNICODE. We do this check 
    only if it's an actual FILE pointer & not a string
*/

#define _VALIDATE_STREAM_ANSI_RETURN( stream, errorcode, retexpr )                   \
    {                                                                                \
        FILE *_Stream=stream;                                                        \
        int fn;                                                                      \
        _VALIDATE_RETURN(( (_Stream->_flag & _IOSTRG) ||                             \
                           ( fn = _fileno(_Stream),                                  \
                             ( (_textmode_safe(fn) == __IOINFO_TM_ANSI) &&           \
                               !_tm_unicode_safe(fn)))),                             \
                         errorcode, retexpr)                                         \
    }

/* 
    We use _VALIDATE_STREAM_ANSI_SETRET to ensure that ANSI file operations(
    fprintf etc) aren't called on files opened as UNICODE. We do this check 
    only if it's an actual FILE pointer & not a string. It doesn't actually return
    immediately
*/

#define _VALIDATE_STREAM_ANSI_SETRET( stream, errorcode, retval, retexpr)            \
    {                                                                                \
        FILE *_Stream=stream;                                                        \
        int fn;                                                                      \
        _VALIDATE_SETRET(( (_Stream->_flag & _IOSTRG) ||                             \
                           ( fn = _fileno(_Stream),                                  \
                             ( (_textmode_safe(fn) == __IOINFO_TM_ANSI) &&           \
                               !_tm_unicode_safe(fn)))),                             \
                         errorcode, retval, retexpr)                                 \
    }

/*
 * Assert in debug builds.
 * Return value (do not set errno)
 */

#define _VALIDATE_RETURN_NOERRNO( expr, retexpr )                              \
    {                                                                          \
        int _Expr_val=!!(expr);                                                \
        _ASSERT_EXPR( ( _Expr_val ), _CRT_WIDE(#expr) );                       \
        if ( !( _Expr_val ) )                                                  \
        {                                                                      \
            _INVALID_PARAMETER(_CRT_WIDE(#expr));                              \
            return ( retexpr );                                                \
        }                                                                      \
    }

/*
 * Assert in debug builds.
 * set errno and return errorcode
 */

#ifndef _WCE_BOOTCRT
#define _VALIDATE_RETURN_ERRCODE( expr, errorcode )                            \
    {                                                                          \
        int _Expr_val=!!(expr);                                                \
        _ASSERT_EXPR( ( _Expr_val ), _CRT_WIDE(#expr) );                       \
        if ( !( _Expr_val ) )                                                  \
        {                                                                      \
            errno = errorcode;                                                 \
            _INVALID_PARAMETER(_CRT_WIDE(#expr));                              \
            return ( errorcode );                                              \
        }                                                                      \
    }    
#else
#define _SET_ERRNO(_errnoval)  /* not supported on WinCE */

#define _VALIDATE_RETURN_ERRCODE( expr, errorcode )                            \
    {                                                                          \
        int _Expr_val=!!(expr);                                                \
        _ASSERT_EXPR( ( _Expr_val ), _CRT_WIDE(#expr) );                       \
        if ( !( _Expr_val ) )                                                  \
        {                                                                      \
            _SET_ERRNO(errorcode);                                             \
            _INVALID_PARAMETER(_CRT_WIDE(#expr));                              \
            return ( errorcode );                                              \
        }                                                                      \
    }
#endif // _WCE_BOOTCRT

#define _VALIDATE_RETURN_ERRCODE_NOEXC( expr, errorcode )                      \
    {                                                                          \
        if (!(expr))                                                           \
        {                                                                      \
            errno = errorcode;                                                 \
            return ( errorcode );                                              \
        }                                                                      \
    }    

#define _VALIDATE_CLEAR_OSSERR_RETURN( expr, errorcode, retexpr )              \
    {                                                                          \
        int _Expr_val=!!(expr);                                                \
        _ASSERT_EXPR( ( _Expr_val ), _CRT_WIDE(#expr) );                       \
        if ( !( _Expr_val ) )                                                  \
        {                                                                      \
            _doserrno = 0L;                                                    \
            errno = errorcode;                                                 \
            _INVALID_PARAMETER(_CRT_WIDE(#expr) );                             \
            return ( retexpr );                                                \
        }                                                                      \
    }

#define _CHECK_FH_CLEAR_OSSERR_RETURN( handle, errorcode, retexpr )            \
    {                                                                          \
        if(handle == _NO_CONSOLE_FILENO)                                       \
        {                                                                      \
            _doserrno = 0L;                                                    \
            errno = errorcode;                                                 \
            return ( retexpr );                                                \
        }                                                                      \
    }

#define _VALIDATE_CLEAR_OSSERR_RETURN_ERRCODE( expr, errorcode )               \
    {                                                                          \
        int _Expr_val=!!(expr);                                                \
        _ASSERT_EXPR( ( _Expr_val ), _CRT_WIDE(#expr) );                       \
        if ( !( _Expr_val ) )                                                  \
        {                                                                      \
            _doserrno = 0L;                                                    \
            errno = errorcode;                                                 \
            _INVALID_PARAMETER(_CRT_WIDE(#expr));                              \
            return ( errorcode );                                              \
        }                                                                      \
    }    

#define _CHECK_FH_CLEAR_OSSERR_RETURN_ERRCODE( handle, retexpr )               \
    {                                                                          \
        if(handle == _NO_CONSOLE_FILENO)                                       \
        {                                                                      \
            _doserrno = 0L;                                                    \
            return ( retexpr );                                                \
        }                                                                      \
    }

#ifdef _DEBUG
extern size_t __crtDebugFillThreshold;
#endif

#if !defined(_SECURECRT_FILL_BUFFER_THRESHOLD)
#ifdef _DEBUG
#define _SECURECRT_FILL_BUFFER_THRESHOLD __crtDebugFillThreshold
#else
#define _SECURECRT_FILL_BUFFER_THRESHOLD ((size_t)0)
#endif
#endif

#if _SECURECRT_FILL_BUFFER
#define _SECURECRT__FILL_STRING(_String, _Size, _Offset)                            \
    if ((_Size) != ((size_t)-1) && (_Size) != INT_MAX &&                            \
        ((size_t)(_Offset)) < (_Size))                                              \
    {                                                                               \
        memset((_String) + (_Offset),                                               \
            _SECURECRT_FILL_BUFFER_PATTERN,                                         \
            (_SECURECRT_FILL_BUFFER_THRESHOLD < ((size_t)((_Size) - (_Offset))) ?   \
                _SECURECRT_FILL_BUFFER_THRESHOLD :                                  \
                ((_Size) - (_Offset))) * sizeof(*(_String)));                       \
    }
#else
#define _SECURECRT__FILL_STRING(_String, _Size, _Offset)
#endif

#if _SECURECRT_FILL_BUFFER
#define _SECURECRT__FILL_BYTE(_Position)                \
    if (_SECURECRT_FILL_BUFFER_THRESHOLD > 0)           \
    {                                                   \
        (_Position) = _SECURECRT_FILL_BUFFER_PATTERN;   \
    }
#else
#define _SECURECRT__FILL_BYTE(_Position)
#endif

#ifdef  __cplusplus
#define _REDIRECT_TO_L_VERSION_FUNC_PROLOGUE extern "C"
#else
#define _REDIRECT_TO_L_VERSION_FUNC_PROLOGUE
#endif

/* helper macros to redirect an mbs function to the corresponding _l version */
#define _REDIRECT_TO_L_VERSION_1(_ReturnType, _FunctionName, _Type1) \
    _REDIRECT_TO_L_VERSION_FUNC_PROLOGUE \
    _ReturnType __cdecl _FunctionName(_Type1 _Arg1) \
    { \
        return _FunctionName##_l(_Arg1, NULL); \
    }

#define _REDIRECT_TO_L_VERSION_2(_ReturnType, _FunctionName, _Type1, _Type2) \
    _REDIRECT_TO_L_VERSION_FUNC_PROLOGUE \
    _ReturnType __cdecl _FunctionName(_Type1 _Arg1, _Type2 _Arg2) \
    { \
        return _FunctionName##_l(_Arg1, _Arg2, NULL); \
    }

#define _REDIRECT_TO_L_VERSION_3(_ReturnType, _FunctionName, _Type1, _Type2, _Type3) \
    _REDIRECT_TO_L_VERSION_FUNC_PROLOGUE \
    _ReturnType __cdecl _FunctionName(_Type1 _Arg1, _Type2 _Arg2, _Type3 _Arg3) \
    { \
        return _FunctionName##_l(_Arg1, _Arg2, _Arg3, NULL); \
    }

#define _REDIRECT_TO_L_VERSION_4(_ReturnType, _FunctionName, _Type1, _Type2, _Type3, _Type4) \
    _REDIRECT_TO_L_VERSION_FUNC_PROLOGUE \
    _ReturnType __cdecl _FunctionName(_Type1 _Arg1, _Type2 _Arg2, _Type3 _Arg3, _Type4 _Arg4) \
    { \
        return _FunctionName##_l(_Arg1, _Arg2, _Arg3, _Arg4, NULL); \
    }

#define _REDIRECT_TO_L_VERSION_5(_ReturnType, _FunctionName, _Type1, _Type2, _Type3, _Type4, _Type5) \
    _REDIRECT_TO_L_VERSION_FUNC_PROLOGUE \
    _ReturnType __cdecl _FunctionName(_Type1 _Arg1, _Type2 _Arg2, _Type3 _Arg3, _Type4 _Arg4, _Type5 _Arg5) \
    { \
        return _FunctionName##_l(_Arg1, _Arg2, _Arg3, _Arg4, _Arg5, NULL); \
    }

#define _REDIRECT_TO_L_VERSION_6(_ReturnType, _FunctionName, _Type1, _Type2, _Type3, _Type4, _Type5, _Type6) \
    _REDIRECT_TO_L_VERSION_FUNC_PROLOGUE \
    _ReturnType __cdecl _FunctionName(_Type1 _Arg1, _Type2 _Arg2, _Type3 _Arg3, _Type4 _Arg4, _Type5 _Arg5, _Type6 _Arg6) \
    { \
        return _FunctionName##_l(_Arg1, _Arg2, _Arg3, _Arg4, _Arg5, _Arg6, NULL); \
    }

/* internal helper functions for encoding and decoding pointers */
void __cdecl _init_pointers();

/* These functions need to be declared with these security attributes. Otherwise they will 
    result in System.Security.SecurityException 
*/

#ifdef _WIN32_WCE
#define EncodePointer(ptr)      (ptr)
#define DecodePointer(ptr)      (ptr)

#else
_SUPPRESS_UNMANAGED_CODE_SECURITY 
SECURITYCRITICAL_ATTRIBUTE
_RELIABILITY_CONTRACT
_INTEROPSERVICES_DLLIMPORT("KERNEL32.dll", "EncodePointer", _CALLING_CONVENTION_WINAPI)
WINBASEAPI
_Ret_maybenull_
PVOID
WINAPI
EncodePointer (
    _In_opt_ PVOID Ptr
    );

_SUPPRESS_UNMANAGED_CODE_SECURITY 
SECURITYCRITICAL_ATTRIBUTE
_RELIABILITY_CONTRACT
_INTEROPSERVICES_DLLIMPORT("KERNEL32.dll", "DecodePointer", _CALLING_CONVENTION_WINAPI)
WINBASEAPI
_Ret_maybenull_
PVOID
WINAPI
DecodePointer (
    _In_opt_ PVOID Ptr
    );
#endif

/* Macros to simplify the use of Secure CRT in the CRT itself. 
 * We should use [_BEGIN/_END]_SECURE_CRT_DEPRECATION_DISABLE sparingly.
 */
#define _BEGIN_SECURE_CRT_DEPRECATION_DISABLE \
    __pragma(warning(push)) \
    __pragma(warning(disable:4996)) \
    __pragma(warning(disable:6053))

#define _END_SECURE_CRT_DEPRECATION_DISABLE \
    __pragma(warning(pop))

#define _ERRCHECK(e) \
    _INVOKE_WATSON_IF_ERROR(e)

#define _ERRCHECK_EINVAL(e) \
    _INVOKE_WATSON_IF_ONEOF(e, EINVAL, EINVAL)

#define _ERRCHECK_EINVAL_ERANGE(e) \
    _INVOKE_WATSON_IF_ONEOF(e, EINVAL, ERANGE)

#define _ERRCHECK_SPRINTF(_PrintfCall) \
    { \
        errno_t _SaveErrno = errno; \
        errno = 0; \
        if ( ( _PrintfCall ) < 0) \
        { \
            _ERRCHECK_EINVAL_ERANGE(errno); \
        } \
        errno = _SaveErrno; \
    }

/* internal helper function to access environment variable in read-only mode */
const wchar_t * __cdecl _wgetenv_helper_nolock(const wchar_t *);
const char * __cdecl _getenv_helper_nolock(const char *);

/* internal helper routines used to query a PE image header. */
BOOL __cdecl _ValidateImageBase(PBYTE pImageBase);
PIMAGE_SECTION_HEADER __cdecl _FindPESection(PBYTE pImageBase, DWORD_PTR rva);
BOOL __cdecl _IsNonwritableInCurrentImage(PBYTE pTarget);

#ifdef  __cplusplus
}
#endif

#define CRT_WARNING_DISABLE_PUSH(x,y)    __pragma(warning(push)); __pragma(warning(disable: x))
#define CRT_WARNING_POP                  __pragma(warning(pop))

/*
 * Helper function to convert an ANSI/MBCS string to wide char using the current ANSI code set
 *
 */
BOOL __cdecl __copy_path_to_wide_string (const char* _Str,  wchar_t** _WStr);

#pragma pack(pop)

#endif  /* _INC_INTERNAL */
