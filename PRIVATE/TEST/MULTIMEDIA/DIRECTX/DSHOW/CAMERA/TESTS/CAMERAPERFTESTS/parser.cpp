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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#include "stdafx.h"

double wtof( WCHAR * pszString )
{
    char *String;
    double dValue = 0.0;
    int nLength;
    
    if(!pszString)
        return 0.0;

    nLength = wcslen(pszString);

    String = new char[nLength + 1];

    if(String)
    {
        WideCharToMultiByte( CP_ACP, 0, pszString, nLength, String, nLength + 1, NULL, NULL );
        String[nLength] = NULL;

        dValue = atof(String);

        delete[] String;
    }

    return dValue;
}


CParser::CParser()
{
}


CParser::~CParser()
{
}

HRESULT CParser::LoadIniFile( WCHAR *wzIniFile )
{
    HRESULT hr = S_OK;
    HANDLE  hFile = INVALID_HANDLE_VALUE;
    DWORD   dwLines = 0;
    DWORD   dwFileSize = 0;
    DWORD   dwRead = 0;
    char    *szBuffer = NULL;
    char    *szPointer;

    ChkBool( wzIniFile != NULL, E_INVALIDARG );
    
    // Open the Ini file
    hFile = CreateFile( wzIniFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, NULL, NULL );
    if( hFile == INVALID_HANDLE_VALUE )
    {
        Error(( L"%d is an Invalid Ini File", wzIniFile ));
        Chk( HRESULT_FROM_WIN32( GetLastError() ));
    }

    // Let's allocate a buffer large enough to load the whole file. 
    // The file shouldn't be larger than a few kilobytes, so there's no need to be smart about loading it
    dwFileSize = GetFileSize( hFile, NULL );
    if( dwFileSize == 0xFFFFFFFF )
    {
        Error(( L"Invalid Ini file size" ));
        Chk( HRESULT_FROM_WIN32( GetLastError() ));
    }

    // Allocate the buffer to read the content of the file
    szBuffer = (char*) LocalAlloc( LMEM_ZEROINIT, dwFileSize + 1 );
    if( szBuffer == NULL )
    {
        Error(( L"Error out of memory" ));
        Chk( E_OUTOFMEMORY );
    }

    // Read the buffer
    if( ReadFile( hFile, szBuffer, dwFileSize, &dwRead, NULL ) == NULL )
    {
        Error(( L"Error reading the file" ));
        Chk( HRESULT_FROM_WIN32( GetLastError() ));
    }

    // Count the number of lines in the ini file. This will be the number of parameters
    szPointer = szBuffer;
    do
    {
        // if it's an empty line, ignore it.
        while(szPointer[0] == '\n' || szPointer[0] == '\r')
            szPointer++;

        // if we found one, then add it.
        if(szPointer = strchr( szPointer, '\n' ))
        {
            szPointer++;
            dwLines++;
        }
    }
    while( szPointer && *szPointer);


    // Allocate the resources on the parameterslist object
    Chk( m_list.CreateList( dwLines ));

    // And parse the individual lines
    szPointer = szBuffer;
    for( DWORD i = 0; i < dwLines; i++ )
    {
        // if it's an empty line, ignore it.
        while(szPointer[0] == '\n' || szPointer[0] == '\r')
            szPointer++;

        // find the carriage return (the 0x0d before the 0x0a)
        char *szPointer2 = strchr( szPointer, '\r' );
        // if the next carriage return was found, then terminate the line at it.
        if(szPointer2)
            *szPointer2 = '\0';

        Chk( ParseLine( szPointer, i ));
        
        // restore the data to it's original form, then move on.
        *szPointer2 = '\r';
        
        // add two to get past the 0x0d0a carriage return/line feed at the end of the line.
        szPointer = szPointer2+2;
    }



Cleanup:
 
    LocalFree( szBuffer );

    if(hFile != INVALID_HANDLE_VALUE)
        CloseHandle(hFile);

    return hr;
}

HRESULT 
CParser::ParseLine( char* szPointer, DWORD dwIndex )
{
    HRESULT hr = S_OK;
    
    char* szEnd;
    WCHAR* wzEnumType = NULL;

    ChkBool( szPointer != NULL, E_INVALIDARG );
    ChkBool(dwIndex < m_list.dwParameterCount, E_INVALIDARG);

    // Let's get the name
    hr = ExtractData( szPointer, &szEnd, &( m_list.pParameters[dwIndex].wzName ));
    if( hr == S_FALSE )
    {
        return S_FALSE;
    }

    // Then the recipient
    szPointer = szEnd + 1;
    Chk( ExtractData( szPointer, &szEnd, &( m_list.pParameters[dwIndex].wzRecipient )));

    // Then the type of the enumeration
    szPointer = szEnd + 1;
    Chk( ExtractData( szPointer, &szEnd, &wzEnumType ));

    // Depending on the type of enumeration, there are 2 different processing
    if( wcsicmp( wzEnumType, L"Range" ) == 0 )
    {
        ExtractRange( szEnd, dwIndex );
    }
    else if( wcsicmp( wzEnumType, L"List" ) == 0 )
    {
        ExtractList( szEnd, dwIndex );
    }
    else
    {
        Chk( E_FAIL );
    }

Cleanup:
    if(wzEnumType)
        LocalFree( wzEnumType );

    return hr;
}


HRESULT
CParser::ExtractRange( char *szBegin, DWORD dwIndex )
{
    HRESULT hr = S_OK;
    double  dFirst, dLast, dIter;
    char    *szEnd;
    WCHAR   *wzTemp;

    ChkBool( szBegin != NULL, E_INVALIDARG );
    ChkBool(dwIndex < m_list.dwParameterCount, E_INVALIDARG);

    // Extract the 3 variables
    Chk( ExtractData( szBegin, &szEnd, &wzTemp ));
    dFirst = wtof( wzTemp );
    LocalFree( wzTemp );
    wzTemp = NULL;

    szBegin = szEnd;
    Chk( ExtractData( szBegin, &szEnd, &wzTemp ));
    dLast = wtof( wzTemp );
    LocalFree( wzTemp );
    wzTemp = NULL;

    szBegin = szEnd;
    Chk( ExtractData( szBegin, &szEnd, &wzTemp ));
    dIter = wtof( wzTemp );
    LocalFree( wzTemp );
    wzTemp = NULL;

    PREFAST_ASSERT(m_list.pParameters[dwIndex].dwCount < 128);

    m_list.pParameters[dwIndex].dwCount = (DWORD) (( dLast - dFirst ) / dIter ) + 1;
    m_list.pParameters[dwIndex].pValues = (double*) LocalAlloc( LMEM_ZEROINIT, m_list.pParameters[dwIndex].dwCount * sizeof( double ));

    ChkBool( m_list.pParameters[dwIndex].pValues, E_OUTOFMEMORY );

    for( DWORD i = 0; i < m_list.pParameters[dwIndex].dwCount; i++ )
    {
        m_list.pParameters[dwIndex].pValues[ i ] = dFirst + i * dIter;
    }

Cleanup: 
    LocalFree( wzTemp );
    return hr;
}


HRESULT
CParser::ExtractList( char *szBegin, DWORD dwIndex )
{
    HRESULT hr = S_OK;
    WCHAR   *wzTemp;
    char    *szEnd;
    char    *szPointer;

    ChkBool( szBegin != NULL, E_INVALIDARG );
    ChkBool(dwIndex < m_list.dwParameterCount, E_INVALIDARG);

    // Count how many items we have in the list
    m_list.pParameters[dwIndex].dwCount = 1;

    szPointer = szBegin;
    while(( *szPointer != '\0' ) && ( *szPointer != '\n' ))
    {
        if( *szPointer == ',' )
        {
            m_list.pParameters[dwIndex].dwCount++;
        }

        szPointer++;
    }


    m_list.pParameters[dwIndex].pValues = (double*) LocalAlloc( LMEM_ZEROINIT, m_list.pParameters[dwIndex].dwCount * sizeof( double ));
    ChkBool( m_list.pParameters[dwIndex].pValues, E_OUTOFMEMORY );       

    // Run a loop until we run out of stuff to read
    for( DWORD i = 0; i < m_list.pParameters[dwIndex].dwCount; i++ )
    {
        Chk( ExtractData( szBegin, &szEnd, &wzTemp ));
        m_list.pParameters[dwIndex].pValues[i] = wtof( wzTemp );
        LocalFree( wzTemp );
        wzTemp = NULL;
        szBegin = szEnd;
    }

Cleanup:
    LocalFree( wzTemp );
    return hr;
}


HRESULT
CParser::ExtractData( char *szBegin, char **pszEnd, WCHAR **pwzValue )
{
    HRESULT hr = S_OK;

    char* szComa;
    char szReplaced = ',';

    ChkBool( szBegin != NULL, E_INVALIDARG );
    ChkBool( pszEnd != NULL, E_INVALIDARG );
    ChkBool( pwzValue != NULL, E_INVALIDARG );

    while( *szBegin == ' ' )
    {
        szBegin++;
    }

    // First step, let's determine the name
    szComa = strchr( szBegin, ',' );
    if( szComa == NULL )
    {
        // if there's no comma, then go to the end of the string
        // and change the replaced character to a NULL (so the string isn't modified)
        szComa = strchr( szBegin, '\0' );
        szReplaced = '\0';
        if( szComa == NULL)
            return E_FAIL;
    }

    *szComa = '\0';
    *pwzValue = (WCHAR*) LocalAlloc( LMEM_ZEROINIT, ( strlen( szBegin ) + 1 ) * 2 );
    if( *pwzValue == NULL )
    {
        // Out of memory
        Error(( L"Out of memory" ));
        Chk( E_OUTOFMEMORY );
    }

    MultiByteToWideChar( CP_ACP, 0, szBegin, strlen( szBegin ), *pwzValue, strlen( szBegin ) + 1 );
    *szComa = szReplaced;

    *pszEnd = szComa + 1;

Cleanup:
    return hr;
}



