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
#include <buildinfo.h>

HRESULT GetOSBuildInfo(PCEOSBUILDINFO pBuildInfo)
{
    BOOL fBuildInfoAvailable = FALSE;

    // set the branch information
    if (NULL != pBuildInfo)
    {
        HKEY hKey;

        pBuildInfo->szBranch[0] = L'\0';
        pBuildInfo->szParentBranchBuild[0] = L'\0';
        pBuildInfo->szTimeStamp[0] = L'\0';
        pBuildInfo->szBuilder[0] = L'\0';

        if (ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT("System\\Versions"), 0, 0, &hKey))
        {
            LPBYTE pData;
            DWORD cbData;
            LRESULT lResult;

            pData = (LPBYTE) pBuildInfo->szBranch;
            cbData = sizeof(pBuildInfo->szBranch);
            lResult = RegQueryValueEx(hKey, TEXT("Label"), NULL, NULL, pData, &cbData);
            fBuildInfoAvailable = (ERROR_SUCCESS == lResult);

            if (fBuildInfoAvailable) 
            {
                pData = (LPBYTE) pBuildInfo->szParentBranchBuild;
                cbData = sizeof(pBuildInfo->szParentBranchBuild);
                lResult = RegQueryValueEx(hKey, TEXT("ParentBranchBuild"), NULL, NULL, pData, &cbData);
                fBuildInfoAvailable = (fBuildInfoAvailable || (ERROR_SUCCESS == lResult));
            }    

            if (fBuildInfoAvailable) 
            {
                pData = (LPBYTE) pBuildInfo->szTimeStamp;
                cbData = sizeof(pBuildInfo->szTimeStamp);
                lResult = RegQueryValueEx(hKey, TEXT("TimeStamp"), NULL, NULL, pData, &cbData);
                fBuildInfoAvailable = (fBuildInfoAvailable || (ERROR_SUCCESS == lResult));
            }    


            if (fBuildInfoAvailable) 
            {
                pData = (LPBYTE) pBuildInfo->szBuilder;
                cbData = sizeof(pBuildInfo->szBuilder);
                lResult = RegQueryValueEx(hKey, TEXT("Builder"), NULL, NULL, pData, &cbData);
                fBuildInfoAvailable = (fBuildInfoAvailable || (ERROR_SUCCESS == lResult));
            }    

            RegCloseKey(hKey);
        }
    }

    return (fBuildInfoAvailable ? S_OK : E_FAIL);
}


HRESULT GetOSBuildString(LPWSTR szBuildString, size_t cchBuildString)
{
    HRESULT hr = E_FAIL;
    CEOSBUILDINFO bi;

    hr = GetOSBuildInfo(&bi);
    if (SUCCEEDED(hr))
    {
        if (wcscmp(bi.szBuilder, TEXT("Buildlab")) != 0) 
        {
            hr = StringCchPrintf(szBuildString, cchBuildString, TEXT("%s.%s(%s).%s"),
                             bi.szParentBranchBuild, bi.szBranch, bi.szBuilder, bi.szTimeStamp);
        } 
        else 
        {
            hr = StringCchPrintf(szBuildString, cchBuildString, TEXT("%s.%s.%s"),
                             bi.szParentBranchBuild, bi.szBranch, bi.szTimeStamp);
        }
            
    }

    return hr;
}
