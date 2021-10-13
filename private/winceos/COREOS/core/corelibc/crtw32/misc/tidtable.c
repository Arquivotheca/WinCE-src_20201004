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
*tidtable.c - Access thread data table
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       This module contains the following routines for multi-thread
*       data support:
*
*       _mtinit     = Initialize the mthread data
*       _getptd     = get the pointer to the per-thread data structure for
*                       the current thread
*       _freeptd    = free up a per-thread data structure and its
*                       subordinate structures
*       __threadid  = return thread ID for the current thread
*       __threadhandle = return pseudo-handle for the current thread
*
*Revision History:
*       05-04-90  JCR   Translated from ASM to C for portable 32-bit OS/2
*       06-04-90  GJF   Changed error message interface.
*       07-02-90  GJF   Changed __threadid() for DCR 1024/2012.
*       08-08-90  GJF   Removed 32 from API names.
*       10-08-90  GJF   New-style function declarators.
*       10-09-90  GJF   Thread ids are of type unsigned long! Also, fixed a
*                       bug in __threadid().
*       10-22-90  GJF   Another bug in __threadid().
*       12-04-90  SRW   Changed to include <oscalls.h> instead of <doscalls.h>
*       12-06-90  SRW   Added _CRUISER_ and _WIN32 conditionals.
*       05-31-91  GJF   Win32 version [_WIN32_].
*       07-18-91  GJF   Fixed many silly errors [_WIN32_].
*       09-29-91  GJF   Conditionally added __getptd_lk/__getptd1_lk so that
*                       DEBUG version of mlock doesn't infinitely recurse
*                       the first time _THREADDATA_LOCK is asserted [_WIN32_].
*       01-30-92  GJF   Must init. _pxcptacttab field to _XcptActTab.
*       02-25-92  GJF   Initialize _holdrand field to 1.
*       02-13-93  GJF   Revised to use TLS API. Also, purged Cruiser support.
*       03-26-93  GJF   Initialize ptd->_holdrand to 1L (see thread.c).
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       04-13-93  SKS   Add _mtterm to do multi-thread termination
*                       Set freed __flsindex to -1 again to prevent mis-use
*       04-26-93  SKS   _mtinit now returns 0 or 1, no longer calls _amsg_exit
*       12-13-93  SKS   Add _freeptd(), which frees up the per-thread data
*                       maintained by the C run-time library.
*       04-12-94  GJF   Made definition of __flsindex conditional on ndef
*                       DLL_FOR_WIN32S. Also, replaced MTHREAD with _MT.
*       01-10-95  CFW   Debug CRT allocs.
*       04-12-95  DAK   Added NT kernel support for C++ exceptions
*       04-18-95  SKS   Add 5 MIPS per-thread variables.
*       04-25-95  DAK   More Kernel EH support
*       05-02-95  SKS   add _initptd() to do initialization of per-thread data
*       05-24-95  CFW   Add _defnewh.
*       06-12-95  JWM   _getptd() now preserves LastError.
*       01-17-97  GJF   _freeptd() must free the thread's copy of the
*                       exception-action table.
*       09-26-97  BWT   Fix NTSUBSET
*       02-03-98  GJF   Changes for Win64: use uintptr_t type for anything with
*                       a HANDLE value.
*       04-27-98  GJF   Added support for per-thread mbc information.
*       07-28-98  JWM   Initialize __pceh (new per-thread data for comerr support).
*       09-03-98  GJF   Added support for per-thread locale information.
*       12-04-98  JWM   Pulled all comerr support.
*       12-08-98  GJF   In _freeptd, fixed several errors in cleaning up the 
*                       threadlocinfo.
*       12-18-98  GJF   Fixed one more error in _freeptd.
*       01-18-99  GJF   Take care not to free up __ptlocinfo when a thread 
*                       exits.
*       03-16-99  GJF   threadlocinfo incorporates more reference counters
*       04-24-99  PML   Added __lconv_intl_refcount
*       04-28-99  PML   Wrap __declspec(allocate()) in _CRTALLOC macro.
*       11-03-99  RDL   Win64 _NTSUBSET_ warning fix.
*       06-08-00  PML   No need to keep per-thread mbcinfo in circular linked
*                       list.  Also, don't free mbcinfo if it's also the global
*                       info (vs7#118174).
*       02-20-01  PML   vs7#172586 Avoid _RT_LOCK by preallocating all locks
*                       that will be required, and returning failure back on
*                       inability to allocate a lock.
*       06-12-01  BWT   ntbug: 414059 - cleanup from mtinit failure
*       07-15-01  PML   Remove all ALPHA, MIPS, and PPC code
*       10-16-01  GB    Added fiber support
*       11-12-01  GB    Added support for new locale implementation.
*       12-07-01  BWT   Remove NTSUBSET support and add _getptd_noexit for cases
*                       when the caller has the ability to return ENOMEM failures.
*       04-02-02  GB    VS7#508599 - FLS and TLS routines will always be redirected
*                       by function pointers.
*       05-20-02  MSL   Added support for _set_purecall_handler
*       07-02-02  GB    Added initialisation code for _setloc_data in initptd.
*       04-01-04  SJ    Design Changes in perthread locale - VSW#141214
*       04-07-04  GB    Added support for beginthread et. al. for /clr:pure.
*                       __fls_setvalue, __fls_getvalue, __get_flsindex are added.
*       02-03-05  AC    Initialize per-thread function pointers
*                       VSW#449539
*       04-03-05  PAL   Free the ptd and return NULL if FlsSetValue fails. (VSW 470625)
*       05-05-05  JL    Store FlsGetValue function pointer in TLS to enhance
*                       security while minimizing performance impact
*       05-11-05  JL    Don't use FLS global function pointers for AMD64 because
*                       they will always be available
*       05-11-05  MSL   fls init fixes
*       01-05-06  GM    __set_flsgetvalue() should return FLS value to avoid
*                       querying for it again later (perf improvement).
*	07-12-07  ATC   ddb#123388 - remove GetProcAddress for Encode/DecodePointer and 
*	                FLS* APIs for amd64 (but x86 remains the same)
*
*******************************************************************************/

#include <sect_attribs.h>
#include <cruntime.h>
#include <oscalls.h>
#include <internal.h>
#ifndef _WIN32_WCE
#include <mtdll.h>
#else
#include <corecrtstorage.h>
#endif
#include <memory.h>
#include <msdos.h>
#include <rterr.h>
#include <stdlib.h>
#include <stddef.h>
#include <dbgint.h>
#include <setlocal.h>
#include <mbstring.h>
#include <awint.h>

extern pthreadmbcinfo __ptmbcinfo;

extern threadlocinfo __initiallocinfo;
extern threadmbcinfo __initialmbcinfo;
extern pthreadlocinfo __ptlocinfo;

void * __cdecl __removelocaleref( pthreadlocinfo);
void __cdecl __addlocaleref( pthreadlocinfo);
void __cdecl __freetlocinfo(pthreadlocinfo);

unsigned long __flsindex = FLS_OUT_OF_INDEXES;

/***
* __get_flsindex - crt wrapper around __flsindex
*
* Purpose:
*       This function helps msvcmXX.dll beginthread and beginthreadex APIs
*       to retrive __flsindex value.
*
*******************************************************************************/

_CRTIMP unsigned long __cdecl __get_flsindex()
{
    return __flsindex;
}

/****
*_mtinit() - Init multi-thread data bases
*
*Purpose:
*       (1) Call _mtinitlocks to create/open all lock semaphores.
*       (2) Allocate a TLS index to hold pointers to per-thread data
*           structure.
*
*       NOTES:
*       (1) Only to be called ONCE at startup
*       (2) Must be called BEFORE any mthread requests are made
*
*Entry:
*       <NONE>
*Exit:
*       returns FALSE on failure
*
*Uses:
*       <any registers may be modified at init time>
*
*Exceptions:
*
*******************************************************************************/
int __cdecl _mtinit (
        void
        )
{
#ifndef _WIN32_WCE
        // Per Thread data initialization done lazily for CE
        _ptiddata ptd;
#endif

        _init_pointers();       /* initialize global function pointers */

        /*
         * Initialize the mthread lock data base
         */

        if ( !_mtinitlocks() ) {
            _mtterm();
            return FALSE;       /* fail to load DLL */
        }

#ifndef _WIN32_WCE
        /*
         * Allocate a TLS index to maintain pointers to per-thread data
         */
        if ( (__flsindex = __crtFlsAlloc(&_freefls)) == FLS_OUT_OF_INDEXES ) {
            _mtterm();
            return FALSE;       /* fail to load DLL */
        }

        /*
         * Create a per-thread data structure for this (i.e., the startup)
         * thread.
         */
        if ( ((ptd = _calloc_crt(1, sizeof(struct _tiddata))) == NULL) ||
             !__crtFlsSetValue(__flsindex, (LPVOID)ptd) ) 
        {
            _mtterm();
            return FALSE;       /* fail to load DLL */
        }

        /*
         * Initialize the per-thread data
         */
        _initptd(ptd,NULL);

        ptd->_tid = GetCurrentThreadId();
        ptd->_thandle = (uintptr_t)(-1);
#elif defined(_WCE_FULLCRT)
        if ( (__flsindex = __flsindex = CeTlsAlloc(_freeptd_ce)) == FLS_OUT_OF_INDEXES ) {
            _mtterm();
            return FALSE;       /* fail to load DLL */
        }
#endif
        return TRUE;
}


/****
*_mtterm() - Clean-up multi-thread data bases
*
*Purpose:
*       (1) Call _mtdeletelocks to free up all lock semaphores.
*       (2) Free up the TLS index used to hold pointers to
*           per-thread data structure.
*
*       NOTES:
*       (1) Only to be called ONCE at termination
*       (2) Must be called AFTER all mthread requests are made
*
*Entry:
*       <NONE>
*Exit:
*       returns
*
*Uses:
*
*Exceptions:
*
*******************************************************************************/

void __cdecl _mtterm (
        void
        )
{
#ifndef _WIN32_WCE
    /*
     * Free up the TLS index
     *
     * (Set __flsindex back to initial state (-1L).)
     */

    if ( __flsindex != FLS_OUT_OF_INDEXES ) {
        __crtFlsFree(__flsindex);
        __flsindex = FLS_OUT_OF_INDEXES;
    }
#endif
    /*
     * Clean up the mthread lock data base
     */

    _mtdeletelocks();
}

#if !defined(_WIN32_WCE) || defined(_WCE_FULLCRT)
/***
*void _initptd(_ptiddata ptd, pthreadlocinfo) - initialize a per-thread data structure
*
*Purpose:
*       This routine handles all of the per-thread initialization
*       which is common to _beginthread, _beginthreadex, _mtinit
*       and _getptd.
*
*Entry:
*       pointer to a per-thread data block
*
*Exit:
*       the common fields in that block are initialized
*
*Exceptions:
*
*******************************************************************************/

_CRTIMP void __cdecl _initptd (
        _ptiddata ptd,
        pthreadlocinfo ptloci
        )
{

    ptd->_pxcptacttab = (void *)_XcptActTab;
#ifndef _WIN32_WCE
    ptd->_terrno = 0;
    ptd->_holdrand = 1L;
#endif


    // Initialize _setloc_data. These are the only valuse that need to be 
    // initialized.
    ptd->_setloc_data._cachein[0]=L'C';
    ptd->_setloc_data._cacheout[0]=L'C';

#if _PTD_LOCALE_SUPPORT
    // It is necessary to always have GLOBAL_LOCALE_BIT set in perthread data
    // because when doing bitwise or, we won't get __UPDATE_LOCALE to work when
    // global per thread locale is set.
    ptd->_ownlocale = _GLOBAL_LOCALE_BIT;

    ptd->ptmbcinfo = &__initialmbcinfo;
    _mlock(_MB_CP_LOCK);
    __try
    {
        InterlockedIncrement(&(ptd->ptmbcinfo->refcount));
    }
    __finally
    {
        _munlock(_MB_CP_LOCK);
    }

	// We need to make sure that ptd->ptlocinfo in never NULL, this saves us
    // perf counts when UPDATING locale.
    _mlock(_SETLOCALE_LOCK);
    __try {
        ptd->ptlocinfo = ptloci;
        /*
         * Note that and caller to _initptd could have passed __ptlocinfo, but
         * that will be a bug as between the call to _initptd and __addlocaleref
         * the global locale may have changed and ptloci may be pointing to invalid
         * memory. Thus if the wants to set the locale to global, NULL should
         * be passed.
         */
        if (ptd->ptlocinfo == NULL)
            ptd->ptlocinfo = __ptlocinfo;
        __addlocaleref(ptd->ptlocinfo);
    }
    __finally {
        _munlock(_SETLOCALE_LOCK);
    }
#endif  /* _PTD_LOCALE_SUPPORT */
}

/***
*_ptiddata _getptd_noexit(void) - get per-thread data structure for the current thread
*
*Purpose:
*
*Entry:
*
*Exit:
*       success = pointer to _tiddata structure for the thread
*       failure = NULL
*
*Exceptions:
*
*******************************************************************************/

_ptiddata __cdecl _getptd_noexit (
        void
        )
{
    _ptiddata ptd;
    DWORD   TL_LastError;

    TL_LastError = GetLastError();


    if ( (ptd = __crtFlsGetValue(__flsindex)) == NULL ) {
        /*
         * no per-thread data structure for this thread. try to create
         * one.
         */
#ifdef _DEBUG
        extern void * __cdecl _calloc_dbg_impl(size_t, size_t, int, const char *, int, int *);
        if ((ptd = _calloc_dbg_impl(1, sizeof(struct _tiddata), _CRT_BLOCK, __FILE__, __LINE__, NULL)) != NULL) {
#else
        if ((ptd = _calloc_crt(1, sizeof(struct _tiddata))) != NULL) {
#endif

            if (__crtFlsSetValue(__flsindex, (LPVOID)ptd) ) {

                /*
                 * Initialize of per-thread data
                 */

                _initptd(ptd,NULL);
#ifndef _WIN32_WCE
                ptd->_tid = GetCurrentThreadId();
                ptd->_thandle = (uintptr_t)(-1);
#endif
            }
            else {

                /*
                 * Return NULL to indicate failure
                 */

                _free_crt(ptd);
                ptd = NULL;
            }
        }
    }

    SetLastError(TL_LastError);

    return(ptd);
}

/***
*_ptiddata _getptd(void) - get per-thread data structure for the current thread
*
*Purpose:
*
*Entry:
*       unsigned long tid
*
*Exit:
*       success = pointer to _tiddata structure for the thread
*       failure = fatal runtime exit
*
*Exceptions:
*
*******************************************************************************/

_ptiddata __cdecl _getptd (
        void
        )
{
        _ptiddata ptd = _getptd_noexit();
        if (!ptd) {
            _amsg_exit(_RT_THREAD); /* write message and die */
        }
        return ptd;
}

/***
*void WINAPI _freefls(void *) - free up a per-fiber data structure
*
*Purpose:
*       Called from _freeptd, as a callback from deleting a fiber, and
*       from deleting an FLS index. This routine frees up the per-fiber
*       buffer associated with a fiber that is going away. The tiddata
*       structure itself is freed, but not until its subordinate buffers
*       are freed.
*
*Entry:
*       pointer to a per-fiber data block (malloc-ed memory)
*
*Exit:
*
*Exceptions:
*
*******************************************************************************/

_CRTIMP void
WINAPI
_freefls (
    void *data
    )

{

    _ptiddata ptd;
#if _PTD_LOCALE_SUPPORT
    pthreadlocinfo ptloci;
    pthreadmbcinfo ptmbci;
#endif  /* _PTD_LOCALE_SUPPORT */

    /*
     * Free up the _tiddata structure & its malloc-ed buffers.
     */

    ptd = data;
    if (ptd != NULL) {
 
        if(ptd->_errmsg)
            _free_crt((void *)ptd->_errmsg);
    
        if(ptd->_namebuf0)
            _free_crt((void *)ptd->_namebuf0);

        if(ptd->_namebuf1)
            _free_crt((void *)ptd->_namebuf1);
       
        if(ptd->_asctimebuf)
            _free_crt((void *)ptd->_asctimebuf);
    
        if(ptd->_wasctimebuf)
            _free_crt((void *)ptd->_wasctimebuf);
    
        if(ptd->_gmtimebuf)
            _free_crt((void *)ptd->_gmtimebuf);
    
        if(ptd->_cvtbuf)
            _free_crt((void *)ptd->_cvtbuf);

        if (ptd->_pxcptacttab != _XcptActTab)
            _free_crt((void *)ptd->_pxcptacttab);

#if _PTD_LOCALE_SUPPORT
        _mlock(_MB_CP_LOCK);
        __try {
            if ( ((ptmbci = ptd->ptmbcinfo) != NULL) && 
                 (InterlockedDecrement(&(ptmbci->refcount)) == 0)
                 && (ptmbci != &__initialmbcinfo)
                 )
                 _free_crt(ptmbci);
        }
        __finally {
            _munlock(_MB_CP_LOCK);
        }
    
        _mlock(_SETLOCALE_LOCK);
    
        __try {
            if ( (ptloci = ptd->ptlocinfo) != NULL )
            {
                __removelocaleref(ptloci);
                if ( (ptloci != __ptlocinfo) &&
                     (ptloci != &__initiallocinfo) &&
                     (ptloci->refcount == 0) )
                    __freetlocinfo(ptloci);
            }
        }
        __finally {
            _munlock(_SETLOCALE_LOCK);
        }
#endif  /* _PTD_LOCALE_SUPPORT */
        _free_crt((void *)ptd);
    }
    return;
}

void
_freeptd_ce(DWORD ThreadID, LPVOID data)
{
    _freefls(data);
}

/***
*void _freeptd(_ptiddata) - free up a per-thread data structure
*
*Purpose:
*       Called from _endthread and from a DLL thread detach handler,
*       this routine frees up the per-thread buffer associated with a
*       thread that is going away.  The tiddata structure itself is
*       freed, but not until its subordinate buffers are freed.
*
*Entry:
*       pointer to a per-thread data block (malloc-ed memory)
*       If NULL, the pointer for the current thread is fetched.
*
*Exit:
*
*Exceptions:
*
*******************************************************************************/

void __cdecl _freeptd (
        _ptiddata ptd
        )
{
        /*
         * Do nothing unless per-thread data has been allocated for this module!
         */

        if ( __flsindex != FLS_OUT_OF_INDEXES ) {

            /*
             * if parameter "ptd" is NULL, get the per-thread data pointer
             * Must NOT call _getptd because it will allocate one if none exists!
             * If FlsGetValue is NULL then ptd may not have been set
             */

            if ( ptd == NULL)
                ptd = __crtFlsGetValue(__flsindex);

            /*
             * Zero out the one pointer to the per-thread data block
             */

            __crtFlsSetValue(__flsindex, (LPVOID)0);

            _freefls(ptd);
        }
}

/***
*__threadid()     - Returns current thread ID
*__threadhandle() - Returns "pseudo-handle" for current thread
*
*Purpose:
*       The two function are simply do-nothing wrappers for the corresponding
*       Win32 APIs (GetCurrentThreadId and GetCurrentThread, respectively).
*
*Entry:
*       void
*
*Exit:
*       thread ID value
*
*Exceptions:
*
*******************************************************************************/

_CRTIMP unsigned long __cdecl __threadid (
        void
        )
{
    return( GetCurrentThreadId() );
}

_CRTIMP uintptr_t __cdecl __threadhandle(
        void
        )
{
    return( (uintptr_t)GetCurrentThread() );
}
#endif // !_WIN32_WCE || _WCE_FULLCRT