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
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <tchar.h>

#ifdef UNDER_CE
#include <bldver.h> //for CE_MAJOR_VER
#else
#include "assert.h"
#endif


#include <tchar.h>
#include "thelper.h"

#define BOOTVARS_KEY            _T("Init\\BootVars")
#define SYSTEMHIVE_VALUE        _T("SYSTEMHIVE")

HKEY    g_Reg_Roots[] = {   HKEY_LOCAL_MACHINE,
                            HKEY_USERS,
                            HKEY_CLASSES_ROOT };

DWORD   g_RegValueTypes[] ={REG_NONE,
                            REG_SZ,
                            REG_EXPAND_SZ,
                            REG_BINARY,
                            REG_DWORD,
                            REG_DWORD_LITTLE_ENDIAN,
                            REG_DWORD_BIG_ENDIAN,
                            REG_MULTI_SZ };



//+ ========================================================================
//  returns false if the value is not present in the given key
//
//  returns false if either the key does not exist or
//  if there was any other problem with RegQueryValueEx
//
//- ========================================================================
BOOL Hlp_IsValuePresent(HKEY hKey, LPCTSTR pszValName)
{
    BOOL    fRetVal=FALSE;
    DWORD   dwTypeGet=0;
    LONG    lRet=0;

    ASSERT(pszValName);

    if (ERROR_SUCCESS != (lRet=RegQueryValueEx(hKey, pszValName, 0, &dwTypeGet, NULL, NULL)))
    {
        if (ERROR_FILE_NOT_FOUND == lRet)
            return FALSE;
        else
            REG_FAIL(RegQueryValueEx, lRet);
    }

    fRetVal = TRUE;
ErrorReturn:
    return fRetVal;

}


//+ ======================================================================
//  returns true of the key is present,
//  false otherwise.
//- ======================================================================
BOOL Hlp_IsKeyPresent(HKEY hRoot, const LPCTSTR pszSubKey)
{
    HKEY    hKey=0;

    if (ERROR_SUCCESS == RegOpenKeyEx(hRoot, pszSubKey, 0, 0, &hKey))
    {
        REG_CLOSE_KEY(hKey);
        return TRUE;
    }
    else
        return FALSE;
}



//+ ======================================================================
//- ======================================================================
TCHAR* Hlp_HKeyToText(HKEY hKey, __out_ecount(cbBuffer) TCHAR *pszBuffer, DWORD cbBuffer)
{
    if (NULL == pszBuffer)
        goto ErrorReturn;

    StringCchCopy(pszBuffer, cbBuffer, _T(""));


    switch((DWORD)hKey)
    {
        case HKEY_LOCAL_MACHINE :
            StringCchCopy(pszBuffer, cbBuffer, _T("HKEY_LOCAL_MACHINE"));
        break;
        case HKEY_USERS :
            StringCchCopy(pszBuffer, cbBuffer, _T("HKEY_USERS"));
        break;
        case HKEY_CURRENT_USER :
            StringCchCopy(pszBuffer, cbBuffer, _T("HKEY_CURRENT_USER"));
        break;
        case HKEY_CLASSES_ROOT :
            StringCchCopy(pszBuffer, cbBuffer, _T("HKEY_CLASSES_ROOT"));
        break;
        default :
            TRACE(_T("WARNING : WinCE does not support this HKey 0x%x\n"), hKey);
    }

ErrorReturn:
    return pszBuffer;
}


//+ ======================================================================
//- ======================================================================
TCHAR* Hlp_HKeyToTLA(HKEY hKey, __out_ecount(cbBuffer) TCHAR *pszBuffer, DWORD cbBuffer)
{
    if (NULL == pszBuffer)
        goto ErrorReturn;

    StringCchCopy(pszBuffer, cbBuffer, _T(""));

    switch((DWORD)hKey)
    {
        case HKEY_LOCAL_MACHINE :
            StringCchCopy(pszBuffer, cbBuffer, _T("HKLM"));
        break;
        case HKEY_USERS :
            StringCchCopy(pszBuffer, cbBuffer, _T("HKU"));
        break;
        case HKEY_CURRENT_USER :
            StringCchCopy(pszBuffer, cbBuffer, _T("HKCU"));
        break;
        case HKEY_CLASSES_ROOT :
            StringCchCopy(pszBuffer, cbBuffer, _T("HKCR"));
        break;
        default :
        {
            TRACE(_T("WARNING : WinCE does not support this HKey 0x%x\n"), hKey);
            ASSERT(0);
        }
    }

ErrorReturn :
    return pszBuffer;
}



//+ ======================================================================
//- ======================================================================
TCHAR* Hlp_KeyType_To_Text(DWORD dwType, __out_ecount(cbBuffer) TCHAR *pszBuffer, DWORD cbBuffer)
{
    if (NULL == pszBuffer)
        goto ErrorReturn;

    switch(dwType)
    {
        case REG_NONE :
            StringCchCopy(pszBuffer, cbBuffer, _T("REG_NONE"));
        break;
        case REG_SZ :
            StringCchCopy(pszBuffer, cbBuffer, _T("REG_SZ"));
        break;
        case REG_EXPAND_SZ :
            StringCchCopy(pszBuffer, cbBuffer, _T("REG_EXPAND_SZ"));
        break;
        case REG_BINARY :
            StringCchCopy(pszBuffer, cbBuffer, _T("REG_BINARY"));
        break;
        case REG_DWORD :
            StringCchCopy(pszBuffer, cbBuffer, _T("REG_DWORD"));
        break;
        case REG_DWORD_BIG_ENDIAN :
            StringCchCopy(pszBuffer, cbBuffer, _T("REG_DWORD_BIG_ENDIAN"));
        break;
        case REG_LINK :
            StringCchCopy(pszBuffer, cbBuffer, _T("REG_LINK"));
        break;
        case REG_MULTI_SZ :
            StringCchCopy(pszBuffer, cbBuffer, _T("REG_MULTI_SZ"));
        break;
        case REG_RESOURCE_LIST :
            StringCchCopy(pszBuffer, cbBuffer, _T("REG_RESOURCE_LIST"));
        break;
        case REG_FULL_RESOURCE_DESCRIPTOR  :
            StringCchCopy(pszBuffer, cbBuffer, _T("REG_FULL_RESOURCE_DESCRIPTOR "));
        break;
        case REG_RESOURCE_REQUIREMENTS_LIST :
            StringCchCopy(pszBuffer, cbBuffer, _T("REG_RESOURCE_REQUIREMENTS_LIST"));
        break;
        default :
            TRACE(_T(">>>>> WARNING : WinCE does not support this Val Type0x%x\n"), dwType);
    }

ErrorReturn :
    return pszBuffer;
}


//+ ===============================================================
//  Creates a value
//  Closes it.
//- ===============================================================
BOOL  Hlp_Write_Value(HKEY hRoot, LPCTSTR pszSubKey, LPCTSTR pszValue,
                            DWORD dwType, const BYTE* pbData, DWORD cbData)
{
    BOOL    fRetVal=FALSE;
    DWORD   dwErr=0;
    HKEY    hKey=0;
    DWORD   dwDispo=0;

    dwErr = RegOpenKeyEx(hRoot, pszSubKey, 0, 0, &hKey);
    if (ERROR_FILE_NOT_FOUND ==dwErr)
    {
        dwErr = RegCreateKeyEx(hRoot, pszSubKey, 0, NULL, 0, 0, NULL, &hKey, &dwDispo);
        if (dwErr)
            REG_FAIL(RegCreateKeyEx, dwErr);
    }
    else if (ERROR_SUCCESS != dwErr)
        REG_FAIL(RegOpenKeyEx, dwErr);


    dwErr = RegSetValueEx(hKey, pszValue, 0, dwType, pbData, cbData);
    if (dwErr)
    {
       if(dwErr == ERROR_DISK_FULL)
            goto ErrorReturn;
        else
            REG_FAIL(RegSetValueEx, dwErr);
    }

    fRetVal=TRUE;
ErrorReturn:
    REG_CLOSE_KEY(hKey);
    return fRetVal;
}



//+ ======================================================================
//  Function    :   Hlp_Write_Random_Value
//
//  Purpose     :   Helper function to populate the registry with test data.
//
//  Params  :   hKey        - [IN] Must be valid
//              pszSubKey   - [IN] Can be NULL. If it's null then the
//                              value is generated under hKey. If
//                              a valid pszSubKey is passed, then the value
//                              is created in that subkey of hKey.
//              pszValName  - [IN] Must be valid
//              dwType      - [in/out] if dwType=0, then a random type is selected.
//              pbData      - [IN]  Must be valid
//              cbData      - [in/out] : must be valid.
//- ======================================================================
BOOL Hlp_Write_Random_Value(HKEY hKey, LPCTSTR pszSubKey, LPCTSTR pszValName, DWORD *pdwType, __out_ecount(*pcbData) PBYTE pbData, DWORD *pcbData)
{
    BOOL    fRetVal=FALSE;
    LONG    lRet=0;
    HKEY    hKeyChild=0;
    DWORD   dwDispo=0;
    size_t szlen = 0;
    StringCchLength(pszValName, STRSAFE_MAX_CCH, &szlen);

    if ((NULL == pszValName) ||
        (NULL == pbData) ||
        (0 == szlen) ||
        (0 == *pcbData) ||
        (NULL == pdwType))
    {
        TRACE(TEXT("Test Error : Invalid arguments passed to Hlp_Write_Random_Value. Returning\r\n"));
        goto ErrorReturn;
    }

    //  Generate some value data depending on the dwtype
    if (!Hlp_GenRandomValData(pdwType, pbData, pcbData))
        goto ErrorReturn;

    //  If a subkey is specified, the open/Create the subkey
    //  and write the value under there.
     StringCchLength(pszSubKey, STRSAFE_MAX_CCH, &szlen);
    if ((pszSubKey) && szlen)
    {
        lRet = RegOpenKeyEx(hKey, pszSubKey, 0, 0, &hKeyChild);
        if (ERROR_FILE_NOT_FOUND ==lRet)
        {
            lRet = RegCreateKeyEx(hKey, pszSubKey, 0, NULL, 0, 0, NULL, &hKeyChild, &dwDispo);
            if (lRet)
                REG_FAIL(RegCreateKeyEx, lRet);
        }
        else if (ERROR_SUCCESS != lRet)
            REG_FAIL(RegOpenKeyEx, lRet);

        //  Write the value.
        lRet = RegSetValueEx(hKeyChild, pszValName, 0, *pdwType, pbData, *pcbData);
        if (lRet)
        {
            if(lRet == ERROR_DISK_FULL)
                goto ErrorReturn;
            else
                REG_FAIL(RegSetValueEx, lRet);
        }
    }
    else
    {
        lRet = RegSetValueEx(hKey, pszValName, 0, *pdwType, pbData, *pcbData);
        if (lRet)
        {
           if(lRet == ERROR_DISK_FULL)
                goto ErrorReturn;
            else
                REG_FAIL(RegSetValueEx, lRet);
        }
    }

    fRetVal=TRUE;
ErrorReturn :
    REG_CLOSE_KEY(hKeyChild);
    return fRetVal;
}



//+ ===============================================================
//  pbData cannot be NULL.
//
//- ===============================================================
LONG  Hlp_Read_Value(HKEY hRoot, LPCTSTR pszSubKey, LPCTSTR pszValue,
                            DWORD *pdwType, __out_ecount(*pcbData) PBYTE pbData, DWORD *pcbData)
{
    LONG    lRetVal=ERROR_INVALID_PARAMETER;
    HKEY    hKey=0;
    LONG    lRet=0;

    //  Check Params
    if ((NULL == pbData) ||
        (0==*pcbData) ||
        (0==hRoot) ||
        (!pszValue))
    {
        TRACE(TEXT("Thelper : pbData = 0x%x, cbData=%d, pszValue=%s\r\n"), pbData, *pcbData, pszValue);
        TRACE(TEXT("Thelper : Invalid parameter sent to Hlp_ReadValue. %s %u \r\n"), _T(__FILE__), __LINE__);
        goto ErrorReturn;
    }

    lRet = RegOpenKeyEx(hRoot, pszSubKey, 0, 0, &hKey);
    if (ERROR_SUCCESS != lRet)
        REG_FAIL(RegOpenKeyEx, lRet);

    lRet = RegQueryValueEx(hKey, pszValue, NULL, pdwType, pbData, pcbData);
    if (lRet != ERROR_SUCCESS)
    {
        lRetVal=lRet;
        goto ErrorReturn;
    }


    lRetVal=ERROR_SUCCESS;
ErrorReturn :
    REG_CLOSE_KEY(hKey);
    return lRetVal;
}


//+ =========================================================================
//  Generate random registry data to fill in there.
//
//  Params :
//
//  pdwType  :   specify the type of data you want generated.
//              if type is not specified, then a random type is selected.
//  pbData  :   Buffer to contain the data. pbData cannot be NULL.
//  cbData  :   Buffer size that controls the amount of data to generate.
//
//  The function by default will resize the buffer if needed.
//  If dwtype is not specified, then a randomly selected type will dictate
//  the size of the data generated.
//  eg. if you pass in a 100 byte buffer and specify DWORD, then just 4 bytes
//  of random data will be generated.
//
//- =========================================================================
BOOL Hlp_GenRandomValData(DWORD *pdwType, __out_ecount(*pcbData) PBYTE pbData, DWORD *pcbData)
{
    BOOL    fRetVal=FALSE;
    DWORD   dwChars=0;
    DWORD   i=0;
    size_t szlen = 0;
    ASSERT(pbData);
    ASSERT(*pcbData);

    if (!pbData)
    {
        TRACE(TEXT("ERROR : Null buffer specified to Hlp_GetValueData. %s %u \n"), _T(__FILE__), __LINE__);
        goto ErrorReturn;
    }

    UINT uNumber = 0;
    //  if no type is specified, then choose a random type
    if (0==*pdwType)
    {
        rand_s(&uNumber);
        *pdwType = g_RegValueTypes[uNumber%TST_NUM_REG_TYPES]; //  set a random data type
    }

    switch(*pdwType)
    {
        case REG_NONE :
        case REG_BINARY :
        case REG_LINK :
        case REG_RESOURCE_LIST :
        case REG_RESOURCE_REQUIREMENTS_LIST :
        case REG_FULL_RESOURCE_DESCRIPTOR :
            if (!Hlp_FillBuffer(pbData, *pcbData, HLP_FILL_RANDOM))
                goto ErrorReturn;
        break;

        case REG_SZ :
            dwChars = *pcbData/sizeof(TCHAR);
            ASSERT(dwChars);
            if(!Hlp_GenStringData((LPTSTR)pbData, dwChars, TST_FLAG_ALPHA_NUM))
                goto ErrorReturn;
        break;

        case REG_EXPAND_SZ :
            StringCchLength(_T("%SystemRoot%"), STRSAFE_MAX_CCH, &szlen);
            ASSERT(*pcbData >=  (szlen+1) * sizeof(TCHAR) );

            if (*pcbData <  (szlen+1) * sizeof(TCHAR) )
                goto ErrorReturn;

            StringCchCopy((LPTSTR)pbData, (*pcbData) / sizeof(TCHAR), _T("%SystemRoot%"));
            *pcbData= (szlen+1)*sizeof(TCHAR);
            break;

        case REG_DWORD_BIG_ENDIAN :
        case REG_DWORD :
//        case REG_DWORD_LITTLE_ENDIAN :
            ASSERT(*pcbData >= sizeof(DWORD));
            if (*pcbData < sizeof(DWORD))
            {
                TRACE(TEXT("Insufficient mem passed to Hlp_GenValueData. Send %d bytes, required %d. %s %u\r\n"),
                            *pcbData, sizeof(DWORD), _T(__FILE__), __LINE__);
                goto ErrorReturn;
            }
            rand_s(&uNumber);
            *(DWORD*)pbData = uNumber;
            *pcbData = sizeof(DWORD);

        break;

        //  This generates 3 strings in 1.
        case REG_MULTI_SZ :
            dwChars = *pcbData/sizeof(TCHAR);
            memset(pbData, 33, *pcbData);

            ASSERT(dwChars > 6);

            //  First Generate a string
            if(!Hlp_GenStringData((LPTSTR)pbData, dwChars, TST_FLAG_ALPHA_NUM))
                goto ErrorReturn;

            //  Now throw in some random terminating NULLs to make it a multi_sz
            for (i=dwChars/3; i<dwChars; i+=dwChars/3)
            {
                ASSERT(i<dwChars);
                *((LPTSTR)pbData+i) = _T('\0');
            }

            //  and make sure the last 2 chars are also NULL terminators.
            *((LPTSTR)pbData+dwChars-1)= _T('\0');
            *((LPTSTR)pbData+dwChars-2)= _T('\0');
        break;

        default :
            TRACE(TEXT("TestError : Unknown reg type sent to Hlp_GenValueData : %d. %s %u\n"), *pdwType, _T(__FILE__), __LINE__);
            goto ErrorReturn;
    }

    fRetVal=TRUE;
ErrorReturn :
    return fRetVal;
}


//+ ===================================================================
//
//  This is currently hard coded based on the value
//              set for HKLM\init\bootvars\StartDevMgr
//
//              In the future this should make use of the system API
//
//- ===================================================================
BOOL L_IsHDDHive(void)
{
    BOOL    fRet=FALSE;
    BYTE    pbData[MAX_PATH] = {0};
    DWORD   cbData=MAX_PATH;
    LONG    lRet=0;

    lRet = Hlp_Read_Value(HKEY_LOCAL_MACHINE, _T("Init\\BootVars") , _T("Start DevMgr"),
                            NULL, pbData, &cbData);
    if (lRet != ERROR_SUCCESS)
    {
        TRACE(TEXT("Could not read HKLM\\Init\\BootVars\\Start DevMgr 0x%x %s %u \r \n"), lRet, _T(__FILE__), __LINE__);
        goto ErrorReturn;
    }

    if (1 == (DWORD)*pbData)
        return TRUE;
    else
        return FALSE;


ErrorReturn :
        return fRet;
}


//+ =========================================================================
//- =========================================================================
BOOL Hlp_IsRegAlive(void)
{
    LONG    lRet=0;
    HKEY    hKey=0;

    lRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("init"), 0, 0, &hKey);
    if (ERROR_SUCCESS == lRet)
    {
        RegCloseKey(hKey);
        return TRUE;
    }
    return FALSE;
}

//+ ========================================================================
//
//  function    :   Hlp_GetNumSubKeys
//
//  Description :   Given a Key, finds out the num of subkeys.
//
//- ========================================================================
BOOL Hlp_GetNumSubKeys(HKEY hKey, DWORD* pdwNumKeys)
{
    DWORD   cbMaxValueData=0;
    DWORD   dwErr=0;
    BOOL    fRetVal=FALSE;

    dwErr = RegQueryInfoKey(hKey,
                            NULL,
                            NULL,
                            NULL,
                            pdwNumKeys,   //&dwNumSubKeys,
                            NULL,   //&cbMaxKeyName,
                            NULL,
                            NULL,   //&dwNumValues,
                            NULL,   //&cbMaxValueName,
                            &cbMaxValueData,
                            NULL,
                            NULL);
    if (ERROR_SUCCESS != dwErr)
        REG_FAIL(RegQueryInfoKey, dwErr);

    fRetVal=TRUE;

ErrorReturn :
    return fRetVal;
}


//+ ========================================================================
//
//  function    :   Hlp_GetNumValues
//
//  Description :   Given a Key, finds out the num of values.
//
//- ========================================================================
BOOL Hlp_GetNumValues(HKEY hKey, DWORD* pdwNumVals)
{
    DWORD   cbMaxValueData=0;
    DWORD   dwErr=0;
    BOOL    fRetVal=FALSE;

    dwErr = RegQueryInfoKey(hKey,
                            NULL,
                            NULL,
                            NULL,
                            NULL,   //&dwNumSubKeys,
                            NULL,   //&cbMaxKeyName,
                            NULL,
                            pdwNumVals,   //&dwNumValues,
                            NULL,   //&cbMaxValueName,
                            &cbMaxValueData,
                            NULL,
                            NULL);
    if (ERROR_SUCCESS != dwErr)
        REG_FAIL(RegQueryInfoKey, dwErr);

    fRetVal=TRUE;

ErrorReturn :
    return fRetVal;
}


//+ ========================================================================
//
//  Given a Key, returns the max Value Data present in that key.
//  on failure, returns -1;
//
//- ========================================================================
DWORD Hlp_GetMaxValueDataLen(HKEY hKey)
{
    DWORD   cbMaxValueData=0;
    DWORD   dwErr=0;

    dwErr = RegQueryInfoKey(hKey,
                            NULL,
                            NULL,
                            NULL,
                            NULL,   //&dwNumSubKeys,
                            NULL,   //&cbMaxKeyName,
                            NULL,
                            NULL,   //&dwNumValues,
                            NULL,   //&cbMaxValueName,
                            &cbMaxValueData,
                            NULL,
                            NULL);
    if (ERROR_SUCCESS != dwErr)
        REG_FAIL(RegQueryInfoKey, dwErr);

    return cbMaxValueData;
ErrorReturn :
    return (DWORD)-1;
}

//+ ========================================================================
//
//  Given a Key, returns the max Value Data present in that key.
//  on failure, returns -1;
//
//- ========================================================================
DWORD Hlp_GetMaxValueNameChars(HKEY hKey)
{
    DWORD   cbMaxValueName=0;
    DWORD   dwErr=0;

    dwErr = RegQueryInfoKey(hKey,
                            NULL,
                            NULL,
                            NULL,
                            NULL,               // &dwNumSubKeys,
                            NULL,               // &cbMaxKeyName,
                            NULL,               // &cbMaxClassLen
                            NULL,               // &dwNumValues,
                            &cbMaxValueName,    // &ccMaxValueName,
                            NULL,               // &MaxValueDataLen
                            NULL,
                            NULL);
    if (ERROR_SUCCESS != dwErr)
        REG_FAIL(RegQueryInfoKey, dwErr);

    return cbMaxValueName;
ErrorReturn :
    return (DWORD)-1;
}

//+ ===================================================================
//
//  This is currently hard coded based on the value
//              set for HKLM\init\bootvars\StartDevMgr
//
//              In the future this should make use of the system API
//
//  Params  :
//  [in/out] pszFileName    : the hive filename
//  [in]     cbBuffer       :   size of the buffer pointed by pszFileName
//
//+ ===================================================================
BOOL Hlp_GetSystemHiveFileName(__out_ecount(cbBuffer) LPTSTR pszFileName, DWORD cbBuffer)
{
    BOOL    fRetVal=FALSE;
    LONG    lRet=0;
    BYTE    pbData[MAX_PATH] = {0};
    DWORD   cbData=MAX_PATH;
    size_t szlen = 0;

    if (!pszFileName || !cbBuffer)
        return FALSE;

    StringCchCopy(pszFileName, cbBuffer, _T(""));

    if (L_IsHDDHive())
    {
        StringCchLength(_T("Hard Disk"), STRSAFE_MAX_CCH, &szlen);
        ASSERT(cbBuffer >= (szlen*sizeof(TCHAR)));
        StringCchCopy(pszFileName, cbBuffer, _T("Hard Disk"));
    }

    cbData = MAX_PATH;
    lRet = Hlp_Read_Value(HKEY_LOCAL_MACHINE, _T("Init\\BootVars") , _T("SYSTEMHIVE"),
                            NULL, pbData, &cbData);
    if (lRet != ERROR_SUCCESS)
    {
        TRACE(TEXT("Could not read HKLM\\Init\\BootVars\\SYSTEMHIVE 0x%x %s %u \r \n"), lRet, _T(__FILE__), __LINE__);
        goto ErrorReturn;
    }

    StringCchLength((LPTSTR)pbData, STRSAFE_MAX_CCH, &szlen);
    ASSERT(cbBuffer >= (szlen*sizeof(TCHAR)));
    StringCchCat (pszFileName, cbBuffer, (LPTSTR)pbData);


    fRetVal = TRUE;
ErrorReturn :
    return fRetVal;

}

#ifdef UNDER_CE

//+ ===================================================================
//
//  This is currently hard coded based on the value
//              set for HKLM\init\bootvars\StartDevMgr
//
//              In the future this should make use of the system API
//
//  Params  :
//  [in/out] pszFileName    : the hive filename
//  [in]     cbBuffer       :   size of the buffer pointed by pszFileName
//
//+ ===================================================================
BOOL Hlp_GetUserHiveFileName(__out_ecount(cbFileName) LPTSTR pszFileName, DWORD cbFileName)
{
    BOOL    fRetVal=FALSE;
    LONG    lRet=0;
    BYTE    pbData[MAX_PATH] = {0};
    DWORD   cbData=MAX_PATH ;
    size_t szlen = 0;
    if (!pszFileName || !cbFileName)
        return FALSE;

   if (L_IsHDDHive())
        StringCchCopy(pszFileName, cbFileName, _T("Hard Disk"));
    else
        StringCchCopy(pszFileName, cbFileName, _T(""));

    cbData = MAX_PATH;
    lRet = Hlp_Read_Value(HKEY_LOCAL_MACHINE, _T("Init\\BootVars") , _T("PROFILEDIR"),
                            NULL, pbData, &cbData);
    if (lRet != ERROR_SUCCESS)
    {
        TRACE(TEXT("Could not read HKLM\\Init\\BootVars\\PROFILEDIR 0x%x %s %u \r \n"), lRet, _T(__FILE__), __LINE__);
        goto ErrorReturn;
    }

    const _TCHAR * sBackWhack = _T("\\");
    StringCchCat(pszFileName, cbFileName, (LPTSTR)pbData);
    StringCchLength(pszFileName, STRSAFE_MAX_CCH, &szlen);
    StringCchCatN(pszFileName, cbFileName, sBackWhack, cbFileName - szlen);

    //  Get the current logged on user name.
    cbData = MAX_PATH/sizeof(TCHAR);
    if (!GetUserNameEx(NameWindowsCeLocal, (LPTSTR)pbData, &cbData))
        GENERIC_FAIL(GetCurrentUser);

    StringCchCat(pszFileName, cbFileName, (LPTSTR)pbData);
    StringCchCat(pszFileName, cbFileName, _T("\\User.hv"));


    fRetVal = TRUE;
ErrorReturn :
    return fRetVal;
}


#endif

//+ ===================================================================
//- ===================================================================
BOOL Hlp_DeleteValue(HKEY hRootKey, LPCTSTR pszSubKey, LPCTSTR pszValue)
{
    BOOL    fRetVal=FALSE;
    HKEY    hKey=0;
    LONG    lRet=0;

    lRet = RegOpenKeyEx(hRootKey, pszSubKey, 0, 0, &hKey);
    if (ERROR_SUCCESS != lRet)
        REG_FAIL(RegOpenKeyEx, lRet);

    lRet = RegDeleteValue(hKey, pszValue);
    if (ERROR_SUCCESS != lRet)
        REG_FAIL(RegDeleteValue, lRet);


    fRetVal = TRUE;
ErrorReturn :
    REG_CLOSE_KEY(hKey);
    return fRetVal;
 }


//+ ======================================================================
//
//  Function    :   Hlp_DumpSubKeys
//
//- ======================================================================
BOOL Hlp_DumpSubKeys(HKEY hKey)
{
    DWORD   dwIndex=0;
    TCHAR   rgszKeyName[MAX_PATH]={0};
    DWORD   cbKeyName=MAX_PATH;
    TCHAR   rgszKeyClass[MAX_PATH]={0};
    DWORD   cbKeyClass=MAX_PATH;

    while (ERROR_SUCCESS == RegEnumKeyEx(hKey, dwIndex++, rgszKeyName, &cbKeyName, NULL, rgszKeyClass, &cbKeyClass, NULL))
    {
        cbKeyName=MAX_PATH;
        TRACE(TEXT("Subkey = %s\r\n"), rgszKeyName);
    }

    return TRUE;
}


//+ ======================================================================
//- ======================================================================
BOOL Hlp_CmpRegKeys(HKEY hKey1, HKEY hKey2)
{
    BOOL    fRetVal=TRUE;
    DWORD   dwKey1=0;
    DWORD   dwKey2=0;

    //  ============================
    //  compare the number of subkeys
    //  ============================
    if (!Hlp_GetNumSubKeys(hKey1, &dwKey1))
    {
        TRACE(TEXT("Failed to query the num subkeys for Key1\r\n"));
        DUMP_LOCN;
        return FALSE;
    }

    if (!Hlp_GetNumSubKeys(hKey2, &dwKey2))
    {
        TRACE(TEXT("Failed to query the num subkeys for Key2\r\n"));
        DUMP_LOCN;
        return FALSE;
    }
    if (dwKey1 != dwKey2)
    {
        TRACE(TEXT("The num of subkeys do not match. Key1=%d, Key2=%d\r\n"), dwKey1, dwKey2);
        fRetVal=FALSE;
    }


    //  ============================
    //  compare the number of values
    //  ============================
    if (!Hlp_GetNumValues(hKey1, &dwKey1))
    {
        TRACE(TEXT("Failed to query the num subkeys for Key1\r\n"));
        DUMP_LOCN;
        return FALSE;
    }

    if (!Hlp_GetNumValues(hKey2, &dwKey2))
    {
        TRACE(TEXT("Failed to query the num subkeys for Key2\r\n"));
        DUMP_LOCN;
        return FALSE;
    }
    if (dwKey1 != dwKey2)
    {
        TRACE(TEXT("The num of values do not match. Key1=%d, Key2=%d\r\n"), dwKey1, dwKey2);
        fRetVal=FALSE;
    }


    //  ============================
    //  cmp the max value data.
    //  ============================
    dwKey1 = Hlp_GetMaxValueDataLen(hKey1);
    dwKey2 = Hlp_GetMaxValueDataLen(hKey2);
    if (dwKey1 != dwKey2)
    {
        TRACE(TEXT("The max Value DataLen does not match. Key1=%d, Key2=%d\r\n"), dwKey1, dwKey2);
        fRetVal=FALSE;
    }

    //  =============================
    //  Cmp the max value name length
    //  =============================
    dwKey1 = Hlp_GetMaxValueNameChars(hKey1);
    dwKey2 = Hlp_GetMaxValueNameChars(hKey2);
    if (dwKey1 != dwKey2)
    {
        TRACE(TEXT("The max ValueName size does not match. Key1=%d, Key2=%d\r\n"), dwKey1, dwKey2);
        fRetVal=FALSE;
    }


    return fRetVal;
}

