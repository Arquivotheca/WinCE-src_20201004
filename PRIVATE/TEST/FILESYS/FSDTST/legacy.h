//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>
#include "tuxmain.h"


#ifndef __LEGACY_H__
#define __LEGACY_H__

#define TST_FILE_DESC                   TEXT("%s\\%02.02X.%02.02X")
#define LONG_FILE_NAME                  TEXT("_This_is_a_long_file_name_")
#define TEST_FILE_NAME                  TEXT("W32FILE")
#define TEST_DIR_NAME                   TEXT("W32DIR")
#define TEMP_FILE_NAME                  TEXT("TEMPFILE.NAM")
#define EAT_FILE_NAME                   TEXT("EATFILE.NAM")
#define BIG_FILE_NAME                   TEXT("BIGFILE.NAM")
#define A316_BYTE_PATH_OFF_ROOT1        TEXT("\\This_is_a_316_byte_file_path_that_is_used_for_testing_purposes___It_can_be_truncated_to_the_size_you_need_for_testing_various_conditions___Please_use_it_as_you_like_but_do_not_modify_the_original___")
#define A316_BYTE_PATH_OFF_ROOT2        TEXT("Please_append_A316_BYTE_PATH_OFF_ROOT2_to_A316_BYTE_PATH_OFF_ROOT1_to_make_the_entire_path_to_make_it_316_bytes_long_")

#define countof(a) (sizeof(a)/sizeof(*(a)))
#define CHECKTRUE(b)  if (!(b)) g_pKato->Log(LOG_FAIL, TEXT("CHECKTRUE(%s) failed in %s at line %u [%u]."), TEXT(#b), TEXT(__FILE__), __LINE__, GetLastError());
#define CHECKFALSE(b) if (b)   g_pKato->Log(LOG_FAIL, TEXT("CHECKFALSE(%s) failed in %s at line %u [%u]."), TEXT(#b), TEXT(__FILE__), __LINE__, GetLastError());
#define IS_UNC(path)  ((path[0] == TEXT('\\')) && (path[1] == TEXT('\\')))

// Flags for FillBuffer() to tell it how to fill the buffer.
// NOT_COMPRESSIBLE will make a 4096 byte buffer compress to 4096 bytes.
#define NOT_COMPRESSIBLE    333
// MAX_EXPANSION will make a 4096 byte buffer expand to 5660 bytes.
#define MAX_EXPANSION       344
// ZEROED_BUFFER will fill the buffer with zeros.
#define ZEROED_BUFFER       0
// REPETITIVE_BUFFER will fill the buffer with -1's.
#define REPETITIVE_BUFFER   -1

// Defines for CompareFiles() flags for the reason it failed.
#define FC_NOT_ENOUGH_MEMORY    0x00000001
#define FC_ATTRIBUTE_MISMATCH   0x00000002
#define FC_ERROR_OPENING_FILE   0x00000004
#define FC_FILE_SIZE_MISMATCH   0x00000008
#define FC_ERROR_READING_FILE   0x00000010
#define FC_BUFFER_MISCOMPARE    0x00000020
#define FC_CANT_GET_FILETIME    0x00000040
#define FC_FILETIME_MISMATCH    0x00000080

void FillBuffer( LPBYTE pBuffer, DWORD dwBufSize, int nRandSeed );

void MV_TRACE(LPCTSTR szFormat, ...);
BOOL FileExists(LPTSTR pszFileName);
DWORD FileSize(TCHAR *szFileName );
BOOL TestAllFileAttributes(TCHAR *szFileSpec, DWORD nBufSize);
BOOL TestFileAttributes(TCHAR *szFileSpec, DWORD fdwAttrsAndFlags, BOOL bShouldOpen, DWORD nBufSize );

BOOL TestFileTime(HANDLE hFile, DWORD dwLowFileTime, DWORD dwHighFileTime );
BOOL TestFileTimes(TCHAR *szFilePath, TCHAR *szFileName, DWORD dwFileSize );

void L_PrintMiscompareReason(DWORD fdwReasonFailed);
BOOL CompareFiles( TCHAR *szSrcFileSpec, TCHAR *szDstFileSpec, BOOL bIgnoreArchiveBit, DWORD *fdwReasonFailed );
BOOL CreateTestFile(TCHAR *szFilePath, TCHAR *szFileName, DWORD dwFileSize, HANDLE *phFile, int nDataSeed );
BOOL CreateFilesToTest(TCHAR *szDirSpec, DWORD fdwAttrsAndFlags, DWORD nNumFiles, DWORD nBufSize );
BOOL CopyFilesToTest(TCHAR *szSourcePath, TCHAR *szDestPath, DWORD fdwAttrsAndFlags, 
                      DWORD nNumFiles, DWORD nBufSize, BOOL bTry2CopyAgain );
BOOL MoveFilesToTest(TCHAR *szSourcePath, TCHAR *szDestPath, TCHAR *szCompPath, 
                      DWORD fdwAttrsAndFlags, DWORD nNumFiles, DWORD nBufSize );
BOOL CopyMoveAndDelete(TCHAR *szSourcePath, TCHAR *szCopyPath, TCHAR *szMovePath, 
                        DWORD fdwAttrsAndFlags, DWORD nNumFiles, DWORD nBufSize,
                        BOOL bTry2CopyAgain );
BOOL CopyMoveAndDelForAllFileAttributes(TCHAR *szSourcePath, TCHAR *szCopyPath, 
    TCHAR *szMovePath, DWORD fdwAttrsAndFlags, DWORD nNumFiles, DWORD nBufSize,
    BOOL bTry2CopyAgain);

UINT CompleteCreateDirectory(LPTSTR pszCompletePath);
UINT CompleteRemoveDirectoryMinusPcCard(LPTSTR pszCompletePath, TCHAR *szCardRoot);
BOOL CreateAndRemoveRootDirectory(TCHAR *szDirSpec, TCHAR *szCardRoot);
int GetTestResult();
BOOL DeleteTree(LPCTSTR szPath, BOOL fDontDeleteRoot);
BOOL DeleteTreeRecursize(LPTSTR szPath, BOOL fDontDeleteRoot);
BOOL CreateDirectoryAndFile(TCHAR *szFilePath, TCHAR *szFileName );
BOOL NukeDirectories(TCHAR *szFilePath, TCHAR *szFileSpec );

#endif // __LEGACY_H__
