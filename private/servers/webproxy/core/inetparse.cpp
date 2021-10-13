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

/*++

Copyright (c) 1999 Microsoft Corporation

Module Name :  

    parse.cxx

Abstract:

    Simple parser class for extrapolating HTTP headers information. Stolen from 
    IIS (\nt\private\inet\iis\svcs\infocomm\common\parse.cxx).

Author:

    Ittai Gilat           (IttaiG)        14-Oct-1999

Revision History:

--*/

#include "inetparse.h"


INET_PARSER::INET_PARSER(
    CHAR * pszStart
    ) :
      m_pszPos      ( pszStart ),
      m_pszTokenTerm( NULL ),
      m_pszLineTerm ( NULL )
{
    ASSERT( pszStart );

    //
    //  Chew up any initial white space at the beginning of the buffer
    //  and terminate the first token in the string.
    //

    EatWhite();

    TerminateToken();
}


INET_PARSER::~INET_PARSER(
    VOID
    )
{
    RestoreBuffer();
}

CHAR *
INET_PARSER::QueryToken(
    VOID
    )
{
    if ( !m_pszTokenTerm )
        TerminateToken( '\0' );

    return m_pszPos;
}


CHAR *
INET_PARSER::QueryLine(
    VOID
    )
{
    RestoreToken();

    if ( !m_pszLineTerm )
        TerminateLine();

    return m_pszPos;
}


CHAR *
INET_PARSER::NextLine(
    VOID
    )
{
    RestoreToken();
    RestoreLine();

    m_pszPos = AuxSkipTo( '\n' );

    if ( *m_pszPos )
        m_pszPos++;

    return EatWhite();
}

CHAR *
INET_PARSER::NextToken(
    VOID
    )
{
    //
    //  Make sure the line is terminated so a '\0' will be returned after
    //  the last token is found on this line
    //

    RestoreToken();

    if ( !m_pszLineTerm )
        TerminateLine();

    //
    //  Skip the current token
    //

    EatNonWhite();

    EatWhite();

    TerminateToken();

    return m_pszPos;
}

VOID
INET_PARSER::TerminateToken(
    CHAR ch
    )
{
    ASSERT( !m_pszTokenTerm );

    m_pszTokenTerm = AuxEatNonWhite( ch );

    m_chTokenTerm = *m_pszTokenTerm;

    *m_pszTokenTerm = '\0';
}

VOID
INET_PARSER::RestoreToken(
    VOID
    )
{
    if ( m_pszTokenTerm )
    {
        *m_pszTokenTerm = m_chTokenTerm;
        m_pszTokenTerm = NULL;
    }
}

VOID
INET_PARSER::TerminateLine(
    VOID
    )
{
    ASSERT( !m_pszLineTerm );

    m_pszLineTerm = AuxSkipTo( '\n' );

    //
    //  Now trim any trailing white space on the line
    //

    if ( m_pszLineTerm > m_pszPos )
    {
        m_pszLineTerm--;

        while ( m_pszLineTerm >= m_pszPos &&
                ISWHITEA( *m_pszLineTerm ))
        {
            m_pszLineTerm--;
        }
    }

    //
    //  Go forward one (trimming found the last non-white
    //  character)
    //

    if ( *m_pszLineTerm &&
         *m_pszLineTerm != '\n' &&
         !ISWHITEA( *m_pszLineTerm ))
    {
        m_pszLineTerm++;
    }

    m_chLineTerm = *m_pszLineTerm;

    *m_pszLineTerm = '\0';
}

VOID
INET_PARSER::RestoreLine(
    VOID
    )
{
    if ( m_pszLineTerm )
    {
        *m_pszLineTerm = m_chLineTerm;
        m_pszLineTerm = NULL;
    }
}




CHAR *
INET_PARSER::AuxEatNonWhite(
    CHAR ch
    )
{
    CHAR * psz = m_pszPos;

    while ( *psz           &&
            *psz != '\n'   &&
            !ISWHITEA(*psz)&&
            *psz != ch )
    {
        psz++;
    }

    return psz;
}


CHAR *
INET_PARSER::AuxEatWhite(
    VOID
    )
{
    CHAR * psz = m_pszPos;

    //
    //  Note that ISWHITEA includes '\r'
    //

    while ( *psz           &&
            *psz != '\n'   &&
            ISWHITEA(*psz))
    {
        psz++;
    }

    return psz;
}


CHAR *
INET_PARSER::AuxSkipTo(
    CHAR ch
    )
{
    CHAR * psz = m_pszPos;

    while ( *psz           &&
            *psz != '\n'   &&
            *psz != ch )
    {
        psz++;
    }

    return psz;
}
