//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#ifndef __INETPARSE_H__
#define __INETPARSE_H__

#include "global.h"

#define ISWHITEA( ch )      ((ch) == '\t' || (ch) == ' ' || (ch) == '\r')

//
//  Simple class for parsing all those lovely "Field: value\r\n" protocols
//  Token and copy functions stop at the terminating '\n' and '\r' is
//  considered white space.  All methods except NextLine() stop at the end
//  of the line.
//

class INET_PARSER
{
public:

    INET_PARSER( CHAR * pszStart );

    //
    //  Be careful about destruction.  The Parser will attempt to restore any
    //  modifications to the string it is parsing
    //

    ~INET_PARSER( VOID );

    //
    //  Returns the current zero terminated token.
    //

    CHAR * QueryToken( VOID );

    //
    //  Returns the current zero terminated line
    //

    CHAR * QueryLine( VOID );

    //
    //  Skips to the first token after the next '\n'
    //

    CHAR * NextLine( VOID );

    //
    //  Returns the next non-white string after the current string with a
    //  zero terminator.  The end of the token is '\n' or white space
    //

    CHAR * NextToken( VOID );

    //
    //  Moves the current parse position to the first white or non-white
    //  character after the current position
    //

    CHAR * EatWhite( VOID )
        { return m_pszPos = AuxEatWhite(); }

    CHAR * EatNonWhite( VOID )
        { return m_pszPos = AuxEatNonWhite(); }

    //
    //  Undoes any temporary terminators in the string
    //

    VOID RestoreBuffer( VOID )
        {  RestoreToken(); RestoreLine(); }

protected:

    CHAR * AuxEatWhite( VOID );
    CHAR * AuxEatNonWhite( CHAR ch = '\0' );
    CHAR * AuxSkipTo( CHAR ch );

    VOID TerminateToken( CHAR ch = '\0' );
    VOID RestoreToken( VOID );

    VOID TerminateLine( VOID );
    VOID RestoreLine( VOID );

private:

    //
    //  Current position in parse buffer
    //

    CHAR * m_pszPos;

    //
    //  If we have to temporarily zero terminate a token or line these
    //  members contain the information
    //

    CHAR * m_pszTokenTerm;
    CHAR   m_chTokenTerm;

    CHAR * m_pszLineTerm;
    CHAR   m_chLineTerm;

};

#endif // __INETPARSE_H__
