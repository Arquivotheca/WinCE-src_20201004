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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
////////////////////////////////////////////////////////////////////////////////
//
//  IMGFS_MountHidden TUX DLL
//
//  Module: test.cpp
//          Contains the LTK test function for ensuring correct visibility
//          of the IMGFS file system.
//
//  Revision History:
//  08/18/2005  a-jolenn    Created Test
//
////////////////////////////////////////////////////////////////////////////////

#include "main.h"
#include "globals.h"

#include <storemgr.h>


#define FILE_ATTRIBUTE_INVALID      0xFFFFFFFF
#define IMGFS_SYSTEM_KEYPATH        TEXT ("System\\StorageManager\\IMGFS")
#define IMGFS_PROFILE_KEYFMT        TEXT ("System\\StorageManager\\Profiles\\%s\\IMGFS")
#define IMGFS_FOLDER_VALUENAME      TEXT ("Folder")

#define IMGFS_DEFAULT_MOUNTPOINT    TEXT ("\\IMGFS")
#define IMGFS_PARTITION_TYPE        0x25



////////////////////////////////////////////////////////////////////////////////
// GetIMGFSPartitionProfile
//  Searches all partitions in all stores looking for an IMGFS partition
//
// Parameters:
//  [out] tcsProfile        - String to receive the profile name.
//  [in]  dwProfileChars    - Size (in characters) of the tcsProfile string.
//
// Return value:
//  TRUE if the patition was found and profile name copied into tcsProfile,
//  FALSE if no IMGFS partition could be found.
BOOL GetIMGFSPartitionProfile (LPTSTR tcsProfile, DWORD dwProfileChars)
{
    HANDLE hFindStore = INVALID_HANDLE_VALUE;
    STOREINFO stinfo;

    ZeroMemory (&stinfo, sizeof (stinfo));
    stinfo.cbSize = sizeof (stinfo);

    // Enumerate all of the STORE devices
    hFindStore = FindFirstStore (&stinfo);
    if (INVALID_HANDLE_VALUE != hFindStore)
    {
        do
        {
            HANDLE hFindPart = INVALID_HANDLE_VALUE;
            HANDLE hStore = INVALID_HANDLE_VALUE;
            PARTINFO ptinfo;

            ZeroMemory (&ptinfo, sizeof (ptinfo));
            ptinfo.cbSize = sizeof (ptinfo);

            hStore = OpenStore (stinfo.szDeviceName);
            if (INVALID_HANDLE_VALUE != hStore)
            {
                hFindPart = FindFirstPartition (hStore, &ptinfo);
                if (INVALID_HANDLE_VALUE != hFindPart)
                {
                    do
                    {
                        if (IMGFS_PARTITION_TYPE == ptinfo.bPartType)
                        {
                            g_pKato->Log (LOG_COMMENT, TEXT ("Found IMGFS partition as '%s' on device '%s'.  Profile='%s'"),
                                                                ptinfo.szPartitionName, stinfo.szDeviceName, stinfo.sdi.szProfile);

                            StringCchCopy (tcsProfile, dwProfileChars, stinfo.sdi.szProfile);

                            FindClosePartition (hFindPart);
                            CloseHandle (hStore);
                            FindCloseStore (hFindStore);

                            return TRUE;
                        }
                    }
                    while (FindNextPartition (hFindPart, &ptinfo));
                    FindClosePartition (hFindPart);
                }
                else
                {
                    g_pKato->Log (LOG_ABORT, TEXT ("ABORT: Unable to enumerate partitions for store device '%s'.  GLE=%lu"),
                                    stinfo.szDeviceName, GetLastError ());
                }

                CloseHandle (hStore);
            }
            else
            {
                g_pKato->Log (LOG_ABORT, TEXT ("ABORT: Unable to open store device '%s'.  GLE=%lu"), stinfo.szDeviceName, GetLastError ());
            }
        }
        while (FindNextStore (hFindStore, &stinfo));
        FindCloseStore (hFindStore);
    }
    else
    {
        g_pKato->Log (LOG_ABORT, TEXT ("ABORT: Unable to enumerate storage devices.  GLE=%lu"), GetLastError ());
    }

    return FALSE;
}


////////////////////////////////////////////////////////////////////////////////
// IMGFS_MountHidden
//  Tests if the IMGFS filesystem is visible to the OS.
//
// Parameters:
//  [in] uMsg          - Message code.
//  [in,out] tpParam   - Additional message-dependent data.
//  [in] lpFTE         - Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.

TESTPROCAPI IMGFS_MountHidden (UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    TCHAR tcsIMGFSMountPoint [MAX_PATH];
    TCHAR tcsTmpPath [MAX_PATH];
    DWORD dwTmpPathSize = 0;
    HANDLE hFind = INVALID_HANDLE_VALUE;
    WIN32_FIND_DATA w32fd;
    HKEY hKey = NULL;

    // The shell is asking us for the thread count.
    //   These test are to be run in the main thread
    if(uMsg == TPM_QUERY_THREAD_COUNT)
    {
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 0;
        return TPR_HANDLED;
    }

    ZeroMemory (tcsIMGFSMountPoint, sizeof (tcsIMGFSMountPoint));

    // #0: Get the profile string from the mounted IMGFS partition
    TCHAR tcsIMGFSProfile [PROFILENAMESIZE];
    if (!GetIMGFSPartitionProfile (tcsIMGFSProfile, PROFILENAMESIZE))
    {
        g_pKato->Log (LOG_PASS, TEXT ("No IMGFS partitions could be found on any storage device."));
        goto _exit_test;
    }

    // #1: Try the HKLM\System\StorageManager\Profiles\<ProfileName>\IMGFS key
    TCHAR tcsIMGFSProfileKey [MAX_PATH];

    StringCchPrintf (tcsIMGFSProfileKey, MAX_PATH, IMGFS_PROFILE_KEYFMT, tcsIMGFSProfile);
    if (ERROR_SUCCESS == RegOpenKeyEx (HKEY_LOCAL_MACHINE, tcsIMGFSProfileKey, 0, 0, &hKey))
    {
        ZeroMemory (tcsTmpPath, sizeof (tcsTmpPath));
        dwTmpPathSize = sizeof (tcsTmpPath);
        if (ERROR_SUCCESS == RegQueryValueEx (hKey, IMGFS_FOLDER_VALUENAME, NULL, NULL, (LPBYTE)tcsTmpPath, &dwTmpPathSize))
        {
            g_pKato->Log (LOG_COMMENT, TEXT ("Found IMGFS mount point using registry key 'HKLM\\%s\\%s'"),
                                            tcsIMGFSProfileKey, IMGFS_FOLDER_VALUENAME);
            StringCchPrintf (tcsIMGFSMountPoint, MAX_PATH, TEXT ("\\%s"), tcsTmpPath);
        }

        RegCloseKey (hKey);
    }

    // #2: If still no mount path - Try the HKLM\System\StorageManager\IMGFS key
    if (0 == *tcsIMGFSMountPoint)
    {
        if (ERROR_SUCCESS == RegOpenKeyEx (HKEY_LOCAL_MACHINE, IMGFS_SYSTEM_KEYPATH, NULL, NULL, &hKey))
        {
            ZeroMemory (tcsTmpPath, sizeof (tcsTmpPath));
            dwTmpPathSize = sizeof (tcsTmpPath);
            if (ERROR_SUCCESS == RegQueryValueEx (hKey, IMGFS_FOLDER_VALUENAME, NULL, NULL, (LPBYTE)tcsTmpPath, &dwTmpPathSize))
            {
                g_pKato->Log (LOG_COMMENT, TEXT ("Found IMGFS mount point using registry key 'HKLM\\%s\\%s'"),
                        IMGFS_SYSTEM_KEYPATH, IMGFS_FOLDER_VALUENAME);
                StringCchPrintf (tcsIMGFSMountPoint, MAX_PATH, TEXT ("\\%s"), tcsTmpPath);
            }

            RegCloseKey (hKey);
        }
    }

    // #3: If still no mount point - Use the default
    if (0 == *tcsIMGFSMountPoint)
    {
        // Use the default
        g_pKato->Log (LOG_COMMENT, TEXT ("Using default IMGFS mount point."));
        StringCchCopy (tcsIMGFSMountPoint, MAX_PATH, IMGFS_DEFAULT_MOUNTPOINT);
    }

    g_pKato->Log (LOG_COMMENT, TEXT ("Using '%s' as the IMGFS mount point."), tcsIMGFSMountPoint);

    // Make sure the mount point exists
    DWORD dwIMGFSAttrib = GetFileAttributes (tcsIMGFSMountPoint);
    if (FILE_ATTRIBUTE_INVALID == dwIMGFSAttrib)
    {
        g_pKato->Log (LOG_FAIL, TEXT ("FAIL: The IMGFS mount point does not exist!"));
        goto _exit_test;
    }

    // It should also be a directory
    if (!(FILE_ATTRIBUTE_DIRECTORY & dwIMGFSAttrib))
    {
        g_pKato->Log (LOG_FAIL, TEXT ("FAIL: The IMGFS mount point is not a directory!"));
        goto _exit_test;
    }

    // Enumerate the root directory and look for the IMGFS mount point
    hFind = FindFirstFile (TEXT ("\\*.*"), &w32fd);
    if (INVALID_HANDLE_VALUE == hFind)
    {
        g_pKato->Log (LOG_FAIL, TEXT ("FAIL: Unable to find any files/folders in '\\*.*'!"));
        goto _exit_test;
    }
    else
    {
        bool fFound = false;
        do
        {
            if (0 == _tcsicmp (w32fd.cFileName, tcsIMGFSMountPoint+1))
            {
                g_pKato->Log (LOG_FAIL, TEXT ("FAIL: The IMGFS mount point was found with the attributes 0x%08X"),
                                        w32fd.dwFileAttributes);

            fFound = true;
            }
        }
        while (FindNextFile (hFind, &w32fd));

        FindClose (hFind);

        if (!fFound)
        {
            g_pKato->Log (LOG_PASS, TEXT ("PASS: The IMGFS mount point is not visible."));
        }
    }
   
_exit_test:
    return GetTestResult (g_pKato);
}

////////////////////////////////////////////////////////////////////////////////
