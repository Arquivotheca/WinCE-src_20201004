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


// You are expected to #include this file from your private dllload.c.
//

// Delay loading mechanism.  This allows you to write code as if you are
// calling implicitly linked APIs, and yet have these APIs really be
// explicitly linked.  You can reduce the initial number of DLLs that 
// are loaded (load on demand) using this technique.
//
// Use the following macros to indicate which APIs/DLLs are delay-linked
// and -loaded.
//
//      DELAY_LOAD
//      DELAY_LOAD_HRESULT
//      DELAY_LOAD_SAFEARRAY
//      DELAY_LOAD_UINT
//      DELAY_LOAD_INT
//      DELAY_LOAD_VOID
//
// Use these macros for APIs that are exported by ordinal only.
//
//      DELAY_LOAD_ORD
//      DELAY_LOAD_VOID_ORD
//
// Use these macros for DLLs you want freed after the function call.
//
//      DELAY_LOAD_*_FREE
//
/**********************************************************************/
#ifndef _delayld_h_
#define _delayld_h_


#include <windows.h>

#define DLL_NOT_PRESENT ((HMODULE) -1)
#define ENSURE_LOADED(_hmod, _dll, _ext, pszfn) \
    (_hmod ? (_hmod) : (_hmod = LoadLibrary(TEXT(#_dll) TEXT(".") TEXT(#_ext))))

#ifndef DELAYLD_NO_GETPROCFROMDLL
void _GetProcFromDLL(HMODULE* phmod, LPCTSTR pszDLL, FARPROC* ppfn, LPCTSTR pszProc)
{
    if (*phmod == NULL)
    {
        *phmod = LoadLibrary(pszDLL);
        if (*phmod == NULL) {
            return;
        }
    }
    else if(*ppfn)
    {
        // If it's already loaded, return.
        return;
    }

    *ppfn = GetProcAddress(*phmod, pszProc);
    ASSERT(*ppfn);
}

void _GetOptionalProcFromDLL(HMODULE* phmod, LPCTSTR pszDLL, FARPROC* ppfn, LPCTSTR pszProc)
{
    if (*phmod != DLL_NOT_PRESENT)
    {
        if (*phmod == NULL)
        {
            *phmod = LoadLibrary(pszDLL);
            if (*phmod == NULL) {
                *phmod = DLL_NOT_PRESENT;
                return;
            }
        }
        else if(*ppfn)
        {
            // If it's already loaded, return.
            return;
        }

        *ppfn = GetProcAddress(*phmod, pszProc);
        ASSERT(*ppfn);
    }
}
#else
void _GetProcFromDLL(HMODULE* phmod, LPCTSTR pszDLL, FARPROC* ppfn, LPCTSTR pszProc);
void _GetOptionalProcFromDLL(HMODULE* phmod, LPCTSTR pszDLL, FARPROC* ppfn, LPCTSTR pszProc);
#endif



#define FREE_LIBRARY(hinst)  do{if(hinst){VERIFY(FreeLibrary(hinst));hinst=NULL;}}while(0,0)
#define FREE_OPTIONAL_LIBRARY(hinst)  do{if(hinst && hinst != DLL_NOT_PRESENT){VERIFY(FreeLibrary(hinst));hinst=NULL;}}while(0,0)

#define _GetProcFromSystemDLL _GetProcFromDLL

// NOTE: this takes two parameters that are the function name. the First (_fn) is the name that
// NOTE: the function will be called in this DLL and the other (_fni) is the
// NOTE: name of the function we will GetProcAddress. This helps get around functions that
// NOTE: are defined in the header files with _declspec...

//
//  HMODULE _hmod - where we cache the HMODULE (aka HINSTANCE)
//           _dll - Basename of the target DLL, not quoted
//           _ext - Extension of the target DLL, not quoted (usually DLL)
//           _ret - Data type of return value
//        _fnpriv - Local name for the function
//            _fn - Exported name for the function
//          _args - Argument list in the form (TYPE1 arg1, TYPE2 arg2, ...)
//         _nargs - Argument list in the form (arg1, arg2, ...)
//           _err - Return value if we can't call the actual function
//
#define DELAY_LOAD_NAME_EXT_ERR(_hmod, _dll, _ext, _ret, _fnpriv, _fn, _args, _nargs, _err) \
_ret __stdcall _fnpriv _args                \
{                                       \
    static _ret (__stdcall *_pfn##_fn) _args = NULL;   \
    _GetProcFromSystemDLL(&_hmod, TEXT(#_dll) TEXT(".") TEXT(#_ext), (FARPROC*)&_pfn##_fn, TEXT(#_fn)); \
    if (_pfn##_fn)               \
        return _pfn##_fn _nargs; \
    return (_ret)_err;           \
}

#define DELAY_LOAD_NAME_ERR(_hmod, _dll,       _ret, _fnpriv, _fn, _args, _nargs, _err) \
    DELAY_LOAD_NAME_EXT_ERR(_hmod, _dll,  DLL, _ret, _fnpriv, _fn, _args, _nargs, _err)

#define DELAY_LOAD_ERR(_hmod, _dll, _ret, _fn,      _args, _nargs, _err) \
    DELAY_LOAD_NAME_ERR(_hmod, _dll, _ret, _fn, _fn, _args, _nargs, _err)

#define DELAY_LOAD(_hmod, _dll, _ret, _fn, _args, _nargs) DELAY_LOAD_ERR(_hmod, _dll, _ret, _fn, _args, _nargs, 0)
#define DELAY_LOAD_HRESULT(_hmod, _dll, _fn, _args, _nargs) DELAY_LOAD_ERR(_hmod, _dll, HRESULT, _fn, _args, _nargs, E_FAIL)
#define DELAY_LOAD_SAFEARRAY(_hmod, _dll, _fn, _args, _nargs) DELAY_LOAD_ERR(_hmod, _dll, SAFEARRAY *, _fn, _args, _nargs, NULL)
#define DELAY_LOAD_UINT(_hmod, _dll, _fn, _args, _nargs) DELAY_LOAD_ERR(_hmod, _dll, UINT, _fn, _args, _nargs, 0)
#define DELAY_LOAD_INT(_hmod, _dll, _fn, _args, _nargs) DELAY_LOAD_ERR(_hmod, _dll, INT, _fn, _args, _nargs, 0)
#define DELAY_LOAD_BOOL(_hmod, _dll, _fn, _args, _nargs) DELAY_LOAD_ERR(_hmod, _dll, BOOL, _fn, _args, _nargs, FALSE)
#define DELAY_LOAD_BOOLEAN(_hmod, _dll, _fn, _args, _nargs) DELAY_LOAD_ERR(_hmod, _dll, BOOLEAN, _fn, _args, _nargs, FALSE)
#define DELAY_LOAD_DWORD(_hmod, _dll, _fn, _args, _nargs) DELAY_LOAD_ERR(_hmod, _dll, DWORD, _fn, _args, _nargs, FALSE)
#define DELAY_LOAD_WNET(_hmod, _dll, _fn, _args, _nargs) DELAY_LOAD_ERR(_hmod, _dll, DWORD, _fn, _args, _nargs, WN_NOT_SUPPORTED)
#define DELAY_LOAD_LPVOID(_hmod, _dll, _fn, _args, _nargs) DELAY_LOAD_ERR(_hmod, _dll, LPVOID, _fn, _args, _nargs, 0)

// the NAME variants allow the local function to be called something different from the imported
// function to avoid dll linkage problems.
#define DELAY_LOAD_NAME(_hmod, _dll, _ret, _fn, _fni, _args, _nargs) DELAY_LOAD_NAME_ERR(_hmod, _dll, _ret, _fn, _fni, _args, _nargs, 0)
#define DELAY_LOAD_NAME_HRESULT(_hmod, _dll, _fn, _fni, _args, _nargs) DELAY_LOAD_NAME_ERR(_hmod, _dll, HRESULT, _fn, _fni, _args, _nargs, E_FAIL)
#define DELAY_LOAD_NAME_SAFEARRAY(_hmod, _dll, _fn, _fni, _args, _nargs) DELAY_LOAD_NAME_ERR(_hmod, _dll, SAFEARRAY *, _fn, _fni, _args, _nargs, NULL)
#define DELAY_LOAD_NAME_UINT(_hmod, _dll, _fn, _fni, _args, _nargs) DELAY_LOAD_NAME_ERR(_hmod, _dll, UINT, _fn, _fni, _args, _nargs, 0)
#define DELAY_LOAD_NAME_BOOL(_hmod, _dll, _fn, _fni, _args, _nargs) DELAY_LOAD_NAME_ERR(_hmod, _dll, BOOL, _fn, _fni, _args, _nargs, FALSE)
#define DELAY_LOAD_NAME_DWORD(_hmod, _dll, _fn, _fni, _args, _nargs) DELAY_LOAD_NAME_ERR(_hmod, _dll, DWORD, _fn, _fni, _args, _nargs, 0)

#define DELAY_LOAD_NAME_VOID(_hmod, _dll, _fn, _fni, _args, _nargs)                               \
void __stdcall _fn _args                                                                \
{                                                                                       \
    static void (__stdcall *_pfn##_fni) _args = NULL;                                   \
    if (!ENSURE_LOADED(_hmod, _dll, DLL, TEXT(#_fni)))                                  \
    {                                                                                   \
        return;                                                                         \
    }                                                                                   \
    if (_pfn##_fni == NULL)                                                             \
    {                                                                                   \
        *(FARPROC*)&(_pfn##_fni) = GetProcAddress(_hmod, TEXT(#_fni));                  \
        if (_pfn##_fni == NULL)                                                         \
        {                                                                               \
            ASSERT(0);                                                                  \
            return;                                                                     \
        }                                                                               \
    }                                                                                   \
    _pfn##_fni _nargs;                                                                  \
}

#define DELAY_LOAD_VOID(_hmod, _dll, _fn, _args, _nargs)   DELAY_LOAD_NAME_VOID(_hmod, _dll, _fn, _fn, _args, _nargs)

// For private entrypoints exported by ordinal.
#define DELAY_LOAD_ORD_ERR(_hmod, _dll, _ret, _fn, _ord, _args, _nargs, _err) \
_ret __stdcall _fn _args                \
{                                       \
    static _ret (__stdcall *_pfn##_fn) _args = NULL;   \
    _GetProcFromSystemDLL(&_hmod, TEXT(#_dll), (FARPROC*)&_pfn##_fn, (LPCTSTR)_ord);   \
    if (_pfn##_fn)               \
        return _pfn##_fn _nargs; \
    return (_ret)_err;           \
}

#define DELAY_LOAD_OPT_ORD_ERR(_hmod, _dll, _ret, _fn, _ord, _args, _nargs, _err) \
_ret __stdcall _fn _args                \
{                                       \
    static _ret (__stdcall *_pfn##_fn) _args = NULL;   \
    _GetOptionalProcFromDLL(&_hmod, TEXT(#_dll), (FARPROC*)&_pfn##_fn, (LPCTSTR)_ord);   \
    if (_pfn##_fn)               \
        return _pfn##_fn _nargs; \
    return (_ret)_err;           \
}

#define DELAY_LOAD_ORD(_hmod, _dll, _ret, _fn, _ord, _args, _nargs) DELAY_LOAD_ORD_ERR(_hmod, _dll, _ret, _fn, _ord, _args, _nargs, 0)
#define DELAY_LOAD_EXT_ORD(_hmod, _dll, _ext, _ret, _fn, _ord, _args, _nargs) DELAY_LOAD_ORD_ERR(_hmod, TEXT(#_dll) TEXT(".") TEXT(#_ext), _ret, _fn, _ord, _args, _nargs, 0)


#define DELAY_LOAD_ORD_VOID(_hmod, _dll, _fn, _ord, _args, _nargs)                     \
void __stdcall _fn _args                \
{                                       \
    static void (__stdcall *_pfn##_fn) _args = NULL;   \
    _GetProcFromSystemDLL(&_hmod, TEXT(#_dll), (FARPROC*)&_pfn##_fn, (LPCTSTR)_ord);   \
    if (_pfn##_fn)              \
        _pfn##_fn _nargs;       \
    return;                     \
}

#define DELAY_LOAD_OPT_ORD_VOID(_hmod, _dll, _fn, _ord, _args, _nargs)                     \
void __stdcall _fn _args                \
{                                       \
    static void (__stdcall *_pfn##_fn) _args = NULL;   \
    _GetOptionalProcFromDLL(&_hmod, TEXT(#_dll), (FARPROC*)&_pfn##_fn, (LPCTSTR)_ord);   \
    if (_pfn##_fn)              \
        _pfn##_fn _nargs;       \
    return;                     \
}

#define DELAY_LOAD_VOID_ORD DELAY_LOAD_ORD_VOID // cuz people screw this up all the time

#define DELAY_LOAD_ORD_BOOL(_hmod, _dll, _fn, _ord, _args, _nargs) \
    DELAY_LOAD_ORD_ERR(_hmod, _dll, BOOL, _fn, _ord, _args, _nargs, 0)

#define DELAY_LOAD_OPT_ORD_BOOL(_hmod, _dll, _fn, _ord, _args, _nargs) \
    DELAY_LOAD_OPT_ORD_ERR(_hmod, _dll, BOOL, _fn, _ord, _args, _nargs, 0)

#define DELAY_LOAD_EXT(_hmod, _dll, _ext, _ret, _fn, _args, _nargs) \
        DELAY_LOAD_NAME_EXT_ERR(_hmod, _dll, _ext, _ret, _fn, _fn, _args, _nargs, 0)

#define DELAY_LOAD_EXT_WRAP(_hmod, _dll, _ext, _ret, _fnWrap, _fnOrig, _args, _nargs) \
        DELAY_LOAD_NAME_EXT_ERR(_hmod, _dll, _ext, _ret, _fnWrap, _fnOrig, _args, _nargs, 0)

#define DELAY_LOAD_ORD_HRESULT(_hmod, _dll, _fn, _ord, _args, _nargs) \
    DELAY_LOAD_ORD_ERR(_hmod, _dll, HRESULT, _fn, _ord, _args, _nargs, E_FAIL)

#define DELAY_LOAD_OPT_ORD_HRESULT(_hmod, _dll, _fn, _ord, _args, _nargs) \
    DELAY_LOAD_OPT_ORD_ERR(_hmod, _dll, HRESULT, _fn, _ord, _args, _nargs, E_FAIL)

// NOTE: these versions free the library.
//
//

// NOTE: this takes two parameters that are the function name. the First (_fn) is the name that
// NOTE: the function will be called in this DLL and the other (_fni) is the
// NOTE: name of the function we will GetProcAddress. This helps get around functions that
// NOTE: are defined in the header files with _declspec...

//
//           _dll - Basename of the target DLL, not quoted
//           _ext - Extension of the target DLL, not quoted (usually DLL)
//           _ret - Data type of return value
//        _fnpriv - Local name for the function
//            _fn - Exported name for the function
//          _args - Argument list in the form (TYPE1 arg1, TYPE2 arg2, ...)
//         _nargs - Argument list in the form (arg1, arg2, ...)
//           _err - Return value if we can't call the actual function
//
#define DELAY_LOAD_NAME_EXT_ERR_FREE(_dll, _ext, _ret, _fnpriv, _fn, _args, _nargs, _err) \
_ret __stdcall _fnpriv _args                \
{                                       \
    HINSTANCE _hmod = NULL; \
    _ret (__stdcall *_pfn##_fn) _args = NULL;   \
    _ret ret; \
    _GetProcFromSystemDLL(&_hmod, TEXT(#_dll) TEXT(".") TEXT(#_ext), (FARPROC*)&_pfn##_fn, TEXT(#_fn)); \
    if (_pfn##_fn)               \
        ret = _pfn##_fn _nargs; \
    else \
        ret = (_ret)_err;           \
    FreeLibrary(_hmod); \
    return ret; \
}

#define DELAY_LOAD_NAME_ERR_FREE(_dll,       _ret, _fnpriv, _fn, _args, _nargs, _err) \
    DELAY_LOAD_NAME_EXT_ERR_FREE(_dll,  DLL, _ret, _fnpriv, _fn, _args, _nargs, _err)

#define DELAY_LOAD_ERR_FREE(_dll, _ret, _fn,      _args, _nargs, _err) \
    DELAY_LOAD_NAME_ERR_FREE(_dll, _ret, _fn, _fn, _args, _nargs, _err)

#define DELAY_LOAD_FREE(_dll, _ret, _fn, _args, _nargs) DELAY_LOAD_ERR_FREE(_dll, _ret, _fn, _args, _nargs, 0)
#define DELAY_LOAD_HRESULT_FREE(_dll, _fn, _args, _nargs) DELAY_LOAD_ERR_FREE(_dll, HRESULT, _fn, _args, _nargs, E_FAIL)
#define DELAY_LOAD_SAFEARRAY_FREE(_dll, _fn, _args, _nargs) DELAY_LOAD_ERR_FREE(_dll, SAFEARRAY *, _fn, _args, _nargs, NULL)
#define DELAY_LOAD_UINT_FREE(_dll, _fn, _args, _nargs) DELAY_LOAD_ERR_FREE(_dll, UINT, _fn, _args, _nargs, 0)
#define DELAY_LOAD_INT_FREE(_dll, _fn, _args, _nargs) DELAY_LOAD_ERR_FREE(_dll, INT, _fn, _args, _nargs, 0)
#define DELAY_LOAD_BOOL_FREE(_dll, _fn, _args, _nargs) DELAY_LOAD_ERR_FREE(_dll, BOOL, _fn, _args, _nargs, FALSE)
#define DELAY_LOAD_BOOLEAN_FREE(_dll, _fn, _args, _nargs) DELAY_LOAD_ERR_FREE(_dll, BOOLEAN, _fn, _args, _nargs, FALSE)
#define DELAY_LOAD_DWORD_FREE(_dll, _fn, _args, _nargs) DELAY_LOAD_ERR_FREE(_dll, DWORD, _fn, _args, _nargs, FALSE)
#define DELAY_LOAD_WNET_FREE(_dll, _fn, _args, _nargs) DELAY_LOAD_ERR_FREE(_dll, DWORD, _fn, _args, _nargs, WN_NOT_SUPPORTED)
#define DELAY_LOAD_LPVOID_FREE(_dll, _fn, _args, _nargs) DELAY_LOAD_ERR_FREE(_dll, LPVOID, _fn, _args, _nargs, 0)


// the NAME variants allow the local function to be called something different from the imported
// function to avoid dll linkage problems.  these versions free the library.
#define DELAY_LOAD_NAME_FREE(_dll, _ret, _fn, _fni, _args, _nargs) DELAY_LOAD_NAME_ERR_FREE(_dll, _ret, _fn, _fni, _args, _nargs, 0)
#define DELAY_LOAD_NAME_HRESULT_FREE(_dll, _fn, _fni, _args, _nargs) DELAY_LOAD_NAME_ERR_FREE(_dll, HRESULT, _fn, _fni, _args, _nargs, E_FAIL)
#define DELAY_LOAD_NAME_SAFEARRAY_FREE(_dll, _fn, _fni, _args, _nargs) DELAY_LOAD_NAME_ERR_FREE(_dll, SAFEARRAY *, _fn, _fni, _args, _nargs, NULL)
#define DELAY_LOAD_NAME_UINT_FREE(_dll, _fn, _fni, _args, _nargs) DELAY_LOAD_NAME_ERR_FREE(_dll, UINT, _fn, _fni, _args, _nargs, 0)
#define DELAY_LOAD_NAME_BOOL_FREE(_dll, _fn, _fni, _args, _nargs) DELAY_LOAD_NAME_ERR_FREE(_dll, BOOL, _fn, _fni, _args, _nargs, FALSE)
#define DELAY_LOAD_NAME_DWORD_FREE(_dll, _fn, _fni, _args, _nargs) DELAY_LOAD_NAME_ERR_FREE(_dll, DWORD, _fn, _fni, _args, _nargs, 0)

#define DELAY_LOAD_NAME_VOID_FREE(_dll, _fn, _fni, _args, _nargs)                               \
void __stdcall _fn _args                                                                \
{                                                                                       \
    HINSTANCE _hmod = NULL; \
    void (__stdcall *_pfn##_fni) _args = NULL;                                   \
    if (!ENSURE_LOADED(_hmod, _dll, DLL, TEXT(#_fni)))                                  \
    {                                                                                   \
        return;                                                                         \
    }                                                                                   \
    *(FARPROC*)&(_pfn##_fni) = GetProcAddress(_hmod, TEXT(#_fni));                  \
    ASSERT(_pfn##_fni);                                                             \
    if (_pfn##_fni != NULL)                                                         \
        _pfn##_fni _nargs;                                                                  \
    FreeLibrary(_hmod); \
    return;                                                                     \
}

#define DELAY_LOAD_VOID_FREE(_dll, _fn, _args, _nargs)   DELAY_LOAD_NAME_VOID_FREE(_dll, _fn, _fn, _args, _nargs)

// For private entrypoints exported by ordinal.  these versions free the library.
#define DELAY_LOAD_ORD_ERR_FREE(_dll, _ret, _fn, _ord, _args, _nargs, _err) \
_ret __stdcall _fn _args                \
{                                       \
    HINSTANCE _hmod = NULL; \
    _ret (__stdcall *_pfn##_fn) _args = NULL;   \
    _ret ret; \
    _GetProcFromSystemDLL(&_hmod, TEXT(#_dll), (FARPROC*)&_pfn##_fn, (LPCTSTR)_ord);   \
    if (_pfn##_fn)               \
        ret = _pfn##_fn _nargs; \
    else \
        ret = (_ret)_err;           \
    FreeLibrary(_hmod); \
    return ret; \
}

#define DELAY_LOAD_ORD_FREE(_dll, _ret, _fn, _ord, _args, _nargs) DELAY_LOAD_ORD_ERR_FREE(_dll, _ret, _fn, _ord, _args, _nargs, 0)
#define DELAY_LOAD_EXT_ORD_FREE(_dll, _ext, _ret, _fn, _ord, _args, _nargs) DELAY_LOAD_ORD_ERR_FREE(##_dll.##_ext, _ret, _fn, _ord, _args, _nargs, 0)


#define DELAY_LOAD_ORD_VOID_FREE(_dll, _fn, _ord, _args, _nargs)                     \
void __stdcall _fn _args                \
{                                       \
    HINSTANCE _hmod = NULL; \
    void (__stdcall *_pfn##_fn) _args = NULL;   \
    _GetProcFromSystemDLL(&_hmod, TEXT(#_dll), (FARPROC*)&_pfn##_fn, (LPCTSTR)_ord);   \
    if (_pfn##_fn)              \
        _pfn##_fn _nargs;       \
    FreeLibrary(_hmod); \
    return;                     \
}
#define DELAY_LOAD_VOID_ORD_FREE DELAY_LOAD_ORD_VOID_FREE // cuz people screw this up all the time

#define DELAY_LOAD_ORD_BOOL_FREE(_dll, _fn, _ord, _args, _nargs) \
    DELAY_LOAD_ORD_ERR_FREE(_dll, BOOL, _fn, _ord, _args, _nargs, 0)

#define DELAY_LOAD_ORD_HRESULT_FREE(_dll, _fn, _ord, _args, _nargs) \
    DELAY_LOAD_ORD_ERR_FREE(_dll, HRESULT, _fn, _ord, _args, _nargs, 0)

#define DELAY_LOAD_EXT_FREE(_dll, _ext, _ret, _fn, _args, _nargs) \
        DELAY_LOAD_NAME_EXT_ERR_FREE(_dll, _ext, _ret, _fn, _fn, _args, _nargs, 0)

#define DELAY_LOAD_EXT_WRAP_FREE(_dll, _ext, _ret, _fnWrap, _fnOrig, _args, _nargs) \
        DELAY_LOAD_NAME_EXT_ERR_FREE(_dll, _ext, _ret, _fnWrap, _fnOrig, _args, _nargs, 0)
#endif

