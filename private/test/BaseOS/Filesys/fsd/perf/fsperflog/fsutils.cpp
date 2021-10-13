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
#include "main.h"
#include "globals.h"
#include "fsutils.h"

// --------------------------------------------------------------------
// Constructor : takes a filesystem path
// --------------------------------------------------------------------
CFSUtils::CFSUtils(LPCTSTR pszPath)
{
    m_pszPath = NULL;
    m_pCFSInfo = NULL;
    m_fIsValid = FALSE;
    m_fRootFileSystem = FALSE;
    DWORD dwAttributes = 0;

    // Check the path argument
    if (NULL == pszPath)
    {
        FAIL("CFSUtils::CFSUtils: path parameter is NULL");
        goto done;
    }

    // Check if the path exists and is a directory
    dwAttributes = GetFileAttributes(pszPath);
    if ((0xFFFFFFFF == dwAttributes) || (!(dwAttributes & FILE_ATTRIBUTE_DIRECTORY)))
    {
        FAIL("CFSUtils::CFSUtils: Specified path doesn't exist");
        goto done;
    }

    // Allocate path
    m_pszPath = (TCHAR*) malloc(sizeof(TCHAR) * MAX_PATH);
    if (NULL == m_pszPath)
    {
        FAIL("CFSUtils::CFSUtils: Out of memory()");
        goto done;
    }

    // Copy test path
    VERIFY(SUCCEEDED(StringCchCopy(m_pszPath, MAX_PATH, pszPath)));

    // Perform path validation
    if (0 == _tcsncmp(m_pszPath, _T("\\"), MAX_PATH))
    {
        m_fRootFileSystem = TRUE;
        m_fIsValid = TRUE;
        goto done;
    }

    // Allocate CFSInfo object with the specified path
    m_pCFSInfo = new CFSInfo(pszPath);
    if (NULL == m_pCFSInfo)
    {
        FAIL("CFSUtils::CFSUtils: Out of memory()");
        goto done;
    }

    m_fIsValid = TRUE;
done:
    return;
}

// --------------------------------------------------------------------
// Destructor
// --------------------------------------------------------------------
CFSUtils::~CFSUtils()
{
    CHECK_DELETE(m_pszPath);
    CHECK_DELETE(m_pCFSInfo);
}

// --------------------------------------------------------------------
// Creates a FSUtils object with the specified storage profile
// --------------------------------------------------------------------
CFSUtils * CFSUtils::CreateWithProfile(LPCTSTR pszProfile)
{
    CFSUtils * pCFSUtils = NULL;
    LPTSTR pszPath = NULL;

    // Allocate string
    pszPath = (TCHAR*) malloc(sizeof(TCHAR) * MAX_PATH);
    if (NULL == pszPath)
    {
        FAIL("CFSUtils::CreateWithProfile: Out of memory()");
        goto done;
    }
    
    //We want a writable partition
    if ((NULL == CFSInfo::FindStorageWithProfile(pszProfile, pszPath, MAX_PATH, FALSE)))
    {
        FAIL("CFSUtils::CreateWithProfile: CFSInfo::FindStorageWithProfile() failed");
        goto done;
    }
    else if( (pszPath[0] == '\0') )
    {
        pszPath[0] = L'\\';
        pszPath[1] = L'\0';
    }

    // Call private constructor
    pCFSUtils = new CFSUtils(pszPath);
    if (NULL == pCFSUtils)
    {
        FAIL("CFSUtils::CreateWithProfile: new CFSUtils() failed");
        goto done;
    }
done:
    CHECK_FREE(pszPath);
    return pCFSUtils;
}

// --------------------------------------------------------------------
// Creates a FSUtils object with the specified path
// --------------------------------------------------------------------
CFSUtils * CFSUtils::CreateWithPath(LPCTSTR pszPath)
{
    CFSUtils * pCFSUtils = NULL;

    // Call private constructor
    pCFSUtils = new CFSUtils(pszPath);
    if (NULL == pCFSUtils)
    {
        FAIL("CFSUtils::CreateWithProfile: new CFSUtils() failed");
        goto done;
    }

done:
    return pCFSUtils;
}

// --------------------------------------------------------------------
// Returns whether or not the object is valid
// --------------------------------------------------------------------
BOOL CFSUtils::IsValid() const
{
    return m_fIsValid;
}

// --------------------------------------------------------------------
// Copies the test path into the buffer
// --------------------------------------------------------------------
LPTSTR CFSUtils::GetPath(__out_ecount(cbPathBufferLength) LPTSTR pszPathBuffer,
                         DWORD cbPathBufferLength)
{
    if ((NULL == pszPathBuffer) || (cbPathBufferLength < MAX_PATH))
    {
        FAIL("CFSUtils::GetPath: output buffer is NULL or has insufficent space");
        return NULL;
    }

    // Copy path into the buffer
    VERIFY(SUCCEEDED(StringCchCopy(pszPathBuffer, MAX_PATH, m_pszPath)));

    return pszPathBuffer;
}

// --------------------------------------------------------------------
// Creates a file of the specified size in the test path
// --------------------------------------------------------------------
BOOL CFSUtils::CreateTestFile(LPCTSTR pszFileName, DWORD dwFileSize, BOOL fFast)
{
    BOOL fRet = FALSE;
    HANDLE hFile = NULL;
    LPTSTR pszFullPath = NULL;

    // Allocate path buffer
    pszFullPath = (TCHAR*) malloc(sizeof(TCHAR) * MAX_PATH);
    if (NULL == pszFullPath)
    {
        FAIL("CFSUtils::CreateTestFile: Out of memory()");
        goto done;
    }

    // Get the full file path to the test file
    if (NULL == GetFullPath(pszFullPath, MAX_PATH, pszFileName))
    {
        goto done;
    }

    // Create the specified file
    hFile = CreateFile(
        pszFullPath,
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        g_dwFileCreateFlags,
        NULL);
    if (INVALID_HANDLE(hFile))
    {
        FAIL("CFSUtils::CreateTestFile: CreateFile() failed");
        goto done;
    }

    // Call fast create routine
    if (fFast && !CreateTestFileFast(hFile, dwFileSize))
    {
        goto done;
    }

    // Call the slow create routine
    if (!fFast && !CreateTestFileSlow(hFile, dwFileSize))
    {
        goto done;
    }

    fRet = TRUE;
done:
    // Clean Up
    CHECK_CLOSE_HANDLE(hFile);
    CHECK_FREE(pszFullPath);

    return fRet;
}

// --------------------------------------------------------------------
// Concatenates path into the current test path
// --------------------------------------------------------------------
LPTSTR CFSUtils::GetFullPath(__out_ecount(cbPathBufferLength) LPTSTR pszPathBuffer,
                             DWORD cbPathBufferLength,
                             LPCTSTR pszPath)
{
    // Check output buffer
    if ((NULL == pszPathBuffer) || (cbPathBufferLength < MAX_PATH))
    {
        FAIL("CFSUtils::GetFullPath: output buffer is NULL or has insufficent space");
        return NULL;
    }

    // Check input path
    if (NULL == pszPath)
    {
        FAIL("CFSUtils::GetFullPath: invalid input path");
        return NULL;
    }

    // Append the current path to the test path
    if (m_fRootFileSystem)
    {
        VERIFY(SUCCEEDED(StringCchPrintf(
            pszPathBuffer,
            cbPathBufferLength,
            _T("\\%s"),
            pszPath)));
    }
    else
    {
        VERIFY(SUCCEEDED(StringCchPrintf(
            pszPathBuffer,
            cbPathBufferLength,
            _T("%s\\%s"),
            m_pszPath,
            pszPath)));
    }

    return pszPathBuffer;
}

// --------------------------------------------------------------------
// Creates a file of specified size by writing out data
// --------------------------------------------------------------------
BOOL CFSUtils::CreateTestFileSlow(HANDLE hFile, DWORD dwFileSize)
{
    BOOL fRet = FALSE;
    DWORD dwTotalBytesWritten = 0;
    DWORD dwBytesWritten = 0;
    DWORD dwNextWriteSize = 0;
    LPBYTE pBuffer = NULL;

    if (INVALID_HANDLE(hFile))
    {
        FAIL("CFSUtils::CreateTestFileSlow: Invalid file handle");
        goto done;
    }

    // Allocate room for write buffer
    pBuffer = (LPBYTE)LocalAlloc(LMEM_FIXED, DEFAULT_WRITE_SIZE);
    if(NULL == pBuffer)
    {
        FAIL("CFSUtils::CreateTestFileSlow: LocalAlloc() failed");
        goto done;
    }

    while (dwTotalBytesWritten < dwFileSize)
    {
        dwNextWriteSize = min(DEFAULT_WRITE_SIZE, dwFileSize - dwTotalBytesWritten);
        if (!WriteFile(hFile, pBuffer, dwNextWriteSize, &dwBytesWritten, NULL))
        {
            FAIL("CFSUtils::CreateTestFileSlow: WriteFile() failed");
            goto done;
        }
        else if (dwBytesWritten != dwNextWriteSize)
        {
            FAIL("CFSUtils::CreateTestFileSlow WriteFile() failed");
            goto done;
        }
        dwTotalBytesWritten += dwBytesWritten;
    }

    fRet = TRUE;
done:
    // Clean up
    CHECK_LOCAL_FREE(pBuffer);

    return fRet;
}

// --------------------------------------------------------------------
// Creates a file of specified size with SetEndOfFile
// --------------------------------------------------------------------
BOOL CFSUtils::CreateTestFileFast(HANDLE hFile, DWORD dwFileSize)
{
    BOOL fRet = FALSE;

    if (INVALID_HANDLE(hFile))
    {
        FAIL("CFSUtils::CreateTestFileFast: Invalid file handle");
        goto done;
    }

    if (dwFileSize > MAXLONG)
    {
        LOG(_T("CFSUtils::CreateTestFileFast: requested file size %u is greater than %u"),
            dwFileSize,
            MAXLONG);
        FAIL("CFSUtils::CreateTestFileFast");
        goto done;
    }

    // Set the file pointer out to the specified size
    if (0xFFFFFFFF == SetFilePointer(hFile, dwFileSize, NULL, FILE_BEGIN))
    {
        FAIL("CFSUtils::CreateTestFileFast: SetFilePointer() failed");
        goto done;
    }

    // Write out file contents via SetEndOfFile()
    if (!SetEndOfFile(hFile))
    {
        FAIL("CFSUtils::CreateTestFileFast: SetEndOfFile() failed");
        goto done;
    }

    fRet = TRUE;
done:
    return fRet;
}

// --------------------------------------------------------------------
// Creates a fragmented file
// --------------------------------------------------------------------
BOOL CFSUtils::CreateFragmentedTestFile(LPCTSTR pszFileName, DWORD dwFileSize)
{
    BOOL fRet = FALSE;
    HANDLE hFileA = NULL;
    HANDLE hFileB = NULL;
    DWORD dwBytesWrittenA = 0;
    DWORD dwBytesWrittenB = 0;
    DWORD dwNextWriteSize = 0;
    DWORD dwTotalBytesWritten = 0;
    DWORD dwDataSize = NATURAL_WRITE_SIZE;
    LPTSTR pszFullPathA = NULL;
    LPTSTR pszFullPathB = NULL;
    PBYTE pBuffer = NULL;

    // Allocate path buffers
    pszFullPathA = (TCHAR*) malloc(sizeof(TCHAR) * MAX_PATH);
    pszFullPathB = (TCHAR*) malloc(sizeof(TCHAR) * MAX_PATH);
    if ((NULL == pszFullPathA) || (NULL == pszFullPathB))
    {
        FAIL("CFSUtils::CreateFragmentedTestFile: Out of memory()");
        goto done;
    }

    // Get the full file path to the test file
    if (NULL == GetFullPath(pszFullPathA, MAX_PATH, pszFileName))
    {
        goto done;
    }

    // Get the full file path to the interleaving file
    if (NULL == GetFullPath(pszFullPathB, MAX_PATH, pszFileName))
    {
        goto done;
    }

    // Append suffix to interleaving file name
    VERIFY(SUCCEEDED(StringCchCat(pszFullPathB, MAX_PATH, _T(".intr"))));

    // Allocate room for write buffer
    pBuffer = (LPBYTE)LocalAlloc(LMEM_FIXED, dwDataSize);

    if(NULL == pBuffer)
    {
        FAIL("CFSUtils::CreateFragmentedTestFile: Out of memory()");
        goto done;
    }

    // Log some output
    LOG(_T("CFSUtils::CreateFragmentedTestFile() : creating fragmented file %s and %s of size %u"),
        pszFullPathA,
        pszFullPathB,
        dwFileSize);

    // Open two files for writing
    hFileA = CreateFile(
        pszFullPathA,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_WRITE,
        0,
        CREATE_ALWAYS,
        FILE_FLAG_NO_BUFFERING,
        0);

    hFileB = CreateFile(
        pszFullPathB,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_WRITE,
        0,
        CREATE_ALWAYS,
        FILE_FLAG_NO_BUFFERING,
        0);

    // Check the handles
    if (INVALID_HANDLE(hFileA) || INVALID_HANDLE(hFileB))
    {
        FAIL("CFSUtils::CreateFragmentedTestFile() : CreateFile() failed");
        goto done;
    }

    // Start a write loop
    while(dwTotalBytesWritten < dwFileSize)
    {
        dwNextWriteSize = min(dwDataSize, dwFileSize - dwTotalBytesWritten);
        if (!WriteFile(hFileA, pBuffer, dwNextWriteSize, &dwBytesWrittenA, NULL) ||
            !WriteFile(hFileB, pBuffer, dwNextWriteSize, &dwBytesWrittenB, NULL))
        {
            FAIL("CFSUtils::CreateFragmentedTestFile() : WriteFile() failed");
            goto done;
        }
        // Make sure the correct amount of data was written
        if ((dwBytesWrittenA != dwNextWriteSize) ||
            (dwBytesWrittenB != dwNextWriteSize))
        {
            FAIL("CFSUtils::CreateFragmentedTestFile() : WriteFile() failed");
            goto done;
        }
        dwTotalBytesWritten += dwNextWriteSize;
    }

    // Remove interleaving file
    if (!CloseHandle(hFileB) || !::DeleteFile(pszFullPathB))
    {
        FAIL("CFSUtils::CreateFragmentedTestFile() : DeleteFile() failed");
        goto done;
    }

    fRet = TRUE;
done:
    // Clean Up
    CHECK_CLOSE_HANDLE(hFileA);
    CHECK_CLOSE_HANDLE(hFileB);
    CHECK_LOCAL_FREE(pBuffer);
    CHECK_FREE(pszFullPathA);
    CHECK_FREE(pszFullPathB);

    return fRet;
}

// --------------------------------------------------------------------
// Wraps a call to CreateFile
// --------------------------------------------------------------------
HANDLE CFSUtils::CreateTestFile(LPCTSTR lpFileName,
                                DWORD dwDesiredAccess,
                                DWORD dwShareMode,
                                LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                                DWORD dwCreationDisposition,
                                DWORD dwFlagsAndAttributes,
                                HANDLE hTemplateFile)
{
    HANDLE hFile = INVALID_HANDLE_VALUE;
    LPTSTR pszFullPath = NULL;


    // Allocate path buffer
    pszFullPath = (TCHAR*) malloc(sizeof(TCHAR) * MAX_PATH);
    if (NULL == pszFullPath)
    {
        FAIL("CFSUtils::CreateTestFile: Out of memory()");
        goto done;
    }

    // Get the full file path to the test file
    if (NULL == GetFullPath(pszFullPath, MAX_PATH, lpFileName))
    {
        goto done;
    }

    // Create the specified file
    hFile = ::CreateFile(
        pszFullPath,
        dwDesiredAccess,
        dwShareMode,
        lpSecurityAttributes,
        dwCreationDisposition,
        dwFlagsAndAttributes,
        hTemplateFile);
    if (INVALID_HANDLE(hFile))
    {
        LOG( _T("...FILE FAILED!!!! (%s)"), pszFullPath );
        FAIL("CFSUtils::CreateTestFile: CreateFile() failed");
        goto done;
    }

done:
    // Clean Up
    CHECK_FREE(pszFullPath);

    return hFile;
}

// --------------------------------------------------------------------
// Wraps a call to DeleteFile
// --------------------------------------------------------------------
BOOL CFSUtils::DeleteFile(LPCTSTR pszFileName)
{
    BOOL fRet = FALSE;
    LPTSTR pszFullPath = NULL;

    // Allocate path buffer
    pszFullPath = (TCHAR*) malloc(sizeof(TCHAR) * MAX_PATH);
    if (NULL == pszFullPath)
    {
        FAIL("CFSUtils::DeleteFile: Out of memory()");
        goto done;
    }

    // Get the full file path to the test file
    if (NULL == GetFullPath(pszFullPath, MAX_PATH, pszFileName))
    {
        goto done;
    }

    // Attempt deletion
    if (!::DeleteFile(pszFullPath))
    {
        goto done;
    }

    fRet = TRUE;
done:
    // Clean Up
    CHECK_FREE(pszFullPath);

    return fRet;
}

// --------------------------------------------------------------------
// Wraps a call to CreateDirectory
// --------------------------------------------------------------------
BOOL CFSUtils::CreateDirectory(LPCTSTR pszDirName)
{
    BOOL fRet = FALSE;
    LPTSTR pszFullPath = NULL;

    // Allocate path buffer
    pszFullPath = (TCHAR*) malloc(sizeof(TCHAR) * MAX_PATH);
    if (NULL == pszFullPath)
    {
        FAIL("CFSUtils::DeleteFile: Out of memory()");
        goto done;
    }

    // Get the full file path to the test file
    if (NULL == GetFullPath(pszFullPath, MAX_PATH, pszDirName))
    {
        goto done;
    }

    // Attempt deletion
    if (!::CreateDirectory(pszFullPath, NULL))
    {
        goto done;
    }

    fRet = TRUE;
done:
    // Clean Up
    CHECK_FREE(pszFullPath);

    return fRet;
}

// --------------------------------------------------------------------
// Wraps a call to delete an entire directory tree
// --------------------------------------------------------------------
BOOL CFSUtils::CleanDirectory(LPCTSTR pszDirPath, BOOL fRemoveRoot)
{
    BOOL fRet = FALSE;
    LPTSTR pszFullPath = NULL;

    // Allocate path buffer
    pszFullPath = (TCHAR*) malloc(sizeof(TCHAR) * MAX_PATH);
    if (NULL == pszFullPath)
    {
        FAIL("CFSUtils::DeleteFile: Out of memory()");
        goto done;
    }

    // Get the full file path to the test file
    if (NULL == GetFullPath(pszFullPath, MAX_PATH, pszDirPath))
    {
        goto done;
    }

    // Attempt deletion
    if (!CleanTree(pszFullPath, fRemoveRoot))
    {
        goto done;
    }

    fRet = TRUE;
done:
    // Clean Up
    CHECK_FREE(pszFullPath);

    return fRet;
}

// --------------------------------------------------------------------
// Wraps a call to delete an entire directory tree
// --------------------------------------------------------------------
BOOL CFSUtils::CleanTree(LPCTSTR pszDirPath, BOOL fRemoveRoot)
{
    BOOL fRet = FALSE;
    LPTSTR pszSearch = NULL;
    LPTSTR pszFileName = NULL;
    HANDLE hFindFile = INVALID_HANDLE_VALUE;
    WIN32_FIND_DATA findData = {0};

    // If the current directory does not exist, we're done
    if (0xFFFFFFFF == GetFileAttributes(pszDirPath))
    {
        fRemoveRoot = FALSE;
        fRet = TRUE;
        goto done;
    }

    // Allocate memory for search
    pszSearch = (TCHAR*) malloc(sizeof(TCHAR) * MAX_PATH);
    pszFileName = (TCHAR*) malloc(sizeof(TCHAR) * MAX_PATH);
    if ((NULL == pszSearch) || (NULL == pszFileName))
    {
        FAIL("CFSUtils::CleanTree: Out of memory()");
        goto done;
    }

    // Create a search pattern
    VERIFY(SUCCEEDED(StringCchPrintf(pszSearch, MAX_PATH, _T("%s\\*"), pszDirPath)));

    // Search
    hFindFile = FindFirstFile(pszSearch, &findData);

    if (INVALID_HANDLE(hFindFile))
    {
        // Nothing to do
        fRet = TRUE;
        goto done;
    }

    // Clean the subtree
    do
    {
        // Grab the file name
        VERIFY(SUCCEEDED(StringCchPrintf(pszFileName,
            MAX_PATH,
            _T("%s\\%s"),
            pszDirPath,
            findData.cFileName)));

        // Check the attributes
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            // Recursive call
            if (!CleanTree(pszFileName, TRUE))
            {
                goto done;
            }
        }
        else
        {
            // Delete the file
            if (!::DeleteFile(pszFileName))
            {
                FAIL("CFSUtils::CleanTree: DeleteFile() failed");
                goto done;
            }
        }
    }
    while (FindNextFile(hFindFile, &findData));

    fRet = TRUE;
done:
    CHECK_FIND_CLOSE(hFindFile);
    // Remove current directory if required
    if (fRet && fRemoveRoot && !::RemoveDirectory(pszDirPath))
    {
        FAIL("CFSUtils::CleanTree: RemoveDirectory() failed");
        fRet = FALSE;
    }
    CHECK_FREE(pszSearch);
    CHECK_FREE(pszFileName);
    return fRet;
}

// --------------------------------------------------------------------
// Creates a set of test files
// --------------------------------------------------------------------
BOOL CFSUtils::CreateTestFileSet(LPCTSTR pszRootDir, DWORD dwNumFiles, DWORD dwTotalFileSize)
{
    BOOL fRet = FALSE;
    TCHAR szFileName[MAX_PATH] = _T("");
    DWORD dwFileSize = 0;

    if (!dwNumFiles)
    {
        // Nothing to do
        fRet = TRUE;
        goto done;
    }

    // Make sure each file is at least a byte
    if (dwTotalFileSize < dwNumFiles)
    {
        dwTotalFileSize = dwNumFiles;
    }

    // Calculate the size for each file in the set
    dwFileSize = dwTotalFileSize / dwNumFiles;

    for (DWORD dwI = 1; dwI <= dwNumFiles; dwI++)
    {
        VERIFY(SUCCEEDED(StringCchPrintf(szFileName,
            MAX_PATH,
            _T("%s\\%s.%u"),
            pszRootDir,
            DEFAULT_TEST_FILE_NAME,
            dwI)));
        if (!CreateTestFile(szFileName, dwFileSize, FALSE))
        {
            FAIL("CFSUtils::CreateTestFileSet: CreateTestFile() failed");
            goto done;
        }
    }

    fRet = TRUE;
done:
    // Clean Up
    return fRet;
}

// --------------------------------------------------------------------
// Creates a set of test files where we dont write anything
// --------------------------------------------------------------------
BOOL CFSUtils::CreateTestFileSetNoWrite(LPCTSTR pszRootDir, DWORD dwNumFiles, DWORD dwCreationDispostion)
{
    BOOL fRet = FALSE;
    TCHAR szFileName[MAX_PATH] = _T("");
    LPTSTR pszFullPath = NULL;
    HANDLE hFile=0;

    if (!dwNumFiles)
    {
        // Nothing to do
        fRet = TRUE;
        goto done;
    }
    // Allocate path buffer
    pszFullPath = (TCHAR*) malloc(sizeof(TCHAR) * MAX_PATH);
    if (NULL == pszFullPath)
    {
        FAIL("CFSUtils::CreateTestFileSetNoWrite: Out of memory()");
        goto done;
    }


    for (DWORD dwI = 1; dwI <= dwNumFiles; dwI++)
    {
        VERIFY(SUCCEEDED(StringCchPrintf(szFileName,
            MAX_PATH,
            _T("%s\\%s.%u"),
            pszRootDir,
            DEFAULT_TEST_FILE_NAME,
            dwI)));

        // Get the full file path to the test file
        if (NULL == GetFullPath(pszFullPath, MAX_PATH, szFileName))
        {
            goto done;
        }

        hFile = CreateFile(
        pszFullPath,
        GENERIC_WRITE,
        0,
        NULL,
        dwCreationDispostion,
        g_dwFileCreateFlags,
        NULL);
        if (INVALID_HANDLE(hFile))
        {
            ERRFAIL("CFSUtils::CreateTestFileSetNoWrite :CreateFile() failed");
            goto done;
        }
        CHECK_CLOSE_HANDLE(hFile);
    }

    fRet = TRUE;
done:
    // Clean Up
    CHECK_CLOSE_HANDLE(hFile);
    CHECK_FREE(pszFullPath);
    return fRet;
}

// --------------------------------------------------------------------
// Recursively copies entire directory tree
// --------------------------------------------------------------------
BOOL CFSUtils::CopyDirectoryTree(LPCTSTR pszSrcDir, LPCTSTR pszDstDir)
{
    TCHAR szDirName[MAX_PATH] = _T("");
    TCHAR szSearch[MAX_PATH] = _T("");
    TCHAR szFileName[MAX_PATH] = _T("");
    TCHAR szDstFileName[MAX_PATH] = _T("");
    HANDLE hFindFile = INVALID_HANDLE_VALUE;
    WIN32_FIND_DATA findData = {0};
    BOOL fRet = TRUE;

    VERIFY(SUCCEEDED(StringCchPrintf(szSearch, MAX_PATH, _T("%s\\*"), pszSrcDir)));
    VERIFY(SUCCEEDED(StringCchPrintf(szDirName, MAX_PATH, _T("%s\\"), pszSrcDir)));

    LOG(_T("Copying directory %s"), szDirName);

    if (0xFFFFFFFF == GetFileAttributes(szDirName))
    {
        // Nothing to do
        fRet = TRUE;
        goto done;
    }

    hFindFile = FindFirstFile(szSearch, &findData);

    if (INVALID_HANDLE(hFindFile))
    {
        // Nothing to do
        fRet = TRUE;
        goto done;
    }

    do
    {
        VERIFY(SUCCEEDED(StringCchPrintf(szFileName, MAX_PATH, _T("%s\\%s"), pszSrcDir, findData.cFileName)));
        VERIFY(SUCCEEDED(StringCchPrintf(szDstFileName, MAX_PATH, _T("%s\\%s"), pszDstDir, findData.cFileName)));
        if (FILE_ATTRIBUTE_DIRECTORY == GetFileAttributes(szFileName))
        {
            if (!::CreateDirectory(pszDstDir, NULL))
            {
                FAIL("CreateDirectory()");
                goto done;
            }
            if (!CopyDirectoryTree(szFileName, szDstFileName))
            {
                FAIL("CopyDirectoryTree()");
                goto done;
            }
        }
        else 
        {
            CopyFile(szFileName, szDstFileName, TRUE);
        }
    } while(FindNextFile(hFindFile, &findData));

    fRet = TRUE;
done:
    CHECK_FIND_CLOSE(hFindFile);
    return fRet;
}

// --------------------------------------------------------------------
// Returns space available on the filesystem
// --------------------------------------------------------------------
DWORD CFSUtils::GetDiskFreeSpace()
{
    ULARGE_INTEGER ulFree = {0};
    ULARGE_INTEGER ulTotal = {0};
    ULARGE_INTEGER ulTotalFree = {0};
    DWORD dwBytesFree = 0;

    if (m_fRootFileSystem && ::GetDiskFreeSpaceExW(_T("\\"), &ulFree, &ulTotal, &ulTotalFree))
    {
        dwBytesFree = ulFree.LowPart;
    }
    else if (m_pCFSInfo->GetDiskFreeSpaceExW(&ulFree, &ulTotal, &ulTotalFree))
    {
        dwBytesFree = ulFree.LowPart;
    }

    return dwBytesFree;
}