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
#ifndef __FINDFILES_H__
#define __FINDFILES_H__

#include <windows.h>
#include <tchar.h>

//*****************************************************************************
//
//  The FindFiles class is a wrapper to the FindFirstFile and FindNextFile
//  API calls. Any time FindFiles::Next() is called with a new string, the
//  FindFirstFile call will be made. Subsequent calls using the same search
//  string will call FindNextFile. 
//
//  Typical Usage:
//
//      FindFiles *findFile = new FindFiles();
//      WIN32_FIND_DATA findData = {0};
//      LPTSTR searchString = TEXT("path\\*");
//      
//      while( findFile.next( searchString, &findData ) )
//      {
//          // do something with findData
//      }
//      
//      // no more files match the search string        
//
//******************************************************************************
class FindFiles
{

public:

    //
    // Constructor / Destructor
    //  
    FindFiles();
    ~FindFiles();

    //
    // Public Member Functions
    //
    BOOL Next( LPTSTR, LPWIN32_FIND_DATA );
    void Reset();

private:

    //
    // Private Member Data
    //
    HANDLE  m_hSearch;
    TCHAR   m_szSearchString[MAX_PATH];

};


#endif

