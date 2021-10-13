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
*mtdll.h - DLL/Multi-thread include
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*
*       [Internal]
*
*Revision History:
*       10-27-87  JCR   Module created.
*       11-13-87  SKS   Added _HEAP_LOCK
*       12-15-87  JCR   Added _EXIT_LOCK
*       01-07-88  BCM   Added _SIGNAL_LOCK; upped MAXTHREADID from 16 to 32
*       02-01-88  JCR   Added _dll_mlock/_dll_munlock macros
*       05-02-88  JCR   Added _BHEAP_LOCK
*       06-17-88  JCR   Corrected prototypes for special mthread debug routines
*       08-15-88  JCR   _check_lock now returns int, not void
*       08-22-88  GJF   Modified to also work for the 386 (small model only)
*       06-05-89  JCR   386 mthread support
*       06-09-89  JCR   386: Added values to _tiddata struc (for _beginthread)
*       07-13-89  JCR   386: Added _LOCKTAB_LOCK
*       08-17-89  GJF   Cleanup, now specific to OS/2 2.0 (i.e., 386 flat model)
*       10-30-89  GJF   Fixed copyright
*       01-02-90  JCR   Moved a bunch of definitions from os2dll.inc
*       04-06-90  GJF   Added _INC_OS2DLL stuff and #include <cruntime.h>. Made
*                       all function _CALLTYPE2 (for now).
*       04-10-90  GJF   Added prototypes for _[un]lockexit().
*       08-16-90  SBM   Made _terrno and _tdoserrno int, not unsigned
*       09-14-90  GJF   Added _pxcptacttab, _pxcptinfoptr and _fpecode fields
*                       to _tiddata struct.
*       10-09-90  GJF   Thread ids are of type unsigned long.
*       12-06-90  SRW   Added _OSFHND_LOCK
*       06-04-91  GJF   Win32 version of multi-thread types and prototypes.
*       08-15-91  GJF   Made _tdoserrno an unsigned long for Win32.
*       08-20-91  JCR   C++ and ANSI naming
*       09-29-91  GJF   Conditionally added prototypes for __getptd_lk
*                       and  _getptd1_nolock for Win32 under DEBUG.
*       10-03-91  JCR   Added _cvtbuf to _tiddata structure
*       02-17-92  GJF   For Win32, replaced _NFILE_ with _NHANDLE_ and
*                       _NSTREAM_.
*       03-06-92  GJF   For Win32, made _[un]mlock_[fh|stream]() macros
*                       directly call _[un]lock().
*       03-17-92  GJF   Dropped _namebuf field from _tiddata structure for
*                       Win32.
*       08-05-92  GJF   Function calling type and variable type macros.
*       12-03-91  ETC   Added _wtoken to _tiddata, added intl LOCK's;
*                       added definition of wchar_t (needed for _wtoken).
*       08-14-92  KRS   Port ETC's _wtoken change from other tree.
*       08-21-92  GJF   Merged 08-05-92 and 08-14-92 versions.
*       12-03-92  KRS   Added _mtoken field for MTHREAD _mbstok().
*       01-21-93  GJF   Removed support for C6-386's _cdecl.
*       02-25-93  GJF   Purged Cruiser support and many outdated definitions
*                       and declarations.
*       04-07-93  SKS   Add _CRTIMP keyword for CRT DLL model
*       10-11-93  GJF   Support NT and Cuda builds. Also, purged some old
*                       non-Win32 support (it was incomplete anyway) and
*                       replace MTHREAD with _MT.
*       10-13-93  SKS   Change name from <MTDLL.H> to <MTDLL.H>
*       10-27-93  SKS   Add Per-Thread Variables for C++ Exception Handling
*       12-13-93  SKS   Add _freeptd(), which frees per-thread CRT data
*       12-17-93  CFW   Add Per-Thread Variable for _wasctime().
*       04-15-93  CFW   Add _MB_CP_LOCK.
*       04-21-94  GJF   Made declaration of __flsindex and definition of the
*                       lock macros conditional on ndef DLL_FOR_WIN32S.
*                       Also, conditionally include win32s.h.
*       12-14-94  SKS   Increase file handle and FILE * limits for MSVCRT30.DLL
*       02-14-95  CFW   Clean up Mac merge.
*       03-06-95  GJF   Added _[un]lock_file[2] prototypes, _[un]lock_str2
*                       macros, and changed the _[un]lock_str macros.
*       03-13-95  GJF   _IOB_ENTRIES replaced _NSTREAM_ as the number of
*                       stdio locks in _locktable[].
*       03-29-95  CFW   Add error message to internal headers.
*       04-13-95  DAK   Add NT Kernel EH support
*       04-18-95  SKS   Add 5 per-thread variables for MIPS EH use
*       05-02-95  SKS   Add _initptd() which initializes per-thread data
*       05-08-95  CFW   Official ANSI C++ new handler added.
*       05-19-95  DAK   More Kernel EH work.
*       06-05-95  JWM   _NLG_dwcode & _NLG_LOCK added.
*       06-11-95  GJF   The critical sections for file handles are now in the
*                       ioinfo struct rather than the lock table.
*       07-20-95  CFW   Remove _MBCS ifdef - caused ctime/wctime bug.
*       10-03-95  GJF   Support for new scheme to lock locale including
*                       _[un]lock_locale() macros and decls of _setlc_active
*                       and __unguarded_readlc_active. Also commented out
*                       obsolete *_LOCK macros.
*       10-19-95  BWT   Fixup _NTSUBSET_ usage.
*       12-07-95  SKS   Fix misspelling of _NTSUBSET_ (final _ was missing)
*       12-14-95  JWM   Add "#pragma once".
*       05-02-96  SKS   Variables _setlc_active and __unguarded_readlc_active
*                       are used by MSVCP42*.DLL and so must be _CRTIMP.
*       07-16-96  GJF   Locale locking wasn't really thread safe. Replaced ++
*                       and -- with InterlockedIncrement and
*                       InterlockedDecrement API, respectively.
*       07-18-96  GJF   Further mod to locale locking to fix race condition.
*       02-05-97  GJF   Cleaned out obsolete support for Win32s, _CRTAPI* and
*                       _NTSDK. Also, replaced #if defined with #ifdef where
*                       appropriate.
*       02-02-98  GJF   Changes for Win64: changed _thandle type to uintptr_t.
*       04-27-98  GJF   Added support for per-thread mbc information.
*       07-28-98  JWM   Added __pceh to per-thread data for comerr support.
*       09-10-98  GJF   Added support for per-thread locale information.
*       12-05-98  JWM   Pulled all comerr support.
*       03-24-99  GJF   More reference counters for threadlocinfo.
*       04-24-99  PML   Added lconv_intl_refcount to threadlocinfo.
*       05-17-99  PML   Remove all Macintosh support.
*       10-06-99  PML   Add _W64 modifier to types which are 32 bits in Win32,
*                       64 bits in Win64.
*       11-24-99  GB    Add _werrmsg in struct _tiddata  for error support in
*                       wide char version of strerror.
*       12-10-99  GB    Add _ProcessingThrow in struct _tiddata.
*       12-10-99  GB    Added a new Lock _UNDNAME_LOCK for critical section in
*                       unDName().
*       26-01-00  GB    Added lc_clike in threadlocinfostruct.
*       06-08-00  PML   Remove threadmbcinfo.{pprev,pnext}.  Rename
*                       THREADLOCALEINFO to _THREADLOCALEINFO and
*                       THREADMBCINFO to _THREADMBCINFO.
*       09-06-00  GB    deleted _wctype and _pwctype from threadlocinfo.
*       01-29-01  GB    Added _func function version of data variable in
*                       _lock_locale to work with STATIC_CPPLIB
*       02-20-01  PML   vs7#172586 Avoid _RT_LOCK by preallocating all locks
*                       that will be required, and returning failure back on
*                       inability to allocate a lock.
*       03-22-01  PML   Add _DEBUG_LOCK for _CrtSetReportHook2 (vs7#124998)
*       03-25-01  PML   Add ww_caltype & ww_lcid to __lc_time_data (vs7#196892)
*       07-15-01  PML   Remove all ALPHA, MIPS, and PPC code
*       09-18-01  GB    Support for exception specification.
*       10-16-01  GB    Added fiber support
*       04-02-02  GB    Changed FLS macros to just use redirection funciton pointers.
*       04-15-02  BWT   Remove kernel EH support
*       04-29-02  GB    Added MACROS for __try{ }__FINALLY{ and } namely __TRY __FINALLY
*       05-20-02  MSL   Added support for _set_purecall_handler
*       12-22-03  SJ    VSWhidbey#174074 - Added new lock for serializing 
*                       asserts across threads
*       03-08-04  MSL   Converted locale struct to use native not Win32 types to allow 
*                       external exposure
*       04-07-04  MSL   Double slash removal
*       04-07-04  MSL   Added purecall handler for managed
*       04-07-04  GB    Added support for beginthread et. al. for /clr:pure.
*                       __fls_setvalue, __fls_getvalue, __get_flsindex are added.
*       06-29-04  SJ    Added a new field to _tiddata to detect rethrown exceptions.
*       10-08-04  ESC   Protect against -Zp with pragma pack
*       10-18-04  MSL   Managed new handler
*       11-07-04  MSL   Removed any __clrcall function pointers from ptd (domain safety)
*       11-07-04  MSL   Cleanup for pure compilation with no data imports
*       11-18-04  ATC   SAL Annotations for CRT Headers
*       01-21-05  MSL   Add param names to overload macros
*       01-28-05  SJ    DEBUG_RPT_LOCK is no longer used anywhere, hence removing it.
*       03-23-05  MSL   _P removal - not needed
*                       New deprecation warning with function name
*                       Packing fix
*       04-14-05  MSL   Intellisense locale name cleanup
*       05-23-05  PML   _lock_locale has been useless for years (vsw#497047)
*       01-05-06  GM    __set_flsgetvalue() should return FLS value to avoid
*                       querying for it again later.
*	07-12-07  ATC   ddb#123388 - remove GetProcAddress for Encode/DecodePointer and 
*	                FLS* APIs for amd64 (but x86 remains the same)
*
****/

#pragma once

#ifndef _INC_MTDLL
#define _INC_MTDLL

#include <crtdefs.h>
#include <limits.h>

#pragma pack(push,_CRT_PACKING)

#ifdef  __cplusplus
extern "C" {
#endif

#include <windows.h>

#ifdef _WIN32_WCE 
#ifndef _CASSERT
#define _CASSERT(e) typedef char __CASSERT__[(e)?1:-1]
#endif
#endif // _WIN32_WCE

/*
 * Define the number of supported handles and streams. The definitions
 * here must exactly match those in internal.h (for _NHANDLE_) and stdio.h
 * (for _NSTREAM_).
 */

#define _IOB_ENTRIES    20

/* Lock symbols */

#define _SIGNAL_LOCK    0       /* lock for signal()                */
#define _IOB_SCAN_LOCK  1       /* _iob[] table lock                */
#define _TMPNAM_LOCK    2       /* lock global tempnam variables    */
#define _CONIO_LOCK     3       /* lock for conio routines          */
#define _HEAP_LOCK      4       /* lock for heap allocator routines */
#define _UNDNAME_LOCK   5       /* lock for unDName() routine       */
#define _TIME_LOCK      6       /* lock for time functions          */
#define _ENV_LOCK       7       /* lock for environment variables   */
#define _EXIT_LOCK1     8       /* lock #1 for exit code            */
#define _POPEN_LOCK     9       /* lock for _popen/_pclose database */
#define _LOCKTAB_LOCK   10      /* lock to protect semaphore lock table */
#define _OSFHND_LOCK    11      /* lock to protect _osfhnd array    */
#define _SETLOCALE_LOCK 12      /* lock for locale handles, etc.    */
#define _MB_CP_LOCK     13      /* lock for multibyte code page     */
#define _TYPEINFO_LOCK  14      /* lock for type_info access        */
#define _DEBUG_LOCK     15      /* lock for debug global structs    */

#define _STREAM_LOCKS   16      /* Table of stream locks            */

#define _LAST_STREAM_LOCK  (_STREAM_LOCKS+_IOB_ENTRIES-1) /* Last stream lock */

#define _TOTAL_LOCKS        (_LAST_STREAM_LOCK+1)

#define _LOCK_BIT_INTS     (_TOTAL_LOCKS/(sizeof(unsigned)*8))+1   /* # of ints to hold lock bits */

#ifndef __assembler

/* Multi-thread macros and prototypes */
#define __TRY __try{
#define __FINALLY   }__finally{
#define __END_TRY_FINALLY }

#ifndef _THREADMBCINFO
typedef struct threadmbcinfostruct threadmbcinfo;
#endif

#define MAX_LANG_LEN        64  /* max language name length */
#define MAX_CTRY_LEN        64  /* max country name length */
#define MAX_MODIFIER_LEN    0   /* max modifier name length - n/a */
#define MAX_LC_LEN          (MAX_LANG_LEN+MAX_CTRY_LEN+MAX_MODIFIER_LEN+3)
                                /* max entire locale string length */
#define MAX_CP_LEN          16  /* max code page name length */
#define CATNAMES_LEN        57  /* "LC_COLLATE=;LC_CTYPE=;..." length */

#define LC_INT_TYPE         0
#define LC_STR_TYPE         1
#define LC_WSTR_TYPE        2

#ifndef _SETLOC_STRUCT_DEFINED
struct _is_ctype_compatible {
        unsigned long id;
        int is_clike;
};

typedef struct setloc_struct {
    /* getqloc static variables */
    wchar_t *pchLanguage;
    wchar_t *pchCountry;
    int iLocState;
    int iPrimaryLen;
    BOOL bAbbrevLanguage;
    BOOL bAbbrevCountry;
    UINT        _cachecp;
    wchar_t     _cachein[MAX_LC_LEN];
    wchar_t     _cacheout[MAX_LC_LEN];
    /* _setlocale_set_cat (LC_CTYPE) static variable */
    struct _is_ctype_compatible _Loc_c[5];
    wchar_t _cacheLocaleName[LOCALE_NAME_MAX_LENGTH];
} _setloc_struct, *_psetloc_struct;
#define _SETLOC_STRUCT_DEFINED
#endif

_CRTIMP extern unsigned long __cdecl __threadid(void);
#define _threadid   (__threadid())
_CRTIMP extern uintptr_t __cdecl __threadhandle(void);
#define _threadhandle   (__threadhandle())

#ifdef _WIN32_WCE
typedef struct {
    void *          CurrentExcept;
    void *          ExceptContext;
    int             _ProcessingThrow;   /* for uncaught_exception */
    void *          TargetEntry;
#ifdef _M_IX86
    void *          CurrExceptRethrow;
    char            padding[4];         /* Struct size should be multiple of 8 */
#else
    void *          FrameInfoChain;
    int             _cxxReThrow;
    unsigned long   ImageBase;
    unsigned long   ThrowImageBase; 
#endif
} crtEHData;
#endif

/* Structure for each thread's data */

struct _tiddata {
    unsigned long   _tid;       /* thread ID */
#ifdef  _NTSUBSET_
    struct _tiddata *_next;     /* maintain a linked list */
#else
    uintptr_t _thandle;         /* thread handle */

#ifndef _WIN32_WCE
    int     _terrno;            /* errno value */
    unsigned long   _tdoserrno; /* _doserrno value */
    unsigned int    _fpds;      /* Floating Point data segment */
    char *      _token;         /* ptr to strtok() token */
    wchar_t *   _wtoken;        /* ptr to wcstok() token */
    unsigned char * _mtoken;    /* ptr to _mbstok() token */
    unsigned long   _holdrand;  /* rand() seed value */
#endif

    /* following pointers get malloc'd at runtime */
    char *      _errmsg;        /* ptr to strerror()/_strerror() buff */
    wchar_t *   _werrmsg;       /* ptr to _wcserror()/__wcserror() buff */
    char *      _namebuf0;      /* ptr to tmpnam() buffer */
    wchar_t *   _wnamebuf0;     /* ptr to _wtmpnam() buffer */
    char *      _namebuf1;      /* ptr to tmpfile() buffer */
    wchar_t *   _wnamebuf1;     /* ptr to _wtmpfile() buffer */
    char *      _asctimebuf;    /* ptr to asctime() buffer */
    wchar_t *   _wasctimebuf;   /* ptr to _wasctime() buffer */
    void *      _gmtimebuf;     /* ptr to gmtime() structure */
    char *      _cvtbuf;        /* ptr to ecvt()/fcvt buffer */
    unsigned char _con_ch_buf[MB_LEN_MAX];
                                /* ptr to putch() buffer */
    unsigned short _ch_buf_used;   /* if the _con_ch_buf is used */

    /* following fields are needed by _beginthread code */
    void *      _initaddr;      /* initial user thread address */
    void *      _initarg;       /* initial user thread argument */

    /* following three fields are needed to support signal handling and
     * runtime errors */
    void *      _pxcptacttab;   /* ptr to exception-action table */
    void *      _tpxcptinfoptrs; /* ptr to exception info pointers */
    int         _tfpecode;      /* float point exception code */

#if _PTD_LOCALE_SUPPORT
    /* pointer to the copy of the multibyte character information used by
     * the thread */
    pthreadmbcinfo  ptmbcinfo;

    /* pointer to the copy of the locale informaton used by the thead */
    pthreadlocinfo  ptlocinfo;
    int         _ownlocale;     /* if 1, this thread owns its own locale */
#endif  /* _PTD_LOCALE_SUPPORT */
#endif  /* !_NTSUBSET_ */

#ifndef _WIN32_WCE
    /* following field is needed by NLG routines */
    unsigned long   _NLG_dwCode;

    /*
     * Per-Thread data needed by C++ Exception Handling
     */
    void *      _terminate;     /* terminate() routine */
    void *      _unexpected;    /* unexpected() routine */
    void *      _translator;    /* S.E. translator */
    void *      _purecall;      /* called when pure virtual happens */
    void *      _curexception;  /* current exception */
    void *      _curcontext;    /* current exception context */
    int         _ProcessingThrow; /* for uncaught_exception */
    void *      _curexcspec;	/* for handling exceptions thrown from std::unexpected */
#if defined(_M_X64) || defined(_M_ARM)
    void *      _pExitContext;
    void *      _pUnwindContext;
    void *      _pFrameInfoChain;
#if defined(_WIN64)
    unsigned __int64    _ImageBase;
    unsigned __int64    _ThrowImageBase;
#else
    unsigned __int32    _ImageBase;
    unsigned __int32    _ThrowImageBase;
#endif
    void *      _pForeignException;
#elif defined(_M_IX86)
    void *      _pFrameInfoChain;
#endif
#endif

    _setloc_struct _setloc_data;
#ifndef _WIN32_WCE
    void *      _reserved1;     /* nothing */
    void *      _reserved2;     /* nothing */
    void *      _reserved3;     /* nothing */
#ifdef  _M_IX86
    void *      _reserved4;     /* nothing */
    void *      _reserved5;     /* nothing */

    int _cxxReThrow;        /* Set to True if it's a rethrown C++ Exception */

    unsigned long __initDomain;     /* initial domain used by _beginthread[ex] for managed function */
    int           _initapartment;   /* if 1, this thread has initialized apartment */
#endif
#endif
};

typedef struct _tiddata * _ptiddata;

#ifdef _WIN32_WCE
struct _tiddata_persist {
    int     _terrno;            /* errno value */
    unsigned long   _tdoserrno; /* _doserrno value */
    char *      _token;         /* ptr to strtok() token */
    wchar_t *   _wtoken;        /* ptr to wcstok() token */
    unsigned char * _mtoken;    /* ptr to _mbstok() token */
    unsigned long   _holdrand;

    crtEHData   EHData;
};
typedef struct _tiddata_persist * _ptiddata_persist;

// Compile time Size check for _tiddata_persist
// Changing _tiddata_persist structure will affect CE build. So TLS_CRTSTORAGE_SIZE is
// defined in CE specific header file corecrtstorage.h. Changing the size of _tiddata_persist
// should be done with CE dev being notified.
#include <corecrtstorage.h>
_CASSERT((sizeof (struct _tiddata_persist) == TLS_CRTSTORAGE_SIZE));
#else
#define _tiddata_persist _tiddata
#define _ptiddata_persist ptiddata
#endif
/*
 * Declaration of TLS index used in storing pointers to per-thread data
 * structures.
 */
extern unsigned long __flsindex;

/* macros */

#define _lock_fh(fh)            __lock_fhandle(fh)
#define _lock_str(s)            _lock_file(s)
#define _lock_str2(i,s)         _lock_file2(i,s)
#define _mlock(l)               _lock(l)
#define _munlock(l)             _unlock(l)
#define _unlock_fh(fh)          _unlock_fhandle(fh)
#define _unlock_str(s)          _unlock_file(s)
#define _unlock_str2(i,s)       _unlock_file2(i,s)

/* multi-thread routines */

#ifdef _M_CEE
 #ifndef _CRT_MSVCR_CURRENT
  #ifdef _DEBUG
   #define _CRT_MSVCR_CURRENT "msvcr110D.dll"
  #else
   #define _CRT_MSVCR_CURRENT "msvcr110.dll"
  #endif
 #endif
 #define _INTEROPSERVICES_DLLIMPORT(_DllName , _EntryPoint , _CallingConvention) \
    [System::Runtime::InteropServices::DllImport(_DllName , EntryPoint = _EntryPoint, CallingConvention = _CallingConvention)]
 #define _SUPPRESS_UNMANAGED_CODE_SECURITY [System::Security::SuppressUnmanagedCodeSecurity]
 #define _CALLING_CONVENTION_CDECL System::Runtime::InteropServices::CallingConvention::Cdecl
 #define _CALLING_CONVENTION_WINAPI System::Runtime::InteropServices::CallingConvention::Winapi
 #define _RELIABILITY_CONTRACT \
    [System::Runtime::ConstrainedExecution::ReliabilityContract( \
        System::Runtime::ConstrainedExecution::Consistency::WillNotCorruptState, \
        System::Runtime::ConstrainedExecution::Cer::Success)]
 #define ASSERT_UNMANAGED_CODE_ATTRIBUTE [System::Security::Permissions::SecurityPermissionAttribute(System::Security::Permissions::SecurityAction::Assert, UnmanagedCode = true)]
 #define SECURITYCRITICAL_ATTRIBUTE [System::Security::SecurityCritical]
 #define SECURITYSAFECRITICAL_ATTRIBUTE [System::Security::SecuritySafeCritical]
#else
 #define _INTEROPSERVICES_DLLIMPORT(_DllName , _EntryPoint , _CallingConvention)
 #define _SUPPRESS_UNMANAGED_CODE_SECURITY
 #define _CALLING_CONVENTION_CDECL
 #define _CALLING_CONVENTION_WINAPI
 #define _RELIABILITY_CONTRACT
 #define ASSERT_UNMANAGED_CODE_ATTRIBUTE 
 #define SECURITYCRITICAL_ATTRIBUTE
 #define SECURITYSAFECRITICAL_ATTRIBUTE
#endif

_SUPPRESS_UNMANAGED_CODE_SECURITY
SECURITYCRITICAL_ATTRIBUTE
_RELIABILITY_CONTRACT
_INTEROPSERVICES_DLLIMPORT(_CRT_MSVCR_CURRENT, "_lock", _CALLING_CONVENTION_CDECL)
void __cdecl _lock(_In_ int _File);
void __cdecl _lock_file2(_In_ int _Index, _Inout_ void * _File);
void __cdecl _lockexit(void);
_SUPPRESS_UNMANAGED_CODE_SECURITY
SECURITYCRITICAL_ATTRIBUTE
_RELIABILITY_CONTRACT
_INTEROPSERVICES_DLLIMPORT(_CRT_MSVCR_CURRENT, "_unlock", _CALLING_CONVENTION_CDECL)
void __cdecl _unlock(_Inout_ int _File);
void __cdecl _unlock_file2(_In_ int _Index, _Inout_ void * _File);
void __cdecl _unlockexit(void);
int  __cdecl _mtinitlocknum(_In_ int _LockNum);

#if !defined(_WIN32_WCE) || !defined(_WCE_CORECRT)
_SUPPRESS_UNMANAGED_CODE_SECURITY
_RELIABILITY_CONTRACT
_INTEROPSERVICES_DLLIMPORT(_CRT_MSVCR_CURRENT, "_getptd", _CALLING_CONVENTION_CDECL)
_ptiddata __cdecl _getptd(void);  /* return address of per-thread CRT data */
_ptiddata __cdecl _getptd_noexit(void);  /* return address of per-thread CRT data - doesn't exit on malloc failure */
_CRTIMP void WINAPI _freefls(_Inout_opt_ void * _PerFiberData);         /* free up per-fiber CRT data block */
void __cdecl _freeptd(_Inout_opt_ _ptiddata _Ptd); /* free up a per-thread CRT data block */
_CRTIMP void __cdecl _initptd(_Inout_ _ptiddata _Ptd,_In_opt_ pthreadlocinfo _Locale); /* initialize a per-thread CRT data block */

_CRTIMP unsigned long __cdecl __get_flsindex(void);
#endif

#ifdef _WIN32_WCE
/* return address of persistent per-thread CRT data */
#define GetCRTStorage()     ((_ptiddata_persist)TlsGetValue(TLSSLOT_RUNTIME))
#define _getptd_persist GetCRTStorage
#define _getptd_persist_noexit _getptd_persist

#define __crtFlsGetValue TlsGetValue
#define __crtFlsSetValue TlsSetValue
void _freeptd_ce(DWORD threadid, LPVOID data);
#else
#define _getptd_persist _getptd
#define _getptd_persist_noexit _getptd_noexit
#endif
#endif  /* __assembler */


#ifdef  __cplusplus
}
#endif

#pragma pack(pop)

#endif  /* _INC_MTDLL */
