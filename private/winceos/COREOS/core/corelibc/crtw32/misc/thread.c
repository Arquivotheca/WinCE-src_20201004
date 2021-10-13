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
*thread.c - Begin and end a thread
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       This source contains the _beginthread() and _endthread()
*       routines which are used to start and terminate a thread.
*
*Revision History:
*       05-09-90  JCR   Translated from ASM to C
*       07-25-90  SBM   Removed '32' from API names
*       10-08-90  GJF   New-style function declarators.
*       10-09-90  GJF   Thread ids are of type unsigned long.
*       10-19-90  GJF   Added code to set _stkhqq properly in stub().
*       12-04-90  SRW   Changed to include <oscalls.h> instead of <doscalls.h>
*       12-06-90  SRW   Added _CRUISER_ and _WIN32 conditionals.
*       06-03-91  GJF   Win32 version [_WIN32_].
*       07-18-91  GJF   Fixed many silly errors [_WIN32_].
*       08-19-91  GJF   Allow for newly created thread terminating before
*                       _beginthread returns
*       09-30-91  GJF   Add per-thread initialization and termination calls
*                       for floating point.
*       01-18-92  GJF   Revised try - except statement.
*       02-25-92  GJF   Initialize _holdrand field to 1.
*       09-30-92  SRW   Add WINAPI keyword to _threadstart routine
*       10-30-92  GJF   Error ret for CreateThread is 0 (NULL), not -1.
*       02-13-93  GJF   Revised to use TLS API. Also, purged Cruiser support.
*       03-26-93  GJF   Fixed horribly embarrassing bug: ptd->pxcptacttab
*                       must be initialized to _XcptActTab!
*       04-01-93  CFW   Change try-except to __try-__except
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       04-27-93  GJF   Removed support for _RT_STACK, _RT_INTDIV,
*                       _RT_INVALDISP and _RT_NONCONT.
*       10-26-93  GJF   Replaced PF with _PVFV (defined in internal.h).
*       12-13-93  SKS   Free up per-thread data using a call to _freeptd()
*       01-06-94  GJF   Free up _tiddata struct upon failure in _beginthread.
*                       Also, set errno on failure.
*       01-10-95  CFW   Debug CRT allocs.
*       04-18-95  SKS   Add 5 MIPS per-thread variables.
*       05-02-95  SKS   Call _initptd for initialization of per-thread data.
*       02-03-98  GJF   Changes for Win64: use uintptr_t type for anything with
*                       a HANDLE value.
*       02-02-00  GB    Modified threadstart() to prevent leaking of ptd
*                       allocated during call to getptd while ATTACHing THREAD 
*                       in dlls.
*       08-04-00  PML   Set EINVAL error if thread start address null in
*                       _beginthread (VS7#118688).
*       10-16-01  GB    Added fiber support
*       12-11-01  BWT   _getptd doesn't return NULL.  Change to _getptd_noexit
*                       and don't terminate the process if it can't be allocated
*                       in endthreadex - just exit the thread.
*       12-11-01  BWT   Also, in threadstart - don't exit the process if FlsSetValue
*                       fails - exit the thread instead - this isn't a fatal condition.
*       11-02-03  AC    Added validation.
*       04-07-04  GB    Added support for beginthread et. al. for /clr:pure.
*       11-07-04  MSL   Domain safety
*       01-28-05  SJ    Msclr headers now copied into correct subdirectories 
*                       inside crt_bld.
*       05-24-05  PML   Make sure unencoded global func-ptrs are in .rdata
*                       (vsw#191114)
*
*******************************************************************************/

#include <cruntime.h>
#include <oscalls.h>
#include <internal.h>
#include <mtdll.h>
#include <msdos.h>
#include <malloc.h>
#include <process.h>
#include <stddef.h>
#include <rterr.h>
#include <dbgint.h>
#include <errno.h>
#include <awint.h>
#if defined(_M_CEE) || defined(MRTDLL)
#include <msclr\appdomain.h>
#endif

#pragma warning(disable:4439)	// C4439: function with a managed parameter must have a __clrcall calling convention

/*
 * Startup code for new thread.
 */
static unsigned long WINAPI _threadstart(void *);
static void _callthreadstart(void);

#if defined(_M_CEE) || defined(MRTDLL)
static int _getdomain(DWORD *pDomain)
{
    using System::Runtime::InteropServices::RuntimeEnvironment;

    *pDomain=0;

    // Throws HR exception on failure.
    ICLRRuntimeHost *pClrHost = NULL;
    pClrHost = reinterpret_cast<ICLRRuntimeHost*>(
        RuntimeEnvironment::GetRuntimeInterfaceAsIntPtr(
            msclr::_detail::FromGUID(CLSID_CLRRuntimeHost),
            msclr::_detail::FromGUID(IID_ICLRRuntimeHost)).ToPointer());
    
    DWORD domain=0;
    HRESULT hr = pClrHost->GetCurrentAppDomainId(&domain);
    pClrHost->Release();
    pClrHost=NULL;
    if (FAILED(hr))
    {
        return false;
    }
    *pDomain=domain;
    return true;
}
#endif

/***
*_beginthread() - Create a child thread
*
*Purpose:
*       Create a child thread.
*
*Entry:
*       initialcode = pointer to thread's startup code address
*       stacksize = size of stack
*       argument = argument to be passed to new thread
*
*Exit:
*       success = handle for new thread if successful
*
*       failure = (unsigned long) -1L in case of error, errno and _doserrno
*                 are set
*
*Exceptions:
*
*******************************************************************************/

_CRTIMP uintptr_t __cdecl _beginthread (
        void (__cdecl * initialcode) (void *),
        unsigned stacksize,
        void * argument
        )
{
        _ptiddata ptd;                  /* pointer to per-thread data */
        uintptr_t thdl;                 /* thread handle */
        unsigned long err = 0L;     /* Return from GetLastError() */

        /* validation section */
        _VALIDATE_RETURN(initialcode != NULL, EINVAL, -1);

        /*
         * Allocate and initialize a per-thread data structure for the to-
         * be-created thread.
         */
        if ( (ptd = (_ptiddata)_calloc_crt(1, sizeof(struct _tiddata))) == NULL )
        {
            goto error_return;
        }

        /*
         * Initialize the per-thread data
         */

#if _PTD_LOCALE_SUPPORT
        _initptd(ptd, _getptd()->ptlocinfo);
#else
        _initptd(ptd, NULL);
#endif  /* _PTD_LOCALE_SUPPORT */

        ptd->_initaddr = (void *) initialcode;
        ptd->_initarg = argument;

#if defined(_M_CEE) || defined(MRTDLL)
        if(!_getdomain(&(ptd->__initDomain)))
        {
            goto error_return;
        }
#endif

        /*
         * Create the new thread. Bring it up in a suspended state so that
         * the _thandle and _tid fields are filled in before execution
         * starts.
         */
        if ( (ptd->_thandle = thdl = (uintptr_t)
              CreateThread( NULL,
                            stacksize,
                            _threadstart,
                            (LPVOID)ptd,
                            CREATE_SUSPENDED,
                            (LPDWORD)&(ptd->_tid) ))
             == (uintptr_t)0 )
        {
                err = GetLastError();
                goto error_return;
        }

        /*
         * Start the new thread executing
         */
        if ( ResumeThread( (HANDLE)thdl ) == (DWORD)(-1) ) {
                err = GetLastError();
                goto error_return;
        }

        /*
         * Good return
         */
        return(thdl);

        /*
         * Error return
         */
error_return:
        /*
         * Either ptd is NULL, or it points to the no-longer-necessary block
         * calloc-ed for the _tiddata struct which should now be freed up.
         */
        _free_crt(ptd);

        /*
         * Map the error, if necessary.
         */
        if ( err != 0L )
                _dosmaperr(err);

        return( (uintptr_t)(-1) );
}


/***
*_threadstart() - New thread begins here
*
*Purpose:
*       The new thread begins execution here.  This routine, in turn,
*       passes control to the user's code.
*
*Entry:
*       void *ptd       = pointer to _tiddata structure for this thread
*
*Exit:
*       Never returns - terminates thread!
*
*Exceptions:
*
*******************************************************************************/

static unsigned long WINAPI _threadstart (
        void * ptd
        )
{

        _ptiddata _ptd;                  /* pointer to per-thread data */
        
        /* 
         * Check if ptd is initialised during THREAD_ATTACH call to dll mains
         */
        if ( (_ptd = (_ptiddata)__crtFlsGetValue(__get_flsindex())) == NULL)
        {
            /*
             * Stash the pointer to the per-thread data stucture in TLS
             */
            if ( !__crtFlsSetValue(__get_flsindex(), ptd) )
            {
                ExitThread(GetLastError());
            }
            _ptd = ptd;
        }
        else
        {
            _ptd->_initaddr = ((_ptiddata) ptd)->_initaddr;
            _ptd->_initarg =  ((_ptiddata) ptd)->_initarg;
            _ptd->_thandle =  ((_ptiddata) ptd)->_thandle;
#if defined(_M_CEE) || defined(MRTDLL)
            _ptd->__initDomain=((_ptiddata) ptd)->__initDomain;
#endif
            _freefls(ptd);
            ptd = _ptd;
        }

#if defined(_M_CEE) || defined(MRTDLL)
        DWORD domain=0;
        if(!_getdomain(&domain))
        {
            ExitThread(0);
        }
        if(domain!=_ptd->__initDomain)
        {
            /* need to transition to caller's domain and startup there*/
            ::msclr::call_in_appdomain(_ptd->__initDomain, _callthreadstart);

            return 0L;
        }
#endif

        _callthreadstart();

        return(0L);
}

static void _callthreadstart(void)
{
    _ptiddata ptd;           /* pointer to thread's _tiddata struct */

    /* must always exist at this point */
    ptd = _getptd();

    /*
     * Guard call to user code with a _try - _except statement to
     * implement runtime errors and signal support
     */
    __try 
    {
        ( (void(__CLRCALL_OR_CDECL *)(void *))(((_ptiddata)ptd)->_initaddr) )
            ( ((_ptiddata)ptd)->_initarg );

        _endthread();
    }
    __except ( _XcptFilter(GetExceptionCode(), GetExceptionInformation()) )
    {
            /*
                * Should never reach here
                */
            _exit( GetExceptionCode() );

    } /* end of _try - _except */

}



#ifndef MRTDLL

/***
*_endthread() - Terminate the calling thread
*
*Purpose:
*
*Entry:
*       void
*
*Exit:
*       Never returns!
*
*Exceptions:
*
*******************************************************************************/

void __cdecl _endthread (
        void
        )
{
/*
 * For CE, the ptd is cleaned up via the
 * callback function passed in CeTlsAlloc()
 */
#ifndef _WIN32_WCE
        _ptiddata ptd;           /* pointer to thread's _tiddata struct */

        ptd = _getptd_noexit();
        if (ptd) {
            /*
             * Close the thread handle (if there was one)
             */
            if ( ptd->_thandle != (uintptr_t)(-1) )
                    (void) CloseHandle( (HANDLE)(ptd->_thandle) );
   
            /*
             * Free up the _tiddata structure & its subordinate buffers
             *      _freeptd() will also clear the value for this thread
             *      of the FLS variable __flsindex.
             */
            _freeptd(ptd);
         }
#endif

        /*
         * Terminate the thread
         */
        ExitThread(0);

}

#endif
