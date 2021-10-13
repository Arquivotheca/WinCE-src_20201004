//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//
#include <findfile.h>

#define __FILE_NAME__   TEXT("FINDFILE.CPP")

#ifdef UNDER_NT
#include <crtdbg.h>
#define DEBUGMSG(X,Y)
#define ASSERT _ASSERT
#endif

//******************************************************************************
FindFiles::FindFiles() :
    m_hSearch(INVALID_HANDLE_VALUE)
//
//  Constructor
//
//******************************************************************************    
{   
    Reset();
}

//******************************************************************************
FindFiles::~FindFiles()
//
//  Destructor
//
//******************************************************************************
{
    Reset();
}

//******************************************************************************
void
FindFiles::Reset()
//
//  Resets the internal state of the object; Any search will result in a call
//  to FindFirstFile and a new search handle will be created.
//
//******************************************************************************
{
    if( INVALID_HANDLE_VALUE != m_hSearch )
    {
        if( !FindClose( m_hSearch ) )
        {
            DEBUGMSG(1, (TEXT("WARNING: Unable to close pervious Find Handle. Error %d"), 
        GetLastError() ));            
        }
        m_hSearch = INVALID_HANDLE_VALUE;
    }
    _tcscpy( m_szSearchString, TEXT("\0") );
}    

//******************************************************************************
BOOL 
FindFiles::Next(
    LPTSTR lpm_szSearchString,
    LPWIN32_FIND_DATA lpFileData
    )
//
//  Find the next file matching the specified search string. This function will
//  either make a call to FindFirstFile or FindNextFile.
//
//  Returns TRUE when a file is found and lpFileData is valid, FALSE otherwise.
//
//******************************************************************************
{
    ASSERT( NULL != lpFileData );

    //
    // check to see if it is a new search query, m_hSearch will be null
    // if this is the first search made since instance conception
    //
    if( (INVALID_HANDLE_VALUE != m_hSearch) &&
        _tcscmp( m_szSearchString, lpm_szSearchString ) ) 
    {
        Reset();
    }

    //
    //  If there is no current find file handle, call FindFirstFile to
    //  retrieve file information and a handle
    //
    if( INVALID_HANDLE_VALUE == m_hSearch )
    {
        //
        // save the search string for later comparison
        //
        _tcscpy( m_szSearchString, lpm_szSearchString );
        //
        // Try to search for the first handle based on the search string
        // and get a search handle to continue the search
        //
        m_hSearch = FindFirstFile( lpm_szSearchString, lpFileData );

        // 
        // If no files match the search string, fail
        //
        if( INVALID_HANDLE_VALUE == m_hSearch )
            return FALSE;
    }

    //
    // If there already is a file handle and the search string is the same, 
    // call FindNextFile to retrieve file information. If this fails, there 
    // are no more files matching the search pattern and the function fails
    //
    else
    {
        if( !FindNextFile( m_hSearch, lpFileData ) ) 
        {
            //
            // close the search handle
            //
            FindClose( m_hSearch );
            m_hSearch = INVALID_HANDLE_VALUE;
            return FALSE;
        }
    }

    //
    // lpFileData points to file data, return success
    //
    return TRUE;
}
