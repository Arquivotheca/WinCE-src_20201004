//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//------------------------------------------------------------------------------
// 
//      Bluetooth Test Client
// 
// 
// Module Name:
// 
//      btcomstbs.cxx
// 
// Abstract:
// 
//      This file implements a layer to call COM functions got with LoadLibrary()
//      from ..\bthapi.  This is required because of bthapi's use of ATL and because
//      winceos\comm is built before COM tree is, so we can't link directly.
// 
// 
//------------------------------------------------------------------------------


#define _OLEAUT32_
#define _OLE32_
#include <windows.h>

HINSTANCE ghLibOle32;
HINSTANCE ghLibOleAut32;

#if defined (__cplusplus)
extern "C" {
#endif

const IID IID_IUnknown = {0, 0, 0, {0xc0, 0, 0, 0, 0, 0, 0, 0x46 } };
const IID IID_IErrorInfo =    {0x1CF2B120,0x547D,0x101B,{0x8E,0x65,0x08,0x00,0x2B,0x2B,0xD1,0x19}};
const IID IID_IClassFactory = {0x00000001,0x0000,0x0000,{0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}};
const IID IID_IDataObject =   {0x0000010e,0x0000,0x0000,{0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}};

FARPROC pfnCoInitializeEx;
FARPROC pfnCoUninitialize;
FARPROC pfnCoCreateInstance;
FARPROC pfnCoTaskMemAlloc;
FARPROC pfnCoTaskMemFree;
FARPROC pfnSysAllocString;
FARPROC pfnSysFreeString;
FARPROC pfnSysAllocStringLen;
FARPROC pfnRegisterTypeLib;
FARPROC pfnSysStringLen;
FARPROC pfnSysAllocStringByteLen;
FARPROC pfnSysStringByteLen;
// FARPROC pfnVariantInit;
FARPROC pfnVariantClear;
FARPROC pfnVariantChangeType;
FARPROC pfnLoadRegTypeLib;
FARPROC pfnVariantCopy;
FARPROC pfnSetErrorInfo;
FARPROC pfnCreateErrorInfo;
FARPROC pfnLoadTypeLib;

static void InitCOMStubs(void);

// OLE32
HRESULT CoInitializeEx(LPVOID pvReserved, DWORD dwCoInit) {
    InitCOMStubs();
    if (! pfnCoInitializeEx)
        return E_NOTIMPL;

    typedef HRESULT (WINAPI *PFN_STUB)(LPVOID, DWORD);
    return ((PFN_STUB)pfnCoInitializeEx)(pvReserved, dwCoInit);
}

void CoUninitialize(void) {
    InitCOMStubs();
    if (! pfnCoUninitialize)
        return;

    typedef void (WINAPI *PFN_STUB)(void);
    ((PFN_STUB)pfnCoUninitialize)();
}

HRESULT CoCreateInstance(REFCLSID rclsid, LPUNKNOWN pUnkOuter, DWORD dwClsContext, REFIID riid, PVOID *ppv) {
    InitCOMStubs();
    if (! pfnCoCreateInstance)
        return E_NOTIMPL;

    typedef HRESULT (WINAPI *PFN_STUB)(REFCLSID, LPUNKNOWN, DWORD, REFIID,PVOID*);
    return ((PFN_STUB)pfnCoCreateInstance)(rclsid,pUnkOuter, dwClsContext,riid,ppv);
}


STDAPI_(void*) CoTaskMemAlloc(ULONG cb) {
    InitCOMStubs();
    if (! pfnCoTaskMemAlloc)
        return LocalAlloc (LPTR, cb);

    typedef void* (WINAPI *PFN_STUB)(ULONG);
    return ((PFN_STUB)pfnCoTaskMemAlloc)(cb);
}

STDAPI_(void) CoTaskMemFree(LPVOID pv) {
    InitCOMStubs();
    if (! pfnCoTaskMemAlloc) {	// key on alloc, not free
        LocalFree ((HLOCAL)pv);
        return;
    }

    typedef void (WINAPI *PFN_STUB)(LPVOID);
    ((PFN_STUB)pfnCoTaskMemFree)(pv);

    return;
}

// OLEAUT32
STDAPI_(BSTR) SysAllocString(const OLECHAR FAR* psz) {
    InitCOMStubs();

    if (! pfnSysAllocString)
        return NULL;

    typedef BSTR (WINAPI *PFN_STUB)(const OLECHAR FAR *);
    return ((PFN_STUB)pfnSysAllocString)(psz);
}

STDAPI_(void) SysFreeString(BSTR bstr) {
    InitCOMStubs();

    if (! pfnSysFreeString)
        return;

    typedef BSTR (WINAPI *PFN_STUB)(BSTR bstr);
    ((PFN_STUB)pfnSysFreeString)(bstr);
}

STDAPI_(BSTR) SysAllocStringLen(const OLECHAR FAR* psz, unsigned int len) {
    InitCOMStubs();

    if (! pfnSysAllocStringLen)
        return NULL;

    typedef BSTR (WINAPI *PFN_STUB)(const OLECHAR FAR* psz, unsigned int len);
    return ((PFN_STUB)pfnSysAllocStringLen)(psz,len);
}

STDAPI RegisterTypeLib(ITypeLib *ptlib, LPOLESTR szFullPath, LPOLESTR szHelpDir) {
    InitCOMStubs();

    if (! pfnRegisterTypeLib)
        return E_NOTIMPL;

    typedef HRESULT (WINAPI *PFN_STUB)(ITypeLib *ptlib, LPOLESTR szFullPath, LPOLESTR szHelpDir);
    return ((PFN_STUB)pfnRegisterTypeLib)(ptlib,szFullPath,szHelpDir);
}

STDAPI_(unsigned int) SysStringLen(BSTR bstr) {
    InitCOMStubs();

    if (! pfnSysStringLen)
        return 0;

    typedef unsigned int (WINAPI *PFN_STUB)(BSTR bstr);
    return ((PFN_STUB)pfnSysStringLen)(bstr);
}

STDAPI_(BSTR) SysAllocStringByteLen(const char FAR* psz, unsigned int len) {
    InitCOMStubs();

    if (! pfnSysAllocStringByteLen)
        return NULL;

    typedef BSTR (WINAPI *PFN_STUB)(const char FAR* psz, unsigned int len);
    return ((PFN_STUB)pfnSysAllocStringByteLen)(psz,len);
}

STDAPI_(unsigned int) SysStringByteLen(BSTR bstr) {
    InitCOMStubs();

    if (! pfnSysStringByteLen)
        return 0;

    typedef unsigned int (WINAPI *PFN_STUB)(BSTR bstr);
    return ((PFN_STUB)pfnSysStringByteLen)(bstr);
}

STDAPI_(void) VariantInit(VARIANT FAR* pvarg) {
    V_VT(pvarg) = VT_EMPTY;
}

STDAPI VariantClear(VARIANTARG FAR* pvarg) { 
    InitCOMStubs();

    if (! pfnVariantClear)
        return E_NOTIMPL;

    typedef HRESULT (WINAPI *PFN_STUB)(VARIANTARG FAR* pvarg);
    return ((PFN_STUB)pfnVariantClear)(pvarg);
}

STDAPI VariantChangeType(VARIANTARG FAR* pvargDest, VARIANTARG FAR* pvargSrc, unsigned short wFlags, VARTYPE vt) {
    InitCOMStubs();

    if (! pfnVariantChangeType)
        return E_NOTIMPL;

    typedef HRESULT (WINAPI *PFN_STUB)(VARIANTARG FAR* pvargDest, VARIANTARG FAR* pvargSrc, unsigned short wFlags, VARTYPE vt);
    return ((PFN_STUB)pfnVariantChangeType)(pvargDest, pvargSrc, wFlags, vt);
}

STDAPI LoadRegTypeLib(REFGUID rguid,unsigned short wVerMajor,unsigned short wVerMinor,LCID lcid,ITypeLib FAR* FAR* pptlib) {
    InitCOMStubs();

    if (! pfnLoadRegTypeLib)
        return E_NOTIMPL;

    typedef HRESULT (WINAPI *PFN_STUB)(REFGUID rguid,unsigned short wVerMajor,unsigned short wVerMinor,LCID lcid,ITypeLib FAR* FAR* pptlib);
    return ((PFN_STUB)pfnLoadRegTypeLib)(rguid,wVerMajor,wVerMinor,lcid,pptlib);
}

STDAPI VariantCopy(VARIANTARG FAR* pvargDest, VARIANTARG FAR* pvargSrc) {
    InitCOMStubs();

    if (! pfnVariantCopy)
        return E_NOTIMPL;

    typedef HRESULT (WINAPI *PFN_STUB)(VARIANTARG FAR* pvargDest, VARIANTARG FAR* pvargSrc);
    return ((PFN_STUB)pfnVariantCopy)(pvargDest,pvargSrc);
}

STDAPI SetErrorInfo(unsigned long dwReserved, IErrorInfo FAR* perrinfo) {
    InitCOMStubs();

    if (! pfnSetErrorInfo)
        return E_NOTIMPL;

    typedef HRESULT (WINAPI *PFN_STUB)(unsigned long dwReserved, IErrorInfo FAR* perrinfo);
    return ((PFN_STUB)pfnSetErrorInfo)(dwReserved,perrinfo);
}

STDAPI CreateErrorInfo(ICreateErrorInfo FAR* FAR* pperrinfo) {
    InitCOMStubs();

    if (! pfnCreateErrorInfo)
        return E_NOTIMPL;

    typedef HRESULT (WINAPI *PFN_STUB)(ICreateErrorInfo FAR* FAR* pperrinfo);
    return ((PFN_STUB)pfnCreateErrorInfo)(pperrinfo);
}

STDAPI LoadTypeLib(LPCOLESTR szFile, ITypeLib **pptlib) {
    InitCOMStubs();

    if (! pfnLoadTypeLib)
        return E_NOTIMPL;

    typedef HRESULT (WINAPI *PFN_STUB)(LPCOLESTR szFile, ITypeLib **pptlib);
    return ((PFN_STUB)pfnLoadTypeLib)(szFile,pptlib);;
}

static void InitCOMStubs(void)  {
    if (ghLibOle32 || ghLibOleAut32)
        return;

    ghLibOle32 = LoadLibrary(L"ole32.dll");
    if (ghLibOle32) {
        pfnCoCreateInstance      = GetProcAddress(ghLibOle32,L"CoCreateInstance");
        pfnCoInitializeEx        = GetProcAddress(ghLibOle32,L"CoInitializeEx");
        pfnCoUninitialize        = GetProcAddress(ghLibOle32,L"CoUninitialize");
    }

    ghLibOleAut32 = LoadLibrary(L"oleaut32.dll");
    if (ghLibOleAut32) {
        pfnSysAllocString        = GetProcAddress(ghLibOleAut32,L"SysAllocString");
        pfnSysFreeString         = GetProcAddress(ghLibOleAut32,L"SysFreeString");
        pfnSysAllocStringLen     = GetProcAddress(ghLibOleAut32,L"SysAllocStringLen");
        pfnRegisterTypeLib       = GetProcAddress(ghLibOleAut32,L"RegisterTypeLib");
        pfnSysStringLen          = GetProcAddress(ghLibOleAut32,L"SysStringLen");
        pfnSysAllocStringByteLen = GetProcAddress(ghLibOleAut32,L"SysAllocStringByteLen");
        pfnSysStringByteLen      = GetProcAddress(ghLibOleAut32,L"SysStringByteLen");
        pfnVariantClear          = GetProcAddress(ghLibOleAut32,L"VariantClear");
        pfnVariantChangeType     = GetProcAddress(ghLibOleAut32,L"VariantChangeType");
        pfnLoadRegTypeLib        = GetProcAddress(ghLibOleAut32,L"LoadRegTypeLib");
        pfnVariantCopy           = GetProcAddress(ghLibOleAut32,L"VariantCopy");
        pfnSetErrorInfo          = GetProcAddress(ghLibOleAut32,L"SetErrorInfo");
        pfnCreateErrorInfo       = GetProcAddress(ghLibOleAut32,L"CreateErrorInfo");
        pfnLoadTypeLib           = GetProcAddress(ghLibOleAut32,L"LoadTypeLib");
    }
}


#if defined (__cplusplus)
};			// __cplusplus
#endif
