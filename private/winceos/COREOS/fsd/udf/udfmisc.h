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

#pragma once

#define TOKENS_PER_LIST 10

typedef struct _TOKEN_INFO
{
    DWORD dwOffset;
    DWORD dwLength;
} TOKEN_INFO, *PTOKEN_INFO;

typedef struct _TOKEN_LIST
{
    TOKEN_INFO Info[TOKENS_PER_LIST];
    struct _TOKEN_LIST* pNext;
} TOKEN_LIST, *PTOKEN_LIST;

class CFileName
{
public:
    CFileName();
    ~CFileName();

    BOOL Initialize( const WCHAR* strFileName, BOOL fCanUseWildcards = FALSE );
    BOOL IsMatch( const WCHAR* strName ) const;

    BOOL GotoNextToken();
    BOOL GotoPreviousToken();
    BOOL GotoIndex( DWORD dwIndex );

    BOOL SetStopIndex( DWORD dwIndex );
    
    const WCHAR* operator[]( DWORD dwIndex );

public:
    inline DWORD GetTokenCount() const
    {
        return m_dwNumTokens;
    }

    inline DWORD GetCurrentTokenIndex() const
    {
        return m_dwTokenIndex;
    }

    const DWORD GetFileTokenIndex() const
    {
        return m_dwFileIndex;
    }

    const DWORD GetStreamTokenIndex() const
    {
        return m_dwStreamIndex;
    }

    const WCHAR* GetCurrentToken() const
    {
        return m_strCurrentToken;
    }

    inline BOOL HasStreamName() const
    {
        return m_fHasStream;
    }

    inline BOOL ContainsWildcards() const
    {
        //
        // TODO::This value is never set, so this functionality is broken
        // right now.  It's currently not used anyways.
        //
        return m_fHasWildcards;
    }

    inline BOOL IsFinished() const
    {
        return m_dwTokenIndex >= m_dwTokenStop - 1;
    }

    inline BOOL IsTruncated() const
    {
        return m_dwTokenStop != m_dwNumTokens;
    }

private:
    WCHAR* m_strFileName;
    BOOL   m_fCanUseWildcards;
    
    WCHAR* m_strCurrentToken;
    DWORD  m_dwSizeInChars;
    DWORD  m_dwOffset;

    //
    // This is the token in the string that we are currently accessing.
    //
    DWORD  m_dwTokenIndex;
    
    DWORD  m_dwNumTokens;
    BOOL   m_fHasStream;
    BOOL   m_fHasWildcards;

    DWORD  m_dwFileIndex;
    DWORD  m_dwStreamIndex;

    //
    // This is initially set to the number of tokens in the string.  This is
    // used for the GotoNextToken to determine that we have hit the end of
    // our target.  This value can be changed so that we will only parse a
    // substring in the path.  This value is one greater than the
    // last token to be parsed.
    //
    DWORD m_dwTokenStop;

    TOKEN_LIST m_TokenList;
};

void UdfConvertUdfTimeToFileTime( PTIMESTAMP UdfTime, PFILETIME NtTime );
void UdfConvertNtTimeToUdfTime( PLARGE_INTEGER NtTime, PTIMESTAMP UdfTime );

