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
#include "Disk_common.h"

//global vars common to all disk tests
HANDLE g_hDisk = INVALID_HANDLE_VALUE;
TCHAR g_szDiskName[MAX_PATH] = _T("");
TCHAR g_szProfile[PROFILENAMESIZE] = _T(""); 
DISK_INFO g_diskInfo = {0};
BOOL g_fOpenAsStore = FALSE;
BOOL g_fOpenAsPartition = FALSE;

#ifndef Debug
#define Debug NKDbgPrintfW
#endif

void UsageCommon(LPCTSTR szTestName,CKato* pKato)
{
    if (szTestName)
    {
        pKato->Log(LOG_DETAIL, TEXT("%s:"), szTestName);
    }
    pKato->Log(LOG_DETAIL, TEXT("       /disk <disk>        : name of the disk to test (e.g. DSK1:); default = first detected disk"));
    pKato->Log(LOG_DETAIL, TEXT("       /profile <profile>  : limit to devices of the specified storage profile; default = all profiles"));
    pKato->Log(LOG_DETAIL, TEXT("       /store              : open the disk using the OpenStore() API"));
    pKato->Log(LOG_DETAIL, TEXT("       /part               : open the disk using the OpenPartition() API"));
}

void ProcessCmdLineCommon(LPCTSTR szCmdLine,CKato* pKato )
{
    CClParse cmdLine(szCmdLine);

    if(!cmdLine.GetOptString(_T("disk"), g_szDiskName, MAX_PATH))
    {
        StringCchCopyEx(g_szDiskName, MAX_PATH, _T(""), NULL, NULL, STRSAFE_NULL_ON_FAILURE);
    }
    if(!cmdLine.GetOptString(_T("profile"), g_szProfile, PROFILENAMESIZE))
    {
        StringCchCopyEx(g_szProfile, PROFILENAMESIZE, _T(""), NULL, NULL, STRSAFE_NULL_ON_FAILURE);
    }
    
    g_fOpenAsStore = cmdLine.GetOpt(_T("store"));
    g_fOpenAsPartition = cmdLine.GetOpt(_T("part"));
    if (g_fOpenAsPartition)
    {
        g_fOpenAsStore = FALSE;
    }

    // a user specified disk name will override a user specified profile
    if(g_szDiskName[0])
    {
        pKato->Log(LOG_DETAIL, _T("DISK_COMMON: Disk Device Name = %s"), g_szDiskName);
    }
    else if(g_szProfile[0])
    {
        pKato->Log(LOG_DETAIL, _T("DISK_COMMON: Storage Profile = %s"), g_szProfile);
    }
     
    if(g_fOpenAsStore)
    {
        pKato->Log(LOG_DETAIL, _T("DISK_COMMON: Will open disk as a store (using OpenStore())"));
    }
    if(g_fOpenAsPartition)
    {
        pKato->Log(LOG_DETAIL, _T("DISK_COMMON: Will open disk as a partition (using OpenPartition())"));
    }
}