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
#include <branchinfo.h>

BOOL GetBranchInformation(PBRANCHINFORMATION pbi)
{
    BOOL fBranchInfoAvailable = FALSE;

    // set the branch information
    if (NULL != pbi)
    {
        HKEY hKey;

        pbi->szBranch[0] = L'\0';
        pbi->szBuild[0] = L'\0';
        pbi->szTimeStamp[0] = L'\0';

        if (ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT("System\\Versions"), 0, 0, &hKey))
        {
            LPBYTE pData;
            DWORD cbData;
            LRESULT lResult;

            pData = (LPBYTE) pbi->szBranch;
            cbData = sizeof(pbi->szBranch);
            lResult = RegQueryValueEx(hKey, TEXT("Label"), NULL, NULL, pData, &cbData);
            fBranchInfoAvailable = (fBranchInfoAvailable || (ERROR_SUCCESS == lResult));

            pData = (LPBYTE) pbi->szBuild;
            cbData = sizeof(pbi->szBuild);
            lResult = RegQueryValueEx(hKey, TEXT("Build"), NULL, NULL, pData, &cbData);
            fBranchInfoAvailable = (fBranchInfoAvailable || (ERROR_SUCCESS == lResult));

            pData = (LPBYTE) pbi->szTimeStamp;
            cbData = sizeof(pbi->szTimeStamp);
            lResult = RegQueryValueEx(hKey, TEXT("TimeStamp"), NULL, NULL, pData, &cbData);
            fBranchInfoAvailable = (fBranchInfoAvailable || (ERROR_SUCCESS == lResult));

            RegCloseKey(hKey);
        }
    }

    return fBranchInfoAvailable;
}

