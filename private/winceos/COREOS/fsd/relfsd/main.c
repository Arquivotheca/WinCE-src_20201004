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
#include "relfsd.h"
#include "cefs.h"
#include <RegImportExport.h>



BOOL g_fSecureRelfsd = 0;

#ifdef DEBUG

DBGPARAM dpCurSettings =
{
    TEXT("ReleaseFSD"),
    {
        TEXT("Init"),
        TEXT("Api"),
        TEXT("Error"),
        TEXT("Create"),
        TEXT(""),
        TEXT(""),
        TEXT(""),
        TEXT(""),
        TEXT(""),
        TEXT(""),
        TEXT(""),
        TEXT(""),
        TEXT(""),
        TEXT(""),
        TEXT(""),
        TEXT("")
    },
#if 1
    ZONEMASK_ERROR
#else
    0xFFFF
#endif

};

#endif


VolumeState *g_pvs;

#define DBG_LIST    L"dbglist.txt"
#define APPVERIF_LIST L"verify.txt"

typedef BOOL (*PFN_UpdateList)(wchar_t *pTxt, int nSize);

__inline BOOL IsSeparator (char ch)
{
    return (' ' == ch) || ('\t' == ch) || ('\r' == ch) || ('\n' == ch) || (',' == ch) || (';' == ch);
}

BOOL UpdateDbgList (wchar_t *pTxt, int nSize)
{
    // notify nk for non-empty list
    if (nSize > 2) {
        return KernelLibIoControl ((HANDLE)KMOD_CORE, IOCTL_KLIB_SETDBGLIST, pTxt, nSize*sizeof(WCHAR), NULL, 0, NULL);
    }

    return FALSE;
}

typedef struct _NAMELIST
{
    LPCWSTR pszName;
    struct _NAMELIST *pNext;
}
NAMELIST, *LPNAMELIST;

// Keep this in sync with shell.h
#define TXTSHELL_REG                TEXT("Software\\Microsoft\\TxtShell")
#define TSH_EXT_REG                 TXTSHELL_REG TEXT("\\Extensions")

BOOL SetShellExt (LPCTSTR szShim)
{
    HKEY hKey = NULL;
    DWORD dwDisposition = 0;

    if (ERROR_SUCCESS == RegCreateKeyEx (
        HKEY_LOCAL_MACHINE,
        TSH_EXT_REG,
        0, NULL, 0, 0, NULL,
        & hKey,
        & dwDisposition)) {

        // Blindly set the registry values for shell.exe. We'll let it figure out
        // if this shim actually supports a shell extension.
        if (ERROR_SUCCESS != RegSetValueEx (
            hKey,
            szShim,
            0,
            REG_SZ,
            (BYTE *) szShim,
            (_tcslen (szShim) + 1) * sizeof (TCHAR))) {
            ERRORMSG (1, (_T("Couldn't set value '%s' (%u)\r\n"), szShim, GetLastError ()));
        }

        RegCloseKey (hKey);

        return TRUE;
    }
    else {
        ERRORMSG (1, (_T("Couldn't create reg key '%s' (%u)\r\n"), TSH_EXT_REG, GetLastError ()));

        return FALSE;
    }
}

BOOL SaveShimSettingDWORD(
    LPCWSTR     szShim,
    LPCWSTR     szExe,
    LPCWSTR     szSetting,
    DWORD       dwSetting
    )
{
    LONG lRet = ERROR_SUCCESS;
    TCHAR szSubKey [MAX_PATH];
    HKEY hKey = NULL;
    DWORD dwDisposition = 0;

    szSubKey[0] = L'\0';
    StringCchPrintf (szSubKey, MAX_PATH, _T("Software\\Microsoft\\Shim\\%s\\%s"), szExe, szShim);

    lRet = RegCreateKeyEx (HKEY_LOCAL_MACHINE,
        szSubKey,
        0,
        NULL,
        0,
        KEY_ALL_ACCESS,
        NULL,
        & hKey,
        & dwDisposition);

    if (lRet != ERROR_SUCCESS) {
        DEBUGMSG(1, (_T("SaveShimSettingDWORD: Error (%u) creating key\r\n"), lRet));
        return FALSE;
    }

    lRet = RegSetValueEx (hKey, szSetting, 0, REG_DWORD, (const BYTE *) & dwSetting, sizeof (DWORD));
    if (lRet != ERROR_SUCCESS) {
        DEBUGMSG(1, (_T("SaveShimSettingDWORD: Error (%u) writing value\r\n"), lRet));
        RegCloseKey (hKey);
        return FALSE;
    }

    DEBUGMSG(1, (_T("SaveShimSettingDWORD: wrote %s\\%s:0x%08x\r\n"), szSubKey, szSetting, dwSetting));

    RegCloseKey (hKey);

    return TRUE;
}

BOOL UpdateAppVerifList (wchar_t *pAppVerifList, int nSize)
{
    LPNAMELIST pShims = NULL;
    LPNAMELIST pModules = NULL;
    LPNAMELIST pCurrModule = NULL;
    LPNAMELIST pCurrShim = NULL;
    LPNAMELIST pNext = NULL;
    LPNAMELIST pNew = NULL;
    LPWSTR pLine = NULL;

    const WCHAR  c_szNoMapFiles[]     = L"NoMapFiles";
    const WCHAR  c_szIgnoreNullFree[] = L"IgnoreNullFree";
    const WCHAR  c_szEnableFanOut  [] = L"EnableFanOut";
    const WCHAR  c_szEnableFanOutDlls  [] = L"EnableFanOutDlls";
    const WCHAR  c_szEnableFanOutProcesses  [] = L"EnableFanOutProcesses";
    const WCHAR  c_szFileOutputOnly [] = L"FileOutputOnly";
    const WCHAR  c_szDebugBreakOnError [] = L"DebugBreakOnError";

    // Build up 2 lists - 1 containing modules to test, and 1 containing app verifier modules to apply.
    for (pLine = pAppVerifList; pLine [0]; pLine = pLine + _tcslen (pLine) + 1)
    {
        if (pLine [0] == _T('+'))
        {
            // This line denotes an application verifier module.
            pNew = LocalAlloc (LMEM_ZEROINIT, sizeof (NAMELIST));
            pNew->pszName = pLine + 1;
            pNew->pNext = pShims;
            pShims = pNew;

            SetShellExt (pNew->pszName);
        }
        else if (pLine [0] == _T(':'))
        {
            // This line denotes an option.
            if (!_tcsnicmp (pLine+1, c_szNoMapFiles, _tcslen (c_szNoMapFiles)))
            {
                NKDbgPrintfW (_T("Application verifier autoloader: disabling callstack resolution\r\n"));
                SaveShimSettingDWORD (_T("Verifier"), _T("{default}"), _T("NoMapFiles"), 1);
            }
            else if (!_tcsnicmp (pLine+1, c_szIgnoreNullFree, _tcslen (c_szIgnoreNullFree)))
            {
                NKDbgPrintfW (_T("Application verifier autoloader: setting ignore null free\r\n"));
                SaveShimSettingDWORD (_T("ShimLMem"), _T("{default}"), _T("IgnoreNullFree"), 1);
            }
            else if (!_tcsnicmp (pLine+1, c_szEnableFanOutDlls, _tcslen (c_szEnableFanOutDlls)))
            {
                NKDbgPrintfW (_T("Application verifier autoloader: enabling fan out (dlls)\r\n"));
                SaveShimSettingDWORD (_T("Verifier"), _T("{default}"), _T("PropogateToModules"), 1);
            }
            else if (!_tcsnicmp (pLine+1, c_szEnableFanOutProcesses, _tcslen (c_szEnableFanOutProcesses)))
            {
                NKDbgPrintfW (_T("Application verifier autoloader: enabling fan out (processes)\r\n"));
                SaveShimSettingDWORD (_T("Verifier"), _T("{default}"), _T("PropogateToProcesses"), 1);
            }
            else if (!_tcsnicmp (pLine+1, c_szEnableFanOut, _tcslen (c_szEnableFanOut)))
            {
                NKDbgPrintfW (_T("Application verifier autoloader: enabling fan out (dlls and processes)\r\n"));
                SaveShimSettingDWORD (_T("Verifier"), _T("{default}"), _T("PropogateToModules"), 1);
                SaveShimSettingDWORD (_T("Verifier"), _T("{default}"), _T("PropogateToProcesses"), 1);
            }
            else if (!_tcsnicmp (pLine+1, c_szFileOutputOnly, _tcslen (c_szFileOutputOnly)))
            {
                NKDbgPrintfW (_T("Application verifier autoloader: setting file output only\r\n"));
                SaveShimSettingDWORD (_T("ShimLMem"), _T("{default}"), _T("FileOutputOnly"), 1);
            }
            else if (!_tcsnicmp (pLine+1, c_szDebugBreakOnError, _tcslen (c_szDebugBreakOnError)))
            {
                NKDbgPrintfW (_T("Application verifier autoloader: setting DebugBreak on error\r\n"));
                SaveShimSettingDWORD (_T("Verifier"), _T("{default}"), _T("DebugBreakOnError"), 1);
            }
        }
        else
        {
            // This line denotes a user module.
            pNew = LocalAlloc (LMEM_ZEROINIT, sizeof (NAMELIST));
            pNew->pszName = pLine;
            pNew->pNext = pModules;
            pModules = pNew;
        }
    }

    // Walk the lists, applying the settings.
    for (pCurrModule = pModules; pCurrModule; pCurrModule = pCurrModule->pNext)
    {
        for (pCurrShim = pShims; pCurrShim; pCurrShim = pCurrShim->pNext)
        {
            HKEY hKey = NULL;
            DWORD dwDisposition = 0;

            TCHAR szSubKey [MAX_PATH];
            szSubKey[0] = L'\0';
            StringCchPrintf (szSubKey, MAX_PATH, _T("ShimEngine\\%s\\%s"), pCurrModule->pszName, pCurrShim->pszName);

            NKDbgPrintfW (_T("Application verifier autoloader: applying '%s' to '%s'\r\n"),
                pCurrShim->pszName,
                pCurrModule->pszName
                );

            if (ERROR_SUCCESS == RegCreateKeyEx (
                HKEY_LOCAL_MACHINE,
                szSubKey,
                0,
                NULL,
                0,
                0,
                NULL,
                & hKey,
                & dwDisposition
                ))
            {
                RegCloseKey (hKey);
            }
        }
    }

    // Free the lists.
    for (pCurrShim = pShims; pCurrShim; pCurrShim = pNext)
    {
        pNext = pCurrShim->pNext;
        LocalFree (pCurrShim);
    }

    for (pCurrModule = pModules; pCurrModule; pCurrModule = pNext)
    {
        pNext = pCurrModule->pNext;
        LocalFree (pCurrModule);
    }

    return TRUE;
}

DWORD CopyRegFromDesktop (DWORD hKeySrc, LPCTSTR pszSrc, HKEY hKeyDest, LPCTSTR pszDest)
{
    DWORD hKeySrc2 = 0;
    HKEY hKeyDest2 = 0;
    DWORD dwDisposition = 0;
    DWORD dwIndex = 0;
    DWORD dwSize = 0;
    int nRet = -1;
    CHAR aszTemp [MAX_PATH];
    TCHAR szTemp [MAX_PATH];
    BYTE lpbData [MAX_PATH] = {0};
    DWORD dwType = REG_NONE;
    PTCHAR pTok = NULL;
    DWORD dwNumKeysCopied = 0;
    TCHAR* nexttoken = NULL;

    aszTemp[0] = '\0';
    szTemp[0]  = _T('\0');

    // Open the source (on the desktop)
    if (!WideCharToMultiByte(CP_ACP,
                                          0,
                                          pszSrc,
                                          -1,
                                          aszTemp,
                                          sizeof(aszTemp),
                                          NULL,
                                          NULL)) {
        goto exit;
    }
    
    if (-1 == rRegOpen (hKeySrc, aszTemp, & hKeySrc2))
    {
        goto exit;
    }

    // Open the destination (on the device)
    if (ERROR_SUCCESS != RegCreateKeyEx (hKeyDest, pszDest, 0, NULL, 0, 0, NULL, & hKeyDest2, & dwDisposition))
    {
        goto exit;
    }

    // First, enumerate the (source) keys.
    for (dwIndex = 0;;dwIndex++) {
        dwSize = sizeof(lpbData);
        memset (lpbData, 0, dwSize);
        
        nRet = rRegEnum (hKeySrc2, dwIndex, lpbData, &dwSize);
        if (!nRet || (-1 == nRet))
            break;
        
        dwNumKeysCopied += CopyRegFromDesktop (hKeySrc2, (LPCTSTR) lpbData, hKeyDest2, (LPCTSTR) lpbData);
    }

    // Enumerate the (source) values.

    // We don't have a rRegEnumValues, so we'll have to look for a set of
    // indexed values named 'Valuesnnn', and break it apart.
    for (dwIndex = 1; ; dwIndex++)
    {
        StringCchPrintf (szTemp, MAX_PATH, _T("Values%u"), dwIndex);
        if (!WideCharToMultiByte(CP_ACP,
                                              0,
                                              szTemp,
                                              -1,
                                              aszTemp,
                                              sizeof(aszTemp),
                                              NULL,
                                              NULL)) {
            break;
        }        
        if (dwSize = MAX_PATH * sizeof (TCHAR), nRet = rRegGet (hKeySrc2, aszTemp, & dwType, (BYTE *) szTemp, & dwSize), ((0 == nRet) || (0 == dwSize) || (REG_SZ != dwType)))
        {
            break;
        }

        if (NULL != (pTok = _tcstok_s ((LPTSTR) szTemp, _T(" "), &nexttoken)))
        {
            do
            {
                dwSize = sizeof(lpbData);
                if (!WideCharToMultiByte(CP_ACP,
                                                      0,
                                                      pTok,
                                                      -1,
                                                      aszTemp,
                                                      sizeof(aszTemp),
                                                      NULL,
                                                      NULL)) {
                    goto exit;
                }                        
                if (rRegGet (hKeySrc2, aszTemp, & dwType, lpbData, &dwSize) && dwSize)
                {
                    RegSetValueEx (
                        hKeyDest2,
                        pTok,
                        0,
                        dwType,
                        lpbData,
                        dwSize
                        );

                    dwNumKeysCopied++;
                }
            }
            while (NULL != (pTok = _tcstok_s (NULL, _T(" "), &nexttoken)));
        }
    }

exit:

    if (hKeySrc2) rRegClose (hKeySrc2);
    if (hKeyDest2) RegCloseKey (hKeyDest2);

    return dwNumKeysCopied;
}

BOOL UpdateAppVerifReg (void)
{
    if (g_fSecureRelfsd)
    {
        // Can't read the desktop's registry.
        return FALSE;
    }

    return
        (CopyRegFromDesktop ((DWORD) HKEY_CURRENT_USER, _T("Pegasus\\ApplicationVerifier\\ShimEngine"), HKEY_LOCAL_MACHINE, _T("ShimEngine")) >= 1) &&
        (CopyRegFromDesktop ((DWORD) HKEY_CURRENT_USER, _T("Pegasus\\ApplicationVerifier\\ShimSettings"), HKEY_LOCAL_MACHINE, _T("Software\\Microsoft\\Shim")) >= 0);
}

wchar_t *ConvertListToWchar (char *pTxt, int *pnSize)
{
    int idx = 0, nChars = 0;
    LPWSTR pList;

    // allocate the new list (+2 for max of 2 additional trailing 0)
    if (NULL == (pList = (LPWSTR) LocalAlloc (LMEM_FIXED, (*pnSize + 2) * sizeof (WCHAR)))) {
        return NULL;
    }

    // construct the new list
    do {

        // skip leading white spaces
        while ((idx < *pnSize) && IsSeparator (pTxt[idx])) {
            idx ++;
        }

        // copy the name (non-whitespace chars)
        while ((idx < *pnSize) && (!IsSeparator (pTxt[idx]))) {
            pList[nChars ++] = (WCHAR) pTxt[idx ++];
        }

        // null terminate the string
        pList[nChars ++] = 0;

    } while (idx < *pnSize);

    // add trailing 0
    pList[nChars ++] = 0;

    *pnSize = nChars;

    return pList;
}

BOOL LoadList (const WCHAR *pszFilename, PFN_UpdateList pfnUpdateList)
{
    int     fd = ropen (pszFilename, _O_RDONLY);
    BOOL    fRet = FALSE;

    DEBUGMSG (ZONE_APIS, (L"RefreshDgbList: Opening '%s', fd = %8.8lx\r\n", pszFilename, fd));

    if (-1 != fd) {
        int nSize = rlseek (fd, 0, SEEK_END);     // get the size of the file

        if ((-1 != nSize) && (rlseek (fd, 0, SEEK_SET) != -1)) {        // move back to beginning of file

            char *pTmp = (char *) LocalAlloc (LMEM_FIXED, nSize);       // temp storage
            wchar_t *pList;

            DEBUGMSG (1, (L"size of '%s' = %8.8lx\r\n", pszFilename, nSize));

            if (pTmp && (rread (fd, pTmp, nSize) == nSize)) {

                pList = ConvertListToWchar (pTmp, & nSize);

                if (pList) {
                    fRet = pfnUpdateList (pList, nSize);
                    LocalFree (pList);
                }
            }
            LocalFree (pTmp);
        }

        rclose (fd);
    }

    return fRet;
}

BOOL LoadDbgList (LPCWSTR   pszFile)
{
    return LoadList (pszFile ? pszFile : DBG_LIST, UpdateDbgList);
}

BOOL LoadAppVerifierList (void)
{
    BOOL fRet = TRUE;
    BOOL fTrue = TRUE;

    if (LoadList (APPVERIF_LIST, UpdateAppVerifList) || UpdateAppVerifReg ())
    {
        NKDbgPrintfW (_T("Enabling application verifier.\r\n"));

        // Call into the kernel to set the global flag.
        fRet = KernelLibIoControl (
            (HANDLE) KMOD_APPVERIF,
            IOCTL_APPVERIF_ENABLE,
            & fTrue,
            sizeof (BOOL),
            NULL,
            0,
            NULL
            );
    }

    return fRet;
}

void RegImportLogMessage(DWORD dwLogLevel, LPCWSTR pszFormat, ...)
{
    va_list pArgs;
    va_start (pArgs, pszFormat);
    if (dwLogLevel <= REGIMPEXP_LOG_LEVEL_INFO)
    {
        NKvDbgPrintfW (pszFormat, pArgs);
    }
    va_end (pArgs);
}

// import all settings from the 
void LoadDebugRegistry(LPCWSTR pszRelDirPath)
{
    const WCHAR c_szDebugRegFile[] = L"DebugReg.reg";

    WCHAR szDebugRegPath[MAX_PATH];
    szDebugRegPath[0] = L'\0';

    if (SUCCEEDED(StringCchCopy(szDebugRegPath, _countof(szDebugRegPath), pszRelDirPath))
        && SUCCEEDED(StringCchCat(szDebugRegPath, _countof(szDebugRegPath), L"\\"))
        && SUCCEEDED(StringCchCat(szDebugRegPath, _countof(szDebugRegPath), c_szDebugRegFile))
        && (INVALID_FILE_ATTRIBUTES != GetFileAttributes(szDebugRegPath)))
    {
        RETAILMSG(1, (TEXT("!RELFSD: Importing debug registry from file \"%s\"\n"), szDebugRegPath));
        RegImportFile(szDebugRegPath, RegImportLogMessage, NULL);
    }
}

/*
* Get relfsd registry settings
*   "mount"=?
*   "secure"=dword:?
*/
static HRESULT LoadSettings(HDSK hDsk,
__out_ecount(cchRelDirPath) LPWSTR szRelDirPath,
                            DWORD cchRelDirPath)
{
  HRESULT hr = E_FAIL;
  DWORD dwSecureRelfsd = 0;

  szRelDirPath[0]='\0';

  //get mount dir path
  if (!FSDMGR_GetRegistryString(hDsk, TEXT("mount"), szRelDirPath, cchRelDirPath)) {
    RETAILMSG (1, (TEXT("!RELFSD: Failed to get mount dir from registry. Error=0x%x"), GetLastError()));
    goto Error;
  }

  //get secure relfsd flag
  if (!FSDMGR_GetRegistryValue(hDsk, TEXT("secure"), &dwSecureRelfsd)) {
    RETAILMSG (1, (TEXT("!RELFSD: Failed to get secure flag setting from registry. Error=0x%x"), GetLastError()));
    goto Error;
  }

  hr = S_OK;
Error:
  g_fSecureRelfsd = (dwSecureRelfsd == 1);
  return hr;
}

DWORD RelfsdMountThread(LPVOID lParam)
{
    HANDLE hEvent = INVALID_HANDLE_VALUE;
    HDSK hDsk = (HDSK)lParam;
    WCHAR szRelDirPath[MAX_PATH];

    szRelDirPath[0] = L'\0';

    DEBUGMSG(ZONE_INIT, (L"ReleaseFSD: FSD_MountDisk\n"));

    // load settings from registry
    LoadSettings(hDsk, szRelDirPath, _countof(szRelDirPath));

    EnterCriticalSectionMM(g_pcsMain);
    for(;;) {
        if (PPSHConnect()) {
            g_pvs = LocalAlloc(0, sizeof(VolumeState));
            if (g_pvs) {
                g_pvs->vs_Handle = FSDMGR_RegisterVolume(hDsk, szRelDirPath, (DWORD)g_pvs);
                if (g_pvs->vs_Handle) {
                    DEBUGMSG (ZONE_ERROR, (TEXT("Mounted ReleaseFSD volume '\\%s'\n"),szRelDirPath));
                }
            }
            break;
        } else {
            Sleep(5000);
        }
    }
    LeaveCriticalSectionMM(g_pcsMain);

    // Used for registry functions for PPFS in kernel
    CreateEvent(NULL, TRUE, FALSE, L"WAIT_RELFSD2");

    // load debug list
    if (!g_fSecureRelfsd)
      LoadDbgList(NULL);

    // load app verifier list
    LoadAppVerifierList();

    // load debug registry
    LoadDebugRegistry(szRelDirPath);

    // Set event API
    hEvent = CreateEvent(NULL, TRUE, FALSE, L"ReleaseFSD");
    SetEvent (hEvent);

    return 0;
}

BOOL RELFSD_MountDisk(HDSK hDsk)
{
    // if KITL has already started, connect directly, otherwise create a thread to handle the work
    if (KernelIoControl (IOCTL_EDBG_IS_STARTED, NULL, 0, NULL, 0, NULL)) {
        RelfsdMountThread ((LPVOID)hDsk);
    } else {

        DWORD dwId = 0;
        HANDLE hThread = CreateThread( NULL, 0, (LPTHREAD_START_ROUTINE)RelfsdMountThread, (LPVOID)hDsk, 0, &dwId);
        CloseHandle( hThread);
    }
    return TRUE;
}


/*  RELFSD_UnmountDisk - Deinitialization service called by FSDMGR.DLL
 *
 *  ENTRY
 *      hdsk == FSDMGR disk handle, or NULL to deinit all
 *      frozen volumes on *any* disk that no longer has any open
 *      files or dirty buffers.
 *
 *  EXIT
 *      0 if failure, or the number of devices that were successfully
 *      unmounted.
 */

BOOL RELFSD_UnmountDisk(HDSK hdsk)
{
    FSDMGR_DeregisterVolume(g_pvs->vs_Handle);
    return TRUE;
}

CRITICAL_SECTION g_csMain;
LPCRITICAL_SECTION g_pcsMain;

BOOL WINAPI DllMain(HINSTANCE DllInstance, DWORD dwReason, LPVOID Reserved)
{
    switch(dwReason) {

    case DLL_PROCESS_ATTACH:
        {
            DisableThreadLibraryCalls( (HMODULE)DllInstance);
            DEBUGREGISTER(DllInstance);
            DEBUGMSG(ZONE_INIT,(TEXT("RELFSD!DllMain: DLL_PROCESS_ATTACH\n")));
            g_pcsMain = &g_csMain;
            InitializeCriticalSection(g_pcsMain);
        }
        break;
    case DLL_PROCESS_DETACH:
        {
            DEBUGBREAK((1,1));
            DeleteCriticalSection(g_pcsMain);
            DEBUGMSG(ZONE_INIT,(TEXT("RELFSD!DllMain: DLL_PROCESS_DETACH\n")));
        }
        break;
    default:
        break;
    }
    return TRUE;
}
