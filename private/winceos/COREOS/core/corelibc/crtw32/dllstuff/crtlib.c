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
*crtlib.c - CRT DLL initialization and termination routine (Win32, Dosx32)
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       This module contains initialization entry point for the CRT DLL
*       in the Win32 environment. It also contains some of the supporting
*       initialization and termination code.
*
*Revision History:
*       08-12-91  GJF   Module created. Sort of.
*       01-17-92  GJF   Return exception code value for RTEs corresponding
*                       to exceptions.
*       01-29-92  GJF   Support for wildcard expansion in filenames on the
*                       command line.
*       02-14-92  GJF   Moved file inheritance stuff to ioinit.c. Call to
*                       inherit() is replace by call to _ioinit().
*       08-26-92  SKS   Add _osver, _winver, _winmajor, _winminor
*       09-04-92  GJF   Replaced _CALLTYPE3 with WINAPI.
*       09-30-92  SRW   Call _heap_init before _mtinit
*       10-19-92  SKS   Add "dowildcard" parameter to GetMainArgs()
*                       Prepend a second "_" to name since it is internal-only
*       03-20-93  SKS   Remove obsolete variables _osmode, _cpumode, etc.
*       04-06-93  SKS   Replace _CRTAPI* with _cdecl
*       04-07-93  SKS   Add _CRTIMP keyword for CRT DLL model
*                       Change __GetMainArgs to __getmainargs
*       04-13-93  SKS   Change call to _mtdeletelocks to new routine _mtterm
*       04-26-93  SKS   Change _CRTDLL_INIT to fail loading on failure to
*                       initialize/clean up, rather than calling _amsg_exit().
*       04-27-93  GJF   Removed support for _RT_STACK, _RT_INTDIV,
*                       _RT_INVALDISP and _RT_NONCONT.
*       05-06-93  SKS   Add call to _heap_term to free up all allocated memory
*                       *and* address space.  This must be the last thing done.
*       06-03-93  GJF   Added __proc_attached flag.
*       06-07-93  GJF   Incorporated SteveWo's code to call LoadLibrary, from
*                       crtdll.c.
*       11-05-93  CFW   Undefine GetEnviromentStrings.
*       11-09-93  GJF   Added call to __initmbctable (must happen before
*                       environment strings are processed).
*       11-09-93  GJF   Merged with NT SDK version (primarily the change of
*                       06-07-93 noted above). Also, replace MTHREAD with
*                       _MT.
*       11-23-93  CFW   GetEnviromentStrings undef moved to internal.h.
*       11-23-93  CFW   Wide char enable.
*       11-29-93  CFW   Wide environment.
*       12-02-93  CFW   Remove _UNICODE dependencies since only one version.
*       12-13-93  SKS   Free up per-thread CRT data on DLL_THREAD_DETACH
*                       with a call to _freeptd() in _CRT_INIT()
*       01-11-94  GJF   Use __GetMainArgs name when building libs for NT SDK.
*       02-07-94  CFW   POSIXify.
*       03-04-94  SKS   Add _newmode parameter to _*getmainargs (except NTSDK)
*       03-31-94  CFW   Use __crtGetEnvironmentStrings.
*       04-08-93  CFW   Move __crtXXX calls past initialization.
*       04-28-94  GJF   Major changes for Win32S support! Added
*                       AllocPerProcessDataStuct() to allocate and initialize
*                       the per-process data structure needed in the Win32s
*                       version of msvcrt*.dll. Also, added a function to
*                       free and access functions for all read-write global
*                       variables which might be used by a Win32s app.
*       05-04-94  GJF   Made access functions conditional on _M_IX86, added
*                       some comments to function headers, and fixed a
*                       possible bug in AllocPerProcessDataStruct (the return
*                       value for success was NOT explicitly set to something
*                       nonzero).
*       05-10-94  GJF   Added version check so that Win32 version (Win32s)
*                       will not load on Win32s (resp., Win32).
*       09-06-94  CFW   Remove _INTL switch.
*       09-06-94  CFW   Remove _MBCS_OS switch.
*       09-06-94  GJF   Added __error_mode and __app_type.
*       09-15-94  SKS   Clean up comments to avoid source release problems
*       09-21-94  SKS   Fix typo: no leading _ on "DLL_FOR_WIN32S"
*       10-04-94  CFW   Removed #ifdef _KANJI
*       10-04-94  BWT   Fix _NTSDK build
*       11-22-94  CFW   Must create wide environment if none.
*       12-19-94  GJF   Changed "MSVCRT20" to "MSVCRT30". Also, put testing for
*                       Win32S under #ifdef _M_IX86. Both changes from Richard
*                       Shupak.
*       01-16-95  CFW   Set default debug output for console.
*       02-13-95  GJF   Added initialization for the new _ppd_tzstd and
*                       _ppd_tzdst fields, thereby fixing the definition of
*                       _ppd__tzname.
*       02-15-95  CFW   Make all CRT message boxes look alike.
*       02-24-95  CFW   Use __crtMessageBoxA.
*       02-27-95  CFW   Change __crtMessageBoxA params.
*       03-08-95  GJF   Added initialization for _ppd__nstream. Removed
*                       _ppd_lastiob.
*       02-24-95  CFW   Call _CrtDumpMemoryLeaks.
*       04-06-95  CFW   Use __crtGetEnvironmentStringsA.
*       04-17-95  SKS   Free TLS index ppdindex when it is no longer needed
*       04-26-95  GJF   Added support for winheap in DLL_FOR_WIN32S build.
*       05-02-95  GJF   No _ppd__heap_maxregsize, _ppd__heap_regionsize or
*                       _ppd__heap_resetsize for WINHEAP.
*       05-24-95  CFW   Official ANSI C++ new handler added.
*       06-13-95  CFW   De-install client dump hook as client EXE/DLL is gone.
*       06-14-95  GJF   Changes for new lowio scheme (__pioinof[]) - no more
*                       per-process data initialization needed (Win32s) and
*                       added a call to _ioterm().
*       07-04-95  GJF   Interface to __crtGetEnvironmentStrings and _setenvp
*                       changes slightly.
*       06-27-95  CFW   Add win32s support for debug libs.
*       07-03-95  CFW   Changed offset of _lc_handle[LC_CTYPE], added sanity
*                       check to crtlib.c to catch changes to win32s.h that
*                       modify offset.
*       07-07-95  CFW   Simplify default report mode scheme.
*       07-25-95  CFW   Add win32s support for user visible debug heap vars.
*       08-21-95  SKS   (_ppd_)_CrtDbgMode needs to be initialized for Win32s
*       08-31-95  GJF   Added _dstbias.
*       11-09-95  GJF   Changed "ISTNT" to "IsTNT".
*       03-18-96  SKS   Add _fileinfo to variables implemented as functions.
*       04-22-96  GJF   Check for failure of heap initialization.
*       05-14-96  GJF   Changed where __proc_attached is set so that it 
*                       denotes successful completion of initialization.
*       06-11-96  JWM   Changed string "MSVCRT40" to "MSVCRT" in _CRTDLL_INIT().
*       06-27-96  GJF   Purged Win32s support. Note, the access functions must
*                       be retained for backwards compatibility.
*       06-28-96  SKS   Remove obsolete local variable "hmod".
*       03-17-97  RDK   Add reference to _mbcasemap.
*       07-24-97  GJF   heap_init changed slightly to support option to use 
*                       heap running directly on Win32 API.
*       08-08-97  GJF   Rearranged #ifdef-s so ptd is only defined when it is
*                       used (under ANSI_NEW_HANDLER).
*       10-02-98  GJF   Use GetVersionEx instead of GetVersion and store OS ID
*                       in _osplatform.
*       04-30-99  GJF   Don't clean up system resources if the whole process
*                       if terminating.
*       09-02-99  PML   Put Win32s check in system CRT only.
*       02-02-00  GB    Added ATTACH_THREAD support for _CRT_INIT where we
*                       initialise per thread data so that in case where we
*                       are short of memory, we don't have to kill the whole
*                       process for inavailablity of space.
*       08-22-00  GB    Fixed potentia leak of ptd in CRT_INIT
*       09-06-00  GB    Changed the function definations of _pctype and 
*                       _pwctype to const
*       03-16-01  PML   _alloca the OSVERSIONINFO so /GS can work (vs7#224261)
*       03-19-01  BWT   Add test to preclude msvcrt.dll loading on anything other
*                       than the OS it ships with.
*       03-26-01  PML   Use GetVersionExA, not GetVersionEx (vs7#230286)
*       03-27-01  PML   Fail DLL load instead of calling _amsg_exit, and
*                       propogate error on EXE arg parsing up (vs7#231220).
*       03-28-01  PML   Protect against GetModuleFileName overflow (vs7#231284)
*       04-05-01  PML   Clean up on DLL unload due to FreeLibrary, or on
*                       termination by ExitProcess instead of exit (vs7#235781)
*       04-30-01  BWT   Remove _NTSDK and just return false if the OS doesn't match
*       11-12-01  GB    Added support for new locale implementation.
*       12-12-01  BWT   getptd->getptd_noexit - deal with error here.
*       02-20-02  BWT   prefast fixes - don't use alloca
*       03-13-02  PML   Use FLS_SETVALUE, not TlsSetValue (vs7#338020).
*       06-19-02  MSL   Now we tell _cinit not to initialise floating point 
*                       precision, which will get done elsewhere if it needs to
*                       VS7 550455
*       10-04-02  PK    Added check for already initialized per thread data before 
*                       initializing (_CRTDLL_INIT)
*       11-18-02  GB    Move C terminators in CRTDLL_INIT.
*       01-27-04  SJ    Remove _fileinfo variable
*       04-28-04  AC    Readd _fileinfo variable, will remove in the next LKG.
*       07-30-04  AJS   Reremove _fileinfo variable for LKG9.
*       01-12-05  DJT   Use OS-encoded global function pointers
*       05-27-05  AC    Added check for manifest
*                       VSW#472671
*       06-06-05  PML   Call __security_init_cookie as early as possible,
*                       before any EH with cookie checks is registered.
*       06-10-05  AC    Fixed version check. Added error message for manifest check.
*                       VSW#505257
*       06-10-05  MSL   Manifest check w2k, and always compiled
*       07-26-05  GM    Fix for missing GetSystemWindowsDirectory on Win9x.
*       11-08-06  PMB   Remove support for Win9x and _osplatform and relatives.
*                       DDB#11325
*       04-20-07  GM    Removed __Detect_msvcrt_and_setup().
*       06-20-08  GM    Removed Fusion.
*
*******************************************************************************/

#if defined(CRTDLL)

#include <cruntime.h>
#include <oscalls.h>
#include <dos.h>
#include <internal.h>
#include <malloc.h>
#include <mbctype.h>
#include <mtdll.h>
#include <process.h>
#include <rterr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <awint.h>
#include <tchar.h>
#include <time.h>
#include <io.h>
#include <dbgint.h>
#ifdef _SYSCRT
#include <ntverp.h>
#endif
#include <setlocal.h>

/*
 * This header is included for _getdrives(). This function
 * is used to take dependency on msvcrt.dll
 */
#include <direct.h>

/*
 * flag set iff _CRTDLL_INIT was called with DLL_PROCESS_ATTACH
 */
static int proc_attached = 0;

#ifdef _WIN32_WCE
// Global static data
// CESYSGEN: These should be pulled in always
// This is the ONLY lock inited in DllMain. It is used to protect
// initialization of all the other globals below (incl the other locks)
// The fStdioInited flag tells us if init has been done
CRITICAL_SECTION csfStdioInitLock;
int fStdioInited;
#ifdef _WCE_CORECRT
int __locale_changed = 0;
#endif
#endif // _WIN32_WCE

/*
 * command line, environment, and a few other globals
 */
wchar_t *_wcmdln = NULL;           /* points to wide command line */
char *_acmdln = NULL;              /* points to command line */

char *_aenvptr = NULL;      /* points to environment block */
wchar_t *_wenvptr = NULL;   /* points to wide environment block */

extern int _newmode;    /* declared in <internal.h> */

static void __cdecl inherit(void);  /* local function */

/***
*int __[w]getmainargs - get values for args to main()
*
*Purpose:
*       This function invokes the command line parsing and copies the args
*       to main back through the passsed pointers. The reason for doing
*       this here, rather than having _CRTDLL_INIT do the work and exporting
*       the __argc and __argv, is to support the linked-in option to have
*       wildcard characters in filename arguments expanded.
*
*Entry:
*       int *pargc              - pointer to argc
*       _TCHAR ***pargv         - pointer to argv
*       _TCHAR ***penvp         - pointer to envp
*       int dowildcard          - flag (true means expand wildcards in cmd line)
*       _startupinfo * startinfo- other info to be passed to CRT DLL
*
*Exit:
*       Returns 0 on success, negative if _*setargv returns an error. Values
*       for the arguments to main() are copied through the passed pointers.
*
*******************************************************************************/
#ifdef _WCE_FULLCRT
#if     !defined(_POSIX_)
_CRTIMP int __cdecl __wgetmainargs (
        int *pargc,
        wchar_t ***pargv,
        wchar_t ***penvp,
        int dowildcard,
        _startupinfo * startinfo)
{
        int ret;

        /* set global new mode flag */
        _newmode = startinfo->newmode;

        if ( dowildcard )
            ret = __wsetargv(); /* do wildcard expansion after parsing args */
        else
            ret = _wsetargv();  /* NO wildcard expansion; just parse args */
        if (ret < 0)
#ifdef _SYSCRT
            ExitProcess(-1);      // Failed to parse the cmdline - bail
#else
            return ret;
#endif

        *pargc = __argc;
        *pargv = __wargv;

        /*
         * if wide environment does not already exist,
         * create it from multibyte environment
         */
        if (!_wenviron)
        {
            _wenvptr=__crtGetEnvironmentStringsW();

            if (_wsetenvp() < 0)
            {
                __mbtow_environ();
            }
        }

        *penvp = _wenviron;

        return ret;
}

#endif /* !defined(_POSIX_) */


_CRTIMP int __cdecl __getmainargs (
        int *pargc,
        char ***pargv,
        char ***penvp,
        int dowildcard,
        _startupinfo * startinfo
        )
{
        int ret;

        /* set global new mode flag */
        _newmode = startinfo->newmode;

        if ( dowildcard )
            ret = __setargv();  /* do wildcard expansion after parsing args */
        else
            ret = _setargv();   /* NO wildcard expansion; just parse args */
        if (ret < 0)
#ifdef _SYSCRT
            ExitProcess(-1);      // Failed to parse the cmdline - bail
#else
            return ret;
#endif

        *pargc = __argc;
        *pargv = __argv;
        *penvp = _environ;

        return ret;
}

#endif // _WCE_FULLCRT
#if 0   /* this comment block is stripped in released source */
/*
 * The reason for adding this dummy function is to speed up CRT initialization/cleanup through BBT optimization.
 * BBT provides an instrumentation option /endboot where you can specify the first function executed after startup
 * so that the optimization process will put more weight on the profile data collected from startup scenarios up to
 * the call of that function. See Dev10#716026.
 */
#endif
#pragma optimize ("", off)
void _CrtEndBoot()
{
        /* do nothing, used to mark the end of the init process */
}
#pragma optimize ("", on)

/***
*BOOL _CRTDLL_INIT(hDllHandle, dwReason, lpreserved) - C DLL initialization.
*
*Purpose:
*       This routine does the C runtime initialization.
*
*Entry:
*
*Exit:
*
*******************************************************************************/

typedef void (__stdcall *NTVERSION_INFO_FCN)(PDWORD, PDWORD, PDWORD);

static
BOOL __cdecl
__CRTDLL_INIT(
        HANDLE  hDllHandle,
        DWORD   dwReason,
        LPVOID  lpreserved
        );

BOOL WINAPI
_CRTDLL_INIT(
        HANDLE  hDllHandle,
        DWORD   dwReason,
        LPVOID  lpreserved
        )
{
    if (dwReason == DLL_PROCESS_ATTACH)
    {
        /*
         * The /GS security cookie must be initialized before any exception
         * handling targetting the current image is registered.  No function
         * using exception handling can be called in the current image until
         * after __security_init_cookie has been called.
         */
        __security_init_cookie();
    }

    return __CRTDLL_INIT(hDllHandle, dwReason, lpreserved);
}

__declspec(noinline)
BOOL __cdecl
__CRTDLL_INIT(
        HANDLE  hDllHandle,
        DWORD   dwReason,
        LPVOID  lpreserved
        )
{
        if ( dwReason == DLL_PROCESS_ATTACH ) {
            if ( !_heap_init() )    /* initialize heap */
                /*
                 * The heap cannot be initialized, return failure to the 
                 * loader.
                 */
                return FALSE;

            if(!_mtinit())          /* initialize multi-thread */
            {
                /*
                 * If the DLL load is going to fail, we must clean up
                 * all resources that have already been allocated.
                 */
                _heap_term();       /* heap is now invalid! */

                return FALSE;       /* fail DLL load on failure */
            }
#if defined(_WIN32_WCE) && defined(_WCE_FULLCRT)
            InitializeCriticalSection(&csfStdioInitLock);
            fStdioInited = FALSE;
#endif

#if !defined(_WIN32_WCE) || defined(_WCE_FULLCRT)
            _aenvptr = (char *)__crtGetEnvironmentStringsA();
            _ioinit0();
#endif

#ifndef _WIN32_WCE
            _acmdln = GetCommandLineA();
#ifndef _POSIX_
            _wcmdln = GetCommandLineW();
#endif /* _POSIX_ */
#endif // _WIN32_WCE

#ifdef  _MBCS
            /*
             * Initialize multibyte ctype table. Always done since it is
             * needed for processing the environment strings.
             */
            __initmbctable();
#endif

            if (_setenvp() < 0 ||   /* get environ info */
                _cinit(FALSE) != 0)  /* do C data initialize */
            {
#if !defined(_WIN32_WCE) || defined(_WCE_FULLCRT)
                /* shut down lowio */
                _ioterm();
#endif
                
#if defined(_WIN32_WCE) && defined(_WCE_FULLCRT)
				DeleteCriticalSection(&csfStdioInitLock);
#endif
                _mtterm();          /* free TLS index, call _mtdeletelocks() */
                _heap_term();       /* heap is now invalid! */
                return FALSE;       /* fail DLL load on failure */
            }


            /*
             * Increment flag indicating process attach notification
             * has been received.
             */
            proc_attached++;

        }
        else if ( dwReason == DLL_PROCESS_DETACH ) {
            /*
             * if a client process is detaching, make sure minimal
             * runtime termination is performed and clean up our
             * 'locks' (i.e., delete critical sections).
             */
            if ( proc_attached > 0 ) {
                proc_attached--;
                __try {

                    /*
                     * Any basic clean-up done here may also need
                     * to be done below if Process Attach is partly
                     * processed and then a failure is encountered.
                     */

                    if ( _C_Termination_Done == FALSE )
                        _cexit();

                    // TODO: VS11 Porting: We don't need this as _cexit() 
                    // will call static terminators and calling it again may
                    // cause Access violations
#ifndef _WIN32_WCE
                    __crtdll_callstaticterminators();
#endif

                    /* Free all allocated CRT memory */
                    __freeCrtMemory();

#ifdef  _DEBUG
                    /* Dump all memory leaks */
                    if (_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) & _CRTDBG_LEAK_CHECK_DF)
                    {
                        _CrtSetDumpClient(NULL);
                        _CrtDumpMemoryLeaks();
                    }
#endif

                    /*
                     * What remains is to clean up the system resources we have
                     * used (handles, critical sections, memory,...,etc.). This 
                     * needs to be done if the whole process is NOT terminating.
                     */

                    /* If dwReason is DLL_PROCESS_DETACH, lpreserved is NULL 
                     * if FreeLibrary has been called or the DLL load failed 
                     * and non-NULL if the process is terminating.
                     */
                    if ( lpreserved == NULL )
                    {
                        /*
                         * The process is NOT terminating so we must clean up...
                         */
#if !defined(_WIN32_WCE) || defined(_WCE_FULLCRT)
                        _ioterm();
#endif
#if defined(_WIN32_WCE) && defined(_WCE_FULLCRT)
						DeleteCriticalSection(&csfStdioInitLock);
#endif

                        /* free TLS index, call _mtdeletelocks() */
                        _mtterm();

                        /* This should be the last thing the C run-time does */
                        _heap_term();   /* heap is now invalid! */
                    }

                }
                __finally {
                    /* we shouldn't really have to care about this, because
                       letting an exception escape from DllMain(DLL_PROCESS_DETACH) should
                       result in process termination. Unfortunately, Windows up to Win7 as of now
                       just silently swallows the exception.

                       I have considered all kinds of tricks, but decided to leave it up to OS
                       folks to fix this.

                       For the time being just remove our FLS callback during phase 2 unwind
                       that would otherwise be left pointing to unmapped address space.
                       */
#ifndef _WIN32_WCE // CE doesn't use __flsindex
                     if ( lpreserved == NULL && __flsindex != FLS_OUT_OF_INDEXES )
                         _mtterm();
#endif // _WIN32_WCE
                }
            }
            else
                /* no prior process attach, just return */
                return FALSE;
        }
#ifndef _WIN32_WCE // VS11 porting: This is not reached in CE as CoreDllInit in
				// coredll.c calls DisableThreadLibraryCalls().
        else if ( dwReason == DLL_THREAD_ATTACH )
        {
            _ptiddata ptd;

            if ( (ptd = __crtFlsGetValue(__flsindex)) == NULL)
            {
                if ( ((ptd = _calloc_crt(1, sizeof(struct _tiddata))) != NULL))
                {
                    if (__crtFlsSetValue(__flsindex, (LPVOID)ptd) ) {
                        /*
                        * Initialize of per-thread data
                        */
                        _initptd(ptd, NULL);
                        
                        ptd->_tid = GetCurrentThreadId();
                        ptd->_thandle = (uintptr_t)(-1);
                    } else
                    {
                        _free_crt(ptd);
                        return FALSE;
                    }
                } else
                {
                    return FALSE;
                }
            }
        }
        else if ( dwReason == DLL_THREAD_DETACH )
        {
            _freeptd(NULL);     /* free up per-thread CRT data */
        }
#endif
        /* See above declaration of _CrtEndBoot */
        _CrtEndBoot();

        return TRUE;
}

BOOL WINAPI _CRTDLL_INIT(HANDLE hDllHandle, DWORD dwReason, LPVOID lpreserved);

BOOL WINAPI CrtDllInit (HANDLE  hinstDLL, DWORD fdwReason, DWORD reserved)
{
    
    return _CRTDLL_INIT(hinstDLL, fdwReason, 0);
}

#ifdef _WCE_FULLCRT
#if !defined(SHIP_BUILD)
#include <dbgapi.h>

DBGPARAM dpCurSettings = 
{ 
    L"MSVCRT", 
    {
        L"FixHeap"    
        L"LocalMem",  
        L"Mov",       
        L"HeapChecks",
        L"VirtMem",    
        L"Devices",   
        L"Deprecated API", 
        L"Loader",
        L"Stdio",   
        L"Stdio HiFreq", 
        L"Shell APIs", 
        L"Imm/SEH",
        L"Heap Validate",  
        L"RemoteMem", 
        L"VerboseHeap", 
        L"Undefined" 
    },
    0x00000000 
};
#endif // SHIP_BUILD

/*
 * Earlier the below exports were only for X86, but with invent of /clr:pure,
 * they need to be defined for all the platform.
 */

/*
 * Functions to access user-visible, per-process variables
 */

/*
 * Macro to construct the name of the access function from the variable
 * name.
 */
#define AFNAME(var) __p_ ## var

/*
 * Macro to construct the access function's return value from the variable 
 * name.
 */
#define AFRET(var)  &var

/*
 ***
 ***  Template
 ***

_CRTIMP __cdecl
AFNAME() (void)
{
        return AFRET();
}

 ***
 ***
 ***
 */

#ifdef  _DEBUG

_CRTIMP long *
AFNAME(_crtAssertBusy) (void)
{
        return AFRET(_crtAssertBusy);
}

_CRTIMP long *
AFNAME(_crtBreakAlloc) (void)
{
        return AFRET(_crtBreakAlloc);
}

_CRTIMP int *
AFNAME(_crtDbgFlag) (void)
{
        return AFRET(_crtDbgFlag);
}

#endif  /* _DEBUG */

_CRTIMP char ** __cdecl
AFNAME(_acmdln) (void)
{
        return AFRET(_acmdln);
}

_CRTIMP wchar_t ** __cdecl
AFNAME(_wcmdln) (void)
{
        return AFRET(_wcmdln);
}

_CRTIMP int * __cdecl
AFNAME(__argc) (void)
{
        return AFRET(__argc);
}

_CRTIMP char *** __cdecl
AFNAME(__argv) (void)
{
        return AFRET(__argv);
}

_CRTIMP wchar_t *** __cdecl
AFNAME(__wargv) (void)
{
        return AFRET(__wargv);
}

_CRTIMP int * __cdecl
AFNAME(_commode) (void)
{
        return AFRET(_commode);
}

_CRTIMP int * __cdecl
AFNAME(_daylight) (void)
{
_BEGIN_SECURE_CRT_DEPRECATION_DISABLE
        return AFRET(_daylight);
_END_SECURE_CRT_DEPRECATION_DISABLE
}

_CRTIMP long * __cdecl
AFNAME(_dstbias) (void)
{
_BEGIN_SECURE_CRT_DEPRECATION_DISABLE
        return AFRET(_dstbias);
_END_SECURE_CRT_DEPRECATION_DISABLE
}

_CRTIMP char *** __cdecl
AFNAME(_environ) (void)
{
        return AFRET(_environ);
}

_CRTIMP wchar_t *** __cdecl
AFNAME(_wenviron) (void)
{
        return AFRET(_wenviron);
}

_CRTIMP int * __cdecl
AFNAME(_fmode) (void)
{
_BEGIN_SECURE_CRT_DEPRECATION_DISABLE
        return AFRET(_fmode);
_END_SECURE_CRT_DEPRECATION_DISABLE
}

_CRTIMP char *** __cdecl
AFNAME(__initenv) (void)
{
        return AFRET(__initenv);
}

_CRTIMP wchar_t *** __cdecl
AFNAME(__winitenv) (void)
{
        return AFRET(__winitenv);
}

_CRTIMP FILE *
AFNAME(_iob) (void)
{
        return &_iob[0];
}

_CRTIMP unsigned char * __cdecl
AFNAME(_mbctype) (void)
{
        return &_mbctype[0];
}

_CRTIMP unsigned char * __cdecl
AFNAME(_mbcasemap) (void)
{
        return &_mbcasemap[0];
}

#if _PTD_LOCALE_SUPPORT
_CRTIMP int * __cdecl
AFNAME(__mb_cur_max) (void)
{
        _ptiddata ptd = _getptd();
        pthreadlocinfo ptloci = ptd->ptlocinfo;

        __UPDATE_LOCALE(ptd, ptloci);
        return AFRET(__MB_CUR_MAX(ptloci));
}

_CRTIMP const unsigned short ** __cdecl
AFNAME(_pctype) (void)
{
        _ptiddata ptd = _getptd();
        pthreadlocinfo ptloci = ptd->ptlocinfo;

        __UPDATE_LOCALE(ptd, ptloci);
        return AFRET(ptloci->pctype);
}
#endif  /* _PTD_LOCALE_SUPPORT */

_CRTIMP const unsigned short ** __cdecl
AFNAME(_pwctype) (void)
{
        return AFRET(_pwctype);
}

_CRTIMP char **  __cdecl
AFNAME(_pgmptr) (void)
{
_BEGIN_SECURE_CRT_DEPRECATION_DISABLE
        return AFRET(_pgmptr);
_END_SECURE_CRT_DEPRECATION_DISABLE
}

_CRTIMP wchar_t ** __cdecl
AFNAME(_wpgmptr) (void)
{
_BEGIN_SECURE_CRT_DEPRECATION_DISABLE
        return AFRET(_wpgmptr);
_END_SECURE_CRT_DEPRECATION_DISABLE
}

_CRTIMP long * __cdecl
AFNAME(_timezone) (void)
{
_BEGIN_SECURE_CRT_DEPRECATION_DISABLE
        return AFRET(_timezone);
_END_SECURE_CRT_DEPRECATION_DISABLE
}

_CRTIMP char ** __cdecl
AFNAME(_tzname) (void)
{
_BEGIN_SECURE_CRT_DEPRECATION_DISABLE
        return &_tzname[0];
_END_SECURE_CRT_DEPRECATION_DISABLE
}
#endif /* _WCE_FULLCRT */
#endif  /* CRTDLL */
