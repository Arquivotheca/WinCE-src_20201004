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
#include "storeincludes.hpp"
#include <intsafe.h>

LRESULT MountableDisk_t::BuildFilterList (FSDLOADLIST** ppFilterList)
{
    HKEY hKey = NULL;
    WCHAR RegistryKey[MAX_PATH];
    LRESULT lResult = ERROR_SUCCESS;

    RootRegKeyListItem* pItem;
    for (pItem = m_pRootRegKeyList; pItem != NULL; pItem = pItem->pNext) {

        /*

            Filter search:

            HKLM\System\StorageManager\AutoLoad\<FileSystem>\Filters
            
                    - or -
                    
            HKLM\System\StorageManager\Profiles\<Profile>\<FileSystem>\Filters

                    - and -
                    
            HKLM\System\StorageManager\<FileSystem>\Filters

        */
        
        if (FAILED (::StringCchPrintfW (RegistryKey, MAX_PATH, L"%s\\%s\\Filters", 
            pItem->RootRegKeyName, m_pRegSubKey))) {
            return ERROR_REGISTRY_IO_FAILED;
        }

        lResult = ::FsdRegOpenKey (RegistryKey, &hKey);
        if (ERROR_SUCCESS == lResult) {
            
            *ppFilterList = LoadFSDList (hKey, LOAD_FLAG_SYNC | LOAD_FLAG_ASYNC,
                RegistryKey, *ppFilterList, TRUE);

            ::FsdRegCloseKey (hKey);
            hKey = NULL;
        }

        /*

            Additional filter search:
            
            HKLM\System\StorageManager\AutoLoad\Filters

                    - or -

            HKLM\System\StorageManager\Profiles\<Profile>\Filters

                    - and -

            HKLM\System\StorageManager\Filters
            
        */

        if (FAILED (::StringCchPrintfW (RegistryKey, MAX_PATH, L"%s\\Filters", 
            pItem->RootRegKeyName))) {
            return ERROR_REGISTRY_IO_FAILED;
        }

        lResult = ::FsdRegOpenKey (RegistryKey, &hKey);
        if (ERROR_SUCCESS == lResult) {
            
            *ppFilterList = LoadFSDList (hKey, LOAD_FLAG_SYNC | LOAD_FLAG_ASYNC,
                RegistryKey, *ppFilterList, TRUE);

            ::FsdRegCloseKey (hKey);
            hKey = NULL;
        }
    }
    
    return ERROR_SUCCESS;
}

LogicalDisk_t::~LogicalDisk_t ()
{
    RootRegKeyListItem* pItem;
    RootRegKeyListItem* pNextItem = NULL;
    for (pItem = m_pRootRegKeyList; pItem != NULL; pItem = pNextItem) {
        pNextItem = pItem->pNext;
        delete[] reinterpret_cast<BYTE*> (pItem);
    }
    m_pRootRegKeyList = NULL;

    if (m_pRegSubKey) {
        delete[] m_pRegSubKey;
    }
}

LRESULT LogicalDisk_t::GetRegistryString (const WCHAR* ValueName, __out_ecount(ValueSize) WCHAR* pValue, DWORD ValueSize)
{
    LRESULT lResult = ERROR_FILE_NOT_FOUND;
    HKEY hKey = NULL;
    WCHAR RegistryKey[MAX_PATH];

    RootRegKeyListItem* pItem;
    for (pItem = m_pRootRegKeyList; pItem != NULL; pItem = pItem->pNext) {
        
        if (FAILED (::StringCchPrintfW (RegistryKey, MAX_PATH, L"%s\\%s", 
            pItem->RootRegKeyName, m_pRegSubKey))) {
            return ERROR_REGISTRY_IO_FAILED;
        }

        DEBUGMSG (ZONE_HELPER, (L"FSDMGR!LogicalDisk_t::GetRegistryString LogicalDisk_t=%p Trying key %s\r\n", this, RegistryKey));

        lResult = ::FsdRegOpenKey (RegistryKey, &hKey);
        if (ERROR_SUCCESS == lResult) {

            if (::FsdGetRegistryString (hKey, ValueName, pValue, ValueSize)) {
                
                lResult = ERROR_SUCCESS;
                break;
                
            } else {
            
                lResult = FsdGetLastError (ERROR_FILE_NOT_FOUND);
            }
            ::FsdRegCloseKey (hKey);
            hKey = NULL;
        }
    }    

    if (hKey) {
        ::FsdRegCloseKey (hKey);
        hKey = NULL;
    }

    if (ERROR_SUCCESS != lResult) {
        // Return empty string on failure.
        StringCchCopy (pValue, ValueSize, L"");
    }

    return lResult;
}

LRESULT LogicalDisk_t::GetRegistryValue (const WCHAR* ValueName, DWORD* pValue)
{
    LRESULT lResult = ERROR_FILE_NOT_FOUND;
    HKEY hKey = NULL;
    WCHAR RegistryKey[MAX_PATH];

    // Enumerate all registry keys.
    RootRegKeyListItem* pItem;
    for (pItem = m_pRootRegKeyList; pItem != NULL; pItem = pItem->pNext) {
        
        if (FAILED (::StringCchPrintfW (RegistryKey, MAX_PATH, L"%s\\%s", 
            pItem->RootRegKeyName, m_pRegSubKey))) {
            return ERROR_REGISTRY_IO_FAILED;
        }

        DEBUGMSG (ZONE_HELPER, (L"FSDMGR!LogicalDisk_t::GetRegistryValue LogicalDisk_t=%p Trying key %s\r\n", this, RegistryKey));

        lResult = ::FsdRegOpenKey (RegistryKey, &hKey);
        if (ERROR_SUCCESS == lResult) {

            if (::FsdGetRegistryValue (hKey, ValueName, pValue)) {
                
                lResult = ERROR_SUCCESS;
                break;
                
            } else {
            
                lResult = FsdGetLastError (ERROR_FILE_NOT_FOUND);
            }
            ::FsdRegCloseKey (hKey);
            hKey = NULL;
        }
    }    

    if (hKey) {
        ::FsdRegCloseKey (hKey);
        hKey = NULL;
    }

    if (ERROR_SUCCESS != lResult) {
        // Return zero on failure.
        *pValue = 0;
    }

    return lResult;
}

LRESULT LogicalDisk_t::GetRegistryFlag (const WCHAR* pValueName, DWORD* pFlag, DWORD SetBit)
{
    DWORD Value;
    LRESULT lResult = GetRegistryValue (pValueName, &Value);
    if (ERROR_SUCCESS == lResult) {
        if (Value) {
            *pFlag |= SetBit;
        } else {
            *pFlag &= ~SetBit;
        }
    }
    return lResult;
}

LRESULT LogicalDisk_t::AddRootRegKey (const WCHAR* RootKeyName)
{
    size_t SourceNameChars;
    if (FAILED (::StringCchLengthW (RootKeyName, STRSAFE_MAX_CCH, &SourceNameChars))) {
        return ERROR_INVALID_PARAMETER;
    }

    // Allocate a new list item of sufficient size to contain the entire 
    // root key string. Since RootKeyListItem ends with a single item WCHAR
    // array, there is no need to add an additional WCHAR here for the NULL
    // terminator.

    DWORD ItemBytes = 0;
    // ItemBytes = sizeof (WCHAR) * SourceNameChars + sizeof (RootRegKeyListItem);
    if (FAILED (::DWordMult (sizeof(WCHAR), SourceNameChars, &ItemBytes))
        || FAILED (::DWordAdd (sizeof(RootRegKeyListItem), ItemBytes, &ItemBytes))) {
        return ERROR_ARITHMETIC_OVERFLOW;
    }
    
    RootRegKeyListItem* pItem = (RootRegKeyListItem*)new BYTE[ItemBytes];
    if (!pItem) {
        return ERROR_NOT_ENOUGH_MEMORY;
    }
    
    // Add the new item at the head of the list.
    if (FAILED (::StringCchCopyNW (pItem->RootRegKeyName, SourceNameChars + 1,
        RootKeyName, SourceNameChars))) {
        delete[] pItem;
        return ERROR_INVALID_PARAMETER;
    }

    pItem->pNext = m_pRootRegKeyList;
    m_pRootRegKeyList = pItem;
    
    return ERROR_SUCCESS;
}

