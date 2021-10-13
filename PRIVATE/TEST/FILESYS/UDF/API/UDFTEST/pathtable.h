//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __PATHTABLE_H__
#define __PATHTABLE_H__

#define MAX_PATHTABLE_ENTRIES   2048

#include <windows.h>
#include <tchar.h>

//****************************************************************************** 
class PathTable 
//
// The PathTable class provies a data structure to create and store the full 
// paths of all the directories and subdirectories found in a base directory
//
//****************************************************************************** 
{
public:

    //
    // Constructor / Destructor
    //
    PathTable( );
    ~PathTable( );

    //
    // Public Member Functions
    //
    void GetPath( ULONG, LPTSTR, ULONG );
    ULONG GetTotalEntries( );
    ULONG BuildTable( LPCTSTR );
    void MixTable( );

private:

    //
    // Private Member Functions
    //
    BOOL BuildEntry( LPCTSTR, ULONG );
    void ClearTable( );

    //
    // Private Member Data
    //
    LPTSTR m_rgszPathTable[MAX_PATHTABLE_ENTRIES];
    ULONG m_ulTableSize;

};
//****************************************************************************** 


#endif
