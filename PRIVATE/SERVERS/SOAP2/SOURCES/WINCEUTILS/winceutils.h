//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#if !defined(WinCEUtils_h) && defined(UNDER_CE) && !defined(DESKTOP_BUILD)

#define WinCEUtils_h

#include "msxml2.h"
#include <windows.h>
#include <Oaidl.h>

#define OSVERSIONINFOA OSVERSIONINFO
#define GetVersionExA  GetVersionEx


#define wsprintfA sprintf
#define TIME_ZONE_ID_INVALID ((DWORD)0xFFFFFFFF)
#define RegCreateKeyExA My_RegCreateKeyExA 
#define RegDeleteKeyA My_RegDeleteKeyA
#define RegEnumKeyExA My_RegEnumKeyExA 
#define RegOpenKeyExA My_RegOpenKeyExA
#define RegSetValueExA My_RegSetValueExA
#define CreateFileA My_CreateFileA
#define GetModuleFileNameA My_GetModuleFileNameA

#define LoadLibraryExA My_LoadLibraryExA

//--------------------------------------------------------------------------------------------------
// Compare results.  These are returned as a SUCCESS HResult.  Subtracting
// one gives the usual values of -1 for Less Than, 0 for Equal To, +1 for
// Greater Than.
//
//--> line 472
#define VARCMP_LT   0
#define VARCMP_EQ   1
#define VARCMP_GT   2
#define VARCMP_NULL 3

//-->line 732
#define V_I8(X)          V_UNION(X, llVal)
#define V_I8REF(X)       V_UNION(X, pllVal)


//-->line 742
#define V_UI8(X)         V_UNION(X, ullVal)
#define V_UI8REF(X)      V_UNION(X, pullVal)


HRESULT CoCreateFreeThreadedMarshalerE(
LPUNKNOWN punkOuter,
LPUNKNOWN * ppunkMarshaler );



//--------------------------------------------------------------------------------------------------
DWORD GetEnvironmentVariableW(
  LPCTSTR lpName,  // environment variable name
  LPTSTR lpBuffer, // buffer for variable value
  DWORD nSize      // size of buffer
);

DWORD GetEnvironmentVariableA(
    LPCSTR name,
    LPSTR value,
    DWORD size
);

void *bsearch( const void *key, const void *base, size_t num, size_t width, int ( __cdecl *compare ) ( const void *elem1, const void *elem2 ) );


#define ltoa(a, b, c) _ltoa(a, b, c)
#define stricmp(sz1, sz2) _stricmp(sz1, sz2)
#define strnicmp(sz1, sz2, len) _strnicmp(sz1,sz2, len)
#define IsEqualGUID(rguid1, rguid2) (!memcmp(&rguid1, &rguid2, sizeof(GUID)))

__inline int InlineIsEqualGUID(REFGUID rguid1, REFGUID rguid2)
{
   return (
      ((unsigned long *) &rguid1)[0] == ((unsigned long *) &rguid2)[0] &&
      ((unsigned long *) &rguid1)[1] == ((unsigned long *) &rguid2)[1] &&
      ((unsigned long *) &rguid1)[2] == ((unsigned long *) &rguid2)[2] &&
      ((unsigned long *) &rguid1)[3] == ((unsigned long *) &rguid2)[3]);
}


STDAPI VarDecCmp( 
  LPDECIMAL  pdecLeft,         
  LPDECIMAL  pdecRight  
);




HANDLE
WINAPI
My_CreateFileA(
    LPCSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile
    );



DWORD
WINAPI
My_GetModuleFileNameA(
    HMODULE hModule,
    LPSTR lpFilename,
    DWORD nSize
    );


HMODULE
WINAPI
My_LoadLibraryExA(
     LPCSTR lpLibFileName,
     HANDLE hFile,
     DWORD dwFlags
    );


LONG
APIENTRY
My_RegCreateKeyExA (
     HKEY hKey,
     LPCSTR lpSubKey,
     DWORD Reserved,
     LPSTR lpClass,
     DWORD dwOptions,
     REGSAM samDesired,
     LPSECURITY_ATTRIBUTES lpSecurityAttributes,
     PHKEY phkResult,
     LPDWORD lpdwDisposition
    );


LONG
My_RegDeleteKeyA (
    HKEY hKey,
    LPCSTR lpSubKey
    );



LONG
My_RegEnumKeyExA (
     HKEY hKey,
     DWORD dwIndex,
     LPSTR lpName,
     OUT LPDWORD lpcbName,
     LPDWORD lpReserved,
     OUT LPSTR lpClass,
     OUT LPDWORD lpcbClass,
     PFILETIME lpftLastWriteTime
    );


LONG
My_RegOpenKeyExA (
    HKEY hKey,
    LPCSTR lpSubKey,
    DWORD ulOptions,
    REGSAM samDesired,
    PHKEY phkResult
    );

LONG
My_RegSetValueExA (
     HKEY hKey,
     LPCSTR lpValueName,
     DWORD Reserved,
     DWORD dwType,
     CONST BYTE* lpData,
     DWORD cbData
    );

void  _ui64toa (     
        unsigned __int64 val,
        char *buf,
        unsigned int radix);


#endif

