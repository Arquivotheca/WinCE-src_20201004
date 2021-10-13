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

#include "Udf.h"
#include <NtCompat.h>

// -----------------------------------------------------------------------------
// CFileName Implementation
// -----------------------------------------------------------------------------

// /////////////////////////////////////////////////////////////////////////////
// Constructor
//
CFileName::CFileName()
: m_strFileName( NULL ),
  m_fCanUseWildcards( FALSE ),
  m_strCurrentToken( NULL ),
  m_dwOffset( 0 ),
  m_dwTokenIndex( 0 ),
  m_dwNumTokens( 0 ),
  m_fHasStream( FALSE ),
  m_fHasWildcards( FALSE ),
  m_dwFileIndex( 0 ),
  m_dwStreamIndex( 0 ),
  m_dwTokenStop( 0 )
{
    ZeroMemory( &m_TokenList, sizeof(m_TokenList) );
}

// /////////////////////////////////////////////////////////////////////////////
// Destructor
//
CFileName::~CFileName()
{
    PTOKEN_LIST pTokList = m_TokenList.pNext;
    
    if( m_strFileName )
    {
        delete [] m_strFileName;
    }

    while( pTokList )
    {
        PTOKEN_LIST pCur = pTokList;

        pTokList = pTokList->pNext;

        delete pCur;
    }
}

// /////////////////////////////////////////////////////////////////////////////
// Initialize
//
BOOL CFileName::Initialize( const WCHAR* strFileName, BOOL fCanUseWildcards )
{
    BOOL fResult = TRUE;
    PTOKEN_LIST pTokList = &m_TokenList;
    DWORD dwTokStart = 0;
    DWORD dwWildCharStart = 0;

    SetLastError( ERROR_SUCCESS );

    if( !strFileName )
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        goto exit_initialize;
    }

    m_fCanUseWildcards = fCanUseWildcards;

    __try
    {
        m_dwSizeInChars = wcslen( strFileName );

        if( m_dwSizeInChars == 0 )
        {
            SetLastError( ERROR_BAD_PATHNAME );
            goto exit_initialize;
        }

        m_strFileName = new (UDF_TAG_STRING) WCHAR[ m_dwSizeInChars + 1 ];
        if( !m_strFileName )
        {
            SetLastError( ERROR_NOT_ENOUGH_MEMORY );
            goto exit_initialize;
        }

        ZeroMemory( m_strFileName, (m_dwSizeInChars + 1) * sizeof(WCHAR) );
        CopyMemory( m_strFileName, strFileName, m_dwSizeInChars * sizeof(WCHAR) );
    }
    __except( EXCEPTION_EXECUTE_HANDLER )
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        goto exit_initialize;
    }

    //
    // If the string starts with a '\' then move past it.
    //
    if( m_strFileName[0] == L'\\' )
    {
        if( m_dwSizeInChars == 1 )
        {
            //
            // TODO::can we specify the root directory?
            //
            SetLastError( ERROR_BAD_PATHNAME );
            goto exit_initialize;
        }

        if( m_strFileName[1] == L'\\' )
        {
            SetLastError( ERROR_BAD_PATHNAME );
            goto exit_initialize;
        }

        //
        // We want to move the NULL char too, so we use m_dwSizeInChars instead
        // of m_dwSizeInChars - 1.
        //
        memmove( &m_strFileName[0], 
                 &m_strFileName[1], 
                 m_dwSizeInChars * sizeof(WCHAR) );

        m_dwSizeInChars -= 1;
    }

    //
    // We don't currently support opening directories.  So it cannot end in '\'.
    //
    if( m_strFileName[m_dwSizeInChars-1] == L'\\' )
    {
        m_strFileName[m_dwSizeInChars-1] = NULL;
        m_dwSizeInChars -= 1;
    }

    for( DWORD x = m_dwOffset; x < m_dwSizeInChars; x++ )
    {
        //
        // Make sure a '\\' is legal here, and then update the start of the file
        // name.  (The file name will start after the last '\\'.
        //
        if( m_strFileName[x] == L'\\' )
        {
            if( m_fHasStream )
            {
                //
                // We cannot have a '\' after a ':'
                //
                SetLastError( ERROR_INVALID_PARAMETER );
                goto exit_initialize;
            }
            
            if( m_strFileName[x + 1] == L'\\' )
            {
                SetLastError( ERROR_INVALID_PARAMETER );
                goto exit_initialize;
            }

            dwWildCharStart = x + 1;
            m_dwFileIndex = m_dwNumTokens + 1;
        }

        //
        // Make sure a ':' is legal here and then record that we've found a
        // stream.
        //
        if( m_strFileName[x] == L':' )
        {
            if( m_fHasStream )
            {
                SetLastError( ERROR_INVALID_PARAMETER );
                goto exit_initialize;
            }
            
            if( x == 0 )
            {
                SetLastError( ERROR_INVALID_PARAMETER );
                goto exit_initialize;
            }

            if( x == m_dwSizeInChars - 1 )
            {
                SetLastError( ERROR_INVALID_PARAMETER );
                goto exit_initialize;
            }
            
            if( m_strFileName[x - 1] == L'\\' )
            {
                SetLastError( ERROR_INVALID_PARAMETER );
                goto exit_initialize;
            }

            if( m_strFileName[x + 1] == L'\\' )
            {
                SetLastError( ERROR_INVALID_PARAMETER );
                goto exit_initialize;
            }

            m_fHasStream = TRUE;
            dwWildCharStart = x + 1;
            m_dwStreamIndex = m_dwNumTokens + 1;
        }

        //
        // In either case, we'll need to record the size of the token that just
        // ended, and mark its location to find easier in the future.
        //
        if( m_strFileName[x] == L'\\' ||
            m_strFileName[x] == L':' )
        {
            
            if( m_dwNumTokens == 0 )
            {
                m_strCurrentToken = &m_strFileName[m_dwOffset];

                pTokList->Info[0].dwLength = x - m_dwOffset;
                pTokList->Info[0].dwOffset = m_dwOffset;
            }
            else
            {
                BYTE TokOffset = (BYTE)(m_dwNumTokens % TOKENS_PER_LIST);
                if( TokOffset == 0 )
                {
                    pTokList->pNext = new (UDF_TAG_TOKEN_LIST) TOKEN_LIST;
                    if( !pTokList->pNext )
                    {
                        SetLastError( ERROR_NOT_ENOUGH_MEMORY );
                        goto exit_initialize;
                    }

                    pTokList = pTokList->pNext;
                }

                pTokList->Info[TokOffset].dwLength = x - dwTokStart;
                pTokList->Info[TokOffset].dwOffset = dwTokStart;
            }

            m_strFileName[x] = NULL;
            dwTokStart = x + 1;
            m_dwNumTokens += 1;
        }
        
    }

    //
    // Since the path didn't end in a '\', we need to fix our last token.
    //
    if( m_dwNumTokens == 0 )
    {
        //
        // This is for file names passed in like "file.txt" where no token
        // beyond this would be found.
        //
        m_strCurrentToken = &m_strFileName[m_dwOffset];
        m_TokenList.Info[0].dwOffset = m_dwOffset;
        m_TokenList.Info[0].dwLength = m_dwSizeInChars - m_dwOffset;
    }
    else
    {
        BYTE TokOffset = (BYTE)(m_dwNumTokens % TOKENS_PER_LIST);
        if( TokOffset == 0 )
        {
            pTokList->pNext = new (UDF_TAG_TOKEN_LIST) TOKEN_LIST;
            if( !pTokList->pNext )
            {
                SetLastError( ERROR_NOT_ENOUGH_MEMORY );
                goto exit_initialize;
            }

            pTokList = pTokList->pNext;
        }

        pTokList->Info[TokOffset].dwLength = wcslen( &m_strFileName[dwTokStart] );
        pTokList->Info[TokOffset].dwOffset = dwTokStart;
    }

    m_dwNumTokens += 1;

    //
    // Make sure that there are no wildcards before the file name.
    //
    for( DWORD x = 0; x < dwWildCharStart; x++ )
    {
        if( m_strFileName[x] == L'?' ||
            m_strFileName[x] == L'*' )
        {
            SetLastError( ERROR_INVALID_PARAMETER );
            goto exit_initialize;
        }
    }

    //
    // If we're not allowing wildcards, then check the file name as well.
    //
    if( !m_fCanUseWildcards )
    {
        for( DWORD x = dwWildCharStart; x < m_dwSizeInChars; x++ )
        {
            if( m_strFileName[x] == L'?' ||
                m_strFileName[x] == L'*' )
            {
                SetLastError( ERROR_INVALID_PARAMETER );
                goto exit_initialize;
            }
        }
    }

    m_dwTokenStop = m_dwNumTokens;

exit_initialize:
    if( GetLastError() != ERROR_SUCCESS )
    {
        fResult = FALSE;
    }

    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// IsMatch
//
BOOL CFileName::IsMatch( const WCHAR* strName ) const
{
    BOOL fResult = FALSE;

    if( !m_strCurrentToken )
    {
        return FALSE;
    }

    fResult = MatchesWildcardMask( wcslen( m_strCurrentToken ), 
                                   m_strCurrentToken,
                                   wcslen( strName ), 
                                   strName );

    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// GotoNextToken
//
BOOL CFileName::GotoNextToken()
{
    PTOKEN_LIST pTokList = &m_TokenList;
    BYTE TokOffset = (BYTE)(++m_dwTokenIndex % TOKENS_PER_LIST);

    if( m_dwTokenIndex >= m_dwTokenStop )
    {
        m_dwTokenIndex = m_dwTokenStop;
        m_strCurrentToken = NULL;
        return FALSE;
    }

    for( DWORD x = 0; x < m_dwTokenIndex / TOKENS_PER_LIST; x++ )
    {
        ASSERT( pTokList->pNext != NULL );
        
        pTokList = pTokList->pNext;
    }

    m_dwOffset = pTokList->Info[TokOffset].dwOffset;
    m_strCurrentToken = &m_strFileName[m_dwOffset];

    return TRUE;
}

// /////////////////////////////////////////////////////////////////////////////
// GotoPreviousToken
//
BOOL CFileName::GotoPreviousToken()
{
    PTOKEN_LIST pTokList = &m_TokenList;
    BYTE TokOffset = 0;

    if( m_dwTokenIndex == 0 )
    {
        return TRUE;
    }

    m_dwTokenIndex -= 1;

    TokOffset = (BYTE)(m_dwTokenIndex % TOKENS_PER_LIST);

    for( DWORD x = 0; x < m_dwTokenIndex / TOKENS_PER_LIST; x++ )
    {
        ASSERT( pTokList->pNext != NULL );
        
        pTokList = pTokList->pNext;
    }

    m_dwOffset = pTokList->Info[TokOffset].dwOffset;
    m_strCurrentToken = &m_strFileName[m_dwOffset];

    return TRUE;
}

// /////////////////////////////////////////////////////////////////////////////
// GotoIndex
//
BOOL CFileName::GotoIndex( DWORD dwIndex )
{
    BOOL fResult = TRUE;

    if( dwIndex >= GetTokenCount() )
    {
        fResult = FALSE;
        goto exit_gotoindex;
    }

    while( fResult && GetCurrentTokenIndex() != dwIndex )
    {
        if( GetCurrentTokenIndex() > dwIndex )
        {
            fResult = GotoPreviousToken();
        }
        else
        {
            fResult = GotoNextToken();
        }
    }

exit_gotoindex:

    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// SetStopIndex
//
// Affects GotoNextToken and IsFinished.
//
// This will cause GotoNextToken to stop before returning the given index.
// IsFinished() will return true when we reach this index.
//
BOOL CFileName::SetStopIndex( DWORD dwIndex )
{
    PTOKEN_LIST pTokList = &m_TokenList;
    BYTE TokOffset = 0;

    if( dwIndex <= GetTokenCount() )
    {
        if( m_dwTokenStop == m_dwTokenIndex && dwIndex > m_dwTokenIndex )
        {
            //
            // If we had stopped at the last index, then we had set the
            // current token to NULL.  Now that it is available, we should
            // correct the value.
            //
            TokOffset = (BYTE)(m_dwTokenIndex % TOKENS_PER_LIST);

            for( DWORD x = 0; x < m_dwTokenIndex / TOKENS_PER_LIST; x++ )
            {
                ASSERT( pTokList->pNext != NULL );
                
                pTokList = pTokList->pNext;
            }

            m_dwOffset = pTokList->Info[TokOffset].dwOffset;
            m_strCurrentToken = &m_strFileName[m_dwOffset];
        }
        
        m_dwTokenStop = dwIndex;
        if( m_dwTokenIndex >= m_dwTokenStop )
        {
            m_strCurrentToken = NULL;
        }
        return TRUE;
    }

    return FALSE;
}

// /////////////////////////////////////////////////////////////////////////////
// Indexing operator
//
const WCHAR* CFileName::operator[]( DWORD dwIndex )
{
    PTOKEN_LIST pTokList = &m_TokenList;
    BYTE TokOffset = 0;
    const WCHAR* strReturn = NULL;

    if( dwIndex >= m_dwNumTokens )
    {
        return strReturn;
    }

    for( DWORD x = 0; x < dwIndex / TOKENS_PER_LIST; x++ )
    {
        ASSERT( pTokList->pNext != NULL );
        
        pTokList = pTokList->pNext;
    }

    TokOffset = (BYTE)(dwIndex % TOKENS_PER_LIST);

    strReturn = &m_strFileName[ pTokList->Info[TokOffset].dwOffset ];

    return strReturn;
}

// -----------------------------------------------------------------------------
// Miscellaneous functions
// -----------------------------------------------------------------------------

void UdfConvertUdfTimeToFileTime( PTIMESTAMP UdfTime, PFILETIME NtTime )
{
    SYSTEMTIME SystemTime;
    PLARGE_INTEGER pTime = (PLARGE_INTEGER)NtTime;
    
    SystemTime.wYear = UdfTime->Year;
    SystemTime.wMonth = UdfTime->Month;
    SystemTime.wDay = UdfTime->Day;
    SystemTime.wHour = UdfTime->Hour;
    SystemTime.wMinute = UdfTime->Minute;
    SystemTime.wSecond = UdfTime->Second;
    
    //
    //  This is where it gets hairy.  For some unholy reason, ISO 13346 timestamps
    //  carve the right of the decimal point up into three fields of precision
    //  10^-2, 10^-4, and 10^-6, each ranging from 0-99. Lawdy.
    //
    //  To make it easier, since they cannot cause a wrap into the next second,
    //  just save it all up and add it in after the conversion.
    //
    
    SystemTime.wMilliseconds = 0;
    
    if( UdfTime->Type <= TIMESTAMP_T_LOCAL &&
        ((UdfTime->Zone >= TIMESTAMP_Z_MIN && UdfTime->Zone <= TIMESTAMP_Z_MAX) ||
         UdfTime->Zone == TIMESTAMP_Z_NONE) &&
        SystemTimeToFileTime( &SystemTime, NtTime )) {

        //
        //  Now fold in the remaining sub-second "precision".  Read as coversions
        //  through the 10-3 units, then into our 10-7 base. (centi->milli->micro,
        //  etc).
        //
    
        pTime->QuadPart += ((UdfTime->CentiSecond * (100000)) +  // cs    = 10^-2
                            (UdfTime->Usec100 * 1000) +          // 100us = 10^-4
                            UdfTime->Usec) * 10;                 // us    = 10^-6

        //
        //  Perform TZ normalization if this is a local time with
        //  specified timezone.
        //

        if (UdfTime->Type == 1 && UdfTime->Zone != TIMESTAMP_Z_NONE) {
            
            pTime->QuadPart += Int32x32To64( -UdfTime->Zone, (60 * 10 * 1000 * 1000) );
        }
    
    } 
    else {

        //
        //  Malformed timestamp.
        //

        pTime->QuadPart = 0;
    }
}

void UdfConvertFileTimeToUdfTime( PFILETIME NtTime, PTIMESTAMP UdfTime )
{
    SYSTEMTIME SystemTime;
    ULONGLONG Time = ((PLARGE_INTEGER)NtTime)->QuadPart;

    FileTimeToSystemTime( NtTime, &SystemTime);
    
    UdfTime->Year   = (USHORT)SystemTime.wYear;
    UdfTime->Month  = (UCHAR)SystemTime.wMonth;
    UdfTime->Day    = (UCHAR)SystemTime.wDay;
    
    UdfTime->Hour   = (UCHAR)SystemTime.wHour;
    UdfTime->Minute = (UCHAR)SystemTime.wMinute;
    UdfTime->Second = (UCHAR)SystemTime.wSecond;

    //
    //  Kernel times are all GMT,  so specify local time, GMT offset 0.
    //
    
    UdfTime->Zone = 0;
    UdfTime->Type = TIMESTAMP_T_LOCAL;
    
    //
    //  This is where it gets hairy.  For some unholy reason, ISO 13346 timestamps
    //  carve the right of the decimal point up into three fields of precision
    //  10-2, 10-4, and 10-6, each ranging from 0-99. Lawdy.
    //
    //  10000000           (NTTIME precision is 10^-7)
    //    | | |
    //   cs | -- us ^-6
    //  ^-2 |
    //      100us
    //      ^-4
    //
    //  Isolate the fractional part of the time (10^-1 and below)
    //

    Time %= 10000000;        // 10^7
    Time /= 10;              // ditch the sub us part.

    UdfTime->Usec = (UCHAR)(Time % 100);
    Time /= 100;
    UdfTime->Usec100 = (UCHAR)(Time % 100);
    Time /= 100;
    UdfTime->CentiSecond = (UCHAR)(Time % 100);
}

