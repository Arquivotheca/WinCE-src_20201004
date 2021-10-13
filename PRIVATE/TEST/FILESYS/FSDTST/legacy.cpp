//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "legacy.h"

//****************************************************************************
void MV_TRACE(LPCTSTR szFormat, ...) 
{

   va_list pArgs; 
   va_start(pArgs, szFormat);
   g_pKato->CommentV(LOG_COMMENT, szFormat, pArgs);
   va_end(pArgs);
}

//******************************************************************************
BOOL FileExists(LPTSTR pszFileName)
{
    WIN32_FIND_DATA ff;
    HANDLE          hFF;

    hFF = FindFirstFile( pszFileName, &ff );

    if ( INVALID_HANDLE_VALUE != hFF )
    {
        FindClose( hFF );
        return TRUE;
    }
    else
        return FALSE;
}
/****************************************************************************

    FUNCTION:       FileSize

    PURPOSE:        Returns the size of a file passed in by its string name.
                
****************************************************************************/
DWORD FileSize(TCHAR *szFileName )
{
    DWORD       dwFileSize  = 0;
    HANDLE      hFile=INVALID_HANDLE_VALUE ;
    TCHAR       szLogString[128];
    DWORD       dwLastError=0;

    hFile = CreateFile(szFileName, 0, 0, 
                       NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    dwLastError = GetLastError();
    
    if (INVALID_HANDLE_VALUE == hFile )
    {
        wsprintf(szLogString, TEXT("ERROR : Unable to open file %s in FileSize()"), szFileName) ;
        MV_TRACE(szLogString );
        wsprintf(szLogString, TEXT("Last error == 0x%08X"), dwLastError );
        MV_TRACE(szLogString );
        ERRFAIL("CreateFile()");
    }
    else
    {
        dwFileSize = GetFileSize(hFile, NULL );
        CloseHandle(hFile );
    }

    return dwFileSize;
}


/****************************************************************************

    FUNCTION:   TestAllFileAttributes

    PURPOSE:    Calls TestFileAttributes() with all possible combinations
                of attributes.
    
    PARAMETERS: szFileSpec, Path and file name to create for testing.
                nBufSize, Size of the file to create.
                
****************************************************************************/
BOOL TestAllFileAttributes(TCHAR *szFileSpec, DWORD nBufSize)
{
    BOOL bReturnValue = TRUE;

    MV_TRACE(TEXT("Testing with NORMAL") );
    bReturnValue = TestFileAttributes(szFileSpec, FILE_ATTRIBUTE_NORMAL, TRUE, nBufSize );
    CHECKTRUE(bReturnValue);

    MV_TRACE(TEXT("Testing with ARCHIVE") );
    bReturnValue = TestFileAttributes(szFileSpec, FILE_ATTRIBUTE_ARCHIVE, TRUE, nBufSize );
    CHECKTRUE(bReturnValue);

    MV_TRACE(TEXT("Testing with HIDDEN") );
    bReturnValue = TestFileAttributes(szFileSpec, FILE_ATTRIBUTE_HIDDEN, TRUE, nBufSize );
    CHECKTRUE(bReturnValue);

    MV_TRACE(TEXT("Testing with READONLY") );
    bReturnValue = TestFileAttributes(szFileSpec, FILE_ATTRIBUTE_READONLY, FALSE, nBufSize );
    CHECKTRUE(bReturnValue);

    MV_TRACE(TEXT("Testing with SYSTEM") );
    bReturnValue = TestFileAttributes(szFileSpec, FILE_ATTRIBUTE_SYSTEM, TRUE, nBufSize );
    CHECKTRUE(bReturnValue);

    MV_TRACE(TEXT("Testing with ARCHIVE | HIDDEN") );
    bReturnValue = TestFileAttributes(szFileSpec, FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_HIDDEN, 
                                      TRUE, nBufSize );
    CHECKTRUE(bReturnValue);

    MV_TRACE(TEXT("Testing with ARCHIVE | READONLY") );
    bReturnValue = TestFileAttributes(szFileSpec, FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_READONLY, 
                                      FALSE, nBufSize );
    CHECKTRUE(bReturnValue);

    MV_TRACE(TEXT("Testing with ARCHIVE | SYSTEM") );
    bReturnValue = TestFileAttributes(szFileSpec, FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_SYSTEM, 
                                      TRUE, nBufSize );
    CHECKTRUE(bReturnValue);

    MV_TRACE(TEXT("Testing with HIDDEN | READONLY") );
    bReturnValue = TestFileAttributes(szFileSpec, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY, 
                                      FALSE, nBufSize );
    CHECKTRUE(bReturnValue);

    MV_TRACE(TEXT("Testing with HIDDEN | SYSTEM") );
    bReturnValue = TestFileAttributes(szFileSpec, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM, 
                                      TRUE, nBufSize );
    CHECKTRUE(bReturnValue);

    MV_TRACE(TEXT("Testing with READONLY | SYSTEM") );
    bReturnValue = TestFileAttributes(szFileSpec, FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_SYSTEM, 
                                      FALSE, nBufSize );
    CHECKTRUE(bReturnValue);

    MV_TRACE(TEXT("Testing with ARCHIVE | HIDDEN | READONLY") );
    bReturnValue = TestFileAttributes(szFileSpec, FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY, 
                                      FALSE, nBufSize );
    CHECKTRUE(bReturnValue);

    MV_TRACE(TEXT("Testing with ARCHIVE | HIDDEN | SYSTEM") );
    bReturnValue = TestFileAttributes(szFileSpec, FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM, 
                                      TRUE, nBufSize );
    CHECKTRUE(bReturnValue);

    MV_TRACE(TEXT("Testing with ARCHIVE | READONLY | SYSTEM") );
    bReturnValue = TestFileAttributes(szFileSpec, FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_SYSTEM, 
                                      FALSE, nBufSize );
    CHECKTRUE(bReturnValue);

    MV_TRACE(TEXT("Testing with ARCHIVE | HIDDEN | READONLY | SYSTEM") );
    bReturnValue = TestFileAttributes(szFileSpec, FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_SYSTEM, 
                                      FALSE, nBufSize );
    CHECKTRUE(bReturnValue);

    return bReturnValue;
}

//******************************************************************************
BOOL DumpFileTimeAsSystemTime( FILETIME *pft )
{
    SYSTEMTIME st;
    BOOL bRetVal;
    TCHAR szLogStr[256];

    bRetVal = FileTimeToSystemTime(pft, &st);
    if (bRetVal)
    {
        wsprintf( szLogStr, TEXT("wYear = %u; wMonth = %u; wDay = %u"), st.wYear, st.wMonth, st.wDay );
        g_pKato->Log(LOG_DETAIL, szLogStr );
        wsprintf( szLogStr, TEXT("wHour = %u; wMinute = %u; wSecond = %u; wMilliseconds = %u"), st.wHour, st.wMinute, st.wSecond, st.wMilliseconds );
        g_pKato->Log(LOG_DETAIL, szLogStr );
    }

    return bRetVal;
}

//+ =============================================================================
//- =============================================================================
void L_PrintMiscompareReason(DWORD fdwReasonFailed) 
{
    
    switch(fdwReasonFailed)
    {
    case FC_NOT_ENOUGH_MEMORY    :   // 0x00000001
        MV_TRACE(TEXT("Miscompare reason : FC_NOT_ENOUGH_MEMORY   \n")) ;
        break ;
    case FC_ATTRIBUTE_MISMATCH   :  //0x00000002
        MV_TRACE(TEXT("Miscompare reason : FC_ATTRIBUTE_MISMATCH   \n")) ;
        break ;
    case FC_ERROR_OPENING_FILE   :  //0x00000004
        MV_TRACE(TEXT("Miscompare reason : FC_ERROR_OPENING_FILE   \n")) ;
        break ;
    case FC_FILE_SIZE_MISMATCH   :  //0x00000008
        MV_TRACE(TEXT("Miscompare reason : FC_FILE_SIZE_MISMATCH   \n")) ;
        break ;
    case FC_ERROR_READING_FILE   :  //0x00000010
        MV_TRACE(TEXT("Miscompare reason : FC_ERROR_READING_FILE   \n")) ;
        break ;
    case FC_BUFFER_MISCOMPARE    :  //0x00000020
        MV_TRACE(TEXT("Miscompare reason : FC_BUFFER_MISCOMPARE    \n")) ;
        break ;
    case FC_CANT_GET_FILETIME    :  //0x00000040
        MV_TRACE(TEXT("Miscompare reason : FC_CANT_GET_FILETIME    \n")) ;
        break ;
    case FC_FILETIME_MISMATCH    :  //0x00000080
        MV_TRACE(TEXT("Miscompare reason : FC_FILETIME_MISMATCH    \n")) ;
        break ;
    }
}

/****************************************************************************

    FUNCTION:   CompareFiles

    PURPOSE:    Compares two files, including their attributes and file
                sizes.
    
    PARAMETERS: szSrcFileSpec, First file to compare.
                szDstFileSpec, File to compare szSrcFileSpec to.
                bIgnoreArchiveBit, TRUE if you want it to not warn if the 
                                   archive bit does not match.
                fdwReasonFailed, Pointer to a DWORD flag to store information
                                 about any failures / miscompares.

    NOTE:       It will report warnings as successful, but return non-zero
                in fdwReasonFailed.  If you get a TRUE return but 
                fdwReasonFailed is non-zero then fdwReasonFailed will have 
                the appropriate bits set for the warning(s).

                So far only attribute mismatches are considered warnings.
                
****************************************************************************/
BOOL CompareFiles( TCHAR *szSrcFileSpec, TCHAR *szDstFileSpec, BOOL bIgnoreArchiveBit, DWORD *fdwReasonFailed )
{
    BOOL            bReturnValue    = TRUE;
    BOOL            bCallReturn;
    char            *pBuf1          = NULL;
    char            *pBuf2          = NULL;
    HANDLE          hFile1          = INVALID_HANDLE_VALUE;
    HANDLE          hFile2          = INVALID_HANDLE_VALUE;
    DWORD           dwBufSize       = 4096; //Use 4k buffer because ObjStore uses 4k byte allocation units.
    DWORD           dwFileSize;
    DWORD           dwBytesRead1;
    DWORD           dwBytesRead2;
    DWORD           dwAttrib1;
    DWORD           dwAttrib2;
    DWORD           dwBytesLeftToCompare;
    DWORD           dwBytesToCompare;
    FILETIME        ftCreation1;
    FILETIME        ftCreation2;
    FILETIME        ftLastAccess1;
    FILETIME        ftLastAccess2;
    FILETIME        ftLastWrite1;
    FILETIME        ftLastWrite2;

    *fdwReasonFailed = 0;               //Clear flag for reason failed.

    pBuf1 = (char *)LocalAlloc( 0, dwBufSize );
    pBuf2 = (char *)LocalAlloc( 0, dwBufSize );

    if ( (pBuf1 == NULL) || (pBuf2 == NULL) )
    {
        bReturnValue = FALSE;
        *fdwReasonFailed |= FC_NOT_ENOUGH_MEMORY;
        goto Bail;
    }

    dwAttrib1 = GetFileAttributes(szSrcFileSpec);
    dwAttrib2 = GetFileAttributes(szDstFileSpec);
    if ( bIgnoreArchiveBit )
    {
        //Mask off archive, normal attributes, and compressed.
        dwAttrib1 = dwAttrib1 & (~FILE_ATTRIBUTE_ARCHIVE);
        dwAttrib2 = dwAttrib2 & (~FILE_ATTRIBUTE_ARCHIVE);
        dwAttrib1 = dwAttrib1 & (~FILE_ATTRIBUTE_NORMAL);
        dwAttrib2 = dwAttrib2 & (~FILE_ATTRIBUTE_NORMAL);
        dwAttrib1 = dwAttrib1 & (~FILE_ATTRIBUTE_COMPRESSED);
        dwAttrib2 = dwAttrib2 & (~FILE_ATTRIBUTE_COMPRESSED);
    }
    if ( dwAttrib1 != dwAttrib2 )
    {
        *fdwReasonFailed |= FC_ATTRIBUTE_MISMATCH;
    }
    
    //Attempt to open the files for read.
    hFile1 = CreateFile( szSrcFileSpec, GENERIC_READ, 0, 
                         NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    hFile2 = CreateFile( szDstFileSpec, GENERIC_READ, 0, 
                         NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if ( INVALID_HANDLE_VALUE == hFile1 || INVALID_HANDLE_VALUE == hFile2 )
    {
        bReturnValue = FALSE;
        *fdwReasonFailed |= FC_ERROR_OPENING_FILE;
        goto Bail;
    }

    bCallReturn = GetFileTime( hFile1, &ftCreation1, &ftLastAccess1, &ftLastWrite1 );
    if ( !GetFileTime(hFile2, &ftCreation2, &ftLastAccess2, &ftLastWrite2) 
    || !bCallReturn )
    {
        *fdwReasonFailed |= FC_CANT_GET_FILETIME;
    } 
    else if ( CompareFileTime(&ftLastWrite1, &ftLastWrite2) )
    {
        // We allow a 2 second difference between times since FAT
        // has a second resolution and CEFS is accurate to the nearest 100ns.
        // When we copy files from CEFS to FAT, the file times can vary up to 
        // two seconds.
        DWORDLONG dw1 = *(DWORDLONG*)&ftLastWrite1, dw2 = *(DWORDLONG*)&ftLastWrite2;
        if (((dw1 > dw2) && ((dw1 - dw2) >= (DWORDLONG)20000000)) ||
            ((dw2 > dw1) && ((dw2 - dw1) >= (DWORDLONG)20000000)))
        {
            // Greater than 2 second differnce - flag as error.
            TCHAR       szLogStr[256];
            wsprintf( szLogStr, TEXT("ftCreation1 == %08.08X:%08.08X"), ftCreation1.dwHighDateTime, ftCreation1.dwLowDateTime );
            g_pKato->Log(LOG_DETAIL, szLogStr);
            DumpFileTimeAsSystemTime( &ftCreation1 );
            wsprintf( szLogStr, TEXT("ftCreation2 == %08.08X:%08.08X"), ftCreation2.dwHighDateTime, ftCreation2.dwLowDateTime );
            g_pKato->Log(LOG_DETAIL, szLogStr);
            DumpFileTimeAsSystemTime( &ftCreation2 );
            wsprintf( szLogStr, TEXT("ftLastAccess1 == %08.08X:%08.08X"), ftLastAccess1.dwHighDateTime, ftLastAccess1.dwLowDateTime );
            g_pKato->Log(LOG_DETAIL, szLogStr);
            DumpFileTimeAsSystemTime( &ftLastAccess1 );
            wsprintf( szLogStr, TEXT("ftLastAccess2 == %08.08X:%08.08X"), ftLastAccess2.dwHighDateTime, ftLastAccess2.dwLowDateTime );
            g_pKato->Log(LOG_DETAIL, szLogStr);
            DumpFileTimeAsSystemTime( &ftLastAccess2 );
            wsprintf( szLogStr, TEXT("ftLastWrite1 == %08.08X:%08.08X"), ftLastWrite1.dwHighDateTime, ftLastWrite1.dwLowDateTime );
            g_pKato->Log(LOG_DETAIL, szLogStr);
            DumpFileTimeAsSystemTime( &ftLastWrite1 );
            wsprintf( szLogStr, TEXT("ftLastWrite2 == %08.08X:%08.08X"), ftLastWrite2.dwHighDateTime, ftLastWrite2.dwLowDateTime );
            g_pKato->Log(LOG_DETAIL, szLogStr);
            DumpFileTimeAsSystemTime( &ftLastWrite2 );

            *fdwReasonFailed |= FC_FILETIME_MISMATCH;
        }
    }

    //Reuse dwBytesToCompare below to save memory!
    dwFileSize = GetFileSize(hFile1, &dwBytesToCompare);
    if ( dwFileSize != GetFileSize(hFile2, &dwBytesToCompare) )
    {
        bReturnValue = FALSE;
        *fdwReasonFailed |= FC_FILE_SIZE_MISMATCH;
        goto Bail;
    }
    
    dwBytesLeftToCompare = dwFileSize;
    while ( dwBytesLeftToCompare )
    {
        if ( dwBytesLeftToCompare >= dwBufSize )
            dwBytesToCompare = dwBufSize;
        else
            dwBytesToCompare = dwBytesLeftToCompare;
        
        //Read the data and verify that it is correct.
        bReturnValue = ReadFile( hFile1, pBuf1, dwBytesToCompare, &dwBytesRead1, NULL );
        if ( !bReturnValue || (dwBytesRead1 != dwBytesToCompare) )
        {
            bReturnValue = FALSE;   //In case (dwBytesRead1 != dwBytesToCompare)
            *fdwReasonFailed |= FC_ERROR_READING_FILE;
            goto Bail;
        }
        bReturnValue = ReadFile( hFile2, pBuf2, dwBytesToCompare, &dwBytesRead2, NULL );
        if ( !bReturnValue || (dwBytesRead2 != dwBytesToCompare) )
        {
            bReturnValue = FALSE;   //In case (dwBytesRead2 != dwBytesToCompare)
            *fdwReasonFailed |= FC_ERROR_READING_FILE;
            goto Bail;
        }

        if ( memcmp(pBuf1, pBuf2, dwBytesToCompare) != 0 )
        {
            bReturnValue = FALSE;
            *fdwReasonFailed |= FC_BUFFER_MISCOMPARE;
            goto Bail;
        }

        dwBytesLeftToCompare -= dwBytesToCompare;
    }

Bail:

    if ( hFile1 != INVALID_HANDLE_VALUE )
    {
        CloseHandle( hFile1 );
    }

    if ( hFile2 != INVALID_HANDLE_VALUE )
    {
        CloseHandle( hFile2 );
    }

    if ( pBuf1 )
    {
        LocalFree( pBuf1 );
    }

    if ( pBuf2 )
    {
        LocalFree( pBuf2 );
    }

    return bReturnValue;
}

/****************************************************************************

    FUNCTION:       TestFileAttributes

    PURPOSE:        Tests setting given attributes and attempting to read/write
                the file with these attributes.
    
    PARAMETERS:     szFileSpec, Path and file name to create for testing.
                fdwAttrsAndFlags, Attributes for the file to create.
                bShouldOpen, TRUE if the file should open for read/write.
                             FALSE if the file should not open for write.
                nBufSize, Size of the file to create.
                
****************************************************************************/
BOOL TestFileAttributes(TCHAR *szFileSpec, DWORD fdwAttrsAndFlags, BOOL bShouldOpen, DWORD nBufSize )
{
    BOOL            bReturnValue    = TRUE;
    BOOL            bCallReturn;
    HANDLE          hFile            = INVALID_HANDLE_VALUE;
    DWORD           nBytesWritten;
    DWORD           nBytesRead;
    DWORD           nCount;
    char            *pTestBuffer    = NULL;
    char            *pTempBuffer    = NULL;
    TCHAR           szLogString[256];
    DWORD           dwLastError;
              
    if (nBufSize == 0 )
    {       //Allocate a small buffer so it doesn't GP Fault when it uses the pointers.
        pTestBuffer = (char *)LocalAlloc(0, 1);
        pTempBuffer = (char *)LocalAlloc(0, 1);
    }
    else
    {
        pTestBuffer = (char *)LocalAlloc(0, nBufSize );
        pTempBuffer = (char *)LocalAlloc(0, nBufSize );
    }

    if ((pTestBuffer == NULL) || (pTempBuffer == NULL) )
    {
        MV_TRACE(TEXT("Unable to allocate buffers in TestFileAttributes()") );
        bReturnValue = FALSE;
        goto Bail;
    }

    //Initialize test buffer.
    for (nCount = 0; nCount < nBufSize; nCount++ )
    {
        pTestBuffer[nCount] = (char)nCount;
    } 
    
    //If the file exists, clear its attributes.
    if (FileExists(szFileSpec) )
    {
        MV_TRACE(TEXT("File already exists, clearing its attributes in TestFileAttributes()") );
        bReturnValue = SetFileAttributes(szFileSpec, FILE_ATTRIBUTE_NORMAL );
    }
    
    dwLastError = GetLastError();
    if (!bReturnValue )
    {
        MV_TRACE(TEXT("Unable to clear the attributes in TestFileAttributes()") );
        wsprintf(szLogString, TEXT("Last error == 0x%08X"), dwLastError );
        MV_TRACE(szLogString );
        goto Bail;
    }

    //Create/truncate the file.
    MV_TRACE(TEXT("Creating file in TestFileAttributes()") );
    hFile = CreateFile(szFileSpec, GENERIC_READ | GENERIC_WRITE, 0, 
                        NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    dwLastError = GetLastError();
    if (INVALID_HANDLE_VALUE == hFile )
    {
        bReturnValue = FALSE;
        MV_TRACE(TEXT("Unable to Create/Truncate file in TestFileAttributes()") );
        wsprintf(szLogString, TEXT("Last error == 0x%08X"), dwLastError );
        MV_TRACE(szLogString );
        goto Bail;
    }

    MV_TRACE(TEXT("Writing to file in TestFileAttributes()") );
    bReturnValue = WriteFile(hFile, pTestBuffer, nBufSize, &nBytesWritten, NULL );
    dwLastError = GetLastError();
    if (!bReturnValue || (nBytesWritten != nBufSize) )
    {
        bReturnValue = FALSE;   //In case (nBytesWritten != nBufSize)
        MV_TRACE(TEXT("Error writing buffer to file in TestFileAttributes()") );
        wsprintf(szLogString, TEXT("Last error == 0x%08X"), dwLastError );
        MV_TRACE(szLogString );
        goto Bail;
    }

    nBytesWritten = GetFileSize(hFile, &nBytesRead );
    dwLastError = GetLastError();
    if (nBytesWritten != nBufSize )
    {
        bReturnValue = FALSE;
        wsprintf(szLogString, TEXT("GetFileSize() mismatch, expecting 0x%08.08X got back 0x%08.08X"), nBufSize, nBytesWritten );
        MV_TRACE(szLogString );
        wsprintf(szLogString, TEXT("Last error == 0x%08X"), dwLastError );
        MV_TRACE(szLogString );
        goto Bail;
    }

    MV_TRACE(TEXT("Closing file in TestFileAttributes()") );
    bReturnValue = CloseHandle(hFile );
    hFile=INVALID_HANDLE_VALUE ;
    dwLastError = GetLastError();
    if (!bReturnValue )
    {
        MV_TRACE(TEXT("Unable to close the file in TestFileAttributes()") );
        wsprintf(szLogString, TEXT("Last error == 0x%08X"), dwLastError );
        MV_TRACE(szLogString );
        goto Bail;
    }

    nBytesWritten = FileSize(szFileSpec );
    if (nBytesWritten != nBufSize )
    {        
        TCHAR   szTemp[256];
        bReturnValue = FALSE;
        wsprintf(szTemp, TEXT("FileSize() mismatch, expecting 0x%08.08X got back 0x%08.08X"), nBufSize, nBytesWritten );
        MV_TRACE(szTemp );
        goto Bail;
    }

    MV_TRACE(TEXT("Setting attributes in TestFileAttributes()") );
    bReturnValue = SetFileAttributes(szFileSpec, fdwAttrsAndFlags );
    dwLastError = GetLastError();
    if (!bReturnValue || (fdwAttrsAndFlags != GetFileAttributes(szFileSpec)) )
    {
        bReturnValue = FALSE;   //In case (fdwAttrsAndFlags == SetFileAttributes(szFileSpec))
        MV_TRACE(TEXT("Unable to set the attributes in TestFileAttributes()") );
        wsprintf(szLogString, TEXT("Last error == 0x%08X"), dwLastError );
        MV_TRACE(szLogString );
        goto Bail;
    }

    MV_TRACE(TEXT("Attempting to open file for read/write in TestFileAttributes()") );
    hFile = CreateFile(szFileSpec, GENERIC_READ | GENERIC_WRITE, 0, 
                        NULL, TRUNCATE_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    dwLastError = GetLastError();
    if (INVALID_HANDLE_VALUE == hFile )
    {
        if (bShouldOpen )
        {
            bReturnValue = FALSE;
            MV_TRACE(TEXT("Error opening file for write in TestFileAttributes() when it should have opened") );
            wsprintf(szLogString, TEXT("Last error == 0x%08X"), dwLastError );
            MV_TRACE(szLogString );
            goto Bail;
        }
    }
    else    //The file opened for write so re-write the data to make sure writes do not fail.
    {
        if (!bShouldOpen )
        {
            MV_TRACE(TEXT("Warning, file opened for write when it should not have") );
        }
        MV_TRACE(TEXT("Writing to open file in TestFileAttributes()") );
        bReturnValue = WriteFile(hFile, pTestBuffer, nBufSize, &nBytesWritten, NULL );
        dwLastError = GetLastError();
        if (!bReturnValue || (nBytesWritten != nBufSize) )
        {
            if (bShouldOpen )
            {
                bReturnValue = FALSE;   //In case (nBytesWritten != nBufSize)
                MV_TRACE(TEXT("Error re-writing buffer to file in TestFileAttributes()") );
                wsprintf(szLogString, TEXT("Last error == 0x%08X"), dwLastError );
                MV_TRACE(szLogString );
                goto Bail;
            }
        }
        else if (!bShouldOpen )
        {
            bReturnValue = FALSE;
            MV_TRACE(TEXT("Error, wrote to file when it should not have") );
            goto Bail;
        }

        MV_TRACE(TEXT("Closing file in TestFileAttributes()") );
        bReturnValue = CloseHandle(hFile );
        hFile=INVALID_HANDLE_VALUE ;
        dwLastError = GetLastError();
        if (!bReturnValue )
        {
            MV_TRACE(TEXT("Unable to close the re-opened file in TestFileAttributes()") );
            wsprintf(szLogString, TEXT("Last error == 0x%08X"), dwLastError );
            MV_TRACE(szLogString );
            goto Bail;
        }
    }

    //Attempt to open the file for read.
    MV_TRACE(TEXT("Attempting to open file for read in TestFileAttributes()") );
    hFile = CreateFile(szFileSpec, GENERIC_READ, 0, 
                        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    dwLastError = GetLastError();
    if (INVALID_HANDLE_VALUE == hFile )
    {
        bReturnValue = FALSE;
        MV_TRACE(TEXT("Failed opening file for read in TestFileAttributes()") );
        wsprintf(szLogString, TEXT("Last error == 0x%08X"), dwLastError );
        MV_TRACE(szLogString );
        goto Bail;
    }

    //Read the data and verify that it is correct.
    MV_TRACE(TEXT("Reading file in TestFileAttributes() to verify data") );
    bReturnValue = ReadFile(hFile, pTempBuffer, nBufSize, &nBytesRead, NULL );
    dwLastError = GetLastError();
    if (!bReturnValue || (nBytesRead != nBufSize) )
    {
        bReturnValue = FALSE;   //In case (nBytesRead != nBufSize)
        MV_TRACE(TEXT("Failed reading from file in TestFileAttributes()") );
        wsprintf(szLogString, TEXT("Last error == 0x%08X"), dwLastError );
        MV_TRACE(szLogString );
        goto Bail;
    }

    if ((nBufSize != 0) && (memcmp(pTestBuffer, pTempBuffer, nBufSize) != 0) )
    {
        bReturnValue = FALSE;
        MV_TRACE(TEXT("Buffer miscompare in TestFileAttributes()") );
        goto Bail;
    }

    MV_TRACE(TEXT("Closing file in TestFileAttributes()") );
    bReturnValue = CloseHandle(hFile );
    hFile=INVALID_HANDLE_VALUE ;
    dwLastError = GetLastError();
    if (!bReturnValue )
    {
        MV_TRACE(TEXT("Unable to close the file opened for read in TestFileAttributes()") );
        wsprintf(szLogString, TEXT("Last error == 0x%08X"), dwLastError );
        MV_TRACE(szLogString );
        goto Bail;
    }

    //Attempt to delete the file with its attributes set.
    MV_TRACE(TEXT("Attempting to delete file in TestFileAttributes() without clearing its attributes") );
    bCallReturn = DeleteFile(szFileSpec );
    dwLastError = GetLastError();
    if (!bCallReturn && bShouldOpen )      //If it should open for write, then it should delete!
    {
        MV_TRACE(TEXT("Unable to delete the file in TestFileAttributes(), when it should have") );
        bReturnValue = FALSE;
        wsprintf(szLogString, TEXT("Last error == 0x%08X"), dwLastError );
        MV_TRACE(szLogString );
        goto Bail;
    }

Bail:
    //If the file exists, clear its attributes and delete it to clean up.
    if (INVALID_HANDLE_VALUE != hFile)
        CloseHandle(hFile) ;
    if (FileExists(szFileSpec) )
    {
        BOOL bTemp;
        MV_TRACE(TEXT("Clearing attributes for file in TestFileAttributes()") );
        bTemp = SetFileAttributes(szFileSpec, FILE_ATTRIBUTE_NORMAL );
        dwLastError = GetLastError();
        if (bTemp )
        {
            MV_TRACE(TEXT("Deleting file in TestFileAttributes()") );
            bTemp = DeleteFile(szFileSpec );
            dwLastError = GetLastError();
            if (!bTemp )
            {
                bReturnValue = FALSE;
                MV_TRACE(TEXT("Unable to delete the file in TestFileAttributes()") );
                wsprintf(szLogString, TEXT("Last error == 0x%08X"), dwLastError );
                MV_TRACE(szLogString );
            }
        }
        else
        {
            bReturnValue = FALSE;
            MV_TRACE(TEXT("Unable to clear the attributes before deleting the file in TestFileAttributes()") );
            wsprintf(szLogString, TEXT("Last error == 0x%08X"), dwLastError );
            MV_TRACE(szLogString );
        }
    }

    if (pTestBuffer != NULL )
    {
        LocalFree(pTestBuffer );
    }
    if (pTempBuffer != NULL )
    {
        LocalFree(pTempBuffer );
    }

    return bReturnValue;
}

/****************************************************************************

    FUNCTION:   FillBuffer

    PURPOSE:    Fills a buffer with data that compresses in various ways for
                testing the way compression is integrated into an application.
    
    PARAMETERS: pBuffer, Pointer to the buffer to initialize.
                dwBufSize, Number of bytes to write to the buffer.
                nRandSeed, Flag used to determine how to calculate the data.

    NOTE:       This function always fills in the buffer exactly the same if
                you pass it the same size and value for nRandSeed.  

    Values for nRandSeed:

        ZEROED_BUFFER,      This will fill the buffer with zeros.  This is 
                            important because compression treats zeroed streams
                            differently than it treats other data.
        REPETITIVE_BUFFER,  This fills the buffer with -1's.  This creates 
                            a buffer that compresses as much as possible.
        NOT_COMPRESSIBLE,   If this value is set for a 4096 (4k) buffer it
                            will generate a 4096 byte buffer that compresses
                            to 4096 bytes.  This is important because an app
                            could get confused and use the raw data, but set 
                            the compressed bit or vica-versa.
        MAX_EXPANSION,      This creates a buffer that will expand by 
                            approximately 27%.  A 4096 byte buffer will expand 
                            to 5660 bytes when run through the compression
                            engine.

****************************************************************************/

void FillBuffer( LPBYTE pBuffer, DWORD dwBufSize, int nRandSeed )
{
    DWORD   dwCount;
    DWORD   dwStart;
    
    if ( nRandSeed == NOT_COMPRESSIBLE )
    {   //Fill the first 1097 bytes in the buffer to NULL to make a 4096 byte buffer compress to 4096 bytes.
        dwStart = min(1097, dwBufSize);
        for ( dwCount = 0; dwCount < dwStart; dwCount++ )
        {
            pBuffer[dwCount] = '\0';
        }
    }
    else    //Just fill the buffer with random data using the seed passed in.
    {
        dwStart = 0;
    }
    
    srand( nRandSeed );
    for ( dwCount = dwStart; dwCount < dwBufSize; dwCount++ )
    {
        if ( nRandSeed == ZEROED_BUFFER || nRandSeed == REPETITIVE_BUFFER )
        {   //Fill the buffer with the seed.
            pBuffer[dwCount] = (char)nRandSeed;
        }
        else
        {
            pBuffer[dwCount] = (char)rand();
        }
    }
}

/****************************************************************************

    FUNCTION:   CreateTestFile

    PURPOSE:    To create a file to test with.  If the size is > 10k it will
                break it up into multiple 10k writes.

****************************************************************************/
BOOL CreateTestFile(TCHAR *szFilePath, TCHAR *szFileName, DWORD dwFileSize, HANDLE *phFile, int nDataSeed )
{
    BOOL            bReturnValue            = TRUE;
    BOOL            bCallReturn;
    TCHAR           szFileSpec[MAX_PATH+20];
    HANDLE          hFile                   = INVALID_HANDLE_VALUE;
    LPBYTE          pTestBuffer             = NULL;
    DWORD           nBytesWritten;
    DWORD           dwBlockSize;
    DWORD           dwLastError             = NO_ERROR;

    if (dwFileSize > 4096 )
    {
        dwBlockSize = 4096;
    }
    else
    {
        dwBlockSize = dwFileSize;
    }

    pTestBuffer = (LPBYTE)LocalAlloc(0, dwBlockSize );

    if (pTestBuffer == NULL )
    {
        MV_TRACE(TEXT("Unable to allocate buffer in CreateTestFile()") );
        bReturnValue = FALSE;
        goto Bail;
    }

    //Initialize test buffer.
    FillBuffer(pTestBuffer, dwBlockSize, nDataSeed );

    //Make sure the directory exists.
    if (!FileExists(szFilePath) )
    {
        bReturnValue = (CompleteCreateDirectory(szFilePath ) == TRUE);
        dwLastError = GetLastError();
#ifdef UNDER_CE
        if (!bReturnValue )
        {
            //Reuse szFileSpec to save memory!
            wsprintf(szFileSpec, TEXT("Unable to create directory \"%s\""), szFilePath );
            MV_TRACE(szFileSpec );
            wsprintf(szFileSpec, TEXT("Last error == 0x%08X"), dwLastError );
            MV_TRACE(szFileSpec );
            goto Bail;
        }
#else   //The CompleteCreateDirectory() call fails on the desktop if the path == "A:"
        bReturnValue = TRUE;
#endif
    }
    
    wsprintf(szFileSpec, TEXT("%s\\%s"), szFilePath, szFileName );
    hFile = CreateFile(szFileSpec, GENERIC_READ | GENERIC_WRITE, 0, 
                        NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    dwLastError = GetLastError();
    if (INVALID_HANDLE_VALUE == hFile )
    {
        bReturnValue = FALSE;
        //Reuse szFileSpec to save memory!
        wsprintf(szFileSpec, TEXT("Unable to create file \"%s\\%s\""), szFilePath, szFileName );
        MV_TRACE(szFileSpec );
        wsprintf(szFileSpec, TEXT("Last error == 0x%08X"), dwLastError );
        MV_TRACE(szFileSpec );
        goto Bail;
    }
    MV_TRACE(TEXT("Creating file %s of size %d\n"), szFileSpec, dwFileSize) ;
    while (dwFileSize )
    {
        if (dwFileSize < dwBlockSize )
        {
            dwBlockSize = dwFileSize;
        }
        bReturnValue = WriteFile(hFile, pTestBuffer, dwBlockSize, &nBytesWritten, NULL );
        dwLastError = GetLastError();
        if (!bReturnValue || (nBytesWritten != dwBlockSize) )
        {
            bReturnValue = FALSE;   //In case (nBytesWritten != dwBlockSize)
            MV_TRACE(TEXT("Error writing to file in CreateTestFile()") );
            wsprintf(szFileSpec, TEXT("Last error == 0x%08X"), dwLastError );
            MV_TRACE(szFileSpec );
            goto Bail;
        }
        dwFileSize -= dwBlockSize;
    }

Bail:

    if (hFile != INVALID_HANDLE_VALUE )
    {
        if (phFile )   //Non-NULL pointer means to leave the file open and return its handle.
        {
            *phFile = hFile;
        }
        else    //NULL pointer, so close the file and bail
        {
            bCallReturn = CloseHandle(hFile );
            if (!bCallReturn )
            {
                dwLastError = GetLastError();
                bReturnValue = FALSE;
                MV_TRACE(TEXT("Unable to close the file in CreateTestFile()") );
                wsprintf(szFileSpec, TEXT("Last error == 0x%08X"), dwLastError );
                MV_TRACE(szFileSpec );
            }
        }
    }

    if (pTestBuffer )
    {
        LocalFree(pTestBuffer );
    }

    SetLastError(dwLastError );
    return bReturnValue;
}

/****************************************************************************

    FUNCTION:       CreateFilesToTest

    PURPOSE:        Creates a directory with x number of n to n+x byte files.
    
    PARAMETERS:     szDirSpec, Path to create the files for testing in the 
                           format "c:\testdir" or "\testdir".  Do not pass 
                           a trailing backslash or wildcards.
                fdwAttrsAndFlags, Attributes for the files to create.
                nNumFiles, Number of files to create.
                nBufSize, Size of the files to create.
                
****************************************************************************/
BOOL CreateFilesToTest(TCHAR *szDirSpec, DWORD fdwAttrsAndFlags, DWORD nNumFiles, DWORD nBufSize )
{
    BOOL            bReturnValue            = TRUE;
    HANDLE          hFile                   = INVALID_HANDLE_VALUE;
    DWORD           nBytesWritten;
    DWORD           nBytesRead;
    DWORD           nCount;
    TCHAR           szFileSpec[MAX_PATH+20];
    TCHAR           szFileSpec_Bak[MAX_PATH+20];
    char            *pTestBuffer            = NULL;
    char            *pTempBuffer            = NULL;
    DWORD           dwLastError;
    TCHAR           szMsg[512] ;
              
    if (nNumFiles == 0 )
    {       
        bReturnValue = FALSE;
        goto Bail;
    }

    pTestBuffer = (char *)LocalAlloc(0, nBufSize + nNumFiles );
    pTempBuffer = (char *)LocalAlloc(0, nBufSize + nNumFiles );

    if ((pTestBuffer == NULL) || (pTempBuffer == NULL) )
    {
        MV_TRACE(TEXT("Unable to allocate buffers in CreateFilesToTest()") );
        wsprintf(szFileSpec, TEXT("nBufSize == %lu\tnNumFiles == %lu"), nBufSize, nNumFiles );
        MV_TRACE(szFileSpec );
        bReturnValue = FALSE;
        goto Bail;
    }

    //Initialize test buffer.
    for (nCount = 0; nCount < nBufSize + nNumFiles; nCount++ )
    {
        pTestBuffer[nCount] = (char)nCount;
    } 
    
    //If the directory exists, Kill it!
    if (FileExists(szDirSpec) )
    {
        MV_TRACE(TEXT("Warning, directory already exists in CreateFilesToTest(), killing it...") );
        bReturnValue = DeleteTree(szDirSpec, FALSE);
    }
    
    if (!bReturnValue )
    {
        MV_TRACE(TEXT("Unable to remove the existing directory in CreateFilesToTest()") );
        goto Bail;
    }

    bReturnValue = (CompleteCreateDirectory(szDirSpec ) == TRUE);
    dwLastError = GetLastError();
    if (!bReturnValue )
    {
        //Reuse szFileSpec to save memory!
        wsprintf(szFileSpec, TEXT("Unable to create directory \"%s\" using CompleteCreateDirectory(), Last error == %u"), szDirSpec, dwLastError );
        MV_TRACE(szFileSpec );
        bReturnValue = CreateDirectory(szDirSpec, NULL );
        dwLastError = GetLastError();
        if (!bReturnValue )
        {
            //Reuse szFileSpec to save memory!
            wsprintf(szFileSpec, TEXT("Unable to create directory \"%s\" using CreateDirectory()"), szDirSpec );
            MV_TRACE(szFileSpec );
            wsprintf(szFileSpec, TEXT("Last error == 0x%08X"), dwLastError );
            MV_TRACE(szFileSpec );
            goto Bail;
        }
    }



    //
    //  Create a bunch of files and set their attrs
    for (nCount = 1; nCount <= nNumFiles; nCount++ )
    {
        //  wsprintf(szFileSpec, TEXT("%s\\%08.08X.%08.08X"), szDirSpec, nCount, fdwAttrsAndFlags );
        wsprintf(szFileSpec, TST_FILE_DESC, szDirSpec, nCount, fdwAttrsAndFlags );
        wcscpy(szFileSpec_Bak, szFileSpec) ;
        
        //Create the file.
        hFile = INVALID_HANDLE_VALUE ;
        hFile = CreateFile(szFileSpec, GENERIC_READ | GENERIC_WRITE, 0, 
                            NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        dwLastError = GetLastError();
        if (INVALID_HANDLE_VALUE == hFile )
        {
            bReturnValue = FALSE;
            wsprintf(szFileSpec, TEXT("Unable to Create/Truncate file %s in CreateFilesToTest() 0x%x\n"), 
                                    szFileSpec, GetLastError()) ;
            MV_TRACE(szFileSpec );
            goto Bail;
        }

        //  Write the file
        bReturnValue = WriteFile(hFile, pTestBuffer, nBufSize+nCount, &nBytesWritten, NULL );
        dwLastError = GetLastError();
        if (!bReturnValue || (nBytesWritten != nBufSize+nCount) )
        {
            bReturnValue = FALSE;   //In case (nBytesWritten != nBufSize+nCount)
            wsprintf(szFileSpec, TEXT("Error writing buffer to file %s in CreateFilesToTest()"), szFileSpec_Bak);
            MV_TRACE(szFileSpec );
            wsprintf(szFileSpec, TEXT("Last error == 0x%08X"), dwLastError );
            MV_TRACE(szFileSpec );
            goto Bail;
        }

        //  Close it. 
        bReturnValue = CloseHandle(hFile );
        dwLastError = GetLastError();
        if (!bReturnValue )
        {
            wsprintf(szFileSpec, TEXT("Unable to close the file %s in CreateFilesToTest()"), szFileSpec_Bak);
            wsprintf(szFileSpec, TEXT("Last error == 0x%08X"), dwLastError );
            MV_TRACE(szFileSpec );
            goto Bail;
        }

        //  Set the attributes
        bReturnValue = SetFileAttributes(szFileSpec, fdwAttrsAndFlags );
        if (!bReturnValue)
        {
            wsprintf(szMsg, TEXT("Failed to SetFileAttributs of %s. 0x%x\n"), szFileSpec, GetLastError()) ;
            MV_TRACE(szMsg) ;
            goto Bail ;
        }
        
        dwLastError = GetLastError();
    }

    wsprintf(szMsg, TEXT("Done writing %d files. Verifying now...\n"), nCount-1) ;
    MV_TRACE(szMsg) ;

    //Verify all of the files.
    for (nCount = 1; nCount <= nNumFiles; nCount++ )
    {
        //  wsprintf(szFileSpec, TEXT("%s\\%08.08X.%08.08X"), szDirSpec, nCount, fdwAttrsAndFlags );
        wsprintf(szFileSpec, TST_FILE_DESC, szDirSpec, nCount, fdwAttrsAndFlags );
        wcscpy(szFileSpec_Bak, szFileSpec) ;
        
        nBytesWritten = FileSize(szFileSpec );
        if (nBytesWritten != (nBufSize + nCount) )
        {
            TCHAR   szTemp[256];
            wsprintf(szTemp, TEXT("FileSize() mismatch %s, expecting 0x%08.08X got back 0x%08.08X"), 
                                            szFileSpec_Bak, nBufSize+nCount, nBytesWritten );
            MV_TRACE(szTemp );
            goto Bail;
        }

        //Attempt to open the file for read.
        hFile = CreateFile(szFileSpec, GENERIC_READ, 0, 
                            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        dwLastError = GetLastError();
        if (INVALID_HANDLE_VALUE == hFile )
        {
            bReturnValue = FALSE;
            wsprintf(szFileSpec, TEXT("Failed opening file %s for read in CreateFilesToTest()"), szFileSpec_Bak );
            MV_TRACE(szFileSpec) ;
            wsprintf(szFileSpec, TEXT("Last error == 0x%08X"), dwLastError );
            MV_TRACE(szFileSpec) ;
            goto Bail;
        }

        nBytesWritten = GetFileSize(hFile, &nBytesRead );
        if (nBytesWritten != (nBufSize + nCount) )
        {
            TCHAR   szTemp[256];
            wsprintf(szTemp, TEXT("Error : GetFileSize() mismatch %s, expecting 0x%08.08X got back 0x%08.08X"), 
                                    szFileSpec_Bak, nBufSize, nBytesWritten );
            MV_TRACE(szTemp );
            goto Bail;
        }

        //Read the data and verify that it is correct.
        bReturnValue = ReadFile(hFile, pTempBuffer, nBufSize+nCount, &nBytesRead, NULL );
        dwLastError = GetLastError();
        if (!bReturnValue || (nBytesRead != nBufSize+nCount) )
        {
            bReturnValue = FALSE;   //In case (nBytesRead != nBufSize+nCount)
            wsprintf(szFileSpec, TEXT("Failed reading from file %s in CreateFilesToTest()"), szFileSpec_Bak );
            MV_TRACE(szFileSpec) ;
            wsprintf(szFileSpec, TEXT("Last error == 0x%08X"), dwLastError );
            MV_TRACE(szFileSpec );
            goto Bail;
        }

        if (memcmp(pTestBuffer, pTempBuffer, nBufSize+nCount) != 0 )
        {
            bReturnValue = FALSE;
            wsprintf(szFileSpec, TEXT("Buffer miscompare for file %s in CreateFilesToTest()"), szFileSpec_Bak);
            MV_TRACE(szFileSpec);
            //MV_TRACE(TEXT("Wrote buffer....\n")) ;
            //Hlp_DumpBuffer((PBYTE)pTestBuffer, nBufSize+nCount) ;
            //MV_TRACE(TEXT("Read buffer....\n")) ;
            //Hlp_DumpBuffer((PBYTE)pTempBuffer, nBufSize+nCount) ;

            MV_TRACE(TEXT("\nTrying Again.....")) ;
            if (!SetFilePointer(hFile, 0, NULL, FILE_BEGIN))
            {
                MV_TRACE(TEXT("Failed to setFilePointer\n")) ;
                goto Bail ;
            }
            bReturnValue = ReadFile(hFile, pTempBuffer, nBufSize+nCount, &nBytesRead, NULL );
            dwLastError = GetLastError();
            if (memcmp(pTestBuffer, pTempBuffer, nBufSize+nCount) != 0 )
            {
                MV_TRACE(TEXT("Still failed!!!!!\n")) ;
                goto Bail ;
            }
        }

        bReturnValue = CloseHandle(hFile );
        dwLastError = GetLastError();
        if (!bReturnValue )
        {
            MV_TRACE(TEXT("Unable to close the file opened for read in CreateFilesToTest()") );
            wsprintf(szFileSpec, TEXT("Last error == 0x%08X"), dwLastError );
            MV_TRACE(szFileSpec );
            goto Bail;
        }
    }

Bail:

    if (INVALID_HANDLE_VALUE != hFile )
    {
        CloseHandle(hFile );
    }

    if (pTestBuffer != NULL )
    {
        LocalFree(pTestBuffer );
    }

    if (pTempBuffer != NULL )
    {
        LocalFree(pTempBuffer );
    }
        
    return bReturnValue;
}


/****************************************************************************

    FUNCTION:       CopyFilesToTest

    PURPOSE:        Copies all files created by CreateFilesToTest() from 
                szSourcePath to szDestPath.
    
    PARAMETERS:     szSourcePath, Path to copy the files from.
                szDestPath, Path to copy the files to.
                fdwAttrsAndFlags, Attributes flag, used to calculate
                                  the file extension.
                nNumFiles, Number of files to create.
                nBufSize, Size of the files to create.
                bTry2CopyAgain, If TRUE attempt to copy the data over existing 
                                data.
                
****************************************************************************/
BOOL CopyFilesToTest(TCHAR *szSourcePath, TCHAR *szDestPath, DWORD fdwAttrsAndFlags, 
                      DWORD nNumFiles, DWORD nBufSize, BOOL bTry2CopyAgain )
{
    BOOL            bReturnValue    = TRUE;
    BOOL            bCallReturn;
    DWORD           nCount;
    DWORD           fdwReasonFailed;
    TCHAR           szSrcFileSpec[MAX_PATH+20];
    TCHAR           szDstFileSpec[MAX_PATH+20];
    TCHAR           szMsg[512] ;
    DWORD           dwLastError;
              
    if (nNumFiles == 0 )
    {       
        bReturnValue = FALSE;
        goto Bail;
    }

    //Make sure the directory exists!
    if (!FileExists(szDestPath) )
    {
        bReturnValue = (CompleteCreateDirectory(szDestPath ) == TRUE);
    }
    
    dwLastError = GetLastError();
    if (!bReturnValue )
    {
        //Reuse szSrcFileSpec to save memory!
        wsprintf(szSrcFileSpec, TEXT("Unable to create directory \"%s\" using CompleteCreateDirectory(), Last error == %u"), szDestPath, dwLastError );
        MV_TRACE(szSrcFileSpec );
        bReturnValue = CreateDirectory(szDestPath, NULL );
        dwLastError = GetLastError();
        if (!bReturnValue )
        {
            //Reuse szSrcFileSpec to save memory!
            wsprintf(szSrcFileSpec, TEXT("Unable to create directory \"%s\" using CreateDirectory()"), szDestPath );
            MV_TRACE(szSrcFileSpec );
            wsprintf(szSrcFileSpec, TEXT("Last error == 0x%08X"), dwLastError );
            MV_TRACE(szSrcFileSpec );
            goto Bail;
        }
    }
    
    //Create a temp file to use below.
    bReturnValue = CreateTestFile(szSourcePath, TEMP_FILE_NAME, 69, NULL, REPETITIVE_BUFFER );
    dwLastError = GetLastError();
    if (!bReturnValue )
    {
        //Reuse szSrcFileSpec to save memory!
        wsprintf(szSrcFileSpec, TEXT("Unable to create temp file \"%s\\%s\" in CopyFilesToTest()"), szSourcePath, TEMP_FILE_NAME );
        MV_TRACE(szSrcFileSpec );
        wsprintf(szSrcFileSpec, TEXT("Last error == 0x%08X"), dwLastError );
        MV_TRACE(szSrcFileSpec );
        goto Bail;
    }
    for (nCount = 1; nCount <= nNumFiles; nCount++ )
    {
        //wsprintf(szDstFileSpec, TEXT("%s\\%08.08X.%08.08X"), szDestPath, nCount, fdwAttrsAndFlags );
        wsprintf(szDstFileSpec, TST_FILE_DESC, szDestPath, nCount, fdwAttrsAndFlags );
        if (bTry2CopyAgain )   //copy the temp file.
        {
            wsprintf(szSrcFileSpec, TEXT("%s\\%s"), szSourcePath, TEMP_FILE_NAME );
            bReturnValue = CopyFile(szSrcFileSpec, szDstFileSpec, FALSE );//overwrite if the file already exists.
            dwLastError = GetLastError();
            if (!bReturnValue )
            {
                MV_TRACE(TEXT("CopyFile() failed when it should have succeeded") );
                wsprintf(szSrcFileSpec, TEXT("Last error == 0x%08X"), dwLastError );
                MV_TRACE(szSrcFileSpec );
                goto Bail;
            }
        
            bCallReturn = CompareFiles(szSrcFileSpec, szDstFileSpec, TRUE, &fdwReasonFailed );
            if (!bCallReturn || fdwReasonFailed )          //Fail on warnings also!
            {
                //Reuse szSrcFileSpec to save memory!
                wsprintf(szSrcFileSpec, TEXT("File \"%s\" miscompared, reason #0x%08.08X"), TEMP_FILE_NAME, fdwReasonFailed );
                MV_TRACE(szSrcFileSpec );
                L_PrintMiscompareReason(fdwReasonFailed) ;
                bReturnValue = FALSE;
                goto Bail;
            }

            //wsprintf(szSrcFileSpec, TEXT("%s\\%08.08X.%08.08X"), szSourcePath, nCount, fdwAttrsAndFlags );
            wsprintf(szSrcFileSpec, TST_FILE_DESC, szSourcePath, nCount, fdwAttrsAndFlags );
            bCallReturn = CopyFile(szSrcFileSpec, szDstFileSpec, TRUE );//fail if the file already exists.
            dwLastError = GetLastError();
            if (bCallReturn )
            {
                MV_TRACE(TEXT("CopyFile() was successful when it should have failed") );
                wsprintf(szSrcFileSpec, TEXT("Last error == 0x%08X"), dwLastError );
                MV_TRACE(szSrcFileSpec );
                bReturnValue = FALSE;
                goto Bail;
            }
        
            wsprintf(szSrcFileSpec, TEXT("%s\\%s"), szSourcePath, TEMP_FILE_NAME );
            bCallReturn = CompareFiles(szSrcFileSpec, szDstFileSpec, TRUE, &fdwReasonFailed );
            if (!bCallReturn || fdwReasonFailed )          //Fail on warnings also!
            {
                //Reuse szSrcFileSpec to save memory!
                wsprintf(szSrcFileSpec, TEXT("File \"%s\" miscompared, reason #0x%08.08X"), TEMP_FILE_NAME, fdwReasonFailed );
                MV_TRACE(szSrcFileSpec );
                L_PrintMiscompareReason(fdwReasonFailed) ;
                bReturnValue = FALSE;
                goto Bail;
            }

		}	
        //wsprintf(szSrcFileSpec, TEXT("%s\\%08.08X.%08.08X"), szSourcePath, nCount, fdwAttrsAndFlags );
        wsprintf(szSrcFileSpec, TST_FILE_DESC, szSourcePath, nCount, fdwAttrsAndFlags );
        
        //Copy the file.
        bReturnValue = CopyFile(szSrcFileSpec, szDstFileSpec, FALSE );//Overwrite if the file exists!
        dwLastError = GetLastError();
        if (!bReturnValue )
        {
            MV_TRACE(TEXT("Unable to copy file in CopyFilesToTest()") );
            wsprintf(szSrcFileSpec, TEXT("Last error == 0x%08X"), dwLastError );
            MV_TRACE(szSrcFileSpec );
            goto Bail;
        }
        
        bCallReturn = CompareFiles(szSrcFileSpec, szDstFileSpec, TRUE, &fdwReasonFailed );
        if (!bCallReturn || fdwReasonFailed )          //Fail on warnings also!
        {
            //Reuse szSrcFileSpec to save memory!
            wsprintf(szMsg, TEXT("File %s miscompared, reason #0x%08.08X"), szSrcFileSpec, fdwReasonFailed );
            MV_TRACE(szMsg);
            L_PrintMiscompareReason(fdwReasonFailed) ;
            bReturnValue = FALSE;
            goto Bail;
        }
    }

Bail:

    return bReturnValue;
}



/****************************************************************************

    FUNCTION:       MoveFilesToTest

    PURPOSE:        Moves all files created by CreateFilesToTest() from 
                szSourcePath to szDestPath.
    
    PARAMETERS:     szSourcePath, Path to move the files from.
                szDestPath, Path to move the files to.
                szCompPath, Path to get the files to compare against.
                fdwAttrsAndFlags, Attributes flag, used to calculate
                                  the file extension.
                nNumFiles, Number of files to create.
                nBufSize, Size of the files to create.
                
****************************************************************************/
BOOL MoveFilesToTest(TCHAR *szSourcePath, TCHAR *szDestPath, TCHAR *szCompPath, 
                      DWORD fdwAttrsAndFlags, DWORD nNumFiles, DWORD nBufSize )
{
    BOOL            bReturnValue    = TRUE;
    BOOL            bCallReturn;
    DWORD           nCount;
    DWORD           fdwReasonFailed;
    TCHAR           szSrcFileSpec[MAX_PATH+20];
    TCHAR           szDstFileSpec[MAX_PATH+20];
    TCHAR           szMsg[512] ;
    DWORD           dwLastError;
              
    if (nNumFiles == 0 )
    {       
        bReturnValue = FALSE;
        goto Bail;
    }

    //Make sure the directory exists!
    if (!FileExists(szDestPath) )
    {
        bReturnValue = (CompleteCreateDirectory(szDestPath ) == TRUE);
    }
    
    dwLastError = GetLastError();
    if (!bReturnValue )
    {
        //Reuse szSrcFileSpec to save memory!
        wsprintf(szSrcFileSpec, TEXT("Unable to create directory \"%s\" using CompleteCreateDirectory(), Last error == %u"), szDestPath, dwLastError );
        MV_TRACE(szSrcFileSpec );
        bReturnValue = CreateDirectory(szDestPath, NULL );
        dwLastError = GetLastError();
        if (!bReturnValue )
        {
            //Reuse szSrcFileSpec to save memory!
            wsprintf(szSrcFileSpec, TEXT("Unable to create directory \"%s\" using CreateDirectory()"), szDestPath );
            MV_TRACE(szSrcFileSpec );
            wsprintf(szSrcFileSpec, TEXT("Last error == 0x%08X"), dwLastError );
            MV_TRACE(szSrcFileSpec );
            goto Bail;
        }
    }
    
    for (nCount = 1; nCount <= nNumFiles; nCount++ )
    {
        //  wsprintf(szSrcFileSpec, TEXT("%s\\%08.08X.%08.08X"), szSourcePath, nCount, fdwAttrsAndFlags );
        //  wsprintf(szDstFileSpec, TEXT("%s\\%08.08X.%08.08X"), szDestPath, nCount, fdwAttrsAndFlags );

        wsprintf(szSrcFileSpec, TST_FILE_DESC, szSourcePath, nCount, fdwAttrsAndFlags );
        wsprintf(szDstFileSpec, TST_FILE_DESC, szDestPath, nCount, fdwAttrsAndFlags );

        //Move the file.
        bCallReturn = MoveFile(szSrcFileSpec, szDstFileSpec );
        dwLastError = GetLastError();
        if (!bCallReturn )
        {
            MV_TRACE(TEXT("Unable to move file in MoveFilesToTest()") );
            wsprintf(szSrcFileSpec, TEXT("Last error == 0x%08X"), dwLastError );
            MV_TRACE(szSrcFileSpec );
            if (fdwAttrsAndFlags & FILE_ATTRIBUTE_READONLY )   //Read-only files can't be moved across devices.
            {
                MV_TRACE(TEXT("Warning: Read-only files cannot be moved across devices.") );
                continue;
            }
            else
            {
                bReturnValue = FALSE;
                goto Bail;
            }
        }
        
        if (FileExists(szSrcFileSpec) )
        {
            bReturnValue = FALSE;
            //Reuse szDstFileSpec to save memory!
            wsprintf(szDstFileSpec, TEXT("File \"%s\" still exists after successful move"), szSrcFileSpec );
            MV_TRACE(szDstFileSpec );
            goto Bail;
        }

        //  wsprintf(szSrcFileSpec, TEXT("%s\\%08.08X.%08.08X"), szCompPath, nCount, fdwAttrsAndFlags );
        wsprintf(szSrcFileSpec, TST_FILE_DESC, szCompPath, nCount, fdwAttrsAndFlags );
        bCallReturn = CompareFiles(szSrcFileSpec, szDstFileSpec, TRUE, &fdwReasonFailed );
        if (!bCallReturn || fdwReasonFailed )          //Fail on warnings also!
        {
            //Reuse szSrcFileSpec to save memory!
            wsprintf(szMsg, TEXT("File %s miscompared, reason #0x%08.08X"), szSrcFileSpec, fdwReasonFailed );
            MV_TRACE(szMsg);
            L_PrintMiscompareReason(fdwReasonFailed) ;
            bReturnValue = FALSE;
            goto Bail;
        }
    }

Bail:

    return bReturnValue;
}

/****************************************************************************

    FUNCTION:       TestFileTime

    PURPOSE:        To set, get a files time and test all functions related to 
                file times.

****************************************************************************/
BOOL TestFileTime(HANDLE hFile, DWORD dwLowFileTime, DWORD dwHighFileTime )
{
    BOOL            bReturnValue            = FALSE;
    BOOL            bCallReturn;
    FILETIME        ftOrigCreation;
    FILETIME        ftOrigLastAccess;
    FILETIME        ftOrigLastWrite;
    FILETIME        ftNewCreation;
    FILETIME        ftNewLastAccess;
    FILETIME        ftNewLastWrite;
    FILETIME        ftLocalFileTime;
    FILETIME        ftNewFileTime;
    SYSTEMTIME      stTempSystemTime;
    TCHAR           szTemp[MAX_PATH];
    DWORD           dwLastError;

    //Initialize FILETIME structures.
    ftOrigCreation.dwLowDateTime    = dwLowFileTime;
    ftOrigCreation.dwHighDateTime   = dwHighFileTime;
    ftOrigLastAccess.dwLowDateTime  = dwLowFileTime+1;
    ftOrigLastAccess.dwHighDateTime = dwHighFileTime+1;
    ftOrigLastWrite.dwLowDateTime   = dwLowFileTime+2;
    ftOrigLastWrite.dwHighDateTime  = dwHighFileTime+2;
    
    //Set the file times and verify that they were set OK!
    bCallReturn = SetFileTime(hFile, &ftOrigCreation, &ftOrigLastAccess, &ftOrigLastWrite );
    dwLastError = GetLastError();
    if (!bCallReturn )     //Failed setting the file time.
    {
        wsprintf(szTemp, TEXT("Unable to set the file time to 0x%08.08X:0x%08.08X in TestFileTime()"), dwHighFileTime, dwLowFileTime );
        MV_TRACE(szTemp );
        wsprintf(szTemp, TEXT("Last error == 0x%08.08X"), dwLastError );
        MV_TRACE(szTemp );
        goto Bail;
    }
    bCallReturn = GetFileTime(hFile, &ftNewCreation, &ftNewLastAccess, &ftNewLastWrite );
    dwLastError = GetLastError();
    if (!bCallReturn )     //Failed getting the file time.
    {
        wsprintf(szTemp, TEXT("Unable to get the file time in TestFileTime(), expecting 0x%08.08X:0x%08.08X"), dwHighFileTime, dwLowFileTime );
        MV_TRACE(szTemp );
        wsprintf(szTemp, TEXT("Last error == 0x%08X"), dwLastError );
        MV_TRACE(szTemp );
        goto Bail;
    }
    if ((CompareFileTime(&ftOrigCreation, &ftNewCreation) != 0)
    ||  (CompareFileTime(&ftOrigLastAccess, &ftNewLastAccess) != 0)
    ||  (CompareFileTime(&ftOrigLastWrite, &ftNewLastWrite) != 0))
    {
        MV_TRACE(TEXT("File time miscompare:") );
        wsprintf(szTemp, TEXT("\tftOrigCreation == 0x%08.08X:0x%08.08X"), 
                                ftOrigCreation.dwHighDateTime, ftOrigCreation.dwLowDateTime );
        MV_TRACE(szTemp );
        wsprintf(szTemp, TEXT("\tftOrigLastAccess == 0x%08.08X:0x%08.08X"), 
                                ftOrigLastAccess.dwHighDateTime, ftOrigLastAccess.dwLowDateTime );
        MV_TRACE(szTemp );
        wsprintf(szTemp, TEXT("\tftOrigLastWrite == 0x%08.08X:0x%08.08X"), 
                                ftOrigLastWrite.dwHighDateTime, ftOrigLastWrite.dwLowDateTime );
        MV_TRACE(szTemp );
        wsprintf(szTemp, TEXT("\tftNewCreation == 0x%08.08X:0x%08.08X"), 
                                ftNewCreation.dwHighDateTime, ftNewCreation.dwLowDateTime );
        MV_TRACE(szTemp );
        wsprintf(szTemp, TEXT("\tftNewLastAccess == 0x%08.08X:0x%08.08X"), 
                                ftNewLastAccess.dwHighDateTime, ftNewLastAccess.dwLowDateTime );
        MV_TRACE(szTemp );
        wsprintf(szTemp, TEXT("\tftNewLastWrite == 0x%08.08X:0x%08.08X"), 
                                ftNewLastWrite.dwHighDateTime, ftNewLastWrite.dwLowDateTime );
        MV_TRACE(szTemp );
        goto Bail;
    }

    //Test converting the file time created to local file times.
    //Note: This does not need to be done for all 3 structures because the values will be
    //              tested the next time this is called.
    bCallReturn = FileTimeToLocalFileTime(&ftOrigCreation, &ftLocalFileTime );
    dwLastError = GetLastError();
    if (!bCallReturn )     //Failed converting the file time to local file time.
    {
        MV_TRACE(TEXT("Unable to convert the creation file time to local file time in TestFileTime()") );
        wsprintf(szTemp, TEXT("Last error == 0x%08X"), dwLastError );
        MV_TRACE(szTemp );
        goto Bail;
    }
    bCallReturn = LocalFileTimeToFileTime(&ftLocalFileTime, &ftNewFileTime );
    dwLastError = GetLastError();
    if (!bCallReturn )     //Failed converting the local file time to file time.
    {
        MV_TRACE(TEXT("Unable to convert the creation local file time to file time in TestFileTime()") );
        wsprintf(szTemp, TEXT("Last error == 0x%08X"), dwLastError );
        MV_TRACE(szTemp );
        goto Bail;
    }
    if (CompareFileTime(&ftOrigCreation, &ftNewFileTime) != 0 )
    {
        wsprintf(szTemp, TEXT("File time creation miscompare, expecting 0x%08.08X:0x%08.08X"), dwHighFileTime, dwLowFileTime );
        MV_TRACE(szTemp );
        goto Bail;
    }

    //Test converting the creation file time to system time.
    bCallReturn = FileTimeToSystemTime(&ftOrigCreation, &stTempSystemTime );
    dwLastError = GetLastError();
    if (!bCallReturn )     //Failed converting the file time to system time.
    {
        MV_TRACE(TEXT("Unable to convert the file time to system time in TestFileTime()") );
        wsprintf(szTemp, TEXT("Last error == 0x%08X"), dwLastError );
        MV_TRACE(szTemp );
        goto Bail;
    }
    bCallReturn = SystemTimeToFileTime(&stTempSystemTime, &ftNewFileTime );
    dwLastError = GetLastError();
    if (!bCallReturn )     //Failed converting the system time to file time.
    {
        MV_TRACE(TEXT("Unable to convert the system time to file time in TestFileTime()") );
        wsprintf(szTemp, TEXT("Last error == 0x%08X"), dwLastError );
        MV_TRACE(szTemp );
        goto Bail;
    }
    if (CompareFileTime(&ftOrigCreation, &ftNewFileTime) != 0 )
    {
        wsprintf(szTemp, TEXT("File time miscompare after converting to system time") );
        MV_TRACE(szTemp );
        wsprintf(szTemp, TEXT("\tftOrigCreation == 0x%08.08X:0x%08.08X"), 
                                ftOrigCreation.dwHighDateTime, ftOrigCreation.dwLowDateTime );
        MV_TRACE(szTemp );
        wsprintf(szTemp, TEXT("\tstTempSystemTime.wYear == %u"), stTempSystemTime.wYear );
        MV_TRACE(szTemp );
        wsprintf(szTemp, TEXT("\tstTempSystemTime.wMonth == %u"), stTempSystemTime.wMonth );
        MV_TRACE(szTemp );
        wsprintf(szTemp, TEXT("\tstTempSystemTime.wDayOfWeek == %u"), stTempSystemTime.wDayOfWeek );
        MV_TRACE(szTemp );
        wsprintf(szTemp, TEXT("\tstTempSystemTime.wDay == %u"), stTempSystemTime.wDay );
        MV_TRACE(szTemp );
        wsprintf(szTemp, TEXT("\tstTempSystemTime.wHour == %u"), stTempSystemTime.wHour );
        MV_TRACE(szTemp );
        wsprintf(szTemp, TEXT("\tstTempSystemTime.wMinute == %u"), stTempSystemTime.wMinute );
        MV_TRACE(szTemp );
        wsprintf(szTemp, TEXT("\tstTempSystemTime.wSecond == %u"), stTempSystemTime.wSecond );
        MV_TRACE(szTemp );
        wsprintf(szTemp, TEXT("\tstTempSystemTime.wMilliseconds == %u"), stTempSystemTime.wMilliseconds );
        MV_TRACE(szTemp );
        wsprintf(szTemp, TEXT("\tftNewFileTime == 0x%08.08X:0x%08.08X"), 
                                ftNewFileTime.dwHighDateTime, ftNewFileTime.dwLowDateTime );
        MV_TRACE(szTemp );
        goto Bail;
    }

    //If it made it here then it was successful!!!
    bReturnValue = TRUE;

Bail:

    return bReturnValue;
}

/****************************************************************************

    FUNCTION:       TestFileTimes

    PURPOSE:        To create a file and then set, get its file time and test
                all functions related to file times.

****************************************************************************/
BOOL TestFileTimes(TCHAR *szFilePath, TCHAR *szFileName, DWORD dwFileSize )
{
    BOOL            bReturnValue            = TRUE;
    BOOL            bNotDone                = TRUE;
    BOOL            bCallReturn;
    DWORD           dwHighFileTime          = 0;
    DWORD           dwLowFileTime           = 0;
    HANDLE          hFile                   = INVALID_HANDLE_VALUE;

    bReturnValue = CreateTestFile(szFilePath, szFileName, dwFileSize, &hFile, REPETITIVE_BUFFER );
    if (!bReturnValue || hFile == INVALID_HANDLE_VALUE )
    {
        MV_TRACE(TEXT("Unable to create test file in TestFileTimes()") );
        goto Bail;
    }

    while (bNotDone )
    { 
        bCallReturn = TestFileTime(hFile, dwLowFileTime, dwHighFileTime );
        if (!bCallReturn )
        {
            bReturnValue = FALSE;
        }

        if (dwHighFileTime == 0xFA56EA00 && dwLowFileTime == 0xFA56EA00 )
            bNotDone = FALSE;
        if (dwLowFileTime == 0xFA56EA00 )
        {
            dwLowFileTime = 0;
            dwHighFileTime += 100000000;
        }
        dwLowFileTime += 100000000;
    }
    
Bail:

    if (hFile != INVALID_HANDLE_VALUE )
    {
        bCallReturn = CloseHandle(hFile );
        if (!bCallReturn )
        {
            bReturnValue = FALSE;
            MV_TRACE(TEXT("Unable to close the file in TestFileTimes()") );
        }
    }

    return bReturnValue;
}



/****************************************************************************

    FUNCTION:   CopyMoveAndDelete

    PURPOSE:    Creates a bunch of files within a directory, Copying them 
                to another directory, moving this new directory to another 
                directory and then deleting them from the original directory 
                and the moved to directory.
    
    PARAMETERS: szSourcePath, Path and file name to create for testing.
                szCopyPath, Path to copy the files to.
                szMovePath, Path to move the files to.
                fdwAttrsAndFlags, Attributes to use when creating the files.
                nNumFiles, Number of files to create.
                nBufSize, Starting size of the files to create.
                bTry2CopyAgain, If TRUE attempt to copy the data over existing 
                                data.
                                                            
    NOTE:       This function will create nNumFiles number of files which
                start at nBufSize in size and increase by one byte for 
                each file.                              
                
****************************************************************************/
BOOL CopyMoveAndDelete(TCHAR *szSourcePath, TCHAR *szCopyPath, TCHAR *szMovePath, 
                        DWORD fdwAttrsAndFlags, DWORD nNumFiles, DWORD nBufSize,
                        BOOL bTry2CopyAgain )
{
    BOOL            bReturnValue            = FALSE;
    BOOL            bCallReturn;
    TCHAR           szLogString[MAX_PATH+20];
    DWORD           dwLastError;


    wsprintf(szLogString, TEXT("Creating files in directory \"%s\""), szSourcePath );
    MV_TRACE(szLogString );
    //Create nNumFiles new files to test with.
    bCallReturn = CreateFilesToTest(szSourcePath, fdwAttrsAndFlags, nNumFiles, nBufSize );
    dwLastError = GetLastError();
    if (!bCallReturn )
    {
        wsprintf(szLogString, TEXT("Unable to create files in \"%s\""), szSourcePath );
        MV_TRACE(szLogString );
        wsprintf(szLogString, TEXT("Last error == 0x%08X"), dwLastError );
        MV_TRACE(szLogString );
        goto Bail;
    }

    MV_TRACE(TEXT("Done creating files\n")) ;
    
    wsprintf(szLogString, TEXT("Copying files to \"%s\""), szCopyPath );
    MV_TRACE(szLogString );
    //Copy the files to another directory.
    bCallReturn = CopyFilesToTest(szSourcePath, szCopyPath, fdwAttrsAndFlags, nNumFiles, 
                                    nBufSize, bTry2CopyAgain );
    dwLastError = GetLastError();
    if (!bCallReturn )
    {
        wsprintf(szLogString, TEXT("Unable to copy files to \"%s\""), szCopyPath );
        MV_TRACE(szLogString );
        wsprintf(szLogString, TEXT("Last error == 0x%08X"), dwLastError );
        MV_TRACE(szLogString );
        goto Bail;
    }

    MV_TRACE(TEXT("Done Copying files\n")) ;
    
    wsprintf(szLogString, TEXT("Moving files to \"%s\""), szMovePath );
    MV_TRACE(szLogString );
    //Move the files from the new directory to another directory.
    bCallReturn = MoveFilesToTest(szCopyPath, szMovePath, szSourcePath, fdwAttrsAndFlags, nNumFiles, nBufSize );
    dwLastError = GetLastError();
    if (!bCallReturn )
    {
        wsprintf(szLogString, TEXT("Unable to move files to \"%s\""), szMovePath );
        MV_TRACE(szLogString );
        wsprintf(szLogString, TEXT("Last error == 0x%08X"), dwLastError );
        MV_TRACE(szLogString );
        goto Bail;
    }

    //Clean up szMovePath
    bCallReturn = DeleteTree(szMovePath, FALSE);
    dwLastError = GetLastError();
    if (!bCallReturn )
    {
        bReturnValue = FALSE;
        wsprintf(szLogString, TEXT("Unable to remove \"%s\""), szMovePath );
        MV_TRACE(szLogString );
        wsprintf(szLogString, TEXT("Last error == 0x%08X"), dwLastError );
        MV_TRACE(szLogString );
        goto Bail;
    }
    //Clean up szCopyPath
    bCallReturn = DeleteTree(szCopyPath, FALSE);
    dwLastError = GetLastError();
    if (!bCallReturn )
    {
        bReturnValue = FALSE;
        wsprintf(szLogString, TEXT("Unable to remove \"%s\""), szCopyPath );
        MV_TRACE(szLogString );
        wsprintf(szLogString, TEXT("Last error == 0x%08X"), dwLastError );
        MV_TRACE(szLogString );
        goto Bail;
    }
    //Clean up szSourcePath
    bCallReturn = DeleteTree(szSourcePath, FALSE);
    dwLastError = GetLastError();
    if (!bCallReturn )
    {
        bReturnValue = FALSE;
        wsprintf(szLogString, TEXT("Unable to remove \"%s\""), szSourcePath );
        MV_TRACE(szLogString );
        wsprintf(szLogString, TEXT("Last error == 0x%08X"), dwLastError );
        MV_TRACE(szLogString );
        goto Bail;
    }

    //Make sure the directory is gone!
    //NOTE: From time to time the RemoveDirectory() call turns the directory into a 0 byte file
    //              under NT, but returns successful.
    if (FileExists(szSourcePath) )
    {
        bReturnValue = FALSE;
        wsprintf(szLogString, TEXT("Error: \"%s\" still exists"), szSourcePath );
        MV_TRACE(szLogString );
        goto Bail;
    }

    bReturnValue = TRUE;
    
Bail: 

    return bReturnValue;
}

/****************************************************************************

    FUNCTION:   CopyMoveAndDelForAllFileAttributes

    PURPOSE:    Calls CopyMoveAndDelete() with all possible combinations
                of attributes.
    
    PARAMETERS: szSourcePath, Path and file name to create for testing.
                szCopyPath, Path to copy the files to.
                szMovePath, Path to move the files to.
                fdwAttrsAndFlags, Attributes to use when creating the files.
                nNumFiles, Number of files to create.
                nBufSize, Starting size of the files to create.
                
****************************************************************************/
BOOL CopyMoveAndDelForAllFileAttributes(TCHAR *szSourcePath, TCHAR *szCopyPath, 
    TCHAR *szMovePath, DWORD fdwAttrsAndFlags, DWORD nNumFiles, DWORD nBufSize,
    BOOL bTry2CopyAgain)
{
    BOOL bReturnValue    = TRUE;

    MV_TRACE(TEXT("Testing with NORMAL"));
    bReturnValue = CopyMoveAndDelete(szSourcePath, szCopyPath, szMovePath, 
                   FILE_ATTRIBUTE_NORMAL, nNumFiles, nBufSize, bTry2CopyAgain );
    CHECKTRUE(bReturnValue);

    MV_TRACE(TEXT("Testing with ARCHIVE"));
    bReturnValue = CopyMoveAndDelete(szSourcePath, szCopyPath, szMovePath, 
                   FILE_ATTRIBUTE_ARCHIVE, nNumFiles, nBufSize, bTry2CopyAgain );
    CHECKTRUE(bReturnValue);

    MV_TRACE(TEXT("Testing with HIDDEN") );
    bReturnValue = CopyMoveAndDelete(szSourcePath, szCopyPath, szMovePath, 
                  FILE_ATTRIBUTE_HIDDEN, nNumFiles, nBufSize, bTry2CopyAgain );
    CHECKTRUE(bReturnValue);

    MV_TRACE(TEXT("Testing with READONLY") );
    bReturnValue = CopyMoveAndDelete(szSourcePath, szCopyPath, szMovePath, 
                  FILE_ATTRIBUTE_READONLY, nNumFiles, nBufSize, bTry2CopyAgain );
    CHECKTRUE(bReturnValue);

    MV_TRACE(TEXT("Testing with SYSTEM") );
    bReturnValue = CopyMoveAndDelete(szSourcePath, szCopyPath, szMovePath, 
                  FILE_ATTRIBUTE_SYSTEM, nNumFiles, nBufSize, bTry2CopyAgain );
    CHECKTRUE(bReturnValue);

/*
    MV_TRACE(TEXT("Testing with ARCHIVE | HIDDEN") );
    bReturnValue = CopyMoveAndDelete(szSourcePath, szCopyPath, szMovePath, 
                  FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_HIDDEN, nNumFiles, 
                  nBufSize, bTry2CopyAgain );
    CHECKTRUE(bReturnValue);

    MV_TRACE(TEXT("Testing with ARCHIVE | READONLY") );
    bReturnValue = CopyMoveAndDelete(szSourcePath, szCopyPath, szMovePath, 
                  FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_READONLY, nNumFiles, 
                  nBufSize, bTry2CopyAgain );
    CHECKTRUE(bReturnValue);

    MV_TRACE(TEXT("Testing with ARCHIVE | SYSTEM") );
    bReturnValue = CopyMoveAndDelete(szSourcePath, szCopyPath, szMovePath, 
                  FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_SYSTEM, nNumFiles, 
                  nBufSize, bTry2CopyAgain );
    CHECKTRUE(bReturnValue);

    MV_TRACE(TEXT("Testing with HIDDEN | READONLY") );
    bReturnValue = CopyMoveAndDelete(szSourcePath, szCopyPath, szMovePath, 
                  FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY, nNumFiles, 
                  nBufSize, bTry2CopyAgain );
    CHECKTRUE(bReturnValue);

    MV_TRACE(TEXT("Testing with HIDDEN | SYSTEM") );
    bReturnValue = CopyMoveAndDelete(szSourcePath, szCopyPath, szMovePath, 
                  FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM, nNumFiles, 
                  nBufSize, bTry2CopyAgain );
    CHECKTRUE(bReturnValue);

    MV_TRACE(TEXT("Testing with READONLY | SYSTEM") );
    bReturnValue = CopyMoveAndDelete(szSourcePath, szCopyPath, szMovePath, 
                  FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_SYSTEM, nNumFiles, 
                  nBufSize, bTry2CopyAgain );
    CHECKTRUE(bReturnValue);

    MV_TRACE(TEXT("Testing with ARCHIVE | HIDDEN | READONLY") );
    bReturnValue = CopyMoveAndDelete(szSourcePath, szCopyPath, szMovePath, 
                  FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY, 
                  nNumFiles, nBufSize, bTry2CopyAgain );
    CHECKTRUE(bReturnValue);

    MV_TRACE(TEXT("Testing with ARCHIVE | HIDDEN | SYSTEM") );
    bReturnValue = CopyMoveAndDelete(szSourcePath, szCopyPath, szMovePath, 
                  FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM, 
                  nNumFiles, nBufSize, bTry2CopyAgain );
    CHECKTRUE(bReturnValue);

    MV_TRACE(TEXT("Testing with ARCHIVE | READONLY | SYSTEM") );
    bReturnValue = CopyMoveAndDelete(szSourcePath, szCopyPath, szMovePath, 
                  FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_SYSTEM, 
                  nNumFiles, nBufSize, bTry2CopyAgain );
    CHECKTRUE(bReturnValue);
*/

    MV_TRACE(TEXT("Testing with ARCHIVE | HIDDEN | READONLY | SYSTEM") );
    bReturnValue = CopyMoveAndDelete(szSourcePath, szCopyPath, szMovePath, 
                  FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_SYSTEM, 
                  nNumFiles, nBufSize, bTry2CopyAgain );
    CHECKTRUE(bReturnValue);

    return bReturnValue;
}

//******************************************************************************
//
// "Completely" create the dir, and all the dirs along its path.
//
// Since there is no concept of "current dir" in object store, the
// path always starts with a '\'.
//
UINT CompleteCreateDirectory(
    LPTSTR pszCompletePath
)
{
    LPTSTR      pszPath, pszDir, pszBackSlash;
    UINT        uStatus = TRUE;

    if (!pszCompletePath)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    pszPath = (LPTSTR)LocalAlloc(0, (_tcslen(pszCompletePath)+1)*sizeof(TCHAR));
    if (!pszPath)
    {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }

    _tcscpy(pszPath, pszCompletePath);
    if (IS_UNC(pszPath)) {
        UINT uSlashCnt = 0;
        
        // UNC, skip server and share
        for (pszDir = pszPath; *pszDir && (uSlashCnt < 4); pszDir++)
            if (*pszDir == TEXT('\\'))
                uSlashCnt++;
        
        if (!*pszDir) {
            LocalFree(pszPath);
            return FALSE;
        }
        pszDir--;
    }
    else
        pszDir = pszPath;
    
    for(;;)
    {
        pszBackSlash = _tcschr(pszDir+1, TEXT('\\'));
        if (!pszBackSlash)
            break;

        *pszBackSlash = TEXT('\0');
        // The WinCE redir returns ACCESS_DENIED if dir already exists (like Win95)
        if (!CreateDirectory(pszPath, NULL) && (GetLastError() != ERROR_ALREADY_EXISTS) &&
            (!IS_UNC(pszPath) || (GetLastError() != ERROR_ACCESS_DENIED)))
        {
            LocalFree(pszPath);
            return FALSE;
        }
        
        *pszBackSlash = TEXT('\\');
        pszDir = pszBackSlash;
    }

    if (!CreateDirectory(pszPath, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
        uStatus = FALSE;

    LocalFree(pszPath);
    return uStatus;
}   

//
// Remove the dir and its parent dirs.
//
UINT CompleteRemoveDirectoryMinusPcCard(
    LPTSTR pszCompletePath,
    TCHAR *szCardRoot
)
{
    LPTSTR      pszPath, pszBackSlash;
    UINT        uStatus     = TRUE;
    DWORD       dwLastError = ERROR_SUCCESS;

    if (!pszCompletePath)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    pszPath = (LPTSTR)LocalAlloc(0, (_tcslen(pszCompletePath)+1)*sizeof(TCHAR));
    if (!pszPath)
    {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }

    _tcscpy(pszPath, pszCompletePath);

    for(;;)
    {
        if (_tcscmp(pszPath, szCardRoot) == 0 )    //Don't remove Storage Card path off root.
            break;
        if (!RemoveDirectory(pszPath))
        {
            DWORD dwLocalLastError = GetLastError();
            if (dwLocalLastError != ERROR_FILE_NOT_FOUND        //If it's gone.
            && dwLocalLastError != ERROR_FILENAME_EXCED_RANGE   //If name too long, try parent dir.
            && dwLocalLastError != ERROR_PATH_NOT_FOUND)        //If parent not found, try it's parent.
            {
                uStatus = FALSE;
                break;
            }
            else
            {
                dwLastError = dwLocalLastError;
            }
        }

        pszBackSlash = _tcsrchr(pszPath, TEXT('\\'));

        if (pszBackSlash == pszPath)
            break;

        *pszBackSlash = TEXT('\0');
    }

    SetLastError(dwLastError);
    LocalFree(pszPath);
    return uStatus;
}   



BOOL CreateAndRemoveRootDirectory(TCHAR *szDirSpec, TCHAR *szCardRoot )
{
    BOOL        bReturnValue    = TRUE;
    BOOL        bCallReturn;
    DWORD       dwStrLength;
    DWORD       dwLastError;

    dwStrLength = _tcslen(szDirSpec );

    //Make sure the directory does not exist.
    if (FileExists(szDirSpec) )
    {
        MV_TRACE(TEXT("Warning: Directory already exists, deleting it") );
        bReturnValue = CompleteRemoveDirectoryMinusPcCard(szDirSpec, szCardRoot );
        if (!bReturnValue )
        {
            MV_TRACE(TEXT("Unable to remove directory") );
        }
    }
    
    if (bReturnValue )
    {
        bReturnValue = (CompleteCreateDirectory(szDirSpec ) == TRUE);
        dwLastError = GetLastError();
        if (!bReturnValue )
        {
            TCHAR szTemp[128];
            MV_TRACE(TEXT("Unable to Create new directory \"%s\""), szDirSpec );
            wsprintf(szTemp, TEXT("Last error == 0x%08X"), dwLastError );
            MV_TRACE(szTemp );
        }
        MV_TRACE(TEXT("%s dir created"), szDirSpec);
    }

    //Blow it away even if it wasn't completely created.  This is because 
    //most of the path probably was created and it needs to be nuked.
    bCallReturn = (CompleteRemoveDirectoryMinusPcCard(szDirSpec, szCardRoot ) == TRUE);
    if (!bCallReturn || FileExists(szDirSpec) )
    {
        bReturnValue = FALSE;       //In case szDirSpec still exists!!!
        MV_TRACE(TEXT("Unable to remove new directory \"%s\""), szDirSpec );
    }

    return bReturnValue;
}

//******************************************************************************
int GetTestResult() {
   
   // Check to see if we had any failures.
   for (int i = 0; i <= LOG_FAIL; i++) {
      if (g_pKato->GetVerbosityCount(i)) {
         return TPR_FAIL;
      }
   }

   // Then check to see if we had any aborts.
   for (; i <= LOG_ABORT; i++) {
      if (g_pKato->GetVerbosityCount(i)) {
         return TPR_ABORT;
      }
   }

   // Then check to see if we had any skips.
   for (; i <= LOG_NOT_IMPLEMENTED; i++) {
      if (g_pKato->GetVerbosityCount(i)) {
         return TPR_SKIP;
      }
   }

   // If none of the above, then the test passed.
   return TPR_PASS;
}

//******************************************************************************
BOOL DeleteTree(LPCTSTR szPath, BOOL fDontDeleteRoot) {
   TCHAR szBuf[MAX_PATH];

   MV_TRACE(TEXT("Cleaning up \"%s\"..."), szPath);
    
   // Copy the path to a temp buffer.
   _tcsncpy(szBuf, szPath, MAX_PATH);
   szBuf[MAX_PATH-1]=_T('\0');

   // Get the length of the path.
   int length = _tcslen(szBuf);

   // Ensure the path ends with a wack.
   if (szBuf[length - 1] != TEXT('\\')) {
      szBuf[length++] = TEXT('\\');
      szBuf[length] = TEXT('\0');
   }

   return DeleteTreeRecursize(szBuf, fDontDeleteRoot);
}

//******************************************************************************
BOOL DeleteTreeRecursize(LPTSTR szPath, BOOL fDontDeleteRoot) {

   BOOL fResult = TRUE;

   

   // Locate the end of the path.
   LPTSTR szAppend = szPath + _tcslen(szPath);

   // Append our search spec to the path.
   _tcscpy(szAppend, TEXT("*"));

   // Start the file/subdirectory find.
   WIN32_FIND_DATA w32fd;
   HANDLE hFind = FindFirstFile(szPath, &w32fd);

   // Loop on each file/subdirectory in this directory.
   if (hFind != INVALID_HANDLE_VALUE) {
      do {
         
         // Check to see if the find is a directory.
         if (w32fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            
            // Make sure the directory is not "." or ".."
            if (_tcscmp(w32fd.cFileName, TEXT(".")) && _tcscmp(w32fd.cFileName, TEXT(".."))) {

               // Append the directory to our path and recurse into it.
               _tcscat(_tcscpy(szAppend, w32fd.cFileName), TEXT("\\"));
               fResult &= DeleteTreeRecursize(szPath, FALSE);
            }
         
         // Otherwise the find must be a file.
         } else {

            // Append the file to our path.
            _tcscpy(szAppend, w32fd.cFileName);

            // If the file is read-only, change to read/write.
            if (w32fd.dwFileAttributes & FILE_ATTRIBUTE_READONLY) {
               if (!SetFileAttributes(szPath, FILE_ATTRIBUTE_NORMAL)) {
                  LOG(TEXT("Could not set attributes: \"%s\" [%u]"), szPath, GetLastError());
               }
            }

            // Delete the file.
            if (!DeleteFile(szPath)) {
               LOG(TEXT("Could not delete: \"%s\" [%u]"), szPath, GetLastError());
               fResult = FALSE;
            }
         }

      // Get next file/directory.
      } while (FindNextFile(hFind, &w32fd));

      // Close our find.
      FindClose(hFind);
   }

   if (!fDontDeleteRoot) {

      // Delete the directory;
      *(szAppend - 1) = TEXT('\0');
      if (!RemoveDirectory(szPath)) {
         LOG(TEXT("Could not remove: \"%s\" [%u]"), szPath, GetLastError());
         return FALSE;
      }
   }

   return fResult;
}
/****************************************************************************

    FUNCTION:       CreateDirectoryAndFile

    PURPOSE:        To create a directory and then a file inside this directory.

****************************************************************************/
BOOL CreateDirectoryAndFile(TCHAR *szFilePath, TCHAR *szFileName )
{
    BOOL            bReturnValue    = TRUE;
    TCHAR           szFileSpec[MAX_PATH+20];
    HANDLE          hFile;
    DWORD           dwLastError;
    
    if (bReturnValue )
    {
        bReturnValue = (CompleteCreateDirectory(szFilePath ) == TRUE);
        dwLastError = GetLastError();
        if (!bReturnValue )
        {
            //Reuse szFileSpec to save memory!
            wsprintf(szFileSpec, TEXT("Unable to create directory \"%s\""), szFilePath );
            MV_TRACE(szFileSpec );
            wsprintf(szFileSpec, TEXT("Last error == 0x%08X"), dwLastError );
            MV_TRACE(szFileSpec );
        }
    }

    if (bReturnValue )
    {
        wsprintf(szFileSpec, TEXT("%s\\%s"), szFilePath, szFileName );
        hFile = CreateFile(szFileSpec, GENERIC_READ | GENERIC_WRITE, 0, 
                            NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
        dwLastError = GetLastError();
        if (INVALID_HANDLE_VALUE == hFile )
        {
            bReturnValue = FALSE;
            //Reuse szFileSpec to save memory!
            wsprintf(szFileSpec, TEXT("Unable to create file \"%s\\%s\""), szFilePath, szFileName );
            MV_TRACE(szFileSpec );
            wsprintf(szFileSpec, TEXT("Last error == 0x%08X"), dwLastError );
            MV_TRACE(szFileSpec );
        }
        else
        {
            DWORD   nBufSize;
            DWORD   nBytesWritten;
            nBufSize = _tcslen(szFileSpec );
            bReturnValue = WriteFile(hFile, szFileSpec, nBufSize, &nBytesWritten, NULL );
            dwLastError = GetLastError();
            if (!bReturnValue || (nBytesWritten != nBufSize) )
            {
                bReturnValue = FALSE;   //In case (nBytesWritten != nBufSize)
                MV_TRACE(TEXT("Error writing to file in CreateDirectoryAndFile()") );
                wsprintf(szFileSpec, TEXT("Last error == 0x%08X"), dwLastError );
                MV_TRACE(szFileSpec );
            }

            if (!CloseHandle(hFile) )
            {
                dwLastError = GetLastError();
                bReturnValue = FALSE;
                MV_TRACE(TEXT("Unable to close the file in CreateDirectoryAndFile()") );
                wsprintf(szFileSpec, TEXT("Last error == 0x%08X"), dwLastError );
                MV_TRACE(szFileSpec );
            }
        }
    }

    return bReturnValue;
}

/****************************************************************************

    FUNCTION:   NukeDirectories()

    PURPOSE:    Nukes the directories found using FindFirstFile() and 
                FindNextFile() according to the file spec passed in.

****************************************************************************/
BOOL NukeDirectories(TCHAR *szFilePath, TCHAR *szFileSpec )
{
    WIN32_FIND_DATA     ff;
    HANDLE              hFF                 = INVALID_HANDLE_VALUE;
    HANDLE              hOrigFF             = INVALID_HANDLE_VALUE;
    BOOL                bReturnValue        = TRUE;
    DWORD               dwLastError;
    TCHAR               szLogStr[MAX_PATH+64];

    wsprintf(szLogStr, TEXT("%s\\%s"), szFilePath, szFileSpec );
    hFF = FindFirstFile(szLogStr, &ff );
    hOrigFF = hFF;
    if (hFF == INVALID_HANDLE_VALUE )
    {
        goto Bail;
    }

    wsprintf(szLogStr, TEXT("%s\\%s"), szFilePath, ff.cFileName );
    if (!RemoveDirectory(szLogStr ) )
    {
        dwLastError = GetLastError();
        wsprintf(szLogStr, TEXT("Error removing directory \"%s\\%s\""), szFilePath, ff.cFileName );
        MV_TRACE(szLogStr );
        wsprintf(szLogStr, TEXT("Last error == 0x%08X"), dwLastError );
        MV_TRACE(szLogStr );
        bReturnValue = FALSE;
    }
    
    while (FindNextFile(hFF, &ff ) )
    {
        wsprintf(szLogStr, TEXT("%s\\%s"), szFilePath, ff.cFileName );
        if (!RemoveDirectory(szLogStr ) )
        {
            dwLastError = GetLastError();
            wsprintf(szLogStr, TEXT("Error removing directory \"%s\\%s\""), szFilePath, ff.cFileName );
            MV_TRACE(szLogStr );
            wsprintf(szLogStr, TEXT("Last error == 0x%08X"), dwLastError );
            MV_TRACE(szLogStr );
            bReturnValue = FALSE;
        }
    }

Bail:
    if (hOrigFF != INVALID_HANDLE_VALUE )
    {
        FindClose(hOrigFF );
    }

    return bReturnValue;
}
