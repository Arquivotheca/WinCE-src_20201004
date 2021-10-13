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

#ifdef UNDER_CE

#ifndef __VIRTROOT_HPP__
#define __VIRTROOT_HPP__

static const WCHAR* SYSDIRNAME  = L"Windows";
static const DWORD SYSDIRLEN    = 7;

// Must be called prior to using any root file system functions (ROOTFS_xxx)
// in order to initialize the enumeration API set.
LRESULT InitializeVirtualRoot ();

BOOL IsPsuedoDeviceName (const WCHAR* pName);
BOOL IsConsoleName (const WCHAR* pName);
BOOL IsLegacyDeviceName (const WCHAR* pName);
BOOL IsFullDeviceName(const WCHAR* pName);
BOOL GetMapKeyNameFromDeviceName(const WCHAR* pName, __out_ecount(dwSizeInChars) WCHAR* strDeviceName, DWORD dwSizeInChars);
BOOL GetShortDeviceName(const WCHAR* pName, __out_ecount(dwSizeInChars) WCHAR* pShortName, DWORD dwSizeInChars);
BOOL IsSecondaryStreamName (const WCHAR* pName);
BOOL FileExistsInROM (const WCHAR* pName);

BOOL IsPathInSystemDir(LPCWSTR lpszPath);
BOOL IsPathInRootDir(LPCWSTR lpszPath);


// ROM File System APIs.
//
// These APIs access files on external ROM file systems in the virtual
// system directory, \windows.
//
DWORD ROM_GetFileAttributesW (const WCHAR* pFileName);
HANDLE ROM_CreateFileW (HANDLE hProcess, const WCHAR* pPathName,
    DWORD Access, DWORD ShareMode, SECURITY_ATTRIBUTES* pSecurityAttributes,
    DWORD Create, DWORD FlagsAndAttributes, HANDLE hTemplate);

// Root File System APIs.
//
// These APIs handle the special-case file enumeration performed on the
// merged root and virtual system directories.
//

static const DWORD EnumRoot         = 0x00000001;
static const DWORD EnumMounts       = 0x00000002;
static const DWORD EnumROMs         = 0x00000004;

struct RootFileEnumHandle {
    DWORD EnumFlags;
    HANDLE hInternalSearchContext;
    int MountIndex;
    ROMVolumeListNode* pNextROM;
    size_t SearchSpecChars;
    WCHAR SearchSpec[1];
};

HANDLE ROOTFS_FindFirstFileW(HANDLE hProcess, const WCHAR* pSearchSpec,
    WIN32_FIND_DATA* pFindData, DWORD SizeOfFindData);
BOOL ROOTFS_FindNextFileW (RootFileEnumHandle *pEnum, WIN32_FIND_DATA* pFindData);
BOOL ROOTFS_FindClose(RootFileEnumHandle *pEnum);

extern MountTable_t* g_pMountTable;

#endif // __VIRTROOT_HPP__

#endif /* UNDER_CE */
