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
