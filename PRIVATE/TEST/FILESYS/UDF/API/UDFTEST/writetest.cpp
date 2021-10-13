//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//
#include "writetest.h"
#include "pathtable.h"

#undef __FILE_NAME__
#define __FILE_NAME__    TEXT("WRITETEST.CPP")

//
// internal test functions declarations
//
static BOOL TestRemoveDirectory     ( LPTSTR, const LPWIN32_FIND_DATA );
static BOOL TestCreateDirectory     ( LPTSTR, const LPWIN32_FIND_DATA );
static BOOL TestCreateNewFile       ( LPTSTR, const LPWIN32_FIND_DATA );
static BOOL TestCreateFileForWrite  ( LPTSTR, const LPWIN32_FIND_DATA );
static BOOL TestDeleteFile          ( LPTSTR, const LPWIN32_FIND_DATA );
static BOOL TestDeleteAndRenameFile ( LPTSTR, const LPWIN32_FIND_DATA );
static BOOL TestMoveFile            ( LPTSTR, const LPWIN32_FIND_DATA );
static BOOL TestSetFileAttributes   ( LPTSTR, const LPWIN32_FIND_DATA );
static BOOL TestSetEndOfFile        ( LPTSTR, const LPWIN32_FIND_DATA );
static BOOL TestWriteFile           ( LPTSTR, const LPWIN32_FIND_DATA );
static BOOL TestFlushFileBuffers    ( LPTSTR, const LPWIN32_FIND_DATA );
static BOOL TestSetFileTime         ( LPTSTR, const LPWIN32_FIND_DATA );
static BOOL TestDeleteRenameFile    ( LPTSTR, const LPWIN32_FIND_DATA );
static BOOL TestSetFilePointer      ( LPTSTR, const LPWIN32_FIND_DATA );

//******************************************************************************
TESTPROCAPI 
FSWriteTest(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
//
//  The TUX wrapper routine for the File System Attributes tests.
//
//  Standard Tux arguments, standard Tux return values; see Tux documentation.
//
//  Test Purpose:
//
//      This test is designed to verify the Read-Only properties of the CD/DVD
//      media, and consequently the CDFS and UDF file systems. Each test 
//      variation in this suite tests a modifiable file system property and 
//      verifies the correctness of the generated error code. 
//
//      WARNING: running this test on a modifiable disk (such as your hard
//      disk) can delete all your files. Make sure this test is only run on
//      Read-Only media.
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
    BOOL fSuccess               = FALSE;
    WIN32_FIND_DATA findData    = { 0 };
    DWORD dwFileCount           = 0;
    FindFiles findFiles;

    //
    // String buffers
    //
    TCHAR szPath[_MAX_PATH];
    TCHAR szFile[_MAX_PATH];
    TCHAR szSearch[_MAX_PATH];

    DETAIL("Starting FSWriteTest");
  
    if( g_pPathTable == NULL )
    {
      FAIL("Unable to find media");
      return TPR_FAIL;
    }

    for( UINT i = 0; i < g_pPathTable->GetTotalEntries(); i++ )
    {
        g_pPathTable->GetPath( i, szPath, _MAX_PATH - 2 );
        _stprintf( szSearch, TEXT("%s\\*"), szPath );
                
        while( findFiles.Next( szSearch, &findData ) )
        {
            _stprintf( szFile, TEXT("%s\\%s"), szPath, findData.cFileName );            
            
            //
            // ignore .. and . directories because they only screw it up
            //
            if( _tcscmp( findData.cFileName, TEXT("..") ) &&
                _tcscmp( findData.cFileName, TEXT(".") ) )
            {
                g_pKato->Log( LOG_DETAIL, TEXT("Testing file: %s"), szFile );
                dwFileCount++;
                //
                // dispatch the proper test function based on the user data,
                // an WRITE_TEST value
                //
                switch( lpFTE->dwUserData )
                {
                    case REMOVE_DIRECTORY:
                        fSuccess = TestRemoveDirectory( szFile, &findData );
                        break;
                    case CREATE_DIRECTORY:
                        fSuccess = TestCreateDirectory( szFile, &findData );
                        break;
                    case CREATE_NEW_FILE:
                        fSuccess = TestCreateNewFile( szFile, &findData );
                        break;
                    case CREATE_FILE_WRITE:
                        fSuccess = TestCreateFileForWrite( szFile, &findData );
                        break;
                    case DELETE_FILE:  
                        fSuccess = TestDeleteFile( szFile, &findData ); 
                        break;
                    case DELETE_RENAME_FILE:
                        fSuccess = TestDeleteRenameFile( szFile, &findData );
                        break;
                    case MOVE_FILE:
                        fSuccess = TestMoveFile( szFile, &findData );
                        break;
                    case SET_FILE_ATTRIBUTES:
                        fSuccess = TestSetFileAttributes( szFile, &findData );
                        break;
                    case SET_END_OF_FILE:
                        fSuccess = TestSetEndOfFile( szFile, &findData );
                        break;
                    case WRITE_FILE:
                        fSuccess = TestWriteFile( szFile, &findData );
                        break;
                    case FLUSH_FILE_BUFFERS:
                        fSuccess = TestFlushFileBuffers( szFile, &findData );
                        break;
                    case SET_FILE_TIME:
                        fSuccess = TestSetFileTime( szFile, &findData );
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
                    return TPR_FAIL;
                }
            }
        }    
    }
    g_pKato->Log( LOG_DETAIL, TEXT("Tests ran on %d files"), dwFileCount );
    if( 0 == dwFileCount )
        return TPR_FAIL;
   
    return TPR_PASS;
}

//******************************************************************************   
static BOOL
TestRemoveDirectory(
    LPTSTR szFileName,
    const LPWIN32_FIND_DATA pFindData )     
//******************************************************************************       
{

    ASSERT( pFindData );
    ASSERT( szFileName );

    DWORD dwLastErr   = 0;
    
    //
    // attempt to remove the directory should fail because the file system
    // is read-only
    //
    if( RemoveDirectory( szFileName ) )
    {
        FAIL("RemoveDirectory succeeded on Read-Only file system");
        return FALSE;        
    }

    //
    // the system  error should be "access denied" if it is a directory
    //
    dwLastErr = GetLastError();
    if( ( ERROR_ACCESS_DENIED == dwLastErr ) &&
        ( FILE_ATTRIBUTE_DIRECTORY & pFindData->dwFileAttributes ) )
    {
        SUCCESS("TestWrite");
        return TRUE;
    }

    //
    // if it was a file rather than a directory, then it should be an
    // invalid directory name error
    //
    if( ( ERROR_DIRECTORY == dwLastErr ) && 
        !( FILE_ATTRIBUTE_DIRECTORY & pFindData->dwFileAttributes ) )
    {
        SUCCESS("TestWrite");
        return TRUE;
    }

    //
    // The attempt failed, but the wrong error code resulted
    //
    FAIL("RemoveDirectory failed with unexpected error code");
    SystemErrorMessage( dwLastErr );
    return FALSE;
}

//******************************************************************************   
static BOOL
TestCreateDirectory(
    LPTSTR szFileName,
    const LPWIN32_FIND_DATA pFindData )
//******************************************************************************       
{
    ASSERT( pFindData );
    ASSERT( szFileName );

    DWORD dwLastErr   = 0;
    TCHAR szNewDirectory[MAX_PATH];

    if( !(pFindData->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) )
    {
        DETAIL("Skipping File");
        return TRUE;
    }

    //
    // Because this is a directory, build the name of the directory to create
    //
    _stprintf( 
        szNewDirectory,     
        TEXT("%s\\%s"), 
        szFileName,
        TEST_DIR_NAME );

    //
    // attempt to create the directory should fail because the file system
    // is read-only
    //
    if( CreateDirectory( szNewDirectory, NULL ) )
    {
        FAIL("CreateDirectory succeeded on Read-Only file system");    
        return FALSE;
    }

    //
    // There should be a read only error if the file didn't exists,
    // there should be an alredy exists error if it does
    // 
    dwLastErr = GetLastError();
    if( (ERROR_ACCESS_DENIED == dwLastErr) ||
        (ERROR_ALREADY_EXISTS == dwLastErr) )
    {
        SUCCESS("TestCreateDirectory");
        return TRUE;
    }
    
    FAIL("CreateDirectory failed with unexpected error code");
    SystemErrorMessage( dwLastErr ); 
    return FALSE;
}

//******************************************************************************   
static BOOL
TestDeleteFile(
    LPTSTR szFileName,
    const LPWIN32_FIND_DATA pFindData )
//******************************************************************************       
{

    UNREFERENCED_PARAMETER( pFindData );
    ASSERT( szFileName );

    DWORD dwLastErr   = 0;

    if( DeleteFile( szFileName ) )
    {
        FAIL("DeleteFile succeeded on Read-Only file system");
        return FALSE;
    }

    dwLastErr = GetLastError();
    if( ERROR_ACCESS_DENIED == dwLastErr )
    {
        SUCCESS("TestDeleteFile");
        return TRUE;
    }

    FAIL("DeleteFile failed with unexpected error code");
    SystemErrorMessage( dwLastErr );  
    return FALSE;
}

//******************************************************************************   
static BOOL
TestCreateNewFile(
    LPTSTR szFileName,
    const LPWIN32_FIND_DATA pFindData )
//******************************************************************************       
{

    ASSERT( pFindData );
    ASSERT( szFileName );

    BOOL fRet       = TRUE;  
    DWORD dwLastErr = 0;

    DWORD creationModes[] = { CREATE_NEW, CREATE_ALWAYS, TRUNCATE_EXISTING };
    LPTSTR szCreationModes[] = { TEXT("CREATE_NEW"), TEXT("CREATE_ALWAYS"), 
        TEXT("TRUNCATE_EXISTING") };
        
    DWORD dwLength = sizeof(creationModes) / sizeof(creationModes[0]);

    //
    // Do not test directories
    //
    if( pFindData->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
    {
        DETAIL("Skipping Directory");
        return TRUE;
    }

    //
    // run CreateFile with each of the creation modes that should fail
    //
    for( DWORD i = 0; i < dwLength; i++ )
    {        
        //
        // Open a file with CREATE_NEW
        //
        if( INVALID_HANDLE_VALUE != CreateFile( szFileName,
                                                GENERIC_READ | GENERIC_WRITE,
                                                FILE_SHARE_READ | FILE_SHARE_WRITE,
                                                NULL,
                                                creationModes[i],
                                                FILE_ATTRIBUTE_NORMAL,
                                                NULL ) )
        {
            //
            // should not be able to create the file
            //
            g_pKato->Log( LOG_FAIL,
                TEXT("FAIL in %s at line %d: CreateFile %s succeeded on Read-Only file system"), 
                __FILE_NAME__, __LINE__, szCreationModes[i] );
            g_pKato->Log( LOG_DETAIL, TEXT("%d"), creationModes[i] );
            fRet = FALSE;
        }
        else
        {
            //
            // if the function failed, check the error code for correctness
            //
            dwLastErr = GetLastError();
            if( ERROR_ACCESS_DENIED != dwLastErr )
            {
                g_pKato->Log( LOG_FAIL,
                    TEXT("FAIL in %s at line %d: CreateFile %s had unexpected error code"), 
                    __FILE_NAME__, __LINE__, szCreationModes[i] );
                fRet = FALSE;
                SystemErrorMessage( dwLastErr );
            }
        }
    }

    return fRet;
}

//******************************************************************************   
static BOOL
TestCreateFileForWrite(
    LPTSTR szFileName,
    const LPWIN32_FIND_DATA pFindData )
//******************************************************************************       
{

    ASSERT( pFindData );
    ASSERT( szFileName );

    BOOL fRet       = TRUE;  
    DWORD dwLastErr = 0;

    //
    // Do not test directories
    //
    if( pFindData->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
    {
        DETAIL("Skipping Directory");
        return TRUE;
    }
    
    //
    // Open a file for write-only operation
    //
    if( INVALID_HANDLE_VALUE != CreateFile( szFileName,
                                            GENERIC_WRITE,
                                            FILE_SHARE_WRITE,
                                            NULL,
                                            OPEN_EXISTING,
                                            FILE_ATTRIBUTE_NORMAL,
                                            NULL ) )
    {
        //
        // should not be able to create the file
        //
        FAIL("CreateFile GENERIC_WRITE");
        fRet = FALSE;
    }
    else
    {
        //
        // if the function failed, check the error code for correctness
        //
        dwLastErr = GetLastError();
        if( ERROR_ACCESS_DENIED != dwLastErr )
        {
            FAIL("CreateFile GENERIC_WRITE had unexpected error code");
            fRet = FALSE;
            SystemErrorMessage( dwLastErr );
        }
    }
    
    return fRet;
}


//******************************************************************************   
static BOOL
TestMoveFile(
    LPTSTR szFileName,
    const LPWIN32_FIND_DATA pFindData )
//******************************************************************************       
{

    UNREFERENCED_PARAMETER( pFindData );
    ASSERT( szFileName );

    DWORD dwLastErr   = 0;

    if( MoveFile( szFileName, szFileName ) )
    {
        FAIL("MoveFile succeeded on Read-Only file system");
        return FALSE;
    }

    dwLastErr = GetLastError();
    if( ERROR_ACCESS_DENIED == dwLastErr )
    {
        SUCCESS("TestMoveFile");
        return TRUE;
    }

    FAIL("MoveFile failed with unexpected error code");
    SystemErrorMessage( dwLastErr );  
    return FALSE;
}

//******************************************************************************   
static BOOL
TestDeleteRenameFile(
    LPTSTR szFileName,
    const LPWIN32_FIND_DATA pFindData )
//******************************************************************************       
{

    UNREFERENCED_PARAMETER( pFindData );
    ASSERT( szFileName );

    DWORD dwLastErr   = 0;
    
#if UNDER_CE
    if( DeleteAndRenameFile( szFileName, szFileName ) )
    {
        FAIL("DeleteAndRename succeeded on Read-Only file system");
        return FALSE;
    }
#else
    return TRUE;
#endif

    dwLastErr = GetLastError();
    if( ERROR_ACCESS_DENIED == dwLastErr )
    {
        SUCCESS("TestDeleteAndRenameFile");
        return TRUE;
    }

    FAIL("DeleteAndRename failed with unexpected error code");
    SystemErrorMessage( dwLastErr );  
    return FALSE;
}


//******************************************************************************   
static BOOL
TestSetFileAttributes(
    LPTSTR szFileName,
    const LPWIN32_FIND_DATA pFindData )
//******************************************************************************       
{

    UNREFERENCED_PARAMETER( pFindData );
    ASSERT( szFileName );
    
    DWORD dwLastErr = 0;

    //
    // Attempt to set file attributes to normal
    //
    if( SetFileAttributes( szFileName, FILE_ATTRIBUTE_NORMAL ) )
    {
        FAIL("SetFileAttributes succeeded on a Read-Only file system");
        return FALSE;
    }

    //
    // Should have failed, check correctness of error code
    //
    dwLastErr = GetLastError();
    if( ERROR_ACCESS_DENIED == dwLastErr )
    {
        SUCCESS("TestSetFileAttributes");
        return TRUE;
    }

    //
    // Failed, but error code was incorrect
    //
    FAIL("SetFileAttributes failed with unexpected error code");
    SystemErrorMessage( dwLastErr );
    return FALSE;
}

//******************************************************************************   
static BOOL
TestSetEndOfFile(
    LPTSTR szFileName,
    const LPWIN32_FIND_DATA pFindData )
//******************************************************************************       
{

    UNREFERENCED_PARAMETER( pFindData );
    ASSERT( szFileName );

    DWORD dwLastErr = 0;

    if( pFindData->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
    {
        DETAIL("Skipping Directory");
        return TRUE;
    }
    
    HANDLE hFile    = CreateFile(   szFileName,
                                    GENERIC_READ | GENERIC_WRITE,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                                    NULL,
                                    OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL,
                                    NULL );
    //
    // Check the file handle
    //
    if( INVALID_HANDLE( hFile ) )
    {
        FAIL("CreateFile");
        SystemErrorMessage( GetLastError() );
        return FALSE;
    }

    //
    // Attempt SetEndOfFile
    //    
    if( SetEndOfFile( hFile ) )
    {  
        FAIL("SetEndOfFile succeeded on a Read-only file system");
        if( !CloseHandle( hFile ) )
        {
            FAIL("CloseHandle");
            SystemErrorMessage( GetLastError() );
        }
        return FALSE;
    }
    dwLastErr = GetLastError();    

    //
    // Close the file handle
    //
    if( !CloseHandle( hFile ) )
    {
        FAIL("CloseHandle");
        SystemErrorMessage( GetLastError() );
    }

    //
    // Should have failed, check correctness of error code
    //
    if( ERROR_ACCESS_DENIED == dwLastErr )
    {
        SUCCESS("TestSetEndOfFile");
        return TRUE;
    }

    //
    // Failed, but error code was incorrect
    //
    FAIL("SetEndOfFile failed with unexpected error code");
    SystemErrorMessage( dwLastErr );  
    return FALSE;
}

//******************************************************************************   
static BOOL
TestWriteFile(
    LPTSTR szFileName,
    const LPWIN32_FIND_DATA pFindData )
//******************************************************************************       
{

    UNREFERENCED_PARAMETER( pFindData );
    ASSERT( szFileName );

    //
    // Do not test directories
    //
    if( pFindData->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
    {
        DETAIL("Skipping Directory");
        return TRUE;
    }

    DWORD dwLastErr = 0;
    BYTE bBuffer[WRITE_SIZE] = {0};
    DWORD dwWritten = 0;  
    
    HANDLE hFile = CreateFile(  szFileName,
                                GENERIC_READ | GENERIC_WRITE,
                                FILE_SHARE_READ | FILE_SHARE_WRITE,
                                NULL,
                                OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL,
                                NULL );
    //
    // Check the file handle
    //
    if( INVALID_HANDLE( hFile ) )
    {
        FAIL("CreateFile");
        return FALSE;
    }

    //
    // Attempt To write to the file
    //    
    if( WriteFile(  hFile,
                    bBuffer,
                    WRITE_SIZE,
                    &dwWritten,
                    NULL ) )
    {  
        FAIL("WriteFile succeeded on a Read-only file system");
        if( !CloseHandle( hFile ) )
        {
            FAIL("CloseHandle");
        }
        return FALSE;
    }
    dwLastErr = GetLastError();

    //
    // close the file handle
    //
    if( !CloseHandle( hFile ) )
    {
        FAIL("CloseHandle");
    }

    //
    // Should have failed, check correctness of error code
    //    
    if( ERROR_ACCESS_DENIED == dwLastErr )
    {
        SUCCESS("TestWriteFile");
        return TRUE;
    }

    //
    // Failed, but error code was incorrect
    //
    FAIL("WriteFile failed with unexpected error code");
    SystemErrorMessage( dwLastErr );  
    return FALSE;
}


//******************************************************************************   
static BOOL
TestFlushFileBuffers(
    LPTSTR szFileName,
    const LPWIN32_FIND_DATA pFindData )
//******************************************************************************       
{

    UNREFERENCED_PARAMETER( pFindData );
    ASSERT( szFileName );
    
    //
    // Do not test directories
    //
    if( pFindData->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
    {
        DETAIL("Skipping Directory");
        return TRUE;
    }

    DWORD dwLastErr = 0;
    HANDLE hFile = CreateFile(  szFileName,
                                GENERIC_READ | GENERIC_WRITE,
                                FILE_SHARE_READ | FILE_SHARE_WRITE,
                                NULL,
                                OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL,
                                NULL );
    //
    // Check the file handle
    //
    if( INVALID_HANDLE( hFile ) )
    {
        FAIL("CreateFile");
        return FALSE;
    }

    //
    // Attempt to Flush the file buffers
    //    
    if( FlushFileBuffers( hFile ) )
    {
        FAIL("FlushFileBuffers succeeded on a Read-only file system");
        if( !CloseHandle( hFile ) )
        {
            FAIL("CloseHandle");
        }
        return FALSE;
    } 
    dwLastErr = GetLastError();    
    
    //
    // close the file handle
    //
    if( !CloseHandle( hFile ) )
    {
        FAIL("CloseHandle");
    }

    //
    // Should have failed, check correctness of error code
    //
    if( ERROR_ACCESS_DENIED == dwLastErr )
    {
        SUCCESS("TestFlushFileBuffers");
        return TRUE;
    }

    //
    // Failed, but error code was incorrect
    //
    FAIL("FlushFileBuffers failed with unexpected error code");
    SystemErrorMessage( dwLastErr );  
    return FALSE;
}

//******************************************************************************   
static BOOL
TestSetFileTime(
    LPTSTR szFileName,
    const LPWIN32_FIND_DATA pFindData )
//******************************************************************************       
{

    ASSERT( pFindData );
    ASSERT( szFileName );
    
    //
    // Do not test directories
    //
    if( pFindData->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
    {
        DETAIL("Skipping Directory");
        return TRUE;
    }

    DWORD dwLastErr = 0;
    FILETIME ft     = {0};
    
    HANDLE hFile = CreateFile(  szFileName,
                                GENERIC_READ | GENERIC_WRITE,
                                FILE_SHARE_READ | FILE_SHARE_WRITE,
                                NULL,
                                OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL,
                                NULL );

    dwLastErr = GetLastError();
    
    //
    // Check the file handle
    //
    if( INVALID_HANDLE( hFile ) )
    {
        FAIL("CreateFile");
        SystemErrorMessage( dwLastErr );
        return FALSE;
    }

    //
    // Attempt to set the file time
    //    
    if( SetFileTime( hFile, &ft, &ft, &ft ) )
    {
        FAIL("SetFileTime succeeded on a Read-only file system");
        if( !CloseHandle( hFile ) )
        {
            FAIL("CloseHandle");
        }
        return FALSE;
    } 
    dwLastErr = GetLastError();
    
    //
    // close the file handle
    //
    if( !CloseHandle( hFile ) )
    {
        FAIL("CloseHandle");
    }

    //
    // Should have failed, check correctness of error code
    //
    if( ERROR_ACCESS_DENIED == dwLastErr )
    {
        SUCCESS("TestSetFileTime");
        return TRUE;
    }

    //
    // Failed, but error code was incorrect
    //
    FAIL("SetFileTime failed with unexpected error code");
    SystemErrorMessage( dwLastErr );  
    return FALSE;
}

