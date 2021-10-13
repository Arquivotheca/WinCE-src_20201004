//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//
#include "attributetest.h"

#undef __FILE_NAME__
#define __FILE_NAME__   TEXT("ATTRIBUTETEST.CPP")

//
// internal test functions declarations
//
static BOOL TestAttributes( LPTSTR, const LPWIN32_FIND_DATA );
static BOOL TestAttributesEx( LPTSTR, const LPWIN32_FIND_DATA );
static BOOL TestVersionInfo( LPTSTR, const LPWIN32_FIND_DATA );
static BOOL TestDiskFreeSpace( LPTSTR, const LPWIN32_FIND_DATA );
static BOOL TestInfoByHandle( LPTSTR, const LPWIN32_FIND_DATA );
static BOOL TestFileSize( LPTSTR, const LPWIN32_FIND_DATA );
static BOOL TestFileTime( LPTSTR, const LPWIN32_FIND_DATA );
// Helper functions
static void LogFileAttributes( DWORD );
static void LogFileTime( FILETIME *ft );

//******************************************************************************
TESTPROCAPI 
FSAttributesTest(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
//
//  The TUX wrapper routine for the File System Attributes tests.
//
//  Standard Tux arguments, standard Tux return values; see Tux documentation.
//
//******************************************************************************    
{
    ASSERT( uMsg );
    ASSERT( tpParam );
    ASSERT( lpFTE );
    ASSERT( g_pKato );

    //
    // Check the incoming message
    //
    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 0;
        return SPR_HANDLED;
    }
    
    //
    // The only other message handled is TPM_EXECUTE
    //
    else if( TPM_EXECUTE != uMsg )
    {
        return TPR_NOT_HANDLED;
    }

    //
    // Forward declarations
    //
    BOOL fSuccess               = TRUE;
    BOOL fRet                   = TRUE;
    WIN32_FIND_DATA findData    = { 0 };
    UINT i                      = 0;
    FindFiles findFiles;
    
    //
    // String buffers
    //
    TCHAR szPath[_MAX_PATH];
    TCHAR szFile[_MAX_PATH];
    TCHAR szSearch[_MAX_PATH];

    DETAIL("Starting FSAttributesTest");

    if( g_pPathTable == NULL )
    {
      FAIL("Unable to find media");
      fRet = FALSE;
      goto EXIT;
    }
    
    for( i = 0; i < g_pPathTable->GetTotalEntries(); i++ )
    {
        g_pPathTable->GetPath( i, szPath, _MAX_PATH - 2 );
        _stprintf( szSearch, TEXT("%s\\*"), szPath );
                
        while( findFiles.Next( szSearch, &findData ) )
        {
            //
            // ignore .. and . directories because they only screw it up
            //
            if( _tcscmp( findData.cFileName, TEXT("..") ) &&
                _tcscmp( findData.cFileName, TEXT(".") ) )
            {
                _stprintf( szFile, TEXT("%s\\%s"), szPath, findData.cFileName );
                g_pKato->Log( LOG_DETAIL, TEXT("Testing file: %s"), szFile );
                
                //
                // dispatch the proper test function based on the user data,
                // an ATTRIBUTES_TEST value
                //
                switch( lpFTE->dwUserData )
                {
                    case ATTRIBUTES:
                        fSuccess = TestAttributes( szFile, &findData );
                        break;
                    case ATTRIBUTES_EX:
                        fSuccess = TestAttributesEx( szFile, &findData );
                        break;
                    case DISK_FREE_SPACE:
                        fSuccess = TestDiskFreeSpace( szFile, &findData );
                        break;
                    case INFO_BY_HANDLE:
                        fSuccess = TestInfoByHandle( szFile, &findData );    
                        break;
                    case FILE_SIZE:
                        fSuccess = TestFileSize( szFile, &findData );    
                        break;
                    case FILE_TIME:
                        fSuccess = TestFileTime( szFile, &findData );    
                        break;
                    default:
                        Debug( TEXT("Invalid FSAttributesTest test number") );
                        ASSERT( FALSE );
                        break;
                }
                //
                // the test failed
                //
                if( !fSuccess )
                {
                    fRet = FALSE;
                }
            }
        }
        
    }

EXIT:    
    return fRet ? TPR_PASS : TPR_FAIL;
}

//******************************************************************************   
static BOOL
TestAttributes(
    LPTSTR szFileName,
    const LPWIN32_FIND_DATA pFindData )
//******************************************************************************       
{

    ASSERT( pFindData );
    ASSERT( szFileName );

    BOOL fRet       = TRUE;
    DWORD dwAttribs = GetFileAttributes( szFileName );
    
    //
    // Make sure dwAttribs is valid
    //
    if( GET_FILE_ATTRIBUTE_ERROR == dwAttribs )
    {
        FAIL("GetFileAttributes");
        fRet = FALSE;
        goto EXIT;
    }

    //
    // Compare attributes to those found in the WIN32_FIND_DATA structure
    //
    if( dwAttribs != pFindData->dwFileAttributes )
    {
        FAIL("TestAttributes - File attribute mismatch");
        LogFileAttributes( dwAttribs );
        LogFileAttributes( pFindData->dwFileAttributes );
        fRet = FALSE;
        goto EXIT;
    }

EXIT:
    if( fRet )
    {
        SUCCESS("TestAttributes");
    }  
    
    return fRet;
}

//******************************************************************************   
static BOOL
TestAttributesEx(
    LPTSTR szFileName,
    const LPWIN32_FIND_DATA pFindData )
//******************************************************************************       
{

    ASSERT( pFindData );
    ASSERT( szFileName );

    BOOL fRet                           = TRUE;
    WIN32_FILE_ATTRIBUTE_DATA fileInfo  = { 0 };

    if( !GetFileAttributesEx(
            szFileName,
            GetFileExInfoStandard, // GET_FILEEX_INFO_LEVELS can only be this value
            &fileInfo ) )
    {
        FAIL("GetFileAttributesEx");
        fRet = FALSE;
        goto EXIT;
    }

    // 
    // compare file attributes field
    //
    if( fileInfo.dwFileAttributes != pFindData->dwFileAttributes )
    {
        fRet = FALSE;
        FAIL("TestAttributesEx - File attribute mismatch");
        LogFileAttributes( fileInfo.dwFileAttributes );
        LogFileAttributes( pFindData->dwFileAttributes );
    }

    //
    // compare creation time
    //
    if( CompareFileTime( 
        &fileInfo.ftCreationTime, 
        &pFindData->ftCreationTime ) )
    {
        fRet = FALSE;
        FAIL("TestAttributesEx - Creation time mismatch");
        LogFileTime( &fileInfo.ftCreationTime );
        LogFileTime( &pFindData->ftCreationTime );
    }

    //
    // compare last access time
    //
    if( CompareFileTime( 
        &fileInfo.ftLastAccessTime, 
        &pFindData->ftLastAccessTime) )
    {
        fRet = FALSE;
        FAIL("TestAttributesEx - Access time mismatch");
        LogFileTime( &fileInfo.ftCreationTime );
        LogFileTime( &pFindData->ftCreationTime );
    }

    // 
    // compare last write time
    //
    if( CompareFileTime(
        &fileInfo.ftLastWriteTime,
        &pFindData->ftLastWriteTime ) )
    {
        fRet = FALSE;
        FAIL("TestAttributesEx - Write time mismatch");
        LogFileTime( &fileInfo.ftLastWriteTime );
        LogFileTime( &pFindData->ftLastWriteTime );
    }

    //
    // compare file size high dword
    //
    if( fileInfo.nFileSizeHigh != pFindData->nFileSizeHigh )
    {
        fRet = FALSE;
        FAIL("TestAttributesEx - File size high DWORD mismatch");

        g_pKato->Log( 
            LOG_DETAIL, 
            TEXT("high DWORD = %d"), 
            fileInfo.nFileSizeHigh );
            
        g_pKato->Log( 
            LOG_DETAIL, 
            TEXT("high DWORD = %d"), 
            pFindData->nFileSizeHigh );
    }

    // 
    // compare file size low dword
    //
    if( fileInfo.nFileSizeLow != pFindData->nFileSizeLow )
    {
        fRet = FALSE;
        FAIL("TestAttributesEx - File size low DWORD mismatch");

        g_pKato->Log( 
            LOG_DETAIL, 
            TEXT("low DWORD = %d"), 
            fileInfo.nFileSizeLow );
            
        g_pKato->Log( 
            LOG_DETAIL, 
            TEXT("low DWORD = %d"), 
            pFindData->nFileSizeLow );
    }

EXIT:
    if( fRet )
    {
        SUCCESS("TestAttributesEx");
    }
    
    return fRet;
}

//******************************************************************************   
static BOOL
TestDiskFreeSpace(
    LPTSTR szFileName,
    const LPWIN32_FIND_DATA pFindData )
//******************************************************************************       
{

    ASSERT( pFindData );
    ASSERT( szFileName );

    BOOL fRet       = TRUE;
    DWORD dwLastErr = 0;
    ULARGE_INTEGER ulnCallerFreeBytes;
    ULARGE_INTEGER ulnTotalBytes;
    ULARGE_INTEGER ulnTotalFreeBytes;
    
    //
    // query free disk space
    //
    if( !GetDiskFreeSpaceEx(
        szFileName,
        &ulnCallerFreeBytes,
        &ulnTotalBytes,
        &ulnTotalFreeBytes ) )
    {   
        dwLastErr = GetLastError();
        //
        // the call should fail when called on files
        //        
        if( !(pFindData->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) )
        {
            //
            // should result in ERROR_DIRECTORY (UDFS) or ERROR_PATH_NOT_FOUND (ROM)
            //
            if( ERROR_DIRECTORY == dwLastErr ||
                ERROR_PATH_NOT_FOUND == dwLastErr)
            {
                fRet = TRUE;
            }
            else
            {
                //
                // wrong error message
                //
                FAIL("GetDiskFreeSpaceEx called on a file resulted in unexpected error code");
                SystemErrorMessage( dwLastErr );
                fRet = FALSE;
            }
        }
        else
        {
            //
            // GetDiskFreeSpace failed on a directory
            //
            FAIL("GetDiskFreeSpaceEx");
            SystemErrorMessage( dwLastErr );      
            fRet = FALSE;
        }
        goto EXIT;
    }

    if( !(pFindData->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) )
    {
        //
        // GetDiskFreeSpace succeeded on a file
        //
        FAIL("GetDiskFreeSpaceEx succeeded when called on a file");
        fRet = FALSE;
        goto EXIT;
    }
        
    g_pKato->Log( LOG_DETAIL,
        TEXT("Total free bytes available: %d"), ulnTotalFreeBytes );

EXIT:
    if( fRet )
    {
        SUCCESS("TestDiskFreeSpace");
    }
    
    return fRet;
}

//******************************************************************************   
static BOOL
TestFileSize(
    LPTSTR szFileName,
    const LPWIN32_FIND_DATA pFindData )
//******************************************************************************       
{

    ASSERT( pFindData );
    ASSERT( szFileName );

    BOOL fRet               = TRUE;
    HANDLE hFile            = INVALID_HANDLE_VALUE;
    DWORD dwFileSizeLow     = 0;
    DWORD dwFileSizeHigh    = 0;
    DWORD dwLastError       = 0;
    
    //
    // this test only works on files, not directories, so it skips directories.
    //
    if( pFindData->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
    {
        DETAIL("Skipping Directory");
        fRet = TRUE;
        goto EXIT;
    }

    //
    // Open a handle to the file
    //
    hFile = CreateFile(
        szFileName,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL );

    //
    // Validate the handle
    //
    if( INVALID_HANDLE( hFile ) )
    {
        //
        // some files may be open by the OS in ROM
        //
        if(ERROR_FILE_NOT_FOUND == GetLastError() )
        {
            goto EXIT;
        }
        FAIL("CreateFile");
        SystemErrorMessage( GetLastError() );
        fRet = FALSE;
        goto EXIT;
    }

    //
    // Get the file size
    //
    dwFileSizeLow = GetFileSize( hFile, &dwFileSizeHigh );
    dwLastError = GetLastError();
    
    //
    // Close the handle
    //
    if( !CloseHandle( hFile ) )
    {
        FAIL("CloseHandle");
        SystemErrorMessage( GetLastError() );
        fRet =  FALSE;
        goto EXIT;
    }

    if( (dwFileSizeLow != pFindData->nFileSizeLow) ||
        (dwFileSizeHigh != pFindData->nFileSizeHigh) )
    {
        FAIL("GetFileSize");
        SystemErrorMessage( dwLastError );
        fRet = FALSE;
        goto EXIT;
    }

EXIT:
    if( fRet )
    {
        SUCCESS("TestFileSize");
    }
    
    return fRet;
}

//******************************************************************************   
static BOOL
TestInfoByHandle(
    LPTSTR szFileName,
    const LPWIN32_FIND_DATA pFindData )
//******************************************************************************       
{

    ASSERT( pFindData );
    ASSERT( szFileName );

    BOOL fRet           = TRUE;
    HANDLE hFile        = INVALID_HANDLE_VALUE;
    
    BY_HANDLE_FILE_INFORMATION fileInfo = { 0 };

    //
    // this test only works on files, not directories, so it skips directories.
    //
    if( pFindData->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
    {
        DETAIL("Skipping Directory");
        fRet = TRUE;
        goto EXIT;
    }

    //
    // Open a handle to the file
    //
    hFile = CreateFile(
        szFileName,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL );
        
    //
    // Validate the handle
    //
    if( INVALID_HANDLE( hFile ) )
    {

        //
        // some files may be open by the OS in ROM
        //
        if(ERROR_FILE_NOT_FOUND == GetLastError() )
        {
            goto EXIT;
        }
        FAIL("CreateFile");
        SystemErrorMessage( GetLastError() );
        fRet = FALSE;
        goto EXIT;
    }

    //
    // Call GetFileInformationByHandle
    //
    if( !GetFileInformationByHandle( hFile, &fileInfo ) )
    {
        FAIL("GetFileInformationByHandle");
        SystemErrorMessage( GetLastError() );
        CloseHandle( hFile );
        fRet = FALSE;
        goto EXIT;
    }

    //
    // Close the handle
    //
    if( !CloseHandle( hFile ) )
    {
        fRet = FALSE;
        FAIL("CloseHandle");
        SystemErrorMessage( GetLastError() );
    }

    //
    // compare file attributes field
    //
    if( fileInfo.dwFileAttributes != pFindData->dwFileAttributes )
    {   
        fRet = FALSE;
        FAIL("TestInfoByHandle - File attribute mismatch");
        LogFileAttributes( fileInfo.dwFileAttributes );
        LogFileAttributes( pFindData->dwFileAttributes );
    }

    //
    // compare creation time
    //
    if( CompareFileTime( 
        &fileInfo.ftCreationTime, 
        &pFindData->ftCreationTime ) )
    {
        fRet = FALSE;
        FAIL("TestAttributesEx - Create time mismatch");
        LogFileTime( &fileInfo.ftCreationTime );
        LogFileTime( &pFindData->ftCreationTime );
    }

    //
    // compare last access time
    //
    if( CompareFileTime( 
        &fileInfo.ftLastAccessTime, 
        &pFindData->ftLastAccessTime) )
    {
        fRet = FALSE;
        FAIL("TestAttributesEx - Access time mismatch");
        LogFileTime( &fileInfo.ftCreationTime );
        LogFileTime( &pFindData->ftCreationTime );
    }

    // 
    // compare last write time
    //
    if( CompareFileTime(
        &fileInfo.ftLastWriteTime,
        &pFindData->ftLastWriteTime ) )
    {
        fRet = FALSE;
        FAIL("TestAttributesEx - Write time mismatch");
        LogFileTime( &fileInfo.ftLastWriteTime );
        LogFileTime( &fileInfo.ftLastWriteTime );
    }

    //
    // compare file size high dword
    //
    if( fileInfo.nFileSizeHigh != pFindData->nFileSizeHigh )
    {
        fRet = FALSE;
        FAIL("TestAttributesEx - File size high DWORD mismatch");

        g_pKato->Log( 
            LOG_DETAIL, 
            TEXT("high DWORD = %d"), 
            fileInfo.nFileSizeHigh );
            
        g_pKato->Log( 
            LOG_DETAIL, 
            TEXT("high DWORD = %d"), 
            pFindData->nFileSizeHigh );
    }

    // 
    // compare file size low dword
    //
    if( fileInfo.nFileSizeLow != pFindData->nFileSizeLow )
    {
        fRet = FALSE;
        FAIL("TestAttributesEx - File size low DWORD mismatch");

        g_pKato->Log( 
            LOG_DETAIL, 
            TEXT("low DWORD = %d"), 
            fileInfo.nFileSizeLow );
            
        g_pKato->Log( 
            LOG_DETAIL, 
            TEXT("low DWORD = %d"), 
            pFindData->nFileSizeLow );
    }

EXIT:    
    if( fRet )
    {
        SUCCESS("TestInfoByHandle");
    }
    
    return fRet;
}

//******************************************************************************   
static BOOL
TestFileTime(
    LPTSTR szFileName,
    const LPWIN32_FIND_DATA pFindData )
//******************************************************************************       
{

    ASSERT( pFindData );
    ASSERT( szFileName );

    BOOL fRet               = TRUE;
    HANDLE hFile            = INVALID_HANDLE_VALUE;
    FILETIME ftCreate       = { 0 };
    FILETIME ftAccess       = { 0 };    
    FILETIME ftWrite        = { 0 };
    DWORD dwLastErr         = 0;

    //
    // this test only works on files, not directories, so it skips directories.
    //
    if( pFindData->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
    {
        DETAIL("Skipping Directory");
        fRet = TRUE;
        goto EXIT;
    }

    //
    // Open a handle to the file
    //
    hFile = CreateFile(
        szFileName,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL );

    dwLastErr = GetLastError();

    //
    // Validate the handle
    //
    if( INVALID_HANDLE( hFile ) )
    {
        if( pFindData->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
        {
            if( ERROR_FILE_NOT_FOUND == dwLastErr )
            {
                fRet = TRUE;
                goto EXIT;
            }                
        }
        //
        // some files may be open by the OS in ROM
        //
        if(ERROR_FILE_NOT_FOUND == GetLastError() )
        {
            goto EXIT;
        }
        FAIL("CreateFile");
        SystemErrorMessage( dwLastErr );
        fRet = FALSE;
        goto EXIT;
    }

    //
    // Get the file time values
    //
    if( !GetFileTime(
            hFile,
            &ftCreate,
            &ftAccess,
            &ftWrite ) )
    {
        FAIL("GetFileTime");
        SystemErrorMessage( GetLastError() );
        CloseHandle( hFile );
        fRet = FALSE;
        goto EXIT;
    }
    
    //
    // Close the handle
    //
    if( !CloseHandle( hFile ) )
    {
        fRet = FALSE;
        FAIL("CloseHandle");
        SystemErrorMessage( GetLastError() );
    }

    //
    // Compare the file times to the find file times
    //
    LogFileTime( &ftCreate );
    if( CompareFileTime( &ftCreate, &pFindData->ftCreationTime ) )
    {
        fRet = FALSE;
        FAIL("TestFileTime - Create time mismatch");
    }

    LogFileTime( &ftAccess );    
    if( CompareFileTime( &ftAccess, &pFindData->ftLastAccessTime ) )
    {
        fRet = FALSE;
        FAIL("TestFileTime - Access time mismatch");
    }

    LogFileTime( &ftWrite ); 
    if( CompareFileTime( &ftWrite, &pFindData->ftLastWriteTime ) )
    {
        fRet = FALSE;
        FAIL("TestFileTime - Write time mismatch");
    }

EXIT:    
    if( fRet )
    {
        SUCCESS("TestFileTime");
    }   
    
    return fRet;
}

//******************************************************************************  
static void
LogFileAttributes(
    DWORD dwAttribs )
//******************************************************************************      
{
    g_pKato->Log( LOG_DETAIL, TEXT("File attribute flags (0x%X)"), dwAttribs );
    if( FILE_ATTRIBUTE_ARCHIVE & dwAttribs )
        g_pKato->Log( LOG_DETAIL, TEXT("    FILE_ATTRIBUTE_ARCHIVE") );
    if( FILE_ATTRIBUTE_COMPRESSED & dwAttribs )
        g_pKato->Log( LOG_DETAIL, TEXT("    FILE_ATTRIBUTE_COMPRESSED") );
    if( FILE_ATTRIBUTE_DIRECTORY & dwAttribs )
        g_pKato->Log( LOG_DETAIL, TEXT("    FILE_ATTRIBUTE_DIRECTORY") );
    if( FILE_ATTRIBUTE_ENCRYPTED & dwAttribs )
        g_pKato->Log( LOG_DETAIL, TEXT("    FILE_ATTRIBUTE_ENCRYPTED") );
    if( FILE_ATTRIBUTE_HIDDEN & dwAttribs )
        g_pKato->Log( LOG_DETAIL, TEXT("    FILE_ATTRIBUTE_HIDDEN") );
    if( FILE_ATTRIBUTE_NORMAL & dwAttribs )
        g_pKato->Log( LOG_DETAIL, TEXT("    FILE_ATTRIBUTE_NORMAL") );   
    if( FILE_ATTRIBUTE_READONLY & dwAttribs )
        g_pKato->Log( LOG_DETAIL, TEXT("    FILE_ATTRIBUTE_READONLY") );
    if( FILE_ATTRIBUTE_REPARSE_POINT & dwAttribs )
        g_pKato->Log( LOG_DETAIL, TEXT("    FILE_ATTRIBUTE_REPARSE_POINT") );
    if( FILE_ATTRIBUTE_SPARSE_FILE & dwAttribs )
        g_pKato->Log( LOG_DETAIL, TEXT("    FILE_ATTRIBUTE_SPARSE_FILE") );
    if( FILE_ATTRIBUTE_SYSTEM & dwAttribs )
        g_pKato->Log( LOG_DETAIL, TEXT("    FILE_ATTRIBUTE_SYSTEM") );
    if( FILE_ATTRIBUTE_TEMPORARY & dwAttribs )
        g_pKato->Log( LOG_DETAIL, TEXT("    FILE_ATTRIBUTE_TEMPORARY") );

#ifdef UNDER_CE
    if( FILE_ATTRIBUTE_INROM & dwAttribs )
        g_pKato->Log( LOG_DETAIL, TEXT("    FILE_ATTRIBUTE_INROM") );
    if( FILE_ATTRIBUTE_ROMMODULE & dwAttribs )
        g_pKato->Log( LOG_DETAIL, TEXT("    FILE_ATTRIBUTE_ROMMODULE") );
    if( FILE_ATTRIBUTE_ROMSTATICREF & dwAttribs )
        g_pKato->Log( LOG_DETAIL, TEXT("    FILE_ATTRIBUTE_ROMSTATICREF") );  
#endif // UNDER_CE
}

//******************************************************************************  
static void
LogFileTime(
    FILETIME *pft )
//******************************************************************************      
{
    g_pKato->Log( LOG_DETAIL, 
        TEXT("FILETIME hiDWORD = %ul, loDWORD = %ul"), 
        pft->dwHighDateTime,
        pft->dwLowDateTime );
}        
