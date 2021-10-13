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
*_vcclrit.h
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       This file defined the functions and variables used by users
*       to initialize the CRT and the dll in IJW scenarios. It is no longer
*       necessary to use this file and the functions it contains because
*       CRT initialization no longer causes managed code to execute
*       under the loader lock. It is now possible to use the CRT in
*       the standard way without problems. This header is shipped for
*       compatibility with the previous version only.
*
*Revision History:
*       ??-??-02  GB    Created
*       05-04-04  AJS   Added file back to Whidbey with deprecations. VSWhidbey#58671
*       09-14-04  JL    VSW#360000 - Change __USE_GLOBAL_NAMESPACE to be C++ friendly
*       09-24-04  MSL   Use conformant comments
*       10-19-04  AJS   Avoid duplicate initialization. VSWhidbey:385142
*       11-07-04  MSL   Native startup locking
*       03-08-05  AC    Make locking for native initialisation fiber-safe
*       03-23-05  MSL   Fix to compile as C code again
*                       Fix warnings with OS Exchange pointer API
*                       Block /Za and /clr:pure
*       03-30-05  PAL   Make parameter names ANSI-compliant.
*       04-14-05  MSL   Compile clean under MIDL
*       04-29-05  MSL   C-style comment
*
****/

#pragma once

#ifndef __midl
#ifndef _MSC_EXTENSIONS
#error ERROR: This initialisation code cannot be used with /Za
#endif

#ifdef _M_CEE_PURE
#error ERROR: This code is not useful for /clr:pure mode. Remove the include.
#endif

#include <crtwrn.h>
#include <windowscore.h>

#if !defined(_CRT_VCCLRIT_NO_DEPRECATE)
    #define _CRT_VCCLRIT_DEPRECATE _CRT_DEPRECATE_TEXT("These manual initialisation functions (plus link /noentry) are no longer appropriate for managed code DLLs. Instead, use a normal DLL entrypoint. See online help for more information.")
    #pragma _CRT_WARNING( _VCCLRIT_DEPRECATED )
#else
    #define _CRT_VCCLRIT_DEPRECATE 
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern IMAGE_DOS_HEADER __ImageBase;

BOOL WINAPI _DllMainCRTStartup(
        HANDLE  _HDllHandle,
        DWORD   _DwReason,
        LPVOID  _Lpreserved
        );

typedef enum {
    __uninitialized,
    __initializing,
    __NSinitialized /* Renamed */
} __enative_startup_state;

extern __enative_startup_state __native_startup_state;
extern void *__native_startup_lock;
extern volatile unsigned int __native_vcclrit_reason;
#define __NO_REASON    0xffffffff    /* maximum unsigned int value */
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#define __USE_GLOBAL_NAMESPACE(X)   (::X)
#define __THROW throw()
#define __REINTERPRET_CAST(T) reinterpret_cast<T>
#define __STATIC_CAST(T) static_cast<T>
#define __INLINE inline
#else
/* deliberately no parens here so we don't suppress macro expansion */
#define __USE_GLOBAL_NAMESPACE(X)   X
#define __THROW 
#define __REINTERPRET_CAST(T)   (T)
#define __STATIC_CAST(T)        (T)
#define __INLINE __inline
#endif

#ifdef _M_IX86
/* System version has problems especially for /Wp64 */
__INLINE void* WINAPI _CrtInterlockedExchangePointer(void** pp, void* pNew) __THROW
{
        return( __REINTERPRET_CAST(void*)(__STATIC_CAST(LONG_PTR)(__USE_GLOBAL_NAMESPACE(InterlockedExchange)(__REINTERPRET_CAST(LONG*)(pp), __STATIC_CAST(LONG)(__REINTERPRET_CAST(LONG_PTR)(pNew))))) );
}
#else
#define _CrtInterlockedExchangePointer(p, n) InterlockedExchangePointer(p, n)
#endif

/*
Used to lock 
*/
_CRT_VCCLRIT_DEPRECATE
__declspec( selectany ) LONG  volatile __lock_handle = 0;

/*
Init called
*/
_CRT_VCCLRIT_DEPRECATE
__declspec(selectany) BOOL volatile __initialized = FALSE;

/*
Term called
*/
_CRT_VCCLRIT_DEPRECATE
__declspec( selectany ) BOOL volatile __terminated = FALSE;

_CRT_VCCLRIT_DEPRECATE
__inline BOOL WINAPI __crt_dll_initialize()
{
    /*
    Try to make the variable names unique, so that the variables don't even clash with macros.
    */
    static BOOL volatile (__retval) = FALSE;
    static void * volatile (__lockFiberId) = 0;
    void * volatile (__currentFiberId) = ((PNT_TIB)__USE_GLOBAL_NAMESPACE(NtCurrentTeb)())->StackBase;
    int (__int_var)=0;
    int __nested=FALSE;
    
    /*
    Take Lock, This is needed for multithreaded scenario. Moreover the threads
    need to wait here to make sure that the dll is initialized when they get
    past this function.
    */
    while ( __USE_GLOBAL_NAMESPACE(InterlockedExchange)( &(__lock_handle), 1) == 1 )
        {
        ++(__int_var);
        if ((__lockFiberId) == (__currentFiberId)) 
        {
            return TRUE;
        }
                __USE_GLOBAL_NAMESPACE(Sleep)( (__int_var)>1000?100:0 );

        /*
        If you stop responding in this loop, this implies that your dllMainCRTStartup is not responding on another
        thread. The most likely cause of this is a not responding in one of your static constructors or
        destructors.
        */
        }
    /*
    Note that we don't really need any interlocked stuff here as the writes are always
    in the lock. Only reads are outside the lock.
    */
    (__lockFiberId) = (__currentFiberId);
    __try {
    
        void * __lock_free=0;
        while((__lock_free=InterlockedCompareExchangePointer(&__native_startup_lock, __currentFiberId, 0))!=0)
        {
            if(__lock_free==__currentFiberId)
            {
                __nested=TRUE;
                break;
            }

            /* some other thread is running native startup/shutdown during a cctor/domain unload. 
            Should only happen if this DLL was built using the Everett-compat loader lock fix in vcclrit.h
            */
            /* wait for the other thread to complete init before we return */
            Sleep(1000);
        }

        if ( (__terminated) == TRUE )
        {
            (__retval) = FALSE;
        }
        else if ( (__initialized) == FALSE )
        {
            /* cctor may have initialized native code already */
            if (__native_startup_state == __uninitialized)
            {
                __try
                {
                    __native_vcclrit_reason = DLL_PROCESS_ATTACH;
                    (__retval) = (_DllMainCRTStartup)( ( HINSTANCE )( &__ImageBase ), DLL_PROCESS_ATTACH, 0 );
                }
                __finally
                {
                    __native_vcclrit_reason = __NO_REASON;
                }
                (__initialized) = TRUE;
            }
        }

    } __finally {
        /* revert the __lockFiberId */
        (__lockFiberId) = 0;
        /* Release Lock */
        __USE_GLOBAL_NAMESPACE(InterlockedExchange)( &(__lock_handle), 0 );
        if(!__nested)
        {
            _CrtInterlockedExchangePointer( &(__native_startup_lock), 0 );
        }
    }
    return (__retval);
}

_CRT_VCCLRIT_DEPRECATE
__inline BOOL WINAPI __crt_dll_terminate()
{
    static BOOL volatile (__retval) = TRUE;
    static void * volatile (__lockFiberId) = 0;
    void * volatile (__currentFiberId) = ((PNT_TIB)__USE_GLOBAL_NAMESPACE(NtCurrentTeb)())->StackBase;
    int (__int_var)=0;
    int __nested=FALSE;
    
    /* Take Lock, this lock is needed to keep Terminate in sync with Initialize. */
    while ( __USE_GLOBAL_NAMESPACE(InterlockedExchange)( &(__lock_handle), 1) == 1 )
        {
        ++(__int_var);
        if ((__lockFiberId) == (__currentFiberId)) 
        {
            return TRUE;
        }
                __USE_GLOBAL_NAMESPACE(Sleep)( (__int_var)>1000?100:0 );

        /*
        If you stop responding in this loop, this implies that your dllMainCRTStartup is not responding on another
        thread. The most likely cause of this is a not responding in one of your static constructors or
        destructors.
        */
    }
    /*
    Note that we don't really need any interlocked stuff here as the writes are always
    in the lock. Only reads are outside the lock.
    */
    (__lockFiberId) = (__currentFiberId);
    __try {

        void *__lock_free=0;
        while((__lock_free=__USE_GLOBAL_NAMESPACE(InterlockedCompareExchangePointer)(&__native_startup_lock, __REINTERPRET_CAST(void *)(__STATIC_CAST(INT_PTR)(1)), 0))!=0)
        {
            /* some other thread is running native startup/shutdown during a cctor/domain unload. 
            Should only happen if this DLL was built using the Everett-compat loader lock fix in vcclrit.h
            */
            /* wait for the other thread to complete init before we return */
            Sleep(1000);
        }
        if ( (__initialized) == FALSE )
        {
            (__retval) = FALSE;
        }
        else if ( (__terminated) == FALSE )
        {
            __try
            {
                __native_vcclrit_reason = DLL_PROCESS_DETACH;
                (__retval) = _DllMainCRTStartup( ( HINSTANCE )( &(__ImageBase) ), DLL_PROCESS_DETACH, 0 );
            }
            __finally
            {
                __native_vcclrit_reason = __NO_REASON;
            }
            (__terminated) = TRUE;
        }

    } __finally {
        /* revert the __lockFiberId */
        (__lockFiberId) = 0;
        /* Release Lock */
        __USE_GLOBAL_NAMESPACE(InterlockedExchange)( &(__lock_handle), 0 );
        if(!__nested)
        {
            _CrtInterlockedExchangePointer( &(__native_startup_lock), 0 );
        }
    }
    return (__retval);
}

#endif
