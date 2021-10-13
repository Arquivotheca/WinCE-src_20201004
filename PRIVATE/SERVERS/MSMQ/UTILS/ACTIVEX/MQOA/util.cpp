//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//=--------------------------------------------------------------------------=
// Util.C
//=--------------------------------------------------------------------------=
//
// contains routines that we will find useful.
//
#include "IPServer.H"

#include "Globals.H"
#include "Util.H"


// for ASSERT and FAIL
//
SZTHISFILE


int StringFromGuidW
(
    REFIID   riid,
    LPTSTR    pwszBuf
)
{
    return wsprintf(pwszBuf, L"{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}", riid.Data1, 
            riid.Data2, riid.Data3, riid.Data4[0], riid.Data4[1], riid.Data4[2], 
            riid.Data4[3], riid.Data4[4], riid.Data4[5], riid.Data4[6], riid.Data4[7]);

}

//=--------------------------------------------------------------------------=
// _MakePath
//=--------------------------------------------------------------------------=
// little helper routine for RegisterLocalizedTypeLibs and GetResourceHandle.
// not terrilby efficient or smart, but it's registration code, so we don't
// really care.
//
// Notes:
//
void _MakePath
(
    LPTSTR pwszFull,
    const WCHAR * pwszName,
    LPTSTR pwszOut
)
{
    LPTSTR pwsz;
    LPTSTR pwszLast;

    lstrcpy(pwszOut, pwszFull);
    pwsz = pwszLast = pwszOut;
    while (*pwsz) {
        if (*pwsz == '\\')
            pwszLast = ++pwsz;
        pwsz++;
    }

    // got the last \ character, so just go and replace the name.
    //
    lstrcpy(pwszLast, pwszName);
}


//=--------------------------------------------------------------------------=
// RegisterUnknownObject
//=--------------------------------------------------------------------------=
// registers a simple CoCreatable object.  nothing terribly serious.
// we add the following information to the registry:
//
// HKEY_CLASSES_ROOT\CLSID\<CLSID> = <ObjectName> Object
// HKEY_CLASSES_ROOT\CLSID\<CLSID>\InprocServer32 = <path to local server>
//
// Parameters:
//    LPCSTR       - [in] Object Name
//    REFCLSID     - [in] CLSID of the object
//
// Output:
//    BOOL         - FALSE means couldn't register it all
//
// Notes:
//
BOOL RegisterUnknownObject
(
    LPCTSTR   pszObjectName,
    REFCLSID riidObject
)
{
    HKEY  hk = NULL, hkSub = NULL;
    WCHAR  wszGuidStr[GUID_STR_LEN];
    DWORD dwPathLen, dwDummy;
    WCHAR  wszScratch[MAX_PATH];
    long  l;

    // clean out any garbage
    //
    UnregisterUnknownObject(riidObject);

    // HKEY_CLASSES_ROOT\CLSID\<CLSID> = <ObjectName> Object
    // HKEY_CLASSES_ROOT\CLSID\<CLSID>\InprocServer32 = <path to local server>
    // HKEY_CLASSES_ROOT\CLSID\<CLSID>\InprocServer32  @ThreadingModel = Free
    //
    if (!StringFromGuidW(riidObject, wszGuidStr)) goto CleanUp;
    wsprintf(wszScratch, L"CLSID\\%s", wszGuidStr);
    l = RegCreateKeyEx(HKEY_CLASSES_ROOT, wszScratch, 0, L"", REG_OPTION_NON_VOLATILE,
                       KEY_READ | KEY_WRITE, NULL, &hk, &dwDummy);
    CLEANUP_ON_ERROR(l);

    wsprintf(wszScratch, L"%s Object", pszObjectName);
    l = RegSetValueEx(hk, NULL, 0, REG_SZ, (BYTE *)wszScratch, (lstrlen(wszScratch) + 1)*sizeof(WCHAR));
    CLEANUP_ON_ERROR(l);

    l = RegCreateKeyEx(hk, L"InprocServer32", 0, L"", REG_OPTION_NON_VOLATILE,
                       KEY_READ | KEY_WRITE, NULL, &hkSub, &dwDummy);
    CLEANUP_ON_ERROR(l);

    dwPathLen = GetModuleFileName(g_hInstance, wszScratch, sizeof(wszScratch)/sizeof(wszScratch[0]));
    if (!dwPathLen) goto CleanUp;

    l = RegSetValueEx(hkSub, NULL, 0, REG_SZ, (BYTE *)wszScratch, (dwPathLen + 1)*sizeof(WCHAR));
    CLEANUP_ON_ERROR(l);

    l = RegSetValueEx(hkSub, L"ThreadingModel", 0, REG_SZ, (BYTE *)L"Free", (lstrlen(L"Free")+1)*sizeof(WCHAR));
    CLEANUP_ON_ERROR(l);

    RegCloseKey(hkSub);
    RegCloseKey(hk);

    return TRUE;

    // we are not very happy!
    //
  CleanUp:
    if (hk) RegCloseKey(hk);
    if (hkSub) RegCloseKey(hkSub);
    return FALSE;

}

//=--------------------------------------------------------------------------=
// RegisterAutomationObject
//=--------------------------------------------------------------------------=
// given a little bit of information about an automation object, go and put it
// in the registry.
// we add the following information in addition to that set up in
// RegisterUnknownObject:
//
//
// HKEY_CLASSES_ROOT\<LibraryName>.<ObjectName> = <ObjectName> Object
// HKEY_CLASSES_ROOT\<LibraryName>.<ObjectName>\CLSID = <CLSID>
// HKEY_CLASSES_ROOT\<LibraryName>.<ObjectName>\CurVer = <ObjectName>.Object.<VersionNumber>
//
// HKEY_CLASSES_ROOT\<LibraryName>.<ObjectName>.<VersionNumber> = <ObjectName> Object
// HKEY_CLASSES_ROOT\<LibraryName>.<ObjectName>.<VersionNumber>\CLSID = <CLSID>
//
// HKEY_CLASSES_ROOT\CLSID\<CLSID>\TypeLib = <LibidOfTypeLibrary>
// HKEY_CLASSES_ROOT\CLSID\<CLSID>\ProgID = <LibraryName>.<ObjectName>.<VersionNumber>
// HKEY_CLASSES_ROOT\CLSID\<CLSID>\VersionIndependentProgID = <LibraryName>.<ObjectName>
//
// Parameters:
//    LPCSTR       - [in] Library Name
//    LPCSTR       - [in] Object Name
//    long         - [in] Version Number
//    REFCLSID     - [in] LIBID of type library
//    REFCLSID     - [in] CLSID of the object
//
// Output:
//    BOOL         - FALSE means not all of it was registered
//
// Notes:
//
BOOL RegisterAutomationObject
(
    LPCTSTR   pwszLibName,
    LPCTSTR   pwszObjectName,
    long     lVersion,
    REFCLSID riidLibrary,
    REFCLSID riidObject
)
{
    HKEY  hk = NULL, hkSub = NULL;
    WCHAR  wszGuidStr[GUID_STR_LEN];
    WCHAR  wszScratch[MAX_PATH];
    long  l;
    DWORD dwDummy;

    // first register the simple Unknown stuff.
    //
    if (!RegisterUnknownObject(pwszObjectName, riidObject)) return FALSE;

    // HKEY_CLASSES_ROOT\<LibraryName>.<ObjectName> = <ObjectName> Object
    // HKEY_CLASSES_ROOT\<LibraryName>.<ObjectName>\CLSID = <CLSID>
    // HKEY_CLASSES_ROOT\<LibraryName>.<ObjectName>\CurVer = <ObjectName>.Object.<VersionNumber>
    //
	if (wcslen (pwszLibName) + wcslen(pwszObjectName) + 1 >= MAX_PATH) return FALSE;

    lstrcpy(wszScratch, pwszLibName);
    lstrcat(wszScratch, L".");
    lstrcat(wszScratch, pwszObjectName);

    l = RegCreateKeyEx(HKEY_CLASSES_ROOT, wszScratch, 0L, L"",
                       REG_OPTION_NON_VOLATILE, KEY_READ | KEY_WRITE,
                       NULL, &hk, &dwDummy);
    CLEANUP_ON_ERROR(l);

    wsprintf(wszScratch, L"%s Object", pwszObjectName);
    l = RegSetValueEx(hk, NULL, 0L, REG_SZ, (BYTE *)wszScratch, (lstrlen(wszScratch)+1)*sizeof(WCHAR));
    CLEANUP_ON_ERROR(l);

    l = RegCreateKeyEx(hk, L"CLSID", 0L, L"", REG_OPTION_NON_VOLATILE,
                       KEY_READ | KEY_WRITE, NULL, &hkSub, &dwDummy);
    CLEANUP_ON_ERROR(l);

    if (!StringFromGuidW(riidObject, wszGuidStr))
        goto CleanUp;

    l = RegSetValueEx(hkSub, NULL, 0L, REG_SZ, (BYTE *)wszGuidStr, (lstrlen(wszGuidStr) + 1)*sizeof(WCHAR));
    CLEANUP_ON_ERROR(l);

    RegCloseKey(hkSub);
    l = RegCreateKeyEx(hk, L"CurVer", 0, L"", REG_OPTION_NON_VOLATILE,
                       KEY_READ | KEY_WRITE, NULL, &hkSub, &dwDummy);
    CLEANUP_ON_ERROR(l);

    wsprintf(wszScratch, L"%s.%s.%ld", pwszLibName, pwszObjectName, lVersion);
    l = RegSetValueEx(hkSub, NULL, 0, REG_SZ, (BYTE *)wszScratch, (lstrlen(wszScratch)+1)*sizeof(WCHAR));
    CLEANUP_ON_ERROR(l);

    RegCloseKey(hkSub);
    RegCloseKey(hk);

    // HKEY_CLASSES_ROOT\<LibraryName>.<ObjectName>.<VersionNumber> = <ObjectName> Object
    // HKEY_CLASSES_ROOT\<LibraryName>.<ObjectName>.<VersionNumber>\CLSID = <CLSID>
    //
    l = RegCreateKeyEx(HKEY_CLASSES_ROOT, wszScratch, 0, L"", REG_OPTION_NON_VOLATILE,
                       KEY_READ | KEY_WRITE, NULL, &hk, &dwDummy);
    CLEANUP_ON_ERROR(l);

    wsprintf(wszScratch, L"%s Object", pwszObjectName);
    l = RegSetValueEx(hk, NULL, 0, REG_SZ, (BYTE *)wszScratch, (lstrlen(wszScratch)+1)*sizeof(WCHAR));
    CLEANUP_ON_ERROR(l);

    l = RegCreateKeyEx(hk, L"CLSID", 0, L"", REG_OPTION_NON_VOLATILE,
                       KEY_READ | KEY_WRITE, NULL, &hkSub, &dwDummy);
    CLEANUP_ON_ERROR(l);

    l = RegSetValueEx(hkSub, NULL, 0, REG_SZ, (BYTE *)wszGuidStr, (lstrlen(wszGuidStr) + 1)*sizeof(WCHAR));
    CLEANUP_ON_ERROR(l);

    RegCloseKey(hkSub);
    RegCloseKey(hk);

    // HKEY_CLASSES_ROOT\CLSID\<CLSID>\ProgID = <LibraryName>.<ObjectName>.<VersionNumber>
    // HKEY_CLASSES_ROOT\CLSID\<CLSID>\VersionIndependentProgID = <LibraryName>.<ObjectName>
    // HKEY_CLASSES_ROOT\CLSID\<CLSID>\TypeLib = <LibidOfTypeLibrary>
    //
    if (!StringFromGuidW(riidObject, wszGuidStr)) goto CleanUp;
    wsprintf(wszScratch, L"CLSID\\%s", wszGuidStr);

    l = RegCreateKeyEx(HKEY_CLASSES_ROOT, wszScratch, 0, L"", REG_OPTION_NON_VOLATILE,
                       KEY_READ|KEY_WRITE, NULL, &hk, &dwDummy);
    CLEANUP_ON_ERROR(l);

    l = RegCreateKeyEx(hk, L"VersionIndependentProgID", 0, L"", REG_OPTION_NON_VOLATILE,
                       KEY_READ | KEY_WRITE, NULL, &hkSub, &dwDummy);
    CLEANUP_ON_ERROR(l);

    wsprintf(wszScratch, L"%s.%s", pwszLibName, pwszObjectName);
    l = RegSetValueEx(hkSub, NULL, 0, REG_SZ, (BYTE *)wszScratch, (lstrlen(wszScratch) + 1)*sizeof(WCHAR));
    CLEANUP_ON_ERROR(l);

    RegCloseKey(hkSub);

    l = RegCreateKeyEx(hk, L"ProgID", 0, L"", REG_OPTION_NON_VOLATILE,
                       KEY_READ | KEY_WRITE, NULL, &hkSub, &dwDummy);
    CLEANUP_ON_ERROR(l);

    wsprintf(wszScratch, L"%s.%s.%ld", pwszLibName, pwszObjectName, lVersion);
    l = RegSetValueEx(hkSub, NULL, 0, REG_SZ, (BYTE *)wszScratch, (lstrlen(wszScratch) + 1)*sizeof(WCHAR));
    CLEANUP_ON_ERROR(l);

    RegCloseKey(hkSub);
    l = RegCreateKeyEx(hk, L"TypeLib", 0, L"", REG_OPTION_NON_VOLATILE, KEY_READ | KEY_WRITE,
                       NULL, &hkSub, &dwDummy);

    if (!StringFromGuidW(riidLibrary, wszGuidStr)) goto CleanUp;

    l = RegSetValueEx(hkSub, NULL, 0, REG_SZ, (BYTE *)wszGuidStr, (lstrlen(wszGuidStr) + 1)*sizeof(WCHAR));
    CLEANUP_ON_ERROR(l);

    RegCloseKey(hkSub);
    RegCloseKey(hk);
    return TRUE;

  CleanUp:
    if (hk) RegCloseKey(hkSub);
    if (hk) RegCloseKey(hk);
    return FALSE;
}



//=--------------------------------------------------------------------------=
// UnregisterUnknownObject
//=--------------------------------------------------------------------------=
// cleans up all the stuff that RegisterUnknownObject puts in the
// registry.
//
// Parameters:
//    REFCLSID     - [in] CLSID of the object
//
// Output:
//    BOOL         - FALSE means not all of it was registered
//
// Notes:
//    - WARNING: this routine will blow away all other keys under the CLSID
//      for this object.  mildly anti-social, but likely not a problem.
//
BOOL UnregisterUnknownObject
(
    REFCLSID riidObject
)
{
    WCHAR wszScratch[MAX_PATH];
    HKEY hk;
    BOOL f;
    long l;

    // delete everybody of the form
    //   HKEY_CLASSES_ROOT\CLSID\<CLSID> [\] *
    //
    if (!StringFromGuidW(riidObject, wszScratch))
        return FALSE;

    l = RegOpenKeyEx(HKEY_CLASSES_ROOT, L"CLSID", 0, KEY_ALL_ACCESS, &hk);
    if (l != ERROR_SUCCESS) return FALSE;

    f = DeleteKeyAndSubKeys(hk, wszScratch);
    RegCloseKey(hk);

    return f;
}

//=--------------------------------------------------------------------------=
// UnregisterAutomationObject
//=--------------------------------------------------------------------------=
// unregisters an automation object, including all of it's unknown object
// information.
//
// Parameters:
//    LPCSTR       - [in] Library Name
//    LPCSTR       - [in] Object Name
//    long         - [in] Version Number
//    REFCLSID     - [in] CLSID of the object
//
// Output:
//    BOOL         - FALSE means couldn't get it all unregistered.
//
// Notes:
//
BOOL UnregisterAutomationObject
(
    LPCTSTR   pwszLibName,
    LPCTSTR   pwszObjectName,
    long     lVersion,
    REFCLSID riidObject
)
{
    WCHAR wszScratch[MAX_PATH];
    BOOL f;

    // first thing -- unregister Unknown information
    //
    f = UnregisterUnknownObject(riidObject);
    if (!f) return FALSE;

    // delete everybody of the form:
    //   HKEY_CLASSES_ROOT\<LibraryName>.<ObjectName> [\] *
    //
    wsprintf(wszScratch, L"%s.%s", pwszLibName, pwszObjectName);
    f = DeleteKeyAndSubKeys(HKEY_CLASSES_ROOT, wszScratch);
    if (!f) return FALSE;

    // delete everybody of the form
    //   HKEY_CLASSES_ROOT\<LibraryName>.<ObjectName>.<VersionNumber> [\] *
    //
    wsprintf(wszScratch, L"%s.%s.%ld", pwszLibName, pwszObjectName, lVersion);
    f = DeleteKeyAndSubKeys(HKEY_CLASSES_ROOT, wszScratch);
    if (!f) return FALSE;

    return TRUE;
}

//=--------------------------------------------------------------------------=
// UnregisterTypeLibrary
//=--------------------------------------------------------------------------=
// blows away the type library keys for a given libid.
//
// Parameters:
//    REFCLSID        - [in] libid to blow away.
//
// Output:
//    BOOL            - TRUE OK, FALSE bad.
//
// Notes:
//    - WARNING: this function just blows away the entire type library section,
//      including all localized versions of the type library.  mildly anti-
//      social, but not killer.
//
BOOL UnregisterTypeLibrary
(
    REFCLSID riidLibrary
)
{
    HKEY hk;
    WCHAR wszScratch[GUID_STR_LEN];
    long l;
    BOOL f;

    // convert the libid into a string.
    //
    if (!StringFromGuidW(riidLibrary, wszScratch))
        return FALSE;

    l = RegOpenKeyEx(HKEY_CLASSES_ROOT, L"TypeLib", 0, KEY_ALL_ACCESS, &hk);
    if (l != ERROR_SUCCESS) return FALSE;

    f = DeleteKeyAndSubKeys(hk, wszScratch);
    RegCloseKey(hk);
    return f;
}

//=--------------------------------------------------------------------------=
// DeleteKeyAndSubKeys
//=--------------------------------------------------------------------------=
// deletes a key and all of its subkeys.
//
// Parameters:
//    HKEY                - [in] delete the descendant specified
//    LPCSTR              - [in] i'm the descendant specified
//
// Output:
//    BOOL                - TRUE OK, FALSE baaaad.
//
// Notes:
//    - I don't feel too bad about implementing this recursively, since the
//      depth isn't likely to get all the great.
//    - Despite the win32 docs claiming it does, RegDeleteKey doesn't seem to
//      work with sub-keys under windows 95.
//
BOOL DeleteKeyAndSubKeys
(
    HKEY    hkIn,
    LPCTSTR  pwszSubKey
)
{
    HKEY  hk;
    WCHAR  wszTmp[MAX_PATH];
    DWORD dwTmpSize;
    long  l;
    BOOL  f;

    l = RegOpenKeyEx(hkIn, pwszSubKey, 0, KEY_ALL_ACCESS, &hk);
    if (l != ERROR_SUCCESS) return FALSE;

    // loop through all subkeys, blowing them away.
    //
    f = TRUE;
    while (f) {
        dwTmpSize = MAX_PATH;
        // We're deleting keys, so always enumerate the 0th
        l = RegEnumKeyEx(hk, 0, wszTmp, &dwTmpSize, 0, NULL, NULL, NULL);
        if (l != ERROR_SUCCESS) break;
        f = DeleteKeyAndSubKeys(hk, wszTmp);
    }

    // there are no subkeys left, [or we'll just generate an error and return FALSE].
    // let's go blow this dude away.
    //
    RegCloseKey(hk);
    l = RegDeleteKey(hkIn, pwszSubKey);

    return (l == ERROR_SUCCESS) ? TRUE : FALSE;
}



// from Globals.C
//
extern HINSTANCE    g_hInstResources;

//=--------------------------------------------------------------------------=
// GetResourceHandle
//=--------------------------------------------------------------------------=
// returns the resource handle.  we use the host's ambient Locale ID to
// determine, from a table in the DLL, which satellite DLL to load for
// localized resources.
//
// Output:
//    HINSTANCE
//
// Notes:
//
HINSTANCE GetResourceHandle
(
    void
)
{
    int i;
    WCHAR wszExtension[5], wszTmp[MAX_PATH];
    WCHAR wszDllName[MAX_PATH], wszFinalName[MAX_PATH];

    // crit sect this so that we don't mess anything up.
    //
    EnterCriticalSection(&g_CriticalSection);

    // don't do anything if we don't have to
    //
    if (g_hInstResources || !g_fSatelliteLocalization)
        goto CleanUp;

    // we're going to call GetLocaleInfo to get the abbreviated name for the
    // LCID we've got.
    //
    i = GetLocaleInfo(g_lcidLocale, LOCALE_SABBREVLANGNAME, wszExtension, sizeof(wszExtension));
    if (!i) goto CleanUp;

    // we've got the language extension.  go and load the DLL name from the
    // resources and then tack on the extension.
    // please note that all inproc sers -must- have the string resource 1001
    // defined to the base name of the server if they wish to support satellite
    // localization.
    //
    i = LoadString(g_hInstance, 1001, wszTmp, sizeof(wszTmp)/sizeof(wszTmp[0]));
    ASSERT(i, L"This server doesn't have IDS_SERVERBASENAME defined in their resources!");
    if (!i) goto CleanUp;

    // got the basename and the extention. go and combine them, and then add
    // on the .DLL for them.
    //
    wsprintf(wszDllName, L"%s%s.DLL", wszTmp, wszExtension);

    // try to load in the DLL
    //
    GetModuleFileName(g_hInstance, wszTmp, MAX_PATH);
    _MakePath(wszTmp, wszDllName, wszFinalName);

    g_hInstResources = LoadLibrary(wszFinalName);

    // if we couldn't find it with the entire LCID, try it with just the primary
    // langid
    //
    if (!g_hInstResources) {
        LPTSTR pwsz;
        LCID lcid;
        lcid = MAKELCID(MAKELANGID(PRIMARYLANGID(LANGIDFROMLCID(g_lcidLocale)), SUBLANG_DEFAULT), SORT_DEFAULT);
        i = GetLocaleInfo(lcid, LOCALE_SABBREVLANGNAME, wszExtension, sizeof(wszExtension));
        if (!i) goto CleanUp;

        // reconstruct the DLL name.  the -7 is the length of XXX.DLL. 
        // there are no DBCS lang identifiers.
        // finally, retry the load
        //
        pwsz = wszFinalName + lstrlen(wszFinalName);
        memcpy((LPTSTR)pwsz - (7*sizeof(WCHAR)), wszExtension, 3);
        g_hInstResources = LoadLibrary(wszFinalName);
    }

  CleanUp:
    // if we couldn't load the DLL for some reason, then just return the
    // current resource handle, which is good enough.
    //
    if (!g_hInstResources) g_hInstResources = g_hInstance;
    LeaveCriticalSection(&g_CriticalSection);

    return g_hInstResources;
}

