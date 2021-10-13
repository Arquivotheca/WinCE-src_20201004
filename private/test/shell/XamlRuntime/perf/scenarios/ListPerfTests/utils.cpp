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


#include "PerfListTests.h"
#include "Globals.h"

#define SOURCE_IMAGE_FILENAME_FORMAT L"XRPix%02d.jpg"
#define RESOURCE_PERF_JPEGS_DIR XRPERF_RESOURCE_DIR L"ListPerf\\JPEGs128x128\\"

////////////////////////////////////////////////////////////////////////////////
//
//  CreateImageCopies
//
//   Creates NumItems copies of a jpg source file
//
////////////////////////////////////////////////////////////////////////////////
bool CreateImageCopies( int NumItems )
{
    WCHAR szSourceNameBuffer[MAX_PATH] = {0};
    WCHAR szDestNameBuffer[MAX_PATH] = {0};
    WCHAR szFileName[30] = {0};
    DWORD dwRet = TPR_FAIL;

    int i = 0;

    //make the dest directory
    if( !CreateDirectory( XR_WINDOWSXAMLRUNTIME_PATH, NULL ) )
    {
        //Ignore failure if GetLastError == ERROR_ALREADY_EXISTS
        CHK_BOOL( GetLastError() == ERROR_ALREADY_EXISTS, L"Error creating directory" XR_WINDOWSXAMLRUNTIME_PATH, dwRet );
    }
    if( !CreateDirectory( XR_XAMLRUNTIMETEST_PATH, NULL ) )
    {
        //Ignore failure if GetLastError == ERROR_ALREADY_EXISTS
        CHK_BOOL( GetLastError() == ERROR_ALREADY_EXISTS, L"Error creating directory" XR_XAMLRUNTIMETEST_PATH, dwRet );
    }
    if( !CreateDirectory( XR_XAMLRUNTIMETESTPERF_PATH, NULL ) )
    {
        //Ignore failure if GetLastError == ERROR_ALREADY_EXISTS
        CHK_BOOL( GetLastError() == ERROR_ALREADY_EXISTS, L"Error creating directory" XR_XAMLRUNTIMETESTPERF_PATH, dwRet );
    }
    if( !CreateDirectory( XR_XAMLRUNTIMETESTPERFCOPIES_PATH, NULL ) )
    {
        //Ignore failure if GetLastError == ERROR_ALREADY_EXISTS
        CHK_BOOL( GetLastError() == ERROR_ALREADY_EXISTS, L"Error creating directory" XR_XAMLRUNTIMETESTPERFCOPIES_PATH, dwRet );
    }

    //Copy from \\release to the device
    for( i = 1; i <= 20; i++ )
    {
        //Under the CTK we should not specify a release path
        StringCchPrintfW( szSourceNameBuffer, MAX_PATH, g_bRunningUnderCTK ? SOURCE_IMAGE_FILENAME_FORMAT : RESOURCE_PERF_JPEGS_DIR SOURCE_IMAGE_FILENAME_FORMAT, i );
        StringCchPrintfW( szDestNameBuffer, MAX_PATH, XR_XAMLRUNTIMETESTPERFCOPIES_PATH L"\\" SOURCE_IMAGE_FILENAME_FORMAT, i );
        CHK_BOOL( CopyFile( szSourceNameBuffer, szDestNameBuffer, false ) != false, L"Error copying from \\release to \\windows\\xamlruntime\\test\\perf\\copies", dwRet )
    }


    for( ; i <= NumItems; i++ )
    {
        //Point the source to the copy that is now on the device, so we don't keep copying from \\release
        StringCchPrintfW( szSourceNameBuffer, MAX_PATH, XR_XAMLRUNTIMETESTPERFCOPIES_PATH L"\\" SOURCE_IMAGE_FILENAME_FORMAT, 1+((i-1)%20) );

        StringCchPrintfW( szFileName, 30, SOURCE_IMAGE_FILENAME_FORMAT, i );
        StringCchPrintfW( szDestNameBuffer, MAX_PATH, XR_XAMLRUNTIMETESTPERFCOPIES_PATH L"\\%s", szFileName );

        CHK_BOOL( CopyFile( szSourceNameBuffer, szDestNameBuffer, false ) != false, szFileName, dwRet )
    }

    dwRet = TPR_PASS;
Exit:
    return dwRet == TPR_PASS;
}

////////////////////////////////////////////////////////////////////////////////
//
//  DeleteImageCopies
//
//   Deletes NumItems copies of a jpg source file
//
////////////////////////////////////////////////////////////////////////////////
bool DeleteImageCopies( int NumItems )
{
    WCHAR szNameBuffer[MAX_PATH] = {0};
    WCHAR szFileName[30] = {0};

    int i = 0;

    for( i = 0; i < NumItems; i++ )
    {
        StringCchPrintfW( szFileName, 30, SOURCE_IMAGE_FILENAME_FORMAT, i+1 );
        StringCchPrintfW( szNameBuffer, MAX_PATH, XR_XAMLRUNTIMETESTPERFCOPIES_PATH L"\\%s", szFileName );

        if( !DeleteFile( szNameBuffer ) ) 
        {
            g_pKato->Log(LOG_COMMENT, TEXT("Error 0x%X deleting %s"), GetLastError(), szNameBuffer );
            return false;
        }
    }
    return true;
}