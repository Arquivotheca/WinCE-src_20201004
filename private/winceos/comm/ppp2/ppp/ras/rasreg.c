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
/*****************************************************************************
* 
*
*   @doc EX_RAS
*
*   Date: 11/18/95
*
*   @topic RasPhoneBook |
*       This file implments the ras functions used to access the
*       registry based Ras PhoneBook.
*
*       Functions Included are :
*
*       <f RasEnumEntries>
*       <f RasGetEntryDevConfig>
*       <f RasGetEntryDialParams>
*       <f RasGetEntryProperties>
*       <f RasRenameEntry>
*       <f RasSetEntryDevConfig>
*       <f RasSetEntryDialParams>
*       <f RasSetEntryProperties>
*       <f RasValidateEntryName>
*       <f RasDeleteEntry>
*
*/

/* ----------------------------------------------------------------
*
*   Things to be done.
*
*   1)  Create a numeric based key value, then store the name
*       as the default value.  This makes the rename function
*       trivial (and safe).
*   2)  Update Autodoc for all functions
*   3)  Add Device Info
*
*
*
---------------------------------------------------------------- */



//  Include Files

#include "windows.h"
#include "tchar.h"
#include "memory.h"

#include "rasxport.h"
#include "raserror.h"
#include "unimodem.h"
#include "netui_kernel.h"

#include "cxport.h"
#include "protocol.h"
#include "ppp.h"
#include "mac.h"
#include "md5.h"
#include "crypt.h"
#include "wincrypt_priv.h"
#include "cred.h"

#include <fipsrandom.h>

#define COUNTOF(array) (sizeof(array) / sizeof(array[0]))

static BOOL
ValidateStringLength(
    IN PCWSTR wstr,
    IN DWORD  ccMin,
    IN DWORD  ccMax)
//
//  Return TRUE if wstr contains between ccMin and ccMax characters, inclusive.
//  Return FALSE if wstr contains fewer than ccMin or more than ccMax characters.
//
{
    DWORD ccStr = 0;

    while (TRUE)
    {
        if (*wstr++ == L'\0')
            break;
        ccStr++;
        if (ccStr > ccMax)
            break;
    }

    return ccMin <= ccStr && ccStr <= ccMax;
}

TCHAR szDummyPassword[] = TEXT("****************");


static void
RasCredentialsMakeKey(
    IN  LPRASDIALPARAMS pRasDialParams,
    OUT PWSTR           wszKeyName)
//
//  Form the key name string "PPP_<szEntryName>".
//
{
    ASSERT(wcslen(pRasDialParams->szEntryName) <= RAS_MaxEntryName);

    StringCchCopy(wszKeyName, 4 + RAS_MaxEntryName + 1, L"PPP_");
    StringCchCat(wszKeyName, 4 + RAS_MaxEntryName + 1, pRasDialParams->szEntryName);
}

static DWORD
SplitWackString(
    OUT    PWSTR   String,
    OUT    PWSTR  *pPrefix,
    OUT    PWSTR  *pSuffix,
    OUT    PWSTR  *pDefault)
//
//  Identify the 2 parts of the string, that preceding the '\' 
//  and that trailing the '\'. The string must not contain
//  more than a single '\'. If the string contains no '\' character
//  then it is assigned to pDefault.
//
{
    PWSTR pWack;
    DWORD Result = ERROR_SUCCESS;

    *pPrefix = NULL;
    *pSuffix = NULL;

    pWack = wcschr(String, L'\\');
    if (NULL == pWack)
    {
        // No '\' so no prefix\suffix, just a simple "default" string
        *pDefault = String;
    }
    else
    {
        // A '\' is in String

        *pWack = L'\0';
        if (pWack != String)
        {
            // One or more characters precedes the '\'
            *pPrefix = String;
        }
        if (wcschr(pWack, L'\\'))
        {
            // String contains more than one '\' character
            Result = ERROR_INVALID_PARAMETER;
        }
        if (pWack[1])
        {
            *pSuffix = pWack + 1;
        }
    }

    if (*pPrefix && (*pPrefix)[0] == L'\0')
        *pPrefix = NULL;
    if (*pSuffix && (*pSuffix)[0] == L'\0')
        *pSuffix = NULL;

    return Result;
}

static DWORD
RasCredentialsMakeUser(
    IN  LPRASDIALPARAMS pRasDialParams,
    OUT PWSTR           wszUserName)
//
//  Form the username string "[<domain>\]<user>".
//
{
    DWORD  Result = ERROR_SUCCESS;
    LPWSTR DomainPrefix, DomainSuffix;
    LPWSTR UserPrefix, UserSuffix;
    LPWSTR Domain = NULL, User = NULL;

    ASSERT(wcslen(pRasDialParams->szDomain) <= DNLEN);
    ASSERT(wcslen(pRasDialParams->szUserName) <= UNLEN);

    // To be as liberal as possible in what we accept in the RasDialParams,
    // we will allow for the szDomain field to contain the domain\username,
    // or the szUserName to contain the domain\username. However, the domain
    // cannot be present in both fields, nor can the username be present in
    // both.
    //
    // Valid input:
    //     szDomain               szUserName
    //     --------               ----------
    //     domain                 user
    //     domain\                \user
    //     domain\user            
    //                            domain\user
    //     \user                  domain\
    //
    // Invalid input:
    //     szDomain               szUserName
    //     --------               ----------
    //     domain\user            user
    //     domain                 domain\user
    //     domain\\
    //                            \\user

    Result = SplitWackString(pRasDialParams->szDomain, &DomainPrefix, &DomainSuffix, &DomainPrefix);
    if (Result != ERROR_SUCCESS)
        goto done;

    Result = SplitWackString(pRasDialParams->szUserName, &UserPrefix, &UserSuffix, &UserSuffix);
    if (Result != ERROR_SUCCESS)
        goto done;

    if ((UserPrefix && DomainPrefix)
    ||  (UserSuffix && DomainSuffix))
    {
        // Domain or username specified in both szDomain and szUserName
        Result = ERROR_INVALID_PARAMETER;
        goto done;
    }

    Domain = DomainPrefix ? DomainPrefix : UserPrefix;
    User = DomainSuffix ? DomainSuffix : UserSuffix;

    wszUserName[0] = 0;
    if (Domain && Domain[0])
    {
        StringCchPrintfW(wszUserName, DNLEN + 1 + UNLEN + 1, L"%s\\", Domain);
    }
    if (User)
    {
        StringCchCat(wszUserName, DNLEN + 1 + UNLEN + 1, User);
    }

    ASSERT(wcslen(wszUserName) <= UNLEN + 1 + DNLEN);

done:
    DEBUGMSG(ZONE_ERROR && Result, (L"PPP: ERROR Invalid RasDialParams Domain=%s UserName=%s\n", pRasDialParams->szDomain, pRasDialParams->szUserName));
    return Result;
}

static DWORD
RasCredentialsWrite(
    IN  LPRASDIALPARAMS pRasDialParams,
    IN  BOOL            bDeletePassword)
//
// Save the domain/username/password in the credentials database using
// the keyname PPP_<szRasEntryName>.
//
{
    CRED  cred;
    WCHAR wszKeyName[4 + RAS_MaxEntryName + 1];
    WCHAR wszUserName[DNLEN + 1 + UNLEN + 1];
    DWORD dwResult;
    BOOL  bUpdateExistingCredentials = FALSE;
    PCRED pCred = NULL;

    RasCredentialsMakeKey(pRasDialParams, wszKeyName);
    dwResult = RasCredentialsMakeUser(pRasDialParams, wszUserName);
    if (ERROR_SUCCESS != dwResult)
    {
        goto done;
    }

    // Set up the credential
    // We want credentials to be persisted across soft reset and cold (assuming registry persistence) resets.
    cred.dwVersion = CRED_VER_1;
    cred.dwType = CRED_TYPE_PLAINTEXT_PASSWORD;
    cred.wszTarget = wszKeyName;
    cred.dwTargetLen = wcslen(wszKeyName) + 1;
    cred.dwFlags = CRED_FLAG_PERSIST;
    cred.wszUser = wszUserName;
    cred.dwUserLen = wcslen(wszUserName) + 1;
    if (bDeletePassword)
    {
        cred.pBlob = NULL;
        cred.dwBlobSize = 0;
    }
    else if (0 == _tcscmp(pRasDialParams->szPassword, szDummyPassword))
    {
        // The password is the dummy password ("********"), returned by a prior
        // call to RasGetEntryDialParams.
        // This means that we want to leave the existing password as-is.
        
        bUpdateExistingCredentials = TRUE;        
    }
    else
    {
        // A real password has been specified.
        cred.pBlob = (PBYTE)(pRasDialParams->szPassword);
        cred.dwBlobSize = sizeof(WCHAR) * (wcslen(pRasDialParams->szPassword) + 1);
    }

    if(bUpdateExistingCredentials)
    {
        //Update the credential, without changing the blob
        dwResult = CredRead(wszKeyName, wcslen(wszKeyName) + 1, CRED_TYPE_PLAINTEXT_PASSWORD, CRED_FLAG_NO_DEFAULT, &pCred);
        if(dwResult == ERROR_SUCCESS)
        {
            cred.pBlob = pCred->pBlob;
            cred.dwBlobSize = pCred->dwBlobSize;
        }
        else
        {
            return dwResult;
        }
        dwResult = CredWrite(&cred, 0);
        CredFree((PBYTE) pCred);
    }
    else
    {
        // Write the credential
        dwResult = CredWrite(&cred, 0);
    }

done:
    return dwResult;
}

static DWORD
RasCredentialsRead(
    IN OUT  LPRASDIALPARAMS pRasDialParams,
    IN      BOOL            bOnlyRetrievePassword,
       OUT  PBOOL           pbRetrievedPassword)
//
// Read the domain/username/password from the credentials database using
// the keyname PPP_<szRasEntryName>.
//
{
    PCRED pCred = NULL;
    WCHAR wszKeyName[4 + RAS_MaxEntryName + 1];
    PWSTR pwszUserName,
          pwszSeparator;
    DWORD dwResult;

    if (pbRetrievedPassword)
        *pbRetrievedPassword = FALSE;
    RasCredentialsMakeKey(pRasDialParams, wszKeyName);

    // Read the credential matching the requested target
    // If there is no exact target match, then we dont want default/implicit default creds to be returned
    dwResult = CredRead(wszKeyName, wcslen(wszKeyName) + 1, CRED_TYPE_PLAINTEXT_PASSWORD, CRED_FLAG_NO_DEFAULT, &pCred);

    if (ERROR_SUCCESS != dwResult)
    {
    }
    else if (NULL == pCred)
    {
        dwResult = ERROR_NOT_FOUND;
    }
    else
    {
        if (FALSE == bOnlyRetrievePassword)
        {
            // Convert pCred at wszUser into domain and username.

            memset(pRasDialParams->szDomain, 0, sizeof(pRasDialParams->szDomain));

            pwszUserName = pCred->wszUser;
            pwszSeparator = wcschr(pCred->wszUser, L'\\');
            if (pwszSeparator)
            {
                // Extract the domain
                if (pwszSeparator - pwszUserName > DNLEN)
                {
                    dwResult = ERROR_INSUFFICIENT_BUFFER;
                }
                else
                {
                    memcpy(pRasDialParams->szDomain, pwszUserName, sizeof(WCHAR) * (pwszSeparator - pwszUserName));
                }

                // Username follows the separator
                pwszUserName = pwszSeparator + 1;
            }

            if (wcslen(pwszUserName) > UNLEN)
            {
                dwResult = ERROR_INSUFFICIENT_BUFFER;
            }
            else
            {
                wcscpy(pRasDialParams->szUserName, pwszUserName);
            }
        }

        SecureZeroMemory(pRasDialParams->szPassword, sizeof(pRasDialParams->szPassword));

        if (NULL == pCred->pBlob || 0 == pCred->dwBlobSize)
        {
            // No password stored in the blob.
        }
        else if (pCred->dwBlobSize > sizeof(pRasDialParams->szPassword))
        {
            dwResult = ERROR_INSUFFICIENT_BUFFER;
        }
        else
        {
            if (pbRetrievedPassword)
                *pbRetrievedPassword = TRUE;

            wcsncpy(pRasDialParams->szPassword, (PWSTR)pCred->pBlob, _countof(pRasDialParams->szPassword));
            pRasDialParams->szPassword[PWLEN] = 0;
        }
        CredFree((PBYTE)pCred);
    }
    return dwResult;
}

static DWORD
RasCredentialsDelete(
    IN LPRASDIALPARAMS pRasDialParams)
{
    WCHAR wszKeyName[RAS_MaxEntryName + 10];
    DWORD dwResult;

    RasCredentialsMakeKey(pRasDialParams, wszKeyName);
    dwResult = CredDelete(wszKeyName, wcslen(wszKeyName) + 1, CRED_TYPE_PLAINTEXT_PASSWORD, 0);

    return dwResult;
}

static DWORD
safeCopyEntryName(
    OUT  WCHAR  szEntryName[ RAS_MaxEntryName + 1 ],
    IN   PCWSTR pszEntry)
//
//  Safely copy a RAS entry name from a potentially untrusted source location.
//  Also, validate the name.
//  Return ERROR_INVALID_NAME if any problems are detected with the source name.
//
{
    DWORD dwResult = ERROR_SUCCESS;

    if (NULL == pszEntry)
        dwResult = ERROR_INVALID_NAME;
    else
    {
        __try
        {
            if (S_OK != StringCchCopyW(szEntryName, RAS_MaxEntryName + 1, pszEntry))
                dwResult = ERROR_INVALID_NAME;
            else
                dwResult = RaspCheckEntryName(szEntryName);
        }
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
            dwResult = ERROR_INVALID_NAME;
        }
    }

    return dwResult;
}

DWORD
OpenRasEntryKey(
    IN  LPCTSTR  szPhonebook,
    IN  LPCTSTR  szEntry,
    OUT PHKEY    phKey)
{
    WCHAR   szCopyOfEntryName[ RAS_MaxEntryName + 1 ];
    WCHAR   KeyName[128];
    DWORD   dwResult;

    DEBUGMSG(ZONE_FUNCTION, (TEXT("PPP: +OpenRasEntryKey %s\n"), szEntry ? szEntry : TEXT("NULL")));

    if (NULL != szPhonebook)
    {
        // We only support the system/registry phonebook.
        DEBUGMSG (ZONE_RAS, (TEXT(" OpenRasEntryKey: ERROR-Phonebook is NOT NULL\n")));
        dwResult = ERROR_CANNOT_OPEN_PHONEBOOK;
    }
    else
    {
        dwResult = safeCopyEntryName(szCopyOfEntryName, szEntry);
        if (dwResult == ERROR_SUCCESS)
        {
            StringCchPrintfW(KeyName, COUNTOF(KeyName), L"%s\\%s", RASBOOK_KEY, szCopyOfEntryName);
            DEBUGMSG(ZONE_RAS, (TEXT(" OpenRasEntryKey '%s'\n"), KeyName));
            dwResult = RegOpenKeyEx (HKEY_CURRENT_USER, KeyName, 0, KEY_ALL_ACCESS, phKey);
            if (dwResult != ERROR_SUCCESS)
                dwResult = ERROR_CANNOT_FIND_PHONEBOOK_ENTRY;
        }
    }

    DEBUGMSG(ZONE_FUNCTION, (TEXT("PPP: -OpenRasEntryKey result=%d\n"), dwResult));

    return dwResult;
}

DWORD
CreateRasEntryKey(
    IN  LPCTSTR  szPhonebook,
    IN  LPCTSTR  szEntry,
    OUT PHKEY    phKey)
{
    WCHAR   KeyName[128];
    DWORD   dwResult,
            dwDisp;

    DEBUGMSG(ZONE_FUNCTION, (TEXT("+CreateRasEntryKey %s\n"), szEntry ? szEntry : TEXT("NULL")));

    if (NULL != szPhonebook)
    {
        // We only support the system/registry phonebook.
        DEBUGMSG (ZONE_RAS, (TEXT(" CreateRasEntryKey: ERROR-Phonebook is NOT NULL\n")));
        dwResult = ERROR_CANNOT_OPEN_PHONEBOOK;
    }
    else 
    {
        dwResult = RaspCheckEntryName(szEntry);
        if (dwResult == ERROR_SUCCESS)
        {
            StringCchPrintfW(KeyName, COUNTOF(KeyName), L"%s\\%s", RASBOOK_KEY, szEntry);
            DEBUGMSG(ZONE_RAS, (TEXT(" CreateRasEntryKey '%s'\n"), KeyName));

            dwResult = RegCreateKeyEx(HKEY_CURRENT_USER, KeyName, 0, NULL,
                              REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL,
                              phKey, &dwDisp);

            if (dwResult != ERROR_SUCCESS)
                dwResult = ERROR_CANNOT_FIND_PHONEBOOK_ENTRY;
        }
    }

    DEBUGMSG(ZONE_FUNCTION, (TEXT("-CreateRasEntryKey result=%d\n"), dwResult));

    return dwResult;
}

DWORD
CopyRegEntryValues(
    HKEY    hkeyDst,
    HKEY    hkeySrc)
//
//  Copy all the values from the source key to the destination.
//
{
    DWORD   dwResult, iValue;
    LPTSTR  ValueName;              // The name of the value read in
    DWORD   dwValueLen;             // Length of the name
    LPBYTE  pData;                  // Pointer to the data of the value
    DWORD   dwDataLen;              // Length of the data
    DWORD   dwType;                 // Type of the data 
    DWORD   cValues, cchMaxValueName;
    DWORD   cbMaxValueData;

    // Find out how many values are in the source key, and the max sizes
    dwResult = RegQueryInfoKey (hkeySrc, NULL, NULL, NULL, NULL, NULL, NULL,
                            &cValues, &cchMaxValueName, &cbMaxValueData,
                            NULL, NULL);

    if (ERROR_SUCCESS != dwResult)
    {
        // What should I return for this.
        DEBUGMSG( ZONE_RAS | ZONE_ERROR, (TEXT(" CopyRegEntryValues: ERROR %d from QueryInfo\n"), dwResult));
    }
    else
    {
        // Allocate the space for value names and data
        PREFAST_SUPPRESS(22011, "cchMaxValueName will not be too large");
        ValueName = (LPTSTR)LocalAlloc (LPTR, (cchMaxValueName+1)*sizeof(TCHAR));
        PREFAST_SUPPRESS(22011, "cbMaxValueData will not be too large");
        pData = (LPBYTE)LocalAlloc (LPTR, cbMaxValueData+1);

        DEBUGMSG (ZONE_RAS, (TEXT("ValueName=0x%X (Len=%d) pData=0x%X (Len=%d)\r\n"),
               ValueName, (cchMaxValueName+1)*(sizeof(TCHAR)),
               pData, cbMaxValueData));

        if ((NULL == ValueName) || (NULL == pData))
        {
            DEBUGMSG(ZONE_RAS | ZONE_ERROR, (TEXT(" CopyRegEntryValues: ERROR: Out of memory\n")));
            dwResult = ERROR_CANNOT_FIND_PHONEBOOK_ENTRY;
        }
        else
        {
            // Iterate over the values.

            for (iValue = 0; iValue != cValues; iValue++)
            {
                dwValueLen = cchMaxValueName+1;
                dwDataLen = cbMaxValueData;

                // Read the source value
                dwResult = RegEnumValue (hkeySrc, iValue, ValueName,
                                     &dwValueLen, NULL, &dwType,
                                     pData, &dwDataLen);

                if (ERROR_SUCCESS != dwResult)
                    break;

                DEBUGMSG (ZONE_RAS, (TEXT("Read Entry '%s' Len=%d\n"), ValueName, dwDataLen));

                // Write the dest value.
                dwResult = RegSetValueEx (hkeyDst, ValueName, 0, dwType, pData, dwDataLen);

                if (ERROR_SUCCESS != dwResult)
                    break;
            }
        }
        LocalFree(ValueName);
        LocalFree(pData);
    }

    return dwResult;
}

DWORD
RegCreateNewKey(
    IN  HKEY    hKey, 
    IN  LPCWSTR lpSubKey, 
    OUT PHKEY   phkResult)
//
//  Create a new registry key.
//  Fail and return ERROR_ALREADY_EXISTS if the key is already present.
//
{
    DWORD dwDisposition,
          dwResult;

    dwResult = RegCreateKeyEx(hKey, lpSubKey, 0, NULL, 0, 0, NULL, phkResult, &dwDisposition);
    if (dwResult == NO_ERROR)
    {
        if (dwDisposition == REG_OPENED_EXISTING_KEY)
        {
            // Can't rename to a key that already exists
            dwResult = ERROR_ALREADY_EXISTS;
            RegCloseKey(*phkResult);
            *phkResult = NULL;
        }
    }
    return dwResult;
}

BOOL
GetTempRegKey(
    IN  HKEY    hKeyBase,
    OUT PWSTR   wszTempKeyName,
    OUT PHKEY   phTempKey)
{
    DWORD triesLeft,
          dwRandom,
          dwResult;

    for (triesLeft = 25; triesLeft; triesLeft--)
    {
        FipsGenRandom(sizeof(dwRandom), (PBYTE)&dwRandom);
        swprintf(wszTempKeyName, L"$tmp%d", dwRandom);

        dwResult = RegCreateNewKey(hKeyBase, wszTempKeyName, phTempKey);
        if (dwResult == NO_ERROR)
        {
            // Successfully created new temp key
            break;
        }
    }

    return triesLeft > 0;
}

DWORD
RegMoveKey(
    IN      HKEY    hKeyBase,
    IN  OUT PHKEY   phKeyDst,
    IN      PCWSTR  wszNameDst,
    IN  OUT PHKEY   phKeySrc,
    IN      PCWSTR  wszNameSrc)
//
//  Move the contents of the source key to the destination key.
//  If successful, the source key and its contents will be deleted.
//  If unsuccessful, the source key and its contents will be unaffected, and no new registry entries will be created.
//
//
{
    DWORD dwResult;

    dwResult = CopyRegEntryValues(*phKeyDst, *phKeySrc);
    if (dwResult == NO_ERROR)
    {
        // Successfully copied src to dst, can delete src
        RegCloseKey(*phKeySrc);
        *phKeySrc = NULL;
        dwResult = RegDeleteKey(hKeyBase, wszNameSrc);
    }
    else
    {
        // Unsuccessful attempt to copy src to dst.
        // Cleanup the partially created dst key
        RegCloseKey(*phKeyDst);
        *phKeyDst = NULL;
        (void)RegDeleteKey(hKeyBase, wszNameDst);
    }

    return dwResult;
}

DWORD
RegRenameKey(
    IN  HKEY    hKeyBase,
    IN  PCWSTR  wszOldKeyName,
    IN  PCWSTR  wszNewKeyName)
//
//  Rename hKeyBase\OldKey to hKeyBase\NewKey.
//
//  If OldKey and NewKey are identical, this is a no-op.
//
//  If OldKey and NewKey are different, then we need to copy all of
//  OldKey to NewKey, then delete OldKey.
//
//  If OldKey and NewKey are only different in upper/lower case characters,
//  then we must copy OldKey to a temporary key, delete OldKey, then
//  copy the temporary key to NewKey. Yuck. Would be nice if the OS would
//  support a RenameKey API...
//
{
    DWORD dwResult;
    HKEY  hKeyOld = NULL,
          hKeyNew = NULL,
          hKeyTemp = NULL;
    WCHAR wszTempKeyName[64];

    dwResult = RegOpenKeyEx(hKeyBase, wszOldKeyName, 0, 0, &hKeyOld);
    if (dwResult == NO_ERROR)
    {
        if (0 == wcscmp(wszOldKeyName, wszNewKeyName))
        {
            // Nothing to do, identical names.
        }
        else if (0 == wcsicmp(wszOldKeyName, wszNewKeyName))
        {
            //
            // Changing the case of the key
            //

            // Create a  temporary key to copy old to
            if (FALSE == GetTempRegKey(hKeyBase, &wszTempKeyName[0], &hKeyTemp))
            {
                dwResult = ERROR_OUTOFMEMORY;
            }
            else
            {
                // Copy old to temp (note this deletes the old if successful)
                dwResult = RegMoveKey(hKeyBase, &hKeyTemp, &wszTempKeyName[0], &hKeyOld, wszOldKeyName);
                if (dwResult == NO_ERROR)
                {
                    // Create the new key
                    dwResult = RegCreateNewKey(hKeyBase, wszNewKeyName, &hKeyNew);
                    if (dwResult == NO_ERROR)
                    {
                        // Copy temporary to new
                        dwResult = RegMoveKey(hKeyBase, &hKeyNew, wszNewKeyName, &hKeyTemp, wszTempKeyName);
                    }
                }
            }
        }
        else
        {
            //
            // Renaming to a new key value
            //
            dwResult = RegCreateNewKey(hKeyBase, wszNewKeyName, &hKeyNew);
            if (dwResult == NO_ERROR)
            {
                // Move the old to the new
                dwResult = RegMoveKey(hKeyBase, &hKeyNew, wszNewKeyName, &hKeyOld, wszOldKeyName);
            }
        }

    }

    if (hKeyTemp)
        RegCloseKey(hKeyTemp);

    if (hKeyNew)
        RegCloseKey(hKeyNew);

    if (hKeyOld)
        RegCloseKey(hKeyOld);

    return dwResult;
}

/*****************************************************************************
* 
*   @func   DWORD | RasMakeDefault |  Make the default Ras Entries
*
*   @rdesc  If the function succeeds the value is zero. Else an error
*           from raserror.h is returned.
*   @ecode  ERROR_UNKNOWN | Required reserved parameter is not NULL.
*   @ecode  ERROR_CANNOT_OPEN_PHONEBOOK | Invalid PhoneBookPath specified
*   
*   @parm   LPTSTR  | reserved          | reserved (must be null)
*   @parm   LPTSTR  | lpszPhoneBookPath | 
*       Points to a null terminated string that specifies the full path and
*       filename of the phonebook file.  This parameter can be NULL, in which
*       case the default phonebook file is used.  This parameter should always
*       be NULL for Windows 95 and WinCE since the phonebook entries are
*       stored in the registry.
*
*
*/

DWORD APIENTRY
AfdRasMakeDefault(
    LPCTSTR  reserved, 
    LPCTSTR  lpszPhonebook )
{
    TCHAR       szEntry[128];
    RASENTRY    RasEntry;
    DWORD       dwSize;

    DEBUGMSG( ZONE_RAS | ZONE_FUNCTION,
              (TEXT("+AfdRasMakeDefault(0x%08X, 0x%08X(%s))\r\n"), 
               reserved, lpszPhonebook,
               (lpszPhonebook != NULL) ? lpszPhonebook : TEXT("NULL")));

    RasEntry.dwSize = sizeof(RASENTRY);
    RaspInitRasEntry(&RasEntry);

    RasEntry.ipaddr.d = 192;
    RasEntry.ipaddr.c = 168;
    RasEntry.ipaddr.b = 55;
    RasEntry.ipaddr.a = 100;

    // _tcscat( KeyName, TEXT("`Desktop @ 19200`") );
    // Get the string from the resource file
    dwSize = (DWORD)CallKGetNetString (NETUI_GETNETSTR_DIRECT_NAME,
                                      szEntry, sizeof(szEntry)/sizeof(TCHAR));
    if (!dwSize) {
        HKEY        hKey;
        DWORD       dwDisp;
        LONG        hRes;
        RETAILMSG (1, (TEXT("Unable to get default name, just creating key\r\n")));

        hRes = RegCreateKeyEx(HKEY_CURRENT_USER, RASBOOK_KEY, 0, NULL,
                              REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL,
                              &hKey, &dwDisp);
        if (hRes == ERROR_SUCCESS) {
            RegCloseKey(hKey);
        }
        return 0;
    }

    AfdRasSetEntryProperties(lpszPhonebook, szEntry, (LPBYTE)&RasEntry, sizeof(RASENTRY),
                             NULL,0);
    return 0;
}

/*****************************************************************************
* 
*   @func   DWORD | RasEnumEntries |  Lists all entry names in a remote 
*                                       access phone book.
*
*   @rdesc  If the function succeeds the value is zero. Else an error
*           from raserror.h is returned.
*   @ecode  ERROR_UNKNOWN | Required reserved parameter is not NULL.
*   @ecode  ERROR_CANNOT_OPEN_PHONEBOOK | Invalid PhoneBookPath specified
*   @ecode  ERROR_INVALID_SIZE | Invalid lpcb parameter
*   
*   @parm   LPTSTR  | reserved          | reserved
*   @parm   LPTSTR  | lpszPhoneBookPath | path name of phone book
*   @parm   LPRASENTRYNAME | lprasentryname |
*       Points to a buffer that receives an array of <f RASENTRYNAME>
*       structures, one for each phonebook entry. Before calling the
*       function, an application must set the dwSize member of the first
*       <f RASENTRYNAME> structure in the buffer to sizeof(<f RASENTRYNAME>)
*       in order to identify the version of the structure being passed.
*   @parm   LPDWORD | lpcb              |
*       Points to a variable that contains the size, in bytes, of the buffer
*       specified by lprasentryname. On return, the function sets this
*       variable to the number of bytes required to successfully complete
*       the call.
*   @parm   LPDWORD | lpcEntries        |
*       Points to a variable that the function, if successful, sets to
*       the number of phonebook entries written to the buffer specified
*       by lprasentryname.
*
*
*/

DWORD APIENTRY
AfdRasEnumEntries (IN     LPCTSTR         reserved, 
                   IN     LPCTSTR         lpszPhoneBookPath, 
                   IN OUT LPRASENTRYNAME  lpRasEntryName, 
                   IN     DWORD           cbRasEntryName,
                   IN OUT LPDWORD         lpcb, 
                      OUT LPDWORD         lpcEntries )
{
    HKEY        hKey;
    LONG        hRes;
    DWORD       Index;
    DWORD       cbRequired = 0;
    DWORD       RetVal = ERROR_SUCCESS;
    TCHAR       ValueString[RAS_MaxEntryName + 1];
    DWORD       ValueLen;
    DWORD       cEntries;   // Number of entries found.
    FILETIME    LastWrite;

    DEBUGMSG( ZONE_RAS | ZONE_FUNCTION,
              (TEXT("+AfdRasEnumEntries( %08X, %08X, %08X, %08X, %08X )\r\n"), 
               reserved, lpszPhoneBookPath, lpRasEntryName, lpcb, lpcEntries));

    // Check the required parameters.
    if (NULL != reserved) {
        RetVal = ERROR_UNKNOWN;     
    }
    if (NULL != lpszPhoneBookPath) {
        // We only support the system/registry phonebook.
        RetVal = ERROR_CANNOT_OPEN_PHONEBOOK;
    }
    if (NULL == lpcb) {
        // Invalid argument
        RetVal = ERROR_INVALID_SIZE;
    }

    if (ERROR_SUCCESS != RetVal) {
        DEBUGMSG( ZONE_RAS | ZONE_FUNCTION, (TEXT("-AfdRasEnumEntries() returning %d\r\n"), RetVal));
        return RetVal;
    }
    
    // Initialize the return count information.
    if (NULL != lpcEntries) {
        *lpcEntries = 0;
    }
    
    hRes = RegOpenKeyEx( HKEY_CURRENT_USER, RASBOOK_KEY, 0, KEY_READ, &hKey );

    if (ERROR_SUCCESS != hRes) {
        DEBUGMSG( ZONE_WARN|ZONE_RAS,
                  (TEXT("AfdRasEnumEntries() unable to open Registry Key '%s' Error %d\r\n"),
                   RASBOOK_KEY, hRes));
        AfdRasMakeDefault(reserved, lpszPhoneBookPath);

        // Now try to open the key.
        hRes = RegOpenKeyEx(HKEY_CURRENT_USER,RASBOOK_KEY,0,
                            KEY_READ,&hKey);
    }

    cEntries = 0;

    if (ERROR_SUCCESS == hRes ) {
        // Now loop over the sub-keys getting the names of each.

        for (Index = 0; hRes == ERROR_SUCCESS; Index++) {
            ValueLen = sizeof(ValueString) / sizeof(ValueString[0]);
            hRes = RegEnumKeyEx (hKey, Index, ValueString, &ValueLen, NULL, NULL, NULL, &LastWrite);
            if (hRes == ERROR_SUCCESS) {
                DEBUGMSG(ZONE_RAS | ZONE_FUNCTION, (TEXT("AfdRasEnumEntries: Found entry %s\r\n"), ValueString));
                cbRequired += sizeof(RASENTRYNAME);
                if ((cbRequired <= cbRasEntryName) && (NULL != lpRasEntryName)) {
                    // Stuff the data in.
                    lpRasEntryName[cEntries].dwSize = sizeof(RASENTRYNAME);
                    _tcscpy(lpRasEntryName[cEntries].szEntryName, ValueString);

                    // Tell them how many we wrote.
                    if (NULL != lpcEntries) {
                        (*lpcEntries)++;
                    }
                }
                // Keep track of how many we found?
                cEntries++;
            }
        }
        RegCloseKey (hKey);
    }

    // If no other error then 
    if ((ERROR_SUCCESS == RetVal) && (cbRequired > cbRasEntryName)) {
        RetVal = ERROR_BUFFER_TOO_SMALL;
    }       

    // Number of bytes required/returned.
    *lpcb = cbRequired;

    DEBUGMSG(ZONE_RAS | ZONE_FUNCTION, (TEXT("-AfdRasEnumEntries() returning %d\r\n"), RetVal));
    return RetVal;
}

/*****************************************************************************
* 
*   @func   DWORD | RasGetEntryDialParams |  The <f RasGetEntryDialParams>
*           function retrieves the connection information saved by the
*           last successful call to the <f RasDial> or
*           <f RasGetEntryDialParams> function for a specified phonebook entry.
*
*   @rdesc  If the function succeeds, the return value is zero.
*           If the function fails, the return value can be one of the
*           following error codes.
*   @ecode  ERROR_CANNOT_OPEN_PHONEBOOK | Invalid szPhonebook
*   @ecode  ERROR_CANNOT_FIND_PHONEBOOK_ENTRY | Invalid szEntry
*   @ecode  ERROR_INVALID_PARAMETER     | lpRasDialParams or lpfPassword is NULL,
*                                       | or lpRasDialParams->dwSize is not correct
*   
*   @parm   LPTSTR          | lpszPhoneBook     | pointer to the full path
*               and filename of the phonebook file
*   @parm   LPRASDIALPARAMS | lprasdialparams   | pointer to a structure that
*               receives the connection properties
*   @parm   LPBOOL          | lpfPassword       | Indicates whether the user's
*               password was retrieved.
*               
*
*/

DWORD APIENTRY
AfdRasGetEntryDialParams(
    IN     LPCTSTR         lpszPhonebook, 
    IN OUT LPRASDIALPARAMS lpRasDialParams, 
    IN     DWORD           cbRasDialParams,
    OUT    LPBOOL          lpfPassword)
{
    DWORD   dwResult;

    DEBUGMSG( ZONE_FUNCTION,
             ( TEXT( "+RasGetEntryDialParams( %s, %s)\n"), 
             lpszPhonebook ? lpszPhonebook : TEXT("NULL"),
             lpRasDialParams && lpRasDialParams->szEntryName ?  lpRasDialParams->szEntryName : TEXT("NULL")) );

    if ((NULL == lpRasDialParams)
    ||  (NULL == lpfPassword)
    ||  (sizeof(RASDIALPARAMS) != lpRasDialParams->dwSize)
    ||  !ValidateStringLength(lpRasDialParams->szEntryName,   1, RAS_MaxEntryName))
    {
        dwResult = ERROR_INVALID_PARAMETER;
    }
    else
    {
        // Zero out unused Phone Number fields.
        memset(lpRasDialParams->szPhoneNumber, 0, sizeof(lpRasDialParams->szPhoneNumber));
        memset(lpRasDialParams->szCallbackNumber, 0, sizeof(lpRasDialParams->szCallbackNumber));

        // Get username/domain/password for szEntryName from CredMan
        dwResult = RasCredentialsRead(lpRasDialParams, FALSE, lpfPassword);

        // For security reasons do not expose the password. The user should not need to know it.
        SecureZeroMemory(lpRasDialParams->szPassword, sizeof(lpRasDialParams->szPassword));

        if (*lpfPassword)
            memcpy(lpRasDialParams->szPassword, szDummyPassword, sizeof(szDummyPassword));
    }

    DEBUGMSG (ZONE_FUNCTION, (TEXT("-RasGetEntryDialParams Result=%u (%s,%s)\r\n"),
                  dwResult,
                  lpRasDialParams && lpRasDialParams->szUserName ? lpRasDialParams->szUserName : TEXT("(NULL)"),   
                  lpRasDialParams && lpRasDialParams->szDomain   ? lpRasDialParams->szDomain   : TEXT("(NULL)")));
    return dwResult;
}

//
// Replace the placeholder password with the real one from the registry
//
BOOL 
FixRasPassword(
    IN OUT LPRASDIALPARAMS lpRasDialParams)
{
    DWORD dwResult = ERROR_SUCCESS;

    if (_tcscmp(lpRasDialParams->szPassword, szDummyPassword) == 0)
    {
        // need to replace the dummy password with the real one.
        // Look in the system phone book

        dwResult = RasCredentialsRead(lpRasDialParams, TRUE, NULL);
    }
    return dwResult == ERROR_SUCCESS;
}



/*****************************************************************************
* 
*   @func   DWORD | RasSetEntryDialParams |  The <f RasSetEntryDialParams>
*           function changes the connection information saved by the
*           last successful call to the <f RasDial> or
*           <f RasGetEntryDialParams> function for a specified phonebook entry.
*
*   @rdesc  If the function succeeds, the return value is zero.
*           If the function fails, the return value can be one of the
*           following error codes.
*       @ecode  ERROR_BUFFER_INVALID                |
*           The address or buffer specified by lpRasDialParams is invalid.
*       @ecode  ERROR_CANNOT_OPEN_PHONEBOOK         |
*           The phonebook is corrupted or missing components.
*       @ecode  ERROR_CANNOT_FIND_PHONEBOOK_ENTRY   |
*           The phonebook entry does not exist.
*   
*   @parm   LPTSTR          | lpszPhoneBook     | pointer to the full path
*               and filename of the phonebook file
*   @parm   LPRASDIALPARAMS | lprasdialparams   | pointer to a structure that
*               receives the connection properties
*   @parm   BOOL            | fRemovePassword   | Indicates whether the user's
*               password should be saved.
*               
*
*/

DWORD APIENTRY
AfdRasSetEntryDialParams(
    IN  LPCTSTR         lpszPhonebook, 
    IN  LPRASDIALPARAMS lpRasDialParams, 
    IN  DWORD           cbRasDialParams,
    IN  BOOL            fRemovePassword)
{
    DWORD         dwResult;
    RASDIALPARAMS CopyOfUserRasDialParams;

    DEBUGMSG (ZONE_FUNCTION, (L"RAS: +RasSetEntryDialParams %s %s\n", lpRasDialParams->szUserName, lpRasDialParams->szDomain));

    if (NULL == lpRasDialParams
    ||  sizeof(RASDIALPARAMS) != lpRasDialParams->dwSize)
    {
        dwResult = ERROR_BUFFER_INVALID;
    }
    else
    {
        //
        // Since the user application could attempt to attack the OS
        // by modifying the supplied user data buffer while this function
        // is in the middle of accessing it (e.g. after the Validation checks
        // but prior to using the buffer), the user buffer is first copied into
        // a local buffer, safely protected from an untrusted application modification.
        //
        // Note that for a trusted application, this copy is just from one part of
        // the application stack to another, so it offers no protection.
        //
        CopyOfUserRasDialParams = *lpRasDialParams;
        lpRasDialParams = &CopyOfUserRasDialParams;

        if (!ValidateStringLength(lpRasDialParams->szEntryName,   1, RAS_MaxEntryName)
        ||  !ValidateStringLength(lpRasDialParams->szPhoneNumber, 0, RAS_MaxPhoneNumber)
        ||  !ValidateStringLength(lpRasDialParams->szUserName,    0, UNLEN)
        ||  !ValidateStringLength(lpRasDialParams->szPassword,    0, PWLEN)
        ||  !ValidateStringLength(lpRasDialParams->szDomain,      0, DNLEN))
        {
            dwResult = ERROR_BUFFER_INVALID;
        }
        else
        {
            dwResult = RasCredentialsWrite(lpRasDialParams, fRemovePassword);
        }
    }

    DEBUGMSG (ZONE_RAS, (L"-AfdRasSetEntryDialParams : Result=%u\n", dwResult));
    return dwResult;
}

//
//  Conversion support for CE 3.0 and earlier RASENTRY structures to CE 4.0.
//  That is, for apps that have not been recompiled for 4.0, we translate the
//  old format RASENTRY that they use to the new 4.0 format.
//

#define RAS_MaxDeviceName_V3    32

typedef struct tagRASENTRYW_V3
{
    DWORD       dwSize;
    DWORD       dwfOptions;
    DWORD       dwCountryID;
    DWORD       dwCountryCode;
    WCHAR       szAreaCode[ RAS_MaxAreaCode + 1 ];
    WCHAR       szLocalPhoneNumber[ RAS_MaxPhoneNumber + 1 ];
    DWORD       dwAlternateOffset;
    RASIPADDR   ipaddr;
    RASIPADDR   ipaddrDns;
    RASIPADDR   ipaddrDnsAlt;
    RASIPADDR   ipaddrWins;
    RASIPADDR   ipaddrWinsAlt;
    DWORD       dwFrameSize;
    DWORD       dwfNetProtocols;
    DWORD       dwFramingProtocol;
    WCHAR       szScript[ MAX_PATH ];
    WCHAR       szAutodialDll[ MAX_PATH ];
    WCHAR       szAutodialFunc[ MAX_PATH ];
    WCHAR       szDeviceType[ RAS_MaxDeviceType + 1 ];
    WCHAR       szDeviceName[ RAS_MaxDeviceName_V3 + 1 ];
    WCHAR       szX25PadType[ RAS_MaxPadType + 1 ];
    WCHAR       szX25Address[ RAS_MaxX25Address + 1 ];
    WCHAR       szX25Facilities[ RAS_MaxFacilities + 1 ];
    WCHAR       szX25UserData[ RAS_MaxUserData + 1 ];
    DWORD       dwChannels;
    DWORD       dwReserved1;
    DWORD       dwReserved2;
} RASENTRY_V3, *LPRASENTRY_V3;


/*****************************************************************************
*
*
*   @func   DWORD   |   RasSetEntryProperties | Comment on function.
*
*   @rdesc
*       If the function succeeds, the return value is zero.
*       <nl>If the function fails, the return value can be one of the
*       following error codes: <nl>
*       @ecode  ERROR_INVALID_PARAMETER             |
*           The function is called with an invalid parameter.
*       @ecode  ERROR_BUFFER_INVALID                |
*           The address or buffer specified by lpbEntryInfo is invalid.
*       @ecode  ERROR_BUFFER_TOO_SMALL              |
*           The buffer size indicated in lpdwEntryInfoSize is too small.
*       @ecode  ERROR_CANNOT_OPEN_PHONEBOOK         |
*           The phonebook is corrupted or missing components.
*
*
*   @parm   LPTSTR  |   lpszPhonebook   |
*       Points to a null terminated string that specifies the full path and
*       filename of the phonebook file.  This parameter can be NULL, in which
*       case the default phonebook file is used.  This parameter should always
*       be NULL for Windows 95 and WinCE since the phonebook entries are
*       stored in the registry.
*   @parm   LPTSTR  |   szEntry         |
*       Points to a null terminated string containing an existing entry name.
*       If a "" entry name is specified, structures containing default
*       values are returned.
*   @parm   LPBYTE  |   lpbEntry        |
*       Points to a RASENTRY structure that receives the connection data
*       associated with the phonebook entry specified by the lpszEntry member
*       followed by any additional bytes needed for the alternate phone number
*       list if any.  On entry, the dwSize member of this structure must
*       specify the size of the structure.
*   @parm   DWORD |   dwEntrySize   |
*       The size, in bytes, of the buffer specified by lpEntryInfo.
*   @parm   LPBYTE  |   pDevConfig  |
*       Points to buffer of device-specific configuration information.
*       This parameter may be NULL if the data is not required.
*   @parm   DWORD |   cbDevConfig        |
*       Points to a variable that contains the size, in bytes, of the buffer
*       specified by lpb. 
*
*   @ex     An example of how to use this function follows |
*           No Example
*
*/
DWORD APIENTRY
AfdRasSetEntryProperties(
    LPCTSTR lpszPhonebook,
    LPCTSTR szEntry,
    LPBYTE  lpbEntry,
    DWORD   dwEntrySize,
    LPBYTE  pDevConfig,
    DWORD   cbDevConfig)
{
    HKEY        hKey;
    LONG        hRes;
    DWORD       RetVal;
    RASPENTRY   RaspEntry;  // Private version of structure
    RASENTRY    *pCopyOfUserRasEntry;

    DEBUGMSG(ZONE_RAS, (L"PPP: +AfdRasSetEntryProperties Entry=%s EntrySize=%u cbDevConfig=%u\n", szEntry, dwEntrySize, cbDevConfig));

    pCopyOfUserRasEntry = (RASENTRY *)LocalAlloc(LPTR, sizeof(RASENTRY));

    if (NULL == lpbEntry)
        RetVal = ERROR_BUFFER_INVALID;
    else if (dwEntrySize < sizeof(RASENTRY_V3)
         ||  (dwEntrySize > sizeof(RASENTRY_V3) && dwEntrySize < sizeof(RASENTRY)))
        RetVal = ERROR_BUFFER_TOO_SMALL;
    else if (NULL == pCopyOfUserRasEntry)
        RetVal = ERROR_OUTOFMEMORY;
    else
    {
        //
        // Since the user application could attempt to attack the OS
        // by modifying the supplied user data buffer while this function
        // is in the middle of accessing it (e.g. after the Validation checks
        // but prior to using the buffer), the user buffer is first copied into
        // a local buffer, safely protected from an untrusted application modification.
        //
        // Note that for a trusted application, this copy is just from one part of
        // the application stack to another, so it offers no protection.
        //
        // Note that we don't make a copy of the DevConfigData (lpb), because that
        // data is not interpreted by this function.
        //
        // Note that since CE does not support alternate phone numbers, we truncate
        // the size of the RAS entry for the copy at sizeof(RASENTRY). That is, if
        // the caller passes in a larger buffer with phone numbers tacked onto the
        // end, then we discard those phone numbers in the copy because they would
        // not be used anyway.
        //
        if (dwEntrySize > sizeof(RASENTRY))
            dwEntrySize = sizeof(RASENTRY);

        RetVal = ERROR_SUCCESS;
        __try
        {
            memcpy(pCopyOfUserRasEntry, lpbEntry, dwEntrySize);
        }
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
            RetVal = ERROR_INVALID_PARAMETER;
        }

        if (ERROR_SUCCESS == RetVal)
            RetVal = CreateRasEntryKey(lpszPhonebook, szEntry, &hKey);

        if (ERROR_SUCCESS == RetVal)
        {
            // Convert the value.
            RaspConvPublicPriv (pCopyOfUserRasEntry, &RaspEntry);

            // Use the private structure size.
            dwEntrySize = sizeof(RASPENTRY);

            // Create The value.
            hRes = RegSetValueEx(hKey, RASBOOK_ENTRY_VALUE, 0, REG_BINARY, (LPBYTE)&RaspEntry,  dwEntrySize);
            if (ERROR_SUCCESS != hRes)
            {
                DEBUGMSG(ZONE_ERROR, (L"PPP: AfdRasSetEntryProperties: ERROR %d from RegSetValueEx\n", hRes));
                RetVal = ERROR_CANNOT_OPEN_PHONEBOOK;
            }
            else if (pDevConfig)
            {
                BOOL      fRet;
                DATA_BLOB dataIn, 
                          dataOut = {0};

                if (0 == _tcscmp (RaspEntry.szDeviceType, RASDT_Vpn))
                {
                    // encrypt the device config struct for VPN (mainly so that L2TP secrets are encrypted in the registry)
                    // Note that the secrets are still accessible via RasGetEntryProperties, which has to return the
                    // decrypted version.
                    // Would be more secure  if this work was done by L2TP itself. This would make it possible for the 
                    // secrets to be hidden from the API. However that requires some rearchitecting. 
                    dataIn.cbData = cbDevConfig;
                    dataIn.pbData = pDevConfig;
                    fRet = CryptProtectData(&dataIn, RaspEntry.szDeviceName, NULL, NULL, NULL, CRYPTPROTECT_PRIVATE, &dataOut);
                    if (fRet)
                    {
                        pDevConfig = dataOut.pbData;
                        cbDevConfig = dataOut.cbData;
                    }
                    else
                    {
                        hRes = GetLastError();
                        ASSERT(hRes != ERROR_SUCCESS);
                        DEBUGMSG(ZONE_ERROR, (L"PPP: AfdRasSetEntryProperties: ERROR %d from CryptProtectData\n", hRes));
                    }
                }
                if (ERROR_SUCCESS == hRes)
                    hRes = RegSetValueEx(hKey, RASBOOK_DEVCFG_VALUE, 0, REG_BINARY, pDevConfig, cbDevConfig);
                if (ERROR_SUCCESS != hRes)
                {
                    DEBUGMSG(ZONE_ERROR, (L" AfdRasSetEntryProperties: ERROR %d from RegSetValueEx\n", hRes));
                    RetVal = ERROR_CANNOT_OPEN_PHONEBOOK;
                }
                // CryptProtectData allocates a buffer
                if (dataOut.pbData)
                    LocalFree(dataOut.pbData);
            }

            RegCloseKey (hKey);
        }
    }
    
    LocalFree(pCopyOfUserRasEntry);

    DEBUGMSG(ZONE_RAS | (ZONE_ERROR && RetVal != ERROR_SUCCESS), (TEXT("PPP: -AfdRasSetEntryProperties Result=%d\n"), RetVal));
    return RetVal;
}

/*****************************************************************************
*
*
*   @func   DWORD   |   RasGetEntryProperties |
*       The RasGetEntryProperties function retrieves the properties
*       of a phonebook entry.
*
*   @rdesc
*       If the function succeeds, the return value is zero.
*       <nl>If the function fails, the return value can be one of the
*       following error codes: <nl>
*       @ecode  ERROR_INVALID_PARAMETER             |
*           The function is called with an invalid parameter.
*       @ecode  ERROR_BUFFER_INVALID                |
*           The address or buffer specified by lpbEntryInfo is invalid.
*       @ecode  ERROR_BUFFER_TOO_SMALL              |
*           The buffer size indicated in lpdwEntryInfoSize is too small.
*       @ecode  ERROR_CANNOT_OPEN_PHONEBOOK         |
*           The phonebook is corrupted or missing components.
*       @ecode  ERROR_CANNOT_FIND_PHONEBOOK_ENTRY   |
*           The phonebook entry does not exist.
*
*   @parm   LPTSTR  |   lpszPhonebook   |
*       Points to a null terminated string that specifies the full path and
*       filename of the phonebook file.  This parameter can be NULL, in which
*       case the default phonebook file is used.  This parameter should always
*       be NULL for Windows 95 and WinCE since the phonebook entries are
*       stored in the registry.
*   @parm   LPTSTR  |   szEntry         |
*       Points to a null terminated string containing an existing entry name.
*       If a "" entry name is specified, structures containing default
*       values are returned.
*   @parm   LPBYTE  |   lpbEntry        |
*       Points to a RASENTRY structure that receives the connection data
*       associated with the phonebook entry specified by the lpszEntry member
*       followed by any additional bytes needed for the alternate phone number
*       list if any.  On entry, the dwSize member of this structure must
*       specify the size of the structure.
*   @parm   LPDWORD |   lpdwEntrySize   |
*       Points to a variable that contains the size, in bytes, of the buffer
*       specified by lpEntryInfo.  On return, the function sets this variable
*       to the number of bytes required.  This parameter can be NULL if the
*       information is not required.  The recommended method for determining
*       the required buffer size is to call RasGetEntryProperties with the
*       lpbEntryInfo set to null and the DWORD pointed to by lpdwEntryInfoSize
*       set to zero. The function will return the required buffer size in the
*       DWORD.
*   @parm   LPBYTE  |   lpb             |
*       Points to buffer to receive device-specific configuration information.
*       This parameter may be NULL if the data is not required.  The
*       recommended method for determining the required buffer size is to call
*       RasGetEntryProperties with the lpbDeviceInfo set to null and the DWORD
*       pointed to by lpdwDeviceInfoSize set to zero. The function will return
*       the required buffer size in the DWORD.
*   @parm   LPDWORD |   lpdwSize        |
*       Points to a variable that contains the size, in bytes, of the buffer
*       specified by lpb. On return, the function sets this variable to the
*       number of bytes required. This parameter can be NULL if the
*       information is not required.
*
*   @ex     An example of how to use this function follows |
*           No Example
*
*/
DWORD APIENTRY
AfdRasGetEntryProperties(
    IN      LPCTSTR lpszPhonebook,
    IN      LPCTSTR szEntry,
        OUT LPBYTE  lpbEntry,
    IN      DWORD   cbEntry,
    IN  OUT LPDWORD lpdwEntrySize,
        OUT LPBYTE  lpb,
    IN      DWORD   cbDeviceInfo,
    IN  OUT LPDWORD lpdwSize)
{
    HKEY        hKey;
    LONG        hRes;
    DWORD       RetVal = 0;
    DWORD       dwType;
    DWORD       dwSize;
    LPRASENTRY  pEntry;
    RASPENTRY   RaspEntry;  // Private version of structure.
    WCHAR       szCopyOfEntryName[ RAS_MaxEntryName + 1 ];

    DEBUGMSG(ZONE_RAS, (L"PPP: +AfdRasGetEntryProperties Entry=%s EntrySize=%u\n", szEntry, lpdwEntrySize ? *lpdwEntrySize : 0));

    // If a RASENTRY structure is specified then the size of it must also be specified.
    if (lpbEntry != NULL && lpdwEntrySize == NULL)
        return ERROR_INVALID_PARAMETER;

    szCopyOfEntryName[0] = 0;

    if (NULL == lpdwEntrySize)
        RetVal = ERROR_INVALID_PARAMETER;
    else if (szEntry == NULL)
        RetVal = ERROR_INVALID_NAME;
    else if (S_OK != StringCchCopyW(szCopyOfEntryName, COUNTOF(szCopyOfEntryName), szEntry))
        RetVal = ERROR_INVALID_NAME;
    else if (szCopyOfEntryName[0] == TEXT('\0'))
    {
        // It's ok to pass in a null string
        // you then get an initialized entry.
    }
    else
    {
        RetVal = RaspCheckEntryName(szCopyOfEntryName);
    }

    // Did they just want the size of a RasEntry?
    if ((0 == RetVal) && (NULL == lpbEntry) && (*lpdwEntrySize == 0))
    {
        *lpdwEntrySize = sizeof(RASENTRY);
        DEBUGMSG(ZONE_RAS, (L"PPP: -AfdRasGetEntryProperties Returning sizeof(RASENTRY)\n"));
        return ERROR_BUFFER_TOO_SMALL;
    }

    // Check entry and size
    if (NULL == lpbEntry)
    {
        RetVal = ERROR_BUFFER_INVALID;
    }
    else if (((LPRASENTRY)lpbEntry)->dwSize != sizeof(RASENTRY_V3)
         &&  ((LPRASENTRY)lpbEntry)->dwSize != sizeof(RASENTRY))
    {
        // Not one of the two supported versions
        RetVal = ERROR_INVALID_SIZE;
    }
    else if (*lpdwEntrySize < ((LPRASENTRY)lpbEntry)->dwSize)
    {
        // Request that they provide a buffer sufficiently
        // big to store the data.
        *lpdwEntrySize = ((LPRASENTRY)lpbEntry)->dwSize;
        RetVal = ERROR_BUFFER_TOO_SMALL;
    }

    // Any parm errors?
    if (0 != RetVal)
    {
        DEBUGMSG(ZONE_RAS, (L"-AfdRasGetEntryProperties ParmError %d\n", RetVal));
        return RetVal;
    }

    // Shortcut to the return buffer
    pEntry = (LPRASENTRY)lpbEntry;

    if (TEXT('\0') == szCopyOfEntryName[0])
    {
        // Return an initialized RasEntry structure.
        RaspInitRasEntry(pEntry);
        DEBUGMSG(ZONE_RAS, (L"-AfdRasGetEntryProperties Returning Default Entry\n"));
        return 0;
    }

    RetVal = OpenRasEntryKey(lpszPhonebook, szCopyOfEntryName, &hKey);
    if (ERROR_SUCCESS == RetVal)
    {
        // Get the Entry data.
        dwSize = sizeof(RASPENTRY);
        hRes = RegQueryValueEx(hKey, RASBOOK_ENTRY_VALUE, 0, &dwType, (LPBYTE)&RaspEntry, &dwSize);
        if (ERROR_SUCCESS != hRes)
        {
            RetVal = ERROR_CANNOT_FIND_PHONEBOOK_ENTRY;
        }
        else if ((dwType != REG_BINARY) || (sizeof(RASPENTRY) != dwSize))
        {
            // What's the proper return code when the value is bad?
            RetVal = ERROR_CORRUPT_PHONEBOOK;
        }
        else
        {
            // Convert the private struct to the public one.
            RaspConvPrivPublic(&RaspEntry, pEntry);
        }

        // Did they want the DevConfig?
        if (lpdwSize != NULL) {
            hRes = RegQueryValueEx (hKey, RASBOOK_DEVCFG_VALUE, 0, &dwType, NULL, &dwSize);
            if (ERROR_SUCCESS == hRes) {
                // Do they just want the size?
                if ((lpb == NULL) || (*lpdwSize < dwSize)) {
                    RetVal = ERROR_BUFFER_TOO_SMALL;
                } else {
                    // Must have passed in a buffer, and it's big enough
                    hRes = RegQueryValueEx(hKey, RASBOOK_DEVCFG_VALUE, 0, &dwType, lpb, &dwSize);
                    if (hRes == ERROR_SUCCESS && !_tcscmp (RaspEntry.szDeviceType, RASDT_Vpn))
                    {
                        // For VPN type, the data is stored encrypted, so we have to decrypt it now.
                        BOOL fRet;
                        DATA_BLOB dataIn, dataOut;

                        dataIn.cbData = dwSize;
                        dataIn.pbData = lpb;
                        dataOut.cbData = 0;
                        dataOut.pbData = NULL;
                        fRet = CryptUnprotectData(&dataIn, NULL, NULL, NULL, NULL, CRYPTPROTECT_PRIVATE, &dataOut);
                        if (fRet)
                        {
                            ASSERT(dataOut.cbData <= dwSize);
                            if (dataOut.cbData <= dwSize)
                            {
                                memcpy(lpb, dataOut.pbData, dataOut.cbData);
                            }
                            else
                                RetVal = ERROR_BUFFER_TOO_SMALL;        // this should never happen
                            dwSize = dataOut.cbData;
                            LocalFree(dataOut.pbData);
                        }
                        else
                        {
                            RetVal = ERROR_CORRUPT_PHONEBOOK;
                            hRes = GetLastError();
                        }
                    }

                }
                
                // Return the correct size.
                *lpdwSize = dwSize;
            } else {
                // No dev config for this entry, Let's try to ask the miniport
                PNDISWAN_ADAPTER    pAdapter;
                DWORD               dwDeviceID;
                LPBYTE              pDeviceConfig;
                DWORD               dwDevCfgSize;

                if (SUCCESS == FindAdapter(pEntry->szDeviceName, pEntry->szDeviceType, &pAdapter, &dwDeviceID))
                {
                    if (SUCCESS == NdisTapiGetDevConfig(pAdapter, dwDeviceID, &pDeviceConfig, &dwDevCfgSize))
                    {
                        // Ok it worked.  Now what?
                        if ((NULL != lpb) && (*lpdwSize >= dwDevCfgSize))
                        {
                            memcpy ((LPBYTE)lpb, pDeviceConfig, dwDevCfgSize);
                        }
                        // Tell them how big it is
                        *lpdwSize = dwDevCfgSize;
                        LocalFree (pDeviceConfig);
                    } else {
                        // Initialize to none
                        *lpdwSize = 0;
                    }
                    AdapterDelRef (pAdapter);
                    
                } else {
                    // Initialize to none
                    *lpdwSize = 0;
                }
                
            }
        }
        RegCloseKey (hKey);
    } else {
        // Could not open the key
        RetVal = ERROR_CANNOT_FIND_PHONEBOOK_ENTRY;
    }

    DEBUGMSG (ZONE_RAS || (RetVal && ZONE_ERROR),
              (TEXT("-AfdRasGetEntryProperties Return Code %d\r\n"), RetVal));
    return RetVal;
}

/*****************************************************************************
*
*
*   @func   DWORD   |   RasValidateEntryName    |
*       The RasValidateEntryName function validates the format of
*       a connection entry name.  It must contain at least one
*       non-white-space alpha-numeric character.
*
*   @rdesc
*       ERROR_SUCCESS - name is valid and not in use in phonebook
*       ERROR_ALREADY_EXISTS - name is valid and already in phonebook
*       ERROR_INVALID_NAME - name is invalid
*       ERROR_CANNOT_OPEN_PHONEBOOK - (CE) lpszPhonebook is non-NULL
*
*   @parm   LPTSTR  |   lpszPhonebook   |
*       Points to a null terminated string that specifies the full
*       path and filename of the phonebook file.  This parameter
*       can be NULL in which case the default phonebook file is
*       used.  This parameter should always be NULL for Windows 95 and
*       WinCE since the phonebook entries are stored in the registry.
*   @parm   LPTSTR  |   lpszEntry   |
*       Points to a null terminated string containing a entry name. For
*       WinCE the Entry name must include at least one alpha-numeric
*       character and be a valid registry key name.
*
*/
DWORD APIENTRY
AfdRasValidateEntryName(
    IN  LPCTSTR lpszPhonebook,
    IN  LPCTSTR lpszEntry)
{
    HKEY    hKey;
    DWORD   RetVal;

    DEBUGMSG(ZONE_RAS, (L"PPP: +RasValidateEntryName %s\n", lpszEntry));

    RetVal = OpenRasEntryKey((LPTSTR)lpszPhonebook, (LPTSTR)lpszEntry, &hKey);
    if (ERROR_SUCCESS == RetVal)
    {
        RetVal = ERROR_ALREADY_EXISTS;
        RegCloseKey (hKey);
    }
    else if (ERROR_CANNOT_FIND_PHONEBOOK_ENTRY == RetVal)
    {
        RetVal = ERROR_SUCCESS;
    }

    DEBUGMSG(ZONE_RAS, (L"PPP: -RasValidateEntryName %s Result=%d\n", lpszEntry, RetVal));
    return RetVal;
}

/*****************************************************************************
*
*
*   @func   DWORD   |   RasDeleteEntry |
*       The RasDeleteEntry function deletes an entry from the phone book.
*
*   @rdesc  
*       If the function succeeds, the return value is zero.
*       If the function fails, the return value is one of the following:
*       @ecode  ERROR_CANNOT_OPEN_PHONEBOOK |
*           Invalid phonebook entry specified.  For Win95 and WinCE the
*           lpszPhonebook parameter must be NULL.   
*       @ecode  ERROR_INVALID_NAME |
*           Invalid name specified in lpszEntry parameter.
*       @ecode  Others |
*           Errors returned from RegDeleteKey()
*
*   @parm   LPTSTR  |   lpszPhonebook   |
*       Points to a null terminated string that specifies the
*       full path and filename of the phonebook file.  This
*       parameter can be NULL, in which case the default
*       phonebook file is used.  This parameter should always
*       be NULL for Windows 95 and WinCE since the phonebook
*       entries are stored in the registry.
*   @PARM   LPTSTR  |   lpszEntry       |
*       Points to a null terminated string containing an
*       existing entry name that will be deleted.
*
*/
DWORD WINAPI 
AfdRasDeleteEntry(
    IN  LPCTSTR lpszPhonebook,
    IN  LPCTSTR lpszEntry)
{
    TCHAR   KeyName[128];
    DWORD   RetVal = 0;
    WCHAR   szCopyOfEntryName[ RAS_MaxEntryName + 1 ];

    DEBUGMSG(ZONE_RAS, (L"PPP: RasDeleteEntry %s\n", lpszEntry));

    if (NULL != lpszPhonebook) 
    {
        // We only support the system/registry phonebook.
        RetVal = ERROR_CANNOT_OPEN_PHONEBOOK;
    }
    else
    {
        RetVal = safeCopyEntryName(szCopyOfEntryName, lpszEntry);
        if (ERROR_SUCCESS == RetVal)
        {
            // Build the key name.
            StringCchPrintfW(KeyName, COUNTOF(KeyName), L"%s\\%s", RASBOOK_KEY, szCopyOfEntryName);
     
            // Now try to delete the key and all of its values.
            RetVal = RegDeleteKey (HKEY_CURRENT_USER, KeyName);
        }
    }

    DEBUGMSG(ZONE_RAS, (L"PPP: RasDeleteEntry %s Result=%d\n", lpszEntry, RetVal));
    return RetVal;
}

/*****************************************************************************
*
*
*   @func   DWORD   |   RasRenameEntry  |
*       The RasRenameEntry function changes the name of an entry in
*       the phonebook.
*
*   @rdesc
*       If the function succeeds, the return value is zero.
*       If the function fails, the return value is
*       ERROR_INVALID_NAME, ERROR_ALREADY_EXISTS, or
*       ERROR_CANNOT_FIND_PHONEBOOK_ENTRY.
*
*   @parm   LPTSTR    |   szPhonebook   | PhoneBook name.  Must be NULL.
*   @parm   LPTSTR    |   szOldEntry    | Old entry name.
*   @parm   LPTSTR    |   szNewEntry    | New entry name.
*
*
*/
DWORD  WINAPI
AfdRasRenameEntry(
    IN LPCTSTR lpszPhonebook,
    IN LPCTSTR lpszOldEntry,
    IN LPCTSTR lpszNewEntry)
{
    HKEY           hKeyRasBook;
    DWORD          RetVal;
    PPPP_SESSION   pSession;
    RASDIALPARAMS  dialParams;
    BOOL           bMoveCredentials;
    WCHAR          szOldEntryName[ RAS_MaxEntryName + 1 ];
    WCHAR          szNewEntryName[ RAS_MaxEntryName + 1 ];

    DEBUGMSG(ZONE_RAS, (L"PPP: RasRenameEntry %s --> %s\n", lpszOldEntry, lpszNewEntry));

    //
    // First, copy and check parameters
    //
    RetVal = safeCopyEntryName(szOldEntryName, lpszOldEntry);
    if (ERROR_SUCCESS == RetVal)
        RetVal = safeCopyEntryName(szNewEntryName, lpszNewEntry);
 
    if (RetVal == ERROR_SUCCESS)
    {
        RetVal = RegOpenKeyEx(HKEY_CURRENT_USER, RASBOOK_KEY, 0, 0, &hKeyRasBook);
        if (RetVal == ERROR_SUCCESS)
        {
            //
            // We only need to move the credman credentials for the entry
            // if the szEntryName is changing, and credman ignores case?
            //
            bMoveCredentials = (0 != wcsicmp(szOldEntryName, szNewEntryName));
            if (bMoveCredentials)
            {
                wcscpy(dialParams.szEntryName, szOldEntryName);
                RetVal = RasCredentialsRead(&dialParams, FALSE, NULL);
                if (ERROR_NOT_FOUND == RetVal)
                {
                    // No credentials are associated with this connectoid currently
                    RetVal = ERROR_SUCCESS;
                    bMoveCredentials = FALSE;
                }
            }
            if (ERROR_SUCCESS == RetVal)
            {
                // Copy the credman credentials to the new name
                if (bMoveCredentials)
                {
                    wcscpy(dialParams.szEntryName, szNewEntryName);
                    RetVal = RasCredentialsWrite(&dialParams, FALSE);
                }
                if (ERROR_SUCCESS == RetVal)
                {
                    RetVal = RegRenameKey(hKeyRasBook, szOldEntryName, szNewEntryName);
                    if (ERROR_SUCCESS == RetVal)
                    {
                        wcscpy(dialParams.szEntryName, szOldEntryName);
                    }
                    if (bMoveCredentials)
                    {
                        // Delete the old (or new if RegRenameKeyFailed) credman created credentials
                        if (ERROR_SUCCESS != RasCredentialsDelete(&dialParams))
                        {
                            RetVal = ERROR_INTERNAL_ERROR;
                        }
                    }
                }
            }
            SecureZeroMemory(&dialParams, sizeof(dialParams));
            RegCloseKey(hKeyRasBook);
        }
    }

    if( ERROR_SUCCESS == RetVal )
    {
        EnterCriticalSection (&v_ListCS);

        // Loop through the currently active sessions to update the dynamic linked list with the new name.
        for( pSession = g_PPPSessionList;
             pSession;
             pSession = pSession->Next )
        {
            //
            //  Do not count null sessions (is this possible?) or server sessions
            //
            if(PPPADDREF(pSession, REF_RAS_RENAME))
            {
                //
                //  Do not rename server sessions
                //
                if( !pSession->bIsServer )
                {
                    // Check for the old entry whose name is to be replaced.
                    if (lpszOldEntry
                    &&  0 == _tcsnicmp(pSession->rasDialParams.szEntryName, szOldEntryName, RAS_MaxEntryName))
                    {
                        _tcsncpy(pSession->rasDialParams.szEntryName, szNewEntryName, RAS_MaxEntryName);
                        pSession->rasDialParams.szEntryName[RAS_MaxEntryName] = _T('\0');
                        PPPDELREF( pSession, REF_RAS_RENAME);
                        break;
                    }
                }

                PPPDELREF( pSession, REF_RAS_RENAME);
            }
        }

        LeaveCriticalSection (&v_ListCS);
    }

    DEBUGMSG( ZONE_RAS | (ZONE_ERROR && RetVal), (TEXT("PPP: -AfdRasRenameEntry Returning %d\n"), RetVal));
    return RetVal;
}

static BOOL
IsEncryptedRasEntry(
    IN  HKEY hKey)
//
//  VPN dev configs are currently encrypted. See AfdRasGetEntryProperties.
//  Return TRUE if the type of entry at hKey is RASDT_Vpn
//
{
    RASPENTRY RaspEntry;
    DWORD     dwSize, dwType;
    LONG      hRes;

    dwSize = sizeof(RASPENTRY);
    hRes = RegQueryValueEx(hKey, RASBOOK_ENTRY_VALUE, 0, &dwType, (LPBYTE)&RaspEntry, &dwSize);
    if (ERROR_SUCCESS == hRes
    &&  dwSize == sizeof(RaspEntry)
    &&  (0 == _tcscmp(RaspEntry.szDeviceType, RASDT_Vpn)))
    {
        return TRUE;
    }
    return FALSE;
}

/*****************************************************************************
* 
*   @func   DWORD | RasGetEntryDevConfig |  
*       This function retrieves the device information saved by the last
*       successful call to the <f RasSetEntryDevConfig>
*
*   @rdesc  If the function succeeds, the return value is zero.
*           If the function fails, the return value will be set to the
*           error code.
*   @ecode  ERROR_CANNOT_OPEN_PHONEBOOK | Invalid szPhonebook
*   @ecode  ERROR_CANNOT_FIND_PHONEBOOK_ENTRY | Invalid szEntry
*   @ecode  ERROR_INVALID_NAME | Invalid szEntry
*   @ecode  ERROR_INVALID_PARAMETER | Invalid pdwDeviceID or
*               pdwSize parameter.
*   @ecode  ERROR_BUFFER_TOO_SMALL | Specified buffer was too small.

*   @parm   LPCTSTR | lpszPhoneBook     |
*       Points to a null terminated string that specifies the full path and
*       filename of the phonebook file.  This parameter can be NULL, in which
*       case the default phonebook file is used.  This parameter should always
*       be NULL for Windows 95 and WinCE since the phonebook entries are
*       stored in the registry.
*   @parm   LPCTSTR |   szEntry         |
*       Points to a null terminated string containing an existing entry name.
*   @parm   LPDWORD |   pdwDeviceID     |
*       Returns the saved dwDeviceID.
*   @parm   LPDWORD |   pdwSize         |
*       Size of the pDeviceConfig structure returned.  This should be
*       filled with the size required.
*   @parm   LPVARSTRING | pDeviceConfig |
*       Specifies a pointer to the memory location of the type <f VARSTRING>
*       where the device configuration structure is returned.  If NULL then
*       only the correct size is returned.
*               
*   @ex An example of how to use this function follows |
*
*       DWORD   dwDeviceID;
*       LPVARSTRING pDeviceConfig;
*       DWORD   dwSize;
*
*       dwSize = 0;
*       if (RasGetEntryDeviceConfig (NULL, EntryName, &dwDeviceID,
*               &dwSize, NULL)) {
*           DEBUGMSG (1, (TEXT("Error getting device info size\r\n")));
*           return;
*       }
*       pDeviceConfig = LocalAlloc (LPTR, dwSize);
*       if (RasGetEntryDeviceConfig (NULL, EntryName, &dwDeviceID,
*               &dwSize, pdwDeviceConfig)) {
*           DEBUGMSG (1, (TEXT("Error getting device info\r\n")));
*           return;
*       } else {
*           DEBUGMSG (1, (TEXT("dwDeviceID=%d\r\n"), dwDeviceID));
*           if (pDeviceConfig) {
*               DEBUGMSG (1, (TEXT("Have a device config @ 0x%X\r\n"),
*                   pDeviceConfig));
*               LocalFree(pDeviceConfig);
*           }
*       }
*
*/

DWORD APIENTRY
AfdRasGetEntryDevConfig(
    IN      LPCTSTR     szPhonebook,
    IN      LPCTSTR     szEntry,
        OUT LPDWORD     pdwDeviceID,
    IN  OUT LPDWORD     pdwSize,
    IN  OUT LPVARSTRING pDeviceConfig,
    IN      DWORD       cbDeviceConfig)
{
    HKEY    hKey;
    LONG    hRes;
    DWORD   RetVal;
    DWORD   dwType;
    DWORD   dwSize;

    DEBUGMSG(ZONE_RAS | ZONE_FUNCTION,
             (TEXT( "+RasGetEntryDevConfig( 0x%08X(%s), 0x%08X(%s), ")
              TEXT("0x%08X, 0x%X(%d), 0x%08X)\r\n"), 
              szPhonebook, (szPhonebook != NULL) ? szPhonebook : TEXT("NULL"),
              szEntry, (szEntry != NULL) ? szEntry : TEXT("NULL"),
              pdwDeviceID, pdwSize, (pdwSize != NULL) ? *pdwSize : 0,
              pDeviceConfig) );

    DEBUGMSG (1, (TEXT("RasGetEntryDevConfig is obsolete, use RasGetEntryProperties instead\r\n")));
    
    if (NULL == pdwSize)
    {
        DEBUGMSG (ZONE_RAS|ZONE_ERROR, (TEXT("-RasGetEntryDevConfig Invalid Parameter\n")));
        return ERROR_INVALID_PARAMETER;
    }

    if (pdwDeviceID)
    {
        // We no longer use this
        *pdwDeviceID = 0;
    }
     
    RetVal = OpenRasEntryKey(szPhonebook, szEntry, &hKey);
    if (ERROR_SUCCESS == RetVal)
    {
        if (IsEncryptedRasEntry(hKey))
        {
            // Rather than duplicate the encryption/decryption code in this deprecated API
            // just  error out if the device type is VPN
            RetVal = ERROR_UNKNOWN_DEVICE_TYPE;
        }
        else
        {
            hRes = RegQueryValueEx (hKey, RASBOOK_DEVCFG_VALUE, 0, &dwType,
                                    NULL, &dwSize);
            if (ERROR_SUCCESS == hRes) {
                DEBUGMSG (ZONE_RAS|ZONE_ERROR,
                        (TEXT(" RasGetEntryDevConfig RasQuery Success Size=%d\r\n"),
                        dwSize));
                if (NULL == pDeviceConfig) {
                    // They just want the length...
                    *pdwSize = dwSize + sizeof(VARSTRING);
                } else {
                    if (cbDeviceConfig >= (dwSize + sizeof(VARSTRING))) {
                        // Set up their VARstring data.
                        pDeviceConfig->dwTotalSize = *pdwSize;
                        pDeviceConfig->dwNeededSize = dwSize + sizeof(VARSTRING);
                        pDeviceConfig->dwUsedSize = dwSize + sizeof(VARSTRING);
                        pDeviceConfig->dwStringFormat = 0;
                        pDeviceConfig->dwStringSize = dwSize;
                        pDeviceConfig->dwStringOffset = sizeof(VARSTRING);
                        
                        //
                        // If the user supplied location to read the dev config from is bad,
                        // then  RegQueryValueEx will catch the exception.
                        //
                        hRes = RegQueryValueEx (hKey, RASBOOK_DEVCFG_VALUE, 0,
                                                &dwType,
                                                (LPBYTE)pDeviceConfig + pDeviceConfig->dwStringOffset,
                                                &dwSize);
                    } else {
                        RetVal = ERROR_BUFFER_TOO_SMALL;
                    }
                }
            } else {
                DEBUGMSG(ZONE_ERROR, (TEXT(" RasGetEntryDevConfig Error from RegQuery %d\r\n"), hRes));
                RetVal = ERROR_CANNOT_FIND_PHONEBOOK_ENTRY;
            }
        }
        RegCloseKey (hKey);
    }

    DEBUGMSG (ZONE_RAS, (TEXT("-RasGetEntryDevConfig %d\r\n"), RetVal));
    return RetVal;
}

/*****************************************************************************
* 
*   @func   DWORD | RasSetEntryDevConfig |  
*       This function saves the device information used with a Ras Entry.
*
*   @rdesc  If the function succeeds, the return value is zero.
*           If the function fails, the return value will be set to the
*           error code.
*   @ecode  ERROR_CANNOT_OPEN_PHONEBOOK | Invalid szPhonebook
*   @ecode  ERROR_CANNOT_FIND_PHONEBOOK_ENTRY | Invalid szEntry
*   @ecode  ERROR_INVALID_PARAMETER | Invalid pDeviceConfig parameter
*   
*   @parm   LPCTSTR | lpszPhoneBook     |
*       Points to a null terminated string that specifies the full path and
*       filename of the phonebook file.  This parameter can be NULL, in which
*       case the default phonebook file is used.  This parameter should always
*       be NULL for Windows 95 and WinCE since the phonebook entries are
*       stored in the registry.
*   @parm   LPCTSTR |   szEntry         |
*       Points to a null terminated string containing an existing entry name.
*   @parm   DWORD   |   dwDeviceID     |
*       The TAPI device ID to save.
*   @parm   LPVARSTRING | lpDeviceConfig |
*       Specifies a pointer to the memory location of the type <f VARSTRING>
*       of the device configuration structure.  Prior to calling the
*       application should set the dwNeededSize field of this structure to
*       indicate the amount of memory required for the information.
*       This parameter can be null if the default device config for the device
*       is to be used.
*               
*/

DWORD APIENTRY
AfdRasSetEntryDevConfig (
    IN  LPCTSTR     szPhonebook, 
    IN  LPCTSTR     szEntry,
    IN  DWORD       dwDeviceID, 
    IN  LPVARSTRING pDeviceConfig,
    IN  DWORD       cbDeviceConfig)
{
    HKEY    hKey;
    DWORD   RetVal;

    DEBUGMSG(ZONE_RAS | ZONE_FUNCTION,
             (TEXT( "+RasSetEntryDevConfig( 0x%08X(%s), 0x%08X(%s), ")
              TEXT("%d, 0x%08X)\r\n"), 
              szPhonebook, (szPhonebook != NULL) ? szPhonebook : TEXT("NULL"),
              szEntry, (szEntry != NULL) ? szEntry : TEXT("NULL"),
              dwDeviceID,  pDeviceConfig));

    DEBUGMSG (1, (TEXT("RasSetEntryDevConfig is obsolete, use RasSetEntryProperties instead\r\n")));

    if (pDeviceConfig && pDeviceConfig->dwNeededSize < sizeof(VARSTRING))
    {
        DEBUGMSG(ZONE_RAS, (TEXT("-RasSetEntryDevConfig DeviceConfig too small\n")));
        return ERROR_INVALID_PARAMETER;
    }

    // We require the key to be created already
    RetVal = OpenRasEntryKey(szPhonebook, szEntry, &hKey);
    if (ERROR_SUCCESS == RetVal)
    {
        if (IsEncryptedRasEntry(hKey))
        {
            // Rather than duplicate the encryption/decryption code in this deprecated API
            // just  error out if the device type is VPN
            RetVal = ERROR_UNKNOWN_DEVICE_TYPE;
        }
        else
        {
            if (pDeviceConfig)
            {
                //
                // If the user supplied location to read the dev config from is bad,
                // then  RegSetValueEx will catch the exception.
                //
                RetVal = RegSetValueEx(hKey, RASBOOK_DEVCFG_VALUE, 0, REG_BINARY,
                                    (LPBYTE)pDeviceConfig + pDeviceConfig->dwStringOffset,
                                    pDeviceConfig->dwStringSize);
            }
            else
            {
                // If the delete fails because the value is not present,
                // that's not an error.
                (void) RegDeleteValue (hKey, RASBOOK_DEVCFG_VALUE);
            }
        }
        RegCloseKey (hKey);
    }
    DEBUGMSG (ZONE_RAS, (TEXT("-RasSetEntryDevConfig Returning %d\n"), RetVal));
    return RetVal;
}

// ----------------------------------------------------------------
//
// Private Functions
//
// @doc INTERNAL
//
// ----------------------------------------------------------------


/*****************************************************************************
*
*
*   DWORD   RaspCheckEntryName(LPTSTR lpszEntry)
*
*   Parameters:
*       lpszEntry   :   Entry name to check
*       
*   Returns:
*       ERROR_SUCCESS if OK, else ERROR_INVALID_NAME.
*
*   Description:
*       To be valid the entry name:
*           1. Must not be NULL
*           2. Must not be length 0
*           3. Must not be too long (> RAS_MaxEntryName characters)
*           3. Must not contain any 'control' characters (< 0x20)
*           4. Must not contain any ASCII punctuation character
*           5. Must contain at least one alphanumeric character
*
*/
DWORD
RaspCheckEntryName(LPCTSTR lpszEntry)
{
    DWORD   dwResult = ERROR_INVALID_NAME;

    DEBUGMSG(ZONE_FUNCTION, (TEXT("+RaspCheckEntryName( %s )\n"), lpszEntry ? lpszEntry : TEXT("NULL")));
    if (lpszEntry != NULL)
    {
        if (ValidateStringLength(lpszEntry, 1, RAS_MaxEntryName))
        {
            // Scan the characters in the name
            for (; TRUE; lpszEntry++)
            {
                if (*lpszEntry == TEXT('\0'))
                {
                    // Reached the end with no bad characters
                    break;
                }
                if (*lpszEntry < 0x20 || _tcschr(TEXT("*\\/?><:\"|"), *lpszEntry))
                {
                    // Bad character
                    dwResult = ERROR_INVALID_NAME;
                    DEBUGMSG(ZONE_ERROR, (TEXT(" RaspCheckEntryName: ERROR - bad char %x\n"), *lpszEntry));
                    break;
                }
                if (IsCharAlphaNumeric(*lpszEntry))
                {
                    // Barring a bad character, this is an ok name
                    dwResult = ERROR_SUCCESS;
                }
            }
        }
    }
    DEBUGMSG(ZONE_FUNCTION | (ZONE_ERROR && dwResult), (TEXT("-RaspCheckEntryName(%s) result=%u\n"), lpszEntry, dwResult));
    return dwResult;
}

/*****************************************************************************
*
*
*   VOID    RaspInitRasEntry(LPRASENTRY pRasEntry)
*
*   Parameters:
*       pRasEntry   :   Pointer to RASENTRY struct to initialize
*       
*   Returns:
*       None.
*
*/

VOID  

RaspInitRasEntry( LPRASENTRY pEntry )
{
    RASDEVINFO  RasDevInfo;
    DWORD       cBytes;
    DWORD       cNumDev=0;
    DWORD       dwRetVal;

    DEBUGMSG(ZONE_FUNCTION, (TEXT("+RaspInitRasEntry( %x )\n"), pEntry));
#ifdef TODO
    Review these changes.  There should probably be a better way to determine
    the default device name/type.  Perhaps it should be based on the last one
    used?
#endif
    
    ASSERT(pEntry->dwSize == sizeof(RASENTRY_V3) || pEntry->dwSize == sizeof(RASENTRY));
	if (pEntry->dwSize-sizeof(DWORD) > pEntry->dwSize){ //no underflow
	    DEBUGMSG(ZONE_ERROR, (TEXT("-Data underflow\n")));
		ASSERT(FALSE);
	}

    memset( (char *)&pEntry->dwfOptions, 0, pEntry->dwSize - sizeof(DWORD) );

    pEntry->dwfOptions = RASEO_IpHeaderCompression  |
                         RASEO_SwCompression |
                         RASEO_ProhibitEAP
                         ;

    pEntry->dwfNetProtocols   = RASNP_Ip;
    pEntry->dwFramingProtocol = RASFP_Ppp;
    _tcscpy( pEntry->szDeviceType, RASDT_Direct );

    RasDevInfo.dwSize = sizeof(RASDEVINFO);
    cBytes = sizeof(RasDevInfo);

    // Look for first device of type NULL_MODEM
    dwRetVal = pppMac_EnumDevices (&RasDevInfo, TRUE, &cBytes, &cNumDev);

    if ((dwRetVal == SUCCESS) && (cNumDev > 0)) {
        ASSERT(!_tcscmp (RasDevInfo.szDeviceType, RASDT_Direct));
        _tcscpy (pEntry->szDeviceName, RasDevInfo.szDeviceName);
    }
    
    if (pEntry->szDeviceName[0] == '\0') {
        DEBUGMSG (ZONE_ERROR,
                  (TEXT("Unable to set default device name\r\n")));
    }

    DEBUGMSG(ZONE_FUNCTION, (TEXT("-RaspInitRasEntry\n")));
}

/*****************************************************************************
*
*
*   VOID    RaspConvPrivPublic(LPRASPENTRY pRaspEntry, LPRASENTRY pRasEntry)
*
*   Parameters:
*       pRaspEntry  :   Pointer to RASPENTRY struct source
*       pRasEntry   :   Pointer to RASENTRY struct destination
*       
*   Returns:
*       None.
*
*/

VOID  
RaspConvPrivPublic(
    IN  LPRASPENTRY pRaspEntry,
    OUT LPRASENTRY pRasEntry)
{
    DEBUGMSG (ZONE_FUNCTION, (TEXT("+RaspConvPrivPublic %x %x\n"), pRaspEntry, pRasEntry));

    // First initialize the pRasEntry.

    RaspInitRasEntry (pRasEntry);

    // Move the data over.

    pRasEntry->dwfOptions = pRaspEntry->dwfOptions;
    pRasEntry->dwCountryID = pRaspEntry->dwCountryID;
    pRasEntry->dwCountryCode = pRaspEntry->dwCountryCode;
    pRasEntry->dwfNetProtocols = RASNP_Ip;
    pRasEntry->dwFramingProtocol = RASFP_Ppp;
    _tcscpy (pRasEntry->szAreaCode, pRaspEntry->szAreaCode);
    _tcscpy (pRasEntry->szLocalPhoneNumber, pRaspEntry->szLocalPhoneNumber);
    memcpy ((char *)&(pRasEntry->ipaddr), (char *)&(pRaspEntry->ipaddr),
            sizeof(RASIPADDR));
    memcpy ((char *)&(pRasEntry->ipaddrDns),
            (char *)&(pRaspEntry->ipaddrDns),
            sizeof(RASIPADDR));
    memcpy ((char *)&(pRasEntry->ipaddrDnsAlt),
            (char *)&(pRaspEntry->ipaddrDnsAlt),
            sizeof(RASIPADDR));
    memcpy ((char *)&(pRasEntry->ipaddrWins), 
            (char *)&(pRaspEntry->ipaddrWins),
            sizeof(RASIPADDR));
    memcpy ((char *)&(pRasEntry->ipaddrWinsAlt), 
            (char *)&(pRaspEntry->ipaddrWinsAlt),
            sizeof(RASIPADDR));
    pRasEntry->dwFrameSize = pRaspEntry->dwFrameSize;
    pRasEntry->dwFramingProtocol = pRaspEntry->dwFramingProtocol;
    _tcscpy (pRasEntry->szDeviceType, pRaspEntry->szDeviceType);

    if (pRasEntry->dwSize == sizeof(RASENTRY_V3))
    {
        LPRASENTRY_V3 pRasEntryV3 = (LPRASENTRY_V3)pRasEntry;

        _tcsncpy (pRasEntryV3->szDeviceName, pRaspEntry->szDeviceName, RAS_MaxDeviceName_V3);
        pRasEntryV3->szDeviceName[RAS_MaxDeviceName_V3] = 0;
        _tcscpy (pRasEntryV3->szScript, pRaspEntry->szScript);
    }
    else
    {
        ASSERT(pRasEntry->dwSize >= sizeof(RASENTRY));
        _tcscpy (pRasEntry->szDeviceName, pRaspEntry->szDeviceName);
        _tcscpy (pRasEntry->szScript, pRaspEntry->szScript);
        pRasEntry->dwCustomAuthKey = pRaspEntry->dwCustomAuthKey;
    }

    DEBUGMSG (ZONE_FUNCTION, (TEXT("-RaspConvPrivPublic \n")));
}
/*****************************************************************************
*
*
*   VOID    RaspConvPublicPriv(LPRASENTRY pRasEntry, LPRASPENTRY pRaspEntry)
*
*   Parameters:
*       pRasEntry   :   Pointer to RASENTRY struct source
*       pRaspEntry  :   Pointer to RASPENTRY struct destination
*       
*   Returns:
*       None.
*
*/
VOID  RaspConvPublicPriv(
    IN  LPRASENTRY pRasEntry,
    OUT LPRASPENTRY pRaspEntry)
{
    // Move the data over.

    pRaspEntry->dwfOptions = pRasEntry->dwfOptions;
    pRaspEntry->dwCountryID = pRasEntry->dwCountryID;
    pRaspEntry->dwCountryCode = pRasEntry->dwCountryCode;
    StringCchCopyW(pRaspEntry->szAreaCode, _countof(pRaspEntry->szAreaCode), pRasEntry->szAreaCode);
    StringCchCopyW(pRaspEntry->szLocalPhoneNumber, _countof(pRaspEntry->szLocalPhoneNumber), pRasEntry->szLocalPhoneNumber);
    memcpy ((char *)&(pRaspEntry->ipaddr), (char *)&(pRasEntry->ipaddr),
            sizeof(RASIPADDR));
    memcpy ((char *)&(pRaspEntry->ipaddrDns),
            (char *)&(pRasEntry->ipaddrDns),
            sizeof(RASIPADDR));
    memcpy ((char *)&(pRaspEntry->ipaddrDnsAlt),
            (char *)&(pRasEntry->ipaddrDnsAlt),
            sizeof(RASIPADDR));
    memcpy ((char *)&(pRaspEntry->ipaddrWins), 
            (char *)&(pRasEntry->ipaddrWins),
            sizeof(RASIPADDR));
    memcpy ((char *)&(pRaspEntry->ipaddrWinsAlt),
            (char *)&(pRasEntry->ipaddrWinsAlt),
            sizeof(RASIPADDR));
    pRaspEntry->dwFrameSize = pRasEntry->dwFrameSize;
    pRaspEntry->dwFramingProtocol = pRasEntry->dwFramingProtocol;
    StringCchCopyW(pRaspEntry->szDeviceType, _countof(pRaspEntry->szDeviceType), pRasEntry->szDeviceType);
    StringCchCopyW(pRaspEntry->szDeviceName, _countof(pRaspEntry->szDeviceName), pRasEntry->szDeviceName);

    if (pRasEntry->dwSize == sizeof(RASENTRY_V3))
    {
        LPRASENTRY_V3 pRasEntryV3 = (LPRASENTRY_V3)pRasEntry;

        StringCchCopyW(pRaspEntry->szScript, _countof(pRaspEntry->szScript), pRasEntryV3->szScript);
    }
    else
    {
        ASSERT(pRasEntry->dwSize >= sizeof(RASENTRY));

        StringCchCopyW(pRaspEntry->szScript, _countof(pRaspEntry->szScript), pRasEntry->szScript);
        pRaspEntry->dwCustomAuthKey = pRasEntry->dwCustomAuthKey;
    }
}
