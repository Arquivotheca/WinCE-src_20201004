//
// Portions Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
///////////////////////////////////////////////////////////////////////////////
//
// regtable.cpp - Copyright 1994-2001, Don Box (http://www.donbox.com)
//
// This file contains the implementation of a routine that adds/removes
// the table's registry entries and optionally registers a type library.
//
//     RegistryTableUpdateRegistry - adds/removes registry entries for table
//     

#ifndef _REGTABLE_CPP
#define _REGTABLE_CPP

#include <windows.h>
#include <olectl.h> 
#include "regtable.h"

LONG RegDeleteAllKeys(HKEY hkeyRoot, const TCHAR *pszKeyName)
{
    LONG err = ERROR_BADKEY;
    if (pszKeyName && lstrlen(pszKeyName))
    {
        HKEY    hkey;
        err = RegOpenKeyEx(hkeyRoot, pszKeyName, 0, 
                           KEY_ENUMERATE_SUB_KEYS | DELETE, &hkey );
        if(err == ERROR_SUCCESS)
        {
            while (err == ERROR_SUCCESS )
            {
                enum { MAX_KEY_LEN = 1024 };
                TCHAR szSubKey[MAX_KEY_LEN]; 
                DWORD   dwSubKeyLength = MAX_KEY_LEN;
                err = RegEnumKeyEx(hkey, 0, szSubKey,
                                    &dwSubKeyLength, 0, 0, 0, 0);
                
                if(err == ERROR_NO_MORE_ITEMS)
                {
                    //;
                    break;
                }
                else if (err == ERROR_SUCCESS)
                    err = RegDeleteAllKeys(hkey, szSubKey);
            }
            RegCloseKey(hkey);
            // Windows CE does not allow deleting an open key
            err = RegDeleteKey(hkeyRoot, pszKeyName);
            // Do not save return code because error
            // has already occurred
        }
    }
    return err;
}


// the routine that inserts/deletes Registry keys based on the table
EXTERN_C HRESULT STDAPICALLTYPE 
RegistryTableUpdateRegistry(HINSTANCE hInstance, 
                            REGISTRY_ENTRY *pEntries, 
                            BOOL bRegisterModuleAsTypeLib, 
                            BOOL bInstalling)
{
    HRESULT hr = S_OK;
    if (bInstalling)
    {
        TCHAR szModuleName[MAX_PATH];
        GetModuleFileName(hInstance, szModuleName, MAX_PATH);
        if (bRegisterModuleAsTypeLib)
        {
            ITypeLib *pTypeLib = 0;
            OLECHAR szTypeLibName[MAX_PATH];
#ifdef OLE2ANSI
            strcpy(szTypeLibName, szModuleName);
#else
#ifdef UNICODE
			wcscpy(szTypeLibName, szModuleName);	
#else
            mbstowcs(szTypeLibName, szModuleName, MAX_PATH);
#endif            
#endif
            hr = LoadTypeLib(szTypeLibName, &pTypeLib);
            if (SUCCEEDED(hr)) 
            {
                hr = RegisterTypeLib(pTypeLib, szTypeLibName, 0);
                pTypeLib->Release();
            }
        }
        if (FAILED(hr))
            hr = SELFREG_E_TYPELIB;
        else if (pEntries)
        {
            REGISTRY_ENTRY *pHead = pEntries;
            TCHAR szKey[MAX_PATH];
            DWORD cchKey = 0;
            HKEY hkeyRoot = 0;
        
            while (pEntries->fFlags != -1)
            {
                TCHAR szFullKey[MAX_PATH];
                const TCHAR *pszValue = (pEntries->pszValue != REG_MODULE_NAME) ? pEntries->pszValue : szModuleName;
                if (pEntries->hkeyRoot)
                {
                    hkeyRoot = pEntries->hkeyRoot;
                    _tcsncpy(szKey, pEntries->pszKey, MAX_PATH);
                    szKey[MAX_PATH-1] = '\0';
                    cchKey = _tcslen(szKey);
                    _tcscpy(szFullKey, szKey);
                }
                else
                {
                    _tcscpy(szFullKey, szKey);
                    szFullKey[cchKey] = TEXT('\\');
                    _tcsncpy(szFullKey+cchKey+1, pEntries->pszKey, MAX_PATH-cchKey-1);
                    szFullKey[MAX_PATH-1] = '\0';
                }
                if (hkeyRoot == 0)
                {
                    RegistryTableUpdateRegistry(hInstance, 
                                                pHead, 
                                                bRegisterModuleAsTypeLib, 
                                                FALSE);
                    return SELFREG_E_CLASS;

                }
                HKEY hkey; DWORD dw;
                if (pEntries->fFlags & (REGFLAG_DELETE_BEFORE_REGISTERING|REGFLAG_DELETE_WHEN_REGISTERING))
                {
                    LONG err = RegDeleteAllKeys(hkeyRoot, szFullKey);
                    #ifndef UNDER_CE
                    if (err != ERROR_SUCCESS && err != ERROR_FILE_NOT_FOUND) 
                    {
                        RegistryTableUpdateRegistry(hInstance, 
                                                    pHead, 
                                                    bRegisterModuleAsTypeLib, 
                                                    FALSE);
                        return SELFREG_E_CLASS;
                    }
                    #endif
                }
                if (!(pEntries->fFlags & REGFLAG_DELETE_WHEN_REGISTERING))
                {
                    LONG err = RegCreateKeyEx(hkeyRoot, szFullKey,
                                               0, 0, REG_OPTION_NON_VOLATILE,
                                               KEY_WRITE, 0, &hkey, &dw);
                    if (err == ERROR_SUCCESS)
                    {
                        if (pszValue)
                            err = RegSetValueEx(hkey, pEntries->pszValueName, 0, REG_SZ, (const BYTE *)pszValue, (lstrlen(pszValue) + 1)*sizeof(TCHAR));
                        RegCloseKey(hkey);
                        if (hkeyRoot == 0)
                        {
                            RegistryTableUpdateRegistry(hInstance, 
                                                        pHead, 
                                                        bRegisterModuleAsTypeLib, 
                                                        FALSE);
                            return SELFREG_E_CLASS;

                        }
                    }
                    else
                    {
                        RegistryTableUpdateRegistry(hInstance, 
                                                    pHead, 
                                                    bRegisterModuleAsTypeLib, 
                                                    FALSE);
                        return SELFREG_E_CLASS;

                    }
                }
                pEntries++;
            }
        }
    }
    else
    {
        if (bRegisterModuleAsTypeLib)
        {
            TCHAR szModuleName[MAX_PATH];
            GetModuleFileName(hInstance, szModuleName, MAX_PATH);
            ITypeLib *pTypeLib = 0;
            OLECHAR szTypeLibName[MAX_PATH];
#ifdef OLE2ANSI
            strcpy(szTypeLibName, szModuleName);
#else
#ifdef UNICODE
			wcscpy(szTypeLibName, szModuleName);
#else
            mbstowcs(szTypeLibName, szModuleName, MAX_PATH);
#endif            
#endif
            hr = LoadTypeLib(szTypeLibName, &pTypeLib);
            if (SUCCEEDED(hr)) 
            {
                TLIBATTR *ptla = 0;
                hr = pTypeLib->GetLibAttr(&ptla);
                if (SUCCEEDED(hr))
                {
#ifndef UNDER_CE	// unfortunately UnRegisterTypeLib does not seem to be defined on Windows CE
					
                    hr = UnRegisterTypeLib(ptla->guid, ptla->wMajorVerNum, ptla->wMinorVerNum, ptla->lcid, ptla->syskind);
#endif                    
                    pTypeLib->ReleaseTLibAttr(ptla);
                }
                pTypeLib->Release();
            }
        }
        if (FAILED(hr))
            hr = SELFREG_E_TYPELIB;
        else if (pEntries)
        {
            TCHAR szKey[MAX_PATH];
            DWORD cchKey = 0;
            HKEY hkeyRoot = 0;
        
            while (pEntries->fFlags != -1)
            {
                TCHAR szFullKey[MAX_PATH];
                if (pEntries->hkeyRoot)
                {
                    hkeyRoot = pEntries->hkeyRoot;
                    _tcsncpy(szKey, pEntries->pszKey, MAX_PATH);
                    szKey[MAX_PATH-1] = '\0';
                    cchKey = _tcslen(szKey);
                    _tcscpy(szFullKey, szKey );
                }
                else
                {
                    _tcscpy(szFullKey, szKey);
                    szFullKey[cchKey] =  TEXT('\\');
                    _tcsncpy(szFullKey+cchKey+1, pEntries->pszKey, MAX_PATH - cchKey - 1);
                    szFullKey[MAX_PATH-1] = '\0';
                }
                if ((hkeyRoot != 0) 
                    && !(pEntries->fFlags & REGFLAG_NEVER_DELETE)
                    && !(pEntries->fFlags & REGFLAG_DELETE_WHEN_REGISTERING))
                {
                    LONG err = RegDeleteAllKeys(hkeyRoot, szFullKey);
                    if (err != ERROR_SUCCESS && err != ERROR_FILE_NOT_FOUND)
                        hr = SELFREG_E_CLASS;
                    
                }
                pEntries++;
            }
        }
    }
    return hr;
}



#endif
