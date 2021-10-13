//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//
#include "memmaptest.h"

#undef __FILE_NAME__
#define __FILE_NAME__   TEXT("MEMMAPTEST.CPP")

//******************************************************************************
//
// File Mapping Access Modes
//
// Note that FILE_MAP_WRITE, and FILE_MAP_ALL_ACCESS are not ACTUALLY supported
// under CE even though MSDN says they are
const DWORD g_rgdwMapAccessModes[] = 
{ FILE_MAP_READ }; //, FILE_MAP_WRITE, FILE_MAP_ALL_ACCESS };

const LPTSTR g_rgszMapAccessDesc[] = 
{ TEXT("FILE_MAP_READ") }; //, TEXT("FILE_MAP_WRITE"), TEXT("FILE_MAP_ALL_ACCESS") };

const UINT g_unAccessModes = sizeof( g_rgdwMapAccessModes ) / sizeof( DWORD );

//
// File Mapping Protection Modes
//
// PAGE_READWRITE fails
const DWORD g_rgdwMapProtectionModes[] =
{ PAGE_READONLY, PAGE_READWRITE };

const LPTSTR g_rgszMapProtectionDesc[] =
{ TEXT("PAGE_READONLY"), TEXT("PAGE_READWRITE") };

const UINT g_unProtectionModes = sizeof( g_rgdwMapProtectionModes ) / sizeof( DWORD );

//
// File Attributes
//
const DWORD g_rgdwFileAttributes[] = 
{ FILE_ATTRIBUTE_NORMAL, FILE_FLAG_RANDOM_ACCESS };

const LPTSTR g_rgszFileAttributeDesc[] =
{ TEXT("FILE_ATTRIBUTE_NORMAL"), TEXT("FILE_FLAG_RANDOM_ACCESS") };

const UINT g_unFileAttributes = sizeof( g_rgdwFileAttributes ) / sizeof( DWORD );

//******************************************************************************

//
// internal test function declarations
//
static BOOL MapAndReadFile( LPCTSTR, LPCTSTR, LPWIN32_FIND_DATA, DWORD, DWORD, DWORD );

//******************************************************************************
TESTPROCAPI 
FSMappedIoTest(
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
    UNREFERENCED_PARAMETER(lpFTE);
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
    WIN32_FIND_DATA findData    = { 0 };
    FindFiles findFiles;
    DWORD dwFileCount           = 0;

        
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
            // If it is a file, perform the mapped Io test
            //
            if( !(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) )
            {
                g_pKato->Log( LOG_DETAIL, TEXT("File: %s"), szFile );

                //
                // Loop through each Access Mode
                //
                for( UINT unAccess = 0; unAccess < g_unAccessModes; unAccess++ )
                {
                    //
                    // Loop through each Protection Mode
                    //
                    for( UINT unProtect = 0; unProtect < g_unProtectionModes; unProtect++ )
                    {          
                        //
                        // Loop through each File Attribute
                        //
                        for( UINT unAttrib = 0; unAttrib < g_unFileAttributes; unAttrib++ )
                        {
                            g_pKato->Log( LOG_DETAIL, TEXT("    File Attribute: %s"),
                                g_rgszFileAttributeDesc[unAttrib] );
                            g_pKato->Log( LOG_DETAIL, TEXT("    Map Access: %s"),
                                g_rgszMapAccessDesc[unAccess] );
                            g_pKato->Log( LOG_DETAIL, TEXT("    Map Protection: %s"),
                                g_rgszMapProtectionDesc[unProtect] );

                            if(!MapAndReadFile( szFile,
                                                TEXT("Map Object"),
                                                &findData,
                                                g_rgdwFileAttributes[unAttrib],
                                                g_rgdwMapAccessModes[unAccess],
                                                g_rgdwMapProtectionModes[unProtect] ) )
                            {
                                FAIL("Mapped Io Test");
                                fSuccess = FALSE;
                            }  
                            
                        } // end for each file attribute

                    } // end for each protection mode
                    
                } // end for each access mode
                
                //SUCCESS("File passed all test variations");
                dwFileCount++;
                
            } // end if file
            
        } // end for each file
        
    } // end for each directory

    g_pKato->Log( LOG_DETAIL, TEXT("Tests ran on %d files"), dwFileCount );
    if( 0 == dwFileCount )
        return TPR_FAIL;
   
    return fSuccess ? TPR_PASS : TPR_FAIL;
}



//******************************************************************************
static BOOL
MapAndReadFile( 
    LPCTSTR szFileName,             // file name
    LPCTSTR szMapName,              // name for the file mapping object
    LPWIN32_FIND_DATA pFindData,    // pointer to find data structure
    DWORD dwFileAttributes,         // file attribute flags for CreateFile( )
    DWORD dwMapAccessMode,          // access flags for MapViewOfFile( )
    DWORD dwMapProtectionMode )     // protection flags for CreateFileMapping( )
//
//  Given input data, this function opens and memory maps the specified file,
//  reads the entire file, and verifies the data. Because the file system is
//  expected to be Read-Only, errors are expected when dwMapAccessMode and
//  dwMapProectionMode allow writes to occur.
//
//  Returns TRUE if the test passes, FALSE otherwise.
//
//******************************************************************************    
{
    HANDLE hFile        = INVALID_HANDLE_VALUE;
    HANDLE hMap         = INVALID_HANDLE_VALUE;
    DWORD dwLastErr     = 0;
    PBYTE pbBuffer      = NULL;    
    BOOL fRet           = TRUE;
    BOOL fTestFile      = FALSE;
    UINT i              = 0;
    BYTE bTemp          = 0x00;
    BYTE bMatch         = 0x00;
    
    //
    // Open file and validate file handle --
    // Note that CE has a different version of CreateFile specifically
    // for files that will mapped
    //

    //
    // skip zero sized files because it will fail on UnmapViewOfFile
    //
    if( pFindData->nFileSizeLow == 0 )
    {
        DETAIL("Skipping zero sized file for mapping");
        goto Exit;
    }
    
#ifdef UNDER_CE
    hFile = CreateFileForMapping( szFileName,
                                  GENERIC_READ,
                                  FILE_SHARE_READ,
                                  NULL,
                                  OPEN_EXISTING,
                                  dwFileAttributes,
                                  NULL );
#else
    hFile = CreateFile( szFileName,
                        GENERIC_READ,
                        FILE_SHARE_READ,
                        NULL,
                        OPEN_EXISTING,
                        dwFileAttributes,
                        NULL );
#endif

    dwLastErr = GetLastError();                            
    if( INVALID_HANDLE( hFile ) )
    {
        FAIL("CreateFile");
        SystemErrorMessage( dwLastErr );
        fRet = FALSE;
        goto Exit;
    }

    //
    // Create file mapping and verify map handle
    //
    hMap = CreateFileMapping(   hFile,
                                NULL,
                                dwMapProtectionMode,
                                0,
                                0,
                                szMapName );
    dwLastErr = GetLastError();                                            
    if( INVALID_HANDLE( hMap ) )
    {        
        CloseHandle( hFile );
        
        //
        // mapping should fail on zero sized files
        //
        if( ( 0 == pFindData->nFileSizeLow ) &&
            ( ERROR_FILE_INVALID == dwLastErr ) )
        {
            DETAIL("Unable to memory map empty files");
            fRet = TRUE;
            goto Exit;
        }

        //
        // mapping will fail if the map is writable
        //
        else if( ERROR_ACCESS_DENIED == dwLastErr &&
                 PAGE_READWRITE == dwMapProtectionMode )
        {
            fRet = TRUE;
            goto Exit;
        }

        //
        // otherwise the error was bad
        //
        else
        {
            FAIL("CreateFileMapping");    
            SystemErrorMessage( dwLastErr );            
            fRet = FALSE;
            goto Exit;
        }
    }

    //
    // Map the file to memory and verify that the mapping 
    // succeeded
    //
    pbBuffer = (PBYTE) MapViewOfFile( hMap,
                                      dwMapAccessMode,
                                      0, 0, 0 );
    dwLastErr = GetLastError();                                     
    if( NULL == pbBuffer )
    {
        CloseHandle( hMap );
        CloseHandle( hFile );
        
        //
        // Mapping will fail "access denied" when access mode is WRITE,
        // or ALL_ACCESS -- this is the desired behavior on read-only systems
        //
        if( ( ERROR_ACCESS_DENIED == dwLastErr ) &&
            ( ( FILE_MAP_WRITE == dwMapAccessMode ) ||
              ( FILE_MAP_ALL_ACCESS == dwMapAccessMode ) ) )
        {         
            DETAIL("Unable to map file in WRITE mode");
            fRet = TRUE;
            goto Exit; // success
        }
              
        else
        {
            FAIL("MapViewOfFile");
            SystemErrorMessage( dwLastErr );
            fRet = FALSE;
            goto Exit;
        }
    }

    bMatch = 0;
    fTestFile = IsTestFileName(szFileName);

    //
    // run through the array, verify data if necessary
    //
    __try {
    
      for( i = 0; i < pFindData->nFileSizeLow; i++ )
      {        
          bTemp = pbBuffer[i];

          if( g_fCheckData && fTestFile )
          {
              if( bMatch != bTemp )
              {
                  g_pKato->Log( 
                      LOG_FAIL, TEXT("offset 0x%08X Data Mismatch 0x%02X != 0x%02X"), 
                                i, bTemp, bMatch );            
                  fRet = FALSE;
              }
          }
          
          bMatch++; // will overflow, this is expected
      }
    }

    //
    // in case we hit an inaccessible part of the array
    //
    __except( EXCEPTION_EXECUTE_HANDLER )
    {
        FAIL("Accessing memory mapped array caused an exception!");
        fRet = FALSE;
    }

    //
    // Unmap the file
    //
    if( !UnmapViewOfFile( pbBuffer ) )
    {
        FAIL("UnmapViewOfFile");
        SystemErrorMessage( GetLastError() );
        fRet = FALSE;
        goto Exit;
    }
    
Exit:
    if( VALID_HANDLE( hMap  ) ) CloseHandle( hMap  );
    if( VALID_HANDLE( hFile ) ) CloseHandle( hFile );

    if( fRet )
        SUCCESS("Test Mapped I/O File Read");
    return fRet;
}

