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
//=--------------------------------------------------------------------------=
// Util.H
//=--------------------------------------------------------------------------=
//
// contains utilities that we will find useful.
//
#ifndef _UTIL_H_

#include "Globals.H"


//=--------------------------------------------------------------------------=
// Misc Helper Stuff
//=--------------------------------------------------------------------------=
//
HWND      GetParkingWindow(void);
HINSTANCE GetResourceHandle(void);


// an array of common OLE automation data types and their sizes [in bytes]
//
extern const BYTE g_rgcbDataTypeSize [];


//=--------------------------------------------------------------------------=
// miscellaneous [useful] numerical constants
//=--------------------------------------------------------------------------=
// the length of a guid once printed out with -'s, leading and trailing bracket,
// plus 1 for NULL
//
#define GUID_STR_LEN    40

#if 0
//=--------------------------------------------------------------------------=
// allocates a temporary buffer that will disappear when it goes out of scope
// NOTE: be careful of that -- make sure you use the string in the same or
// nested scope in which you created this buffer. people should not use this
// class directly.  use the macro(s) below.
//
class TempBuffer {
  public:
    TempBuffer(ULONG cBytes) {
        m_pBuf = (cBytes <= 120) ? &m_szTmpBuf : HeapAlloc(g_hHeap, 0, cBytes);
        m_fHeapAlloc = (cBytes > 120);
    }
    ~TempBuffer() {
        if (m_pBuf && m_fHeapAlloc) HeapFree(g_hHeap, 0, m_pBuf);
    }
    void *GetBuffer() {
        return m_pBuf;
    }

  private:
    void *m_pBuf;
    // we'll use this temp buffer for small cases.
    //
    char  m_szTmpBuf[120];
    unsigned m_fHeapAlloc:1;
};
#endif // 0

#if 0
//=--------------------------------------------------------------------------=
// string helpers.
//
// given and ANSI String, copy it into a wide buffer.
// be careful about scoping when using this macro!
//
// how to use the below two macros:
//
//  ...
//  LPSTR pszA;
//  pszA = MyGetAnsiStringRoutine();
//  MAKE_WIDEPTR_FROMANSI(pwsz, pszA);
//  MyUseWideStringRoutine(pwsz);
//  ...
//
// similarily for MAKE_ANSIPTR_FROMWIDE.  note that the first param does not
// have to be declared, and no clean up must be done.
//
#define MAKE_WIDEPTR_FROMANSI(ptrname, ansistr) \
    long __l##ptrname = (lstrlen(ansistr) + 1) * sizeof(WCHAR); \
    TempBuffer __TempBuffer##ptrname(__l##ptrname); \
    MultiByteToWideChar(CP_ACP, 0, ansistr, -1, (LPWSTR)__TempBuffer##ptrname.GetBuffer(), __l##ptrname); \
    LPWSTR ptrname = (LPWSTR)__TempBuffer##ptrname.GetBuffer()

// * 2 for DBCS handling in below length computation
//
#define MAKE_ANSIPTR_FROMWIDE(ptrname, widestr) \
    long __l##ptrname = (lstrlenW(widestr) + 1) * 2 * sizeof(char); \
    TempBuffer __TempBuffer##ptrname(__l##ptrname); \
    WideCharToMultiByte(CP_ACP, 0, widestr, -1, (LPSTR)__TempBuffer##ptrname.GetBuffer(), __l##ptrname, NULL, NULL); \
    LPSTR ptrname = (LPSTR)__TempBuffer##ptrname.GetBuffer()

#endif

#define STR_BSTR   0
#define STR_OLESTR 1
#define BSTRFROMANSI(x)    (BSTR)MakeWideStrFromAnsi((LPSTR)(x), STR_BSTR)
#define OLESTRFROMANSI(x)  (LPOLESTR)MakeWideStrFromAnsi((LPSTR)(x), STR_OLESTR)
#define BSTRFROMRESID(x)   (BSTR)MakeWideStrFromResourceId(x, STR_BSTR)
#define OLESTRFROMRESID(x) (LPOLESTR)MakeWideStrFromResourceId(x, STR_OLESTR)
#define COPYOLESTR(x)      (LPOLESTR)MakeWideStrFromWide(x, STR_OLESTR)
#define COPYBSTR(x)        (BSTR)MakeWideStrFromWide(x, STR_BSTR)

#if 0
LPWSTR MakeWideStrFromAnsi(LPSTR, BYTE bType);
LPWSTR MakeWideStrFromResourceId(WORD, BYTE bType);
LPWSTR MakeWideStrFromWide(LPWSTR, BYTE bType);


// takes a GUID, and a pointer to a buffer, and places the string form of the
// GUID in said buffer.
//

#endif

int StringFromGuidW(REFIID, LPSTR);
//=--------------------------------------------------------------------------=
// registry helpers.
//
// takes some information about an Automation Object, and places all the
// relevant information about it in the registry.
//
BOOL RegSetMultipleValues(HKEY hkey, ...);
BOOL RegisterUnknownObject(LPCTSTR pwszObjectName, REFCLSID riidObject);
BOOL RegisterAutomationObject(LPCTSTR pwszLibName, LPCTSTR pwszObjectName, long lVersion, REFCLSID riidLibrary, REFCLSID riidObject);
// BOOL RegisterControlObject(LPCSTR pszLibName, LPCSTR pszObjectName, long lVersion, REFCLSID riidLibrary, REFCLSID riidObject, DWORD dwMiscStatus, WORD wToolboxBitmapId);
BOOL UnregisterUnknownObject(REFCLSID riidObject);
BOOL UnregisterAutomationObject(LPCTSTR pwszLibName, LPCTSTR pwszObjectName, long lVersion, REFCLSID riidObject);
// #define UnregisterControlObject UnregisterAutomationObject
BOOL UnregisterTypeLibrary(REFCLSID riidLibrary);

// deletes a key in the registr and all of it's subkeys
//
BOOL DeleteKeyAndSubKeys(HKEY hk, LPCTSTR pszSubKey);

#if 0
//=--------------------------------------------------------------------------=
// conversion helpers.
//
void        HiMetricToPixel(const SIZEL *pSizeInHiMetric, SIZEL *pSizeinPixels);
void        PixelToHiMetric(const SIZEL *pSizeInPixels, SIZEL *pSizeInHiMetric);
#endif

#define _UTIL_H_
#endif // _UTIL_H_
