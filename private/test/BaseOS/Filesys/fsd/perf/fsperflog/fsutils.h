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
#ifndef __FSUTILS_H__
#define __FSUTILS_H__

#include <windows.h>
#include <tchar.h>
#include <storehlp.h>

class CFSUtils
{
public:
    ~CFSUtils();

    DWORD GetDiskFreeSpace();

    BOOL CreateFragmentedTestFile(LPCTSTR pszFileName, DWORD dwFileSize);

    BOOL CreateTestFile(LPCTSTR pszFileName, DWORD dwFileSize, BOOL fFast);

    BOOL CreateTestFileSet(LPCTSTR pszRootDir, DWORD dwNumFiles, DWORD dwTotalFileSize);

    BOOL CreateTestFileSetNoWrite(LPCTSTR pszRootDir, DWORD dwNumFiles, DWORD dwCreationDispostion);

    BOOL IsValid() const;

    LPTSTR GetPath(__out_ecount(cbPathBufferLength) LPTSTR pszPathBuffer,
        DWORD cbPathBufferLength);

    LPTSTR GetFullPath(__out_ecount(cbPathBufferLength) LPTSTR pszPathBuffer,
        DWORD cbPathBufferLength,
        LPCTSTR pszPath);

    BOOL DeleteFile(LPCTSTR pszFileName);

    BOOL CreateDirectory(LPCTSTR pszDirName);

    BOOL CleanDirectory(LPCTSTR pszDirPath, BOOL fRemoveRoot);

    HANDLE CFSUtils::CreateTestFile(LPCTSTR lpFileName,
                                DWORD dwDesiredAccess,
                                DWORD dwShareMode,
                                LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                                DWORD dwCreationDisposition,
                                DWORD dwFlagsAndAttributes,
                                HANDLE hTemplateFile);

    static CFSUtils * CreateWithProfile(LPCTSTR pszProfile);

    static CFSUtils * CreateWithPath(LPCTSTR pszPath);

    static BOOL CopyDirectoryTree(LPCTSTR pszSrcDir, LPCTSTR pszDstDir);

    static BOOL CreateTestFileFast(HANDLE hFile, DWORD dwFileSize);

    static BOOL CreateTestFileSlow(HANDLE hFile, DWORD dwFileSize);

    static const DWORD MAX_FILESYS_PARAM_LENGTH = 200;

protected:
    CFSUtils(LPCTSTR pszPath);

private:
    CFSInfo * m_pCFSInfo;

    LPTSTR m_pszPath;

    BOOL m_fIsValid;

    BOOL m_fRootFileSystem;

    static BOOL CleanTree(LPCTSTR pszDirPath, BOOL fRemoveRoot);

    static const DWORD NATURAL_WRITE_SIZE = 32768;
};

#endif // __FSUTILS_H__