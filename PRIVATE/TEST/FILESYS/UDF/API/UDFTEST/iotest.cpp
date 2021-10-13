//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//
#include "iotest.h"

#undef __FILE_NAME__
#define __FILE_NAME__   TEXT("IOTEST.CPP")

//
// internal test function declarations
//
static BOOL TestReadFile( LPCTSTR, DWORD, DWORD, LPDWORD );
static BOOL ReadInFile( HANDLE, DWORD, BOOL, LPDWORD );

//******************************************************************************
//
// File Attributes
//
const DWORD g_rgdwFileAttributes[] = 
{ FILE_ATTRIBUTE_NORMAL, FILE_FLAG_RANDOM_ACCESS };

const LPTSTR g_rgszFileAttributeDesc[] =
{ TEXT("FILE_ATTRIBUTE_NORMAL"), TEXT("FILE_FLAG_RANDOM_ACCESS") };

const UINT g_unFileAttributes = sizeof( g_rgdwFileAttributes ) / sizeof( DWORD );

//******************************************************************************

//******************************************************************************
TESTPROCAPI 
FSIoTest(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
//
//  The TUX wrapper routine for the File System I/O tests.
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
    FindFiles findFiles;
    DWORD dwFileCount           = 0;  
    DWORD dwBufferSize          = lpFTE->dwUserData; // read buffer size

    //
    // String buffers
    //
    TCHAR szPath[_MAX_PATH];
    TCHAR szFile[_MAX_PATH];
    TCHAR szSearch[_MAX_PATH];

    DETAIL("Starting FSIoTest");

    if( g_pPathTable == NULL )
    {
      FAIL("Unable to find media");
      return TPR_FAIL;
    }

    //
    // Loop through each directory listed in the path table
    //
    for( UINT i = 0; i < g_pPathTable->GetTotalEntries(); i++ )
    {
        g_pPathTable->GetPath( i, szPath, _MAX_PATH - 2 );
        _stprintf( szSearch, TEXT("%s\\*"), szPath );

        //
        // Loop through each file in the directory (including directories)
        //
        while( findFiles.Next( szSearch, &findData ) )
        {
            _stprintf( szFile, TEXT("%s\\%s"), szPath, findData.cFileName );

            //
            // If it is a file, read it and verify that it was read correctly
            //
            if( !(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) )
            {
                DWORD dwBytesRead;
                dwFileCount++;
                g_pKato->Log( LOG_DETAIL, TEXT("Testing file: %s, size %d bytes"), 
                    szFile, findData.nFileSizeLow );  

                fSuccess = TRUE;
                
                for( UINT unAttrib = 0; unAttrib < g_unFileAttributes; unAttrib++ )
                {
                    //
                    // Compare the file sizes and make sure they match; note that
                    // this won't work on files over 2GB because we don't check the
                    // nFileSizeHigh field in WIN32_FIND_DATA
                    //
                    if( !TestReadFile( szFile,
                                        g_rgdwFileAttributes[unAttrib],
                                        dwBufferSize,
                                        &dwBytesRead ) )
                    {                        
                        FAIL("File data check error");
                        g_pKato->Log( LOG_FAIL,
                            TEXT("make sure %s is a test file"),
                            szFile );
                        fSuccess = FALSE;
                        break; // stop testing this file
                    }

                    if( dwBytesRead != findData.nFileSizeLow )
                    {
                        FAIL("File size mismatch");
                        g_pKato->Log( LOG_FAIL, TEXT("Read: %d bytes, Actual: %d bytes"),
                            dwBytesRead, findData.nFileSizeLow );
                        fSuccess = FALSE;
                        break; // stop testing this file
                    }

                } // end for each file attribute                                

                //
                // see if the file passed all variations
                //
                if( FALSE == fSuccess )
                {   
                    g_pKato->Log( LOG_FAIL, TEXT("FILE %s failed test"), szFile );
                    fRet = FALSE;
                }
                else
                {
                    g_pKato->Log( LOG_PASS, TEXT("PASS: %s"), szFile );
                }
            } // end if file
            
        } // end for each file 
        
    } // end for each directory
    
    g_pKato->Log( LOG_DETAIL, TEXT("Tests ran on %d files"), dwFileCount );
    
    if( 0 == dwFileCount || 
        FALSE == fRet )
        return TPR_FAIL;
   
    return TPR_PASS;
}



//******************************************************************************
static BOOL
TestReadFile( 
    LPCTSTR szFileName,
    DWORD dwFileAttributes,
    DWORD dwBufferSize,
    LPDWORD lpdwBytesRead )
//
//  Given a file name, access attributes and flags, and a buffer size, this 
//  function opens a file handle calls ReadInFile on it. The number of bytes
//  read is output in lpdwBytesRead.
//
//  Return value: TRUE if the data check passes, FALSE if the check fails.
//
//******************************************************************************    
{
    
    ASSERT( szFileName );
    ASSERT( dwBufferSize > 0 );

    HANDLE hFile    = INVALID_HANDLE_VALUE;
    BOOL fRet       = FALSE;
    BOOL fTestFile  = FALSE;
    
    *lpdwBytesRead  = 0;

    //
    // open a handle to the file
    //
    hFile = CreateFile( szFileName,
                        GENERIC_READ,
                        FILE_SHARE_READ,
                        NULL,
                        OPEN_EXISTING,
                        dwFileAttributes,
                        NULL );

    if( INVALID_HANDLE( hFile ) )
    {
        FAIL("CreateFile");
        SystemErrorMessage( GetLastError() );
        return 0;
    }   

    fTestFile = IsTestFileName(szFileName);

    if( fTestFile && g_fCheckData )
      DETAIL("This is a test file. Will check data.");
    
    fRet = ReadInFile( hFile, dwBufferSize, (g_fCheckData && fTestFile), lpdwBytesRead );

    //
    // close the file handle
    //
    if( !CloseHandle( hFile ) )
    {
        FAIL("CloseHandle");
    }   
    return fRet;    
}

//****************************************************************************** 
static BOOL 
ReadInFile(
    HANDLE hFile,
    DWORD dwBufferSize,
    BOOL fCheckData,
    LPDWORD lpdwBytesRead )
//******************************************************************************     
{

    BOOL fRet           = TRUE;
    DWORD dwBytesRead   = 0;
    LPBYTE lpbBuffer    = new BYTE[dwBufferSize];
    DWORD dwByteCount   = 0;
    BYTE bMatch         = 0x00;

    if( NULL == lpbBuffer )
    {
        fRet = FALSE;
        FAIL("unable to allocate buffer");
        goto Exit;
    }
    
    if( INVALID_HANDLE( hFile ) )
    {
        fRet = FALSE;
        FAIL("Invalid file handle");
        goto Exit;
    }

    if( 0 == dwBufferSize )
    {
        fRet = FALSE;
        goto Exit;
    }

    bMatch = 0;
    
    do
    {
        //
        // zero the buffer
        //
        if( fCheckData )
        {
            memset( lpbBuffer, 0, dwBufferSize );
        }

        if( !ReadFile( hFile, lpbBuffer, dwBufferSize, &dwBytesRead, NULL ) )
        {
            fRet = TRUE;
            FAIL("ReadFile");
            goto Exit;
        }
              
        //
        // verify that the byte value matches its offset into the file modulo 0xFF        
        //        
        if( fCheckData )
        {
            dwByteCount = dwBytesRead;
            for( dwByteCount = 0; dwByteCount < dwBytesRead; dwByteCount++ )
            {
                if( bMatch != lpbBuffer[dwByteCount] )
                {
                    g_pKato->Log( LOG_FAIL, 
                                  TEXT("offset 0x%08X Data Mismatch 0x%02X != 0x%02X"), 
                                  (*lpdwBytesRead + dwByteCount), 
                                  lpbBuffer[dwByteCount], 
                                  bMatch );
                    fRet = FALSE;
                }
                
                bMatch++; // will overflow at 0xFF, this is expected
            }
        }

        //
        // see if the requested amout of data bytes were returned
        //
        if( dwBytesRead != dwBufferSize &&
            dwBytesRead != 0 )
        {
            //
            // this usually happens on the last read from a file when the remaining bytes
            // won't fill the buffer completely
            //
            g_pKato->Log( LOG_DETAIL, TEXT("ReadFile returned fewer bytes than requested: %u"), dwBytesRead );
        }
        *lpdwBytesRead += dwBytesRead;  
    }
    while( dwBytesRead > 0 );


Exit:
    if( lpbBuffer ) delete[] lpbBuffer;
    
    return fRet;    
}
