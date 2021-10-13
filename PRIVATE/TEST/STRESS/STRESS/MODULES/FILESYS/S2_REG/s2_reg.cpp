//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#include <windows.h>
#include <tchar.h>
#include <stressutils.h>
#include "clparse.h"

typedef UINT (*PFN_TESTPROC)(HKEY hKey, LPCTSTR pszKeyName);

// test function prototypes

UINT Tst_EnumRegistryKeyBreadthFirst(HKEY hKey, IN LPCTSTR szKeyName);
UINT Tst_EnumRegistryKeyDepthFirst(HKEY hKey, IN LPCTSTR szKeyName);
UINT Tst_CreateRandomRegistryKeys(HKEY hKey, IN LPCTSTR szKeyName);
UINT Tst_FlushKey(HKEY hKey, IN LPCTSTR szKeyName);

// constants 

static const HKEY ROOT_HKEY_LIST[] = {
    HKEY_LOCAL_MACHINE,
    HKEY_CURRENT_USER
};

static const LPTSTR ROOT_HKEY_NAME_LIST[] = {
    TEXT("HKEY_LOCAL_MACHINE"),
    TEXT("HKEY_CURRENT_USER"),
    TEXT("HKEY_CLASSES_ROOT"),
    TEXT("HKEY_USERS")
};

static const DWORD ROOT_HKEY_LIST_COUNT = sizeof(ROOT_HKEY_LIST)/sizeof(ROOT_HKEY_LIST[0]);

static const DWORD REG_TYPE_LIST[] = {
    REG_BINARY,
    REG_DWORD,
    REG_SZ
};

static const LPTSTR REG_TYPE_NAME_LIST[] = {
    TEXT("REG_BINARY"),
    TEXT("REG_DWORD"),
    TEXT("REG_SZ"),
};

static const DWORD REG_TYPE_COUNT = sizeof(REG_TYPE_LIST)/sizeof(REG_TYPE_LIST[0]);

static const PFN_TESTPROC TEST_FUNCTION_TABLE[] = {
    Tst_EnumRegistryKeyBreadthFirst,
    Tst_EnumRegistryKeyDepthFirst,
    Tst_CreateRandomRegistryKeys,
    Tst_FlushKey
};

static const DWORD TEST_FUNCTION_TABLE_COUNT = sizeof(TEST_FUNCTION_TABLE)/sizeof(TEST_FUNCTION_TABLE[0]);

static const LPTSTR REG_SZ_VALUE_LIST[] = {
    L"This function stores data in the value field of an open registry key. It can also set additional value and type information for the specified key.",
    L"Handle to a currently open key or any of the following predefined reserved handle values:",
    L"Pointer to a string containing the name of the value to set. If a value with this name is not already present in the key, the function adds it to the key. If this parameter is NULL or an empty string, the function sets the type and data for the key's unnamed value. Registry keys do not have default values, but they can have one unnamed value, which can be of any type. The maximum length of a value name is 255, not including the terminating NULL character.",
    L"Reserved; must be zero.",
    L"Specifies the type of information to be stored as the value's data. This parameter can be one of the following values.",
    L"Pointer to a buffer containing the data to be stored with the specified value name.",
    L"Specifies the size, in bytes, of the information pointed to by the lpData parameter. If the data is of type REG_SZ, REG_EXPAND_SZ, or REG_MULTI_SZ, cbData must include the size of the terminating null character. The maximum size of data allowed in Windows CE is 4 KB.",
    L"Windows CE supports only the Unicode version of this function.",
    L"Value lengths are limited by available memory. Long values (more than 2048 bytes) should be stored as files with the filenames stored in the registry. This helps the registry perform efficiently. Application elements such as icons, bitmaps, and executable files should be stored as files and not be placed in the registry."
};
static const DWORD REG_SZ_VALUE_COUNT = sizeof(REG_SZ_VALUE_LIST) / sizeof(REG_SZ_VALUE_LIST[0]);

// globals

TCHAR g_szModuleName[MAX_PATH] = TEXT("");
HANDLE g_hInst = NULL;
CClParse *g_pCmdLine = NULL;
DWORD g_maxRecursion = 3;
DWORD g_cThreads = 3;
DWORD g_maxWidth = 10;

// functions

/////////////////////////////////////////////////////////////////////////////////////
inline BOOL FlipACoin(DWORD odds = 2)
{
    return (0 == (Random() % odds));
}

/////////////////////////////////////////////////////////////////////////////////////
LONG RegSetRandomValue(
    IN HKEY hKey,
    IN LPCTSTR pszValName
    )
{
    DWORD cbData, index;
    PBYTE pData = NULL;

    DWORD type = Random() % REG_TYPE_COUNT;
    LPTSTR pszType = REG_TYPE_NAME_LIST[type];

    type = REG_TYPE_LIST[type];

    switch(type) {

    case REG_BINARY:
    case REG_SZ:
        index = Random() % REG_SZ_VALUE_COUNT;
        pData = (PBYTE)REG_SZ_VALUE_LIST[index];
        cbData = (_tcslen(REG_SZ_VALUE_LIST[index])+1) * sizeof(TCHAR);
        break;

    case REG_DWORD:
        index = Random();
        pData = (PBYTE)&index;
        cbData = sizeof(index);
        break;
    }

    return RegSetValueEx(hKey, pszValName, 0, type, pData, cbData);
    
}
    

/////////////////////////////////////////////////////////////////////////////////////
UINT EnumRegistryValues(
    IN HKEY hKey,
    IN LPCTSTR pszKeyName,
    IN DWORD param
    )
{
    UINT retVal = CESTRESS_PASS;
    BYTE *pData = NULL;
    TCHAR szValue[MAX_PATH] = TEXT("");
    DWORD lastErr, index, cbSize, type;
    
    // enumerate every value in the key
    for(index = 0; ; index++) {

        // enumerate the value at this index
        cbSize = sizeof(szValue);
        lastErr = RegEnumValue(hKey, index, szValue, &cbSize, NULL, &type, NULL, NULL);
        SetLastError(lastErr);
        if(ERROR_NO_MORE_ITEMS == lastErr) {
            LogVerbose(TEXT("RegEnumValue(\"%s\") returned ERROR_NO_MORE_ITEMS"), pszKeyName);
            break; // done enumerating values   
        } else if(ERROR_SUCCESS != lastErr) {
            LogFail(TEXT("RegEnumValue(\"%s\") failed with unexpected system error %u"), pszKeyName, lastErr);
            retVal = CESTRESS_FAIL;
            goto done;
        }

        LogVerbose(TEXT("enum value \"%s\\%s\""), pszKeyName, szValue);

        // call RegQueryValueEx with NULL data and non-NULL size to determine
        // the size of the actual data to retrieve
        cbSize = 0;
        type = REG_MUI_SZ;
        lastErr = RegQueryValueEx(hKey, szValue, NULL, &type, NULL, &cbSize);
        SetLastError(lastErr);
        if(ERROR_FILE_NOT_FOUND == lastErr) {
            continue;
        }
        if(ERROR_SUCCESS != lastErr) {
            LogFail(TEXT("RegQueryValueEx(\"%s\\%s\") (query size) failed with unexpected system error %u"), pszKeyName, szValue, lastErr);
            retVal = CESTRESS_FAIL;
            goto done;
        }

        // we'll handle REG_MUI_SZ as REG_SZ if /skipmui is not set
        // if /skipmui is set, then we will treat it like a REG_MUI_SZ and will
        // not end up loading a DLL
        if(REG_MUI_SZ == type)
        {
            if(g_pCmdLine && g_pCmdLine->GetOpt(TEXT("skipmui")))
            {
                LogVerbose(TEXT("RegQueryValueEx() skipping REG_MUI_SZ read as REG_SZ"));
            }
            else
            {
                // requery with empty type to retrieve the REG_SZ size
                cbSize = 0;
                type = 0;
                lastErr = RegQueryValueEx(hKey, szValue, NULL, &type, NULL, &cbSize);
                SetLastError(lastErr);
                if(ERROR_FILE_NOT_FOUND == lastErr) {
                    continue;
                }
                if(ERROR_SUCCESS != lastErr) {
                    LogFail(TEXT("RegQueryValueEx(\"%s\\%s\") (query size) failed with unexpected system error %u"), pszKeyName, szValue, lastErr);
                    retVal = CESTRESS_FAIL;
                    goto done;
                }
            }
        } 


        // zero sized data seems unlikely
        if(0 == cbSize) {
            LogWarn2(TEXT("RegQueryValueEx(\"%s\\%s\") returned zero data size"), pszKeyName, szValue);
            retVal = CESTRESS_WARN2;
            continue;
        }

        // allocate a buffer to read the data
        pData = new BYTE[cbSize];
        if(NULL == pData) {
            LogWarn2(TEXT("new BYTE[%u] failed; system error %u"), cbSize, GetLastError());
            retVal = CESTRESS_WARN2;
            continue;
        }

        // query the value into the new buffer
        LogVerbose(TEXT("query value \"%s\\%s\""), pszKeyName, szValue);
        lastErr = RegQueryValueEx(hKey, szValue, NULL, &type, pData, &cbSize);
        SetLastError(lastErr);
        if(ERROR_SUCCESS != lastErr && ERROR_FILE_NOT_FOUND == lastErr) {
            LogFail(TEXT("RegQueryValueEx(\"%s\\%s\") (query value) failed with unexpected system error %u"), pszKeyName, szValue, lastErr);
            retVal = CESTRESS_FAIL;
            goto done;
        }

        delete[] pData;
        pData = NULL;
    }

done:
    if(NULL != pData) {
        delete[] pData;
    }
    return retVal;
}

/////////////////////////////////////////////////////////////////////////////////////
UINT RecursiveEnumRegistryKey(
    IN HKEY hKey, 
    IN LPCTSTR pszKeyName,
    IN BOOL fDepthFirst,
    IN DWORD maxRecursion
    )
{
    UINT retVal = CESTRESS_PASS;
    HKEY hSubKey = NULL;    
    TCHAR szSubKey[MAX_PATH] = TEXT("");
    TCHAR szSubKeyFull[MAX_PATH] = TEXT("");
    DWORD lastErr, index, cbSize;
    
    LogVerbose(TEXT("enum key \"%s\""), pszKeyName);

    // breadth-first enumerates values before keys
    if(!fDepthFirst) {
        // enumerate all the values in the key
        retVal = EnumRegistryValues(hKey, pszKeyName, 0);
        if(CESTRESS_FAIL == retVal) {
            goto done;
        }
    }
    
    // in depth-first, enumerate every key first
    if(maxRecursion > 0) {
        
        // break from this loop once RegEnumKeyEx fails
        for(index = 0; ; index++) {

            // enumerate subkey at this index
            cbSize = sizeof(szSubKey);
            lastErr = RegEnumKeyEx(hKey, index, szSubKey, &cbSize, NULL, NULL, 0, NULL);
            SetLastError(lastErr);
            if(ERROR_NO_MORE_ITEMS == lastErr) {
                LogVerbose(TEXT("RegEnumKeyEx(\"%s\") returned ERROR_NO_MORE_ITEMS"), pszKeyName);
                break; // done enumerating sub keys   
                
            } else if(ERROR_SUCCESS != lastErr) {
                LogFail(TEXT("RegEnumKeyEx(\"%s\") failed with unexpected system error %u"), pszKeyName, lastErr);
                retVal = CESTRESS_FAIL;
                goto done;
            } 

            // build the full name of the subkey (for logging purposes only)
            StringCchPrintf(szSubKeyFull, MAX_PATH, TEXT("%s\\%s"), pszKeyName, szSubKey);

            // open an HKEY to the subkey
            lastErr = RegOpenKeyEx(hKey, szSubKey, 0, 0, &hSubKey);
            SetLastError(lastErr);
            if(ERROR_FILE_NOT_FOUND == lastErr) {
                continue;
            }
            if(ERROR_SUCCESS != lastErr) {
                LogFail(TEXT("RegOpenKeyEx(\"%s\") failed; system error %u"), szSubKeyFull, lastErr);
                retVal = CESTRESS_FAIL;
                goto done;
            }

            // recursively enumerate the key
            retVal = RecursiveEnumRegistryKey(hSubKey, szSubKeyFull, fDepthFirst, maxRecursion-1);
            if(CESTRESS_FAIL == retVal) {
                goto done;
            }

            // close the sub key
            lastErr = RegCloseKey(hSubKey);
            SetLastError(lastErr);
            if(ERROR_SUCCESS != lastErr) {
                LogFail(TEXT("RegCloseKey(\"%s\") failed; system error %u"), szSubKeyFull, lastErr);
                retVal = CESTRESS_FAIL;
                goto done;
            }
            hSubKey = NULL;
        }
    }   

    // depth-first enumerates values after keys
    if(fDepthFirst) {
        // enumerate all the values in the key
        retVal = EnumRegistryValues(hKey, pszKeyName, 0);
        if(CESTRESS_FAIL == retVal) {
            goto done;
        }
    }

done:
    if(NULL != hSubKey) {
        RegCloseKey(hSubKey);
    }
    return retVal;
}

UINT RecursiveCreateKeysAndValues(
    IN HKEY hKey,
    IN LPCTSTR pszKeyName,
    IN DWORD maxRecursion
    )
{
    UINT retVal = CESTRESS_PASS;
    HKEY hSubKey = NULL;    
    TCHAR szSubKey[MAX_PATH] = TEXT("");
    TCHAR szSubKeyFull[MAX_PATH] = TEXT("");
    DWORD lastErr, index, disp;
    
    LogVerbose(TEXT("creating random keys & values in \"%s\""), pszKeyName);

    for(index = 0; index < g_maxWidth; index++) {

        // randomly select between creating a key or a value        
        if(maxRecursion > 0 && FlipACoin(3)) {
            
            // create a new random registry key with a unique name
            StringCchPrintf(szSubKey, MAX_PATH, TEXT("KEY_%x_%x"), GetCurrentThreadId(), GetTickCount());

            // this key name would be too long to create
            if((_tcslen(szSubKey) + _tcslen(pszKeyName)) >= MAX_PATH) {
                continue;
            }
            
            StringCchPrintf(szSubKeyFull, MAX_PATH, TEXT("%s\\%s"), pszKeyName, szSubKey);
            lastErr = RegCreateKeyEx(hKey, szSubKey, 0, NULL, 0, 0, NULL, &hSubKey, &disp);
            SetLastError(lastErr);
            if(ERROR_SUCCESS != lastErr) {
                LogFail(TEXT("RegCreateKeyEx(\"%s\") failed; system error %u"), szSubKeyFull, lastErr);
                retVal = CESTRESS_FAIL;
                goto done;
            }
            retVal = RecursiveCreateKeysAndValues(hSubKey, szSubKeyFull, maxRecursion-1);
            if(CESTRESS_FAIL == retVal) {
                goto done;
            }

            retVal = RegCloseKey(hSubKey);
            SetLastError(retVal);
            if(ERROR_SUCCESS != lastErr) {
                LogFail(TEXT("RegCloseKey(\"%s\") failed; system error %u"), szSubKeyFull, lastErr);
                retVal = CESTRESS_FAIL;
                goto done;
            }
            hSubKey = NULL;

            // the delete key should block if someone currently has an open handle to it
            retVal = RegDeleteKey(hKey, szSubKey);
            SetLastError(retVal);
            if(ERROR_SUCCESS != lastErr) {
                LogFail(TEXT("RegDeleteKey(\"%s\") failed; system error %u"), szSubKeyFull, lastErr);
                retVal = CESTRESS_FAIL;
                goto done;
            }
            
        } else {
        
            // create a new random registry value with a unique name
            StringCchPrintf(szSubKey, MAX_PATH, TEXT("VAL_%x_%x"), GetCurrentThreadId(), GetTickCount());

            // this value name would be too long to create
            if((_tcslen(szSubKey) + _tcslen(pszKeyName)) >= MAX_PATH) {
                continue;
            }
            
            StringCchPrintf(szSubKeyFull, MAX_PATH, TEXT("%s\\%s"), pszKeyName, szSubKey);

            LogVerbose(TEXT("creating value \"%s\""), szSubKeyFull);
            lastErr = RegSetRandomValue(hKey, szSubKey);
            SetLastError(lastErr);
            if(ERROR_SUCCESS != lastErr) {
                LogFail(TEXT("RegSetValueEx(\"%s\") failed; system error %u"), szSubKeyFull, lastErr);
                retVal = CESTRESS_FAIL;
                goto done;
            }

            retVal = RegDeleteValue(hKey, szSubKey);
            SetLastError(retVal);
            if(ERROR_SUCCESS != lastErr) {
                LogFail(TEXT("RegDeleteValue(\"%s\") failed; system error %u"), szSubKeyFull, lastErr);
                retVal = CESTRESS_FAIL;
                goto done;
            }
        }
    }
    
done:
    return retVal;
}
    

/////////////////////////////////////////////////////////////////////////////////////
UINT Tst_EnumRegistryKeyBreadthFirst(
    IN HKEY hKey, 
    IN LPCTSTR szKeyName
    )
{
    LogComment(TEXT("enumerating \"%s\" breadth-first"), szKeyName);
    return RecursiveEnumRegistryKey(hKey, szKeyName, FALSE, g_maxRecursion);
}

/////////////////////////////////////////////////////////////////////////////////////
UINT Tst_EnumRegistryKeyDepthFirst(
    IN HKEY hKey, 
    IN LPCTSTR szKeyName
    )
{
    LogComment(TEXT("enumerating \"%s\" depth-first"), szKeyName);
    return RecursiveEnumRegistryKey(hKey, szKeyName, TRUE, g_maxRecursion);
}

/////////////////////////////////////////////////////////////////////////////////////
UINT Tst_CreateRandomRegistryKeys(
    HKEY hKey, 
    IN LPCTSTR szKeyName
    )
{
    LogComment(TEXT("creating random keys & values in \"%s\""), szKeyName);
    return RecursiveCreateKeysAndValues(hKey, szKeyName, g_maxRecursion);
}

/////////////////////////////////////////////////////////////////////////////////////
UINT Tst_FlushKey(
    HKEY hKey, 
    IN LPCTSTR szKeyName
    )
{
    LogComment(TEXT("flushing key \"%s\""), szKeyName);
    RegFlushKey(hKey); // don't check the return value because it might fail
    return CESTRESS_PASS;
}

/////////////////////////////////////////////////////////////////////////////////////
BOOL InitializeStressModule (
                            /*[in]*/ MODULE_PARAMS* pmp, 
                            /*[out]*/ UINT* pnThreads
                            )
{

    *pnThreads = g_cThreads;

    InitializeStressUtils (
                            _T("S2_REG"),                               // Module name to be used in logging
                            LOGZONE(SLOG_SPACE_FILESYS, SLOG_DEFAULT),    // Logging zones used by default
                            pmp                                         // Forward the Module params passed on the cmd line
                            );

    GetModuleFileName((HINSTANCE) g_hInst, g_szModuleName, 256);

    LogComment(_T("Module File Name: %s"), g_szModuleName);

    // save off the command line for this module
    if(pmp->tszUser)
    {
        LogComment(_T("command line: %s"), pmp->tszUser);
        g_pCmdLine = new CClParse(pmp->tszUser);
        if(NULL == g_pCmdLine)
        {
            LogFail(_T("new CClParse() failed; error %u"), GetLastError());
            return FALSE; 
        }
    }
    
    return TRUE;
}



/////////////////////////////////////////////////////////////////////////////////////
UINT DoStressIteration (
                        /*[in]*/ HANDLE hThread, 
                        /*[in]*/ DWORD dwThreadId, 
                        /*[in,out]*/ LPVOID pv /*unused*/)
{ 
    DWORD indexHKEY = Random() % ROOT_HKEY_LIST_COUNT;
    DWORD indexTest = Random() % TEST_FUNCTION_TABLE_COUNT;
    
    return (TEST_FUNCTION_TABLE[indexTest])(ROOT_HKEY_LIST[indexHKEY], ROOT_HKEY_NAME_LIST[indexHKEY]);

// You must return one of these values:
    //return CESTRESS_PASS
    //return CESTRESS_FAIL;
    //return CESTRESS_WARN1;
    //return CESTRESS_WARN2;
    //return CESTRESS_ABORT;  // Use carefully.  This will cause your module to be terminated immediately.
}



/////////////////////////////////////////////////////////////////////////////////////
DWORD TerminateStressModule (void)
{
    // no cleanup
    return ((DWORD) -1);
}



///////////////////////////////////////////////////////////
BOOL WINAPI DllMain(
                    HANDLE hInstance, 
                    ULONG dwReason, 
                    LPVOID lpReserved
                    )
{
    g_hInst = hInstance;
    return TRUE;
}
