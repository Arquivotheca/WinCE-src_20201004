//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
// --------------------------------------------------------------------
//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
//                                                                     
// --------------------------------------------------------------------

#include "main.h"

#define MAX_RECURSION_DEPTH 25

// --------------------------------------------------------------------
BOOL CreateTestDirectory(LPTSTR szPath)
//
//  Create a directory, destroy it if it already exists
// --------------------------------------------------------------------
{
    ASSERT(szPath);

    if(!szPath || !szPath[0])
    {
        return FALSE;
    }

    // see if the directory exists by attempting to create it
    if(CreateDirectory(szPath, NULL))
    {
        return TRUE;
    }

    else if(ERROR_ALREADY_EXISTS != GetLastError())
    {
        return FALSE;
    }

    return RecursiveRemoveDirectory(szPath) && CreateDirectory(szPath, NULL);
}

// --------------------------------------------------------------------
BOOL RecursiveRemoveDirectory(LPTSTR szPath, UINT depth)
//
//  Remove a directory and all its contents
// --------------------------------------------------------------------
{
    TCHAR szFind[MAX_PATH];
    TCHAR szNewPath[MAX_PATH];
    HANDLE hFind = INVALID_HANDLE_VALUE;
    WIN32_FIND_DATA findData;
    DWORD dwErr = ERROR_SUCCESS;

    // see if the max recursion was hit; fail if so
    if(MAX_RECURSION_DEPTH <= depth)
    {
        SetLastError(ERROR_STACK_OVERFLOW);
        return FALSE;
    }
    
    if(!szPath || !szPath[0])
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    // see if there are any files in this directory
    _stprintf(szFind, _T("%s\\*"), szPath);

    // base case occurs when there are no more files
    hFind = FindFirstFile(szFind, &findData);
    
    dwErr = GetLastError();
    if(INVALID_HANDLE_VALUE == hFind)
    {
        if(ERROR_FILE_NOT_FOUND == dwErr || ERROR_PATH_NOT_FOUND == dwErr)
        {
            // directory must be empty
            return TRUE;
        }
        else
        {
            // some other error
            return FALSE;
        }
    }

    // remove each file in the directory
    else
    {
        do
        {
            _stprintf(szNewPath, _T("%s\\%s"), szPath, findData.cFileName);

            // if file is a sub-directory, recursively remove it
            if(FILE_ATTRIBUTE_DIRECTORY & findData.dwFileAttributes)
                RecursiveRemoveDirectory(szNewPath, depth+1);

            // otherwise, delete the file
            else
            {                
                if(!DeleteFile(szNewPath))
                    return FALSE;
            
                NKDbgPrintfW(_T("removed: %s\r\n"), szNewPath);
            }
            
        }
        while(FindNextFile(hFind, &findData));

        return RemoveDirectory(szPath);
    }   
}
