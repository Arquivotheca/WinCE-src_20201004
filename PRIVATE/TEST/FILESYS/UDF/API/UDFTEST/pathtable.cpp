//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//
#include <pathtable.h>

#define __FILE_NAME__ TEXT("PATHTABLE.CPP")

#ifdef UNDER_NT
#include <crtdbg.h>
#define ASSERT _ASSERT
#define DEBUGMSG(X,Y)
#endif

//****************************************************************************** 
PathTable::PathTable( ) :
//
//  constructor
//
//****************************************************************************** 
    m_ulTableSize(0)
{ ; }


//****************************************************************************** 
PathTable::~PathTable( ) 
//
//  destructor
//
//****************************************************************************** 
{
    ClearTable( );
}

//****************************************************************************** 
void
PathTable::ClearTable( )
//
//  Free all the memory that was allocated for strings in the path table and 
//  reset the table size to zero
//
//****************************************************************************** 
{
    
    for( ULONG ulIndex = 0; ulIndex < m_ulTableSize; ulIndex++ )
        delete[] m_rgszPathTable[ulIndex];    

    m_ulTableSize = 0;
}

//******************************************************************************
ULONG
PathTable::GetTotalEntries( )
//
// simply return the number of entries in the table
//
//******************************************************************************
{
    return m_ulTableSize;
}

//****************************************************************************** 
void 
PathTable::GetPath( 
    IN  ULONG ulIndex, 
    OUT LPTSTR szPath, 
    IN  ULONG ulSize )
//
//  Copies the string at ulIndex in the path table into szPath. ulSize is the
//  size of the the buffer pointed to by szPath. Up to ulSize characters from 
//  the path table will be copied into szPath. 
//
//******************************************************************************         
{
    ASSERT( ulIndex < m_ulTableSize );
    
    _tcsncpy( szPath, m_rgszPathTable[ulIndex], ulSize ); 
}

//******************************************************************************     
void
PathTable::MixTable( )
//
//  Randomly swaps positions of items in the path table
//
//******************************************************************************     
{
#ifdef UNDER_CE
    DWORD dwCount       = 0;
    LPTSTR lpszTemp    = NULL;
    DWORD dwItem1       = 0;
    DWORD dwItem2       = 0;

    for( dwCount = 0; dwCount < (m_ulTableSize / 2); dwCount++ )
    {        
        dwItem1 = Random() % m_ulTableSize;
        //
        // Make sure not to swap an item for itself because that's a waste
        //
        do
        {
            dwItem2 = Random() % m_ulTableSize;
        } while( dwItem1 == dwItem2 );
        
        lpszTemp = m_rgszPathTable[dwItem1];
        m_rgszPathTable[dwItem1] = m_rgszPathTable[dwItem2];
        m_rgszPathTable[dwItem2] = lpszTemp;
    }        
#else
    return;
#endif
}

//****************************************************************************** 
ULONG 
PathTable::BuildTable( 
    IN  LPCTSTR szBaseDir )
//
//  Builds a table of directories and subdirectories based on the base directory
//  in szBaseDir. The full paths of the directories are stored as strings. The 
//  table will stop building once MAX_PATHTABLE_ENTRIES is reached.
//
//  If nothing matches the szBaseDir string, then no entries will be made.
//
//******************************************************************************     
{
    ASSERT( NULL != szBaseDir );

    TCHAR szSearchString[_MAX_PATH];
    DWORD dwLastErr = 0;
    WIN32_FIND_DATA findData;
    HANDLE hFindFile;
    ULONG ulIndex = 0;
    
    //
    // If there is already a table, clear it and build a new one
    //
    if( m_ulTableSize > 0 )
        ClearTable();

#ifdef UNDER_CE
    //
    // Perform an initial search to make sure the base directory exists
    //
    hFindFile = FindFirstFile( szBaseDir, &findData );
    if( INVALID_HANDLE_VALUE == hFindFile )
    {
        goto EXIT_FAIL;
    }
    FindClose( hFindFile );
#endif

    //
    // Set the base path table entry
    //
    m_rgszPathTable[0] = new TCHAR[_tcslen(szBaseDir) + 1];

    if( NULL == m_rgszPathTable[0] )
    {
        DEBUGMSG(1, (TEXT("Out of memory")));
        goto EXIT_FAIL;
    }

    _tcscpy(m_rgszPathTable[0], szBaseDir);

    //
    // there is one entry in the table
    //
    m_ulTableSize = 1;

    //
    // Loop through the directory listings in the table (there is
    // one initially, but more get added as it builds) adding
    // directories and their subdirectories to the path table
    //
    for( ulIndex = 0; 
        (ulIndex < m_ulTableSize) && (MAX_PATHTABLE_ENTRIES > m_ulTableSize); 
        ulIndex++ )
    {
        //
        // build the search string based on the directory path 
        // and a wildcard *
        //
        _stprintf( 
            szSearchString,
            TEXT("%s\\*"),
            m_rgszPathTable[ulIndex] );

        //
        // find the first file in the directory
        //
        hFindFile = FindFirstFile( szSearchString, &findData );
        if( INVALID_HANDLE_VALUE != hFindFile )
        {
            // 
            // loop through all the files searching for directories only
            //
            do
            {
                if( ( _tcscmp( findData.cFileName, TEXT("..") ) != 0 ) &&
                    ( _tcscmp( findData.cFileName, TEXT(".") )  != 0 ) &&
                    ( findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) )
                {
                    BuildEntry( findData.cFileName, ulIndex );
                }
            }
            //
            // continue to loop while there are more files in the current 
            // directory or until the table is full
            //
            while( FindNextFile( hFindFile, &findData ) &&
                ( MAX_PATHTABLE_ENTRIES > m_ulTableSize ) );

            //
            // FindNextFile should fail with ERROR_NO_MORE_FILES, otherwise
            // bail out because there something bad happened
            //
            if( ERROR_NO_MORE_FILES != GetLastError() )
            {
                DEBUGMSG(1, (TEXT("FindNextFile")));
                FindClose( hFindFile );                
                goto EXIT_FAIL;
            }

            //
            // Close and reset the search handle
            //
            FindClose( hFindFile );
            hFindFile = INVALID_HANDLE_VALUE;
        }
        
        else  //~VALID_HANDLE( hFindFile )
        {
            dwLastErr = GetLastError();   
            if( ERROR_NO_MORE_FILES != dwLastErr &&
                ERROR_FILE_NOT_FOUND != dwLastErr )
            {
                DEBUGMSG(1, (TEXT("FindFirstFile")));
                DEBUGMSG(1, (TEXT("%s, error %d"), szSearchString, dwLastErr));
                goto EXIT_FAIL;
            }
        }
    
    }

    //
    // Add some randomness to the table
    //
    MixTable();
    return m_ulTableSize; 

EXIT_FAIL:
    ClearTable();
    return 0;
}

//****************************************************************************** 
BOOL 
PathTable::BuildEntry( 
    IN  LPCTSTR szPath, 
    IN  ULONG ulParentIndex )
//
//
//
//******************************************************************************     
{
    ASSERT( szPath );
    ASSERT( ulParentIndex < m_ulTableSize );
    ASSERT( MAX_PATHTABLE_ENTRIES >= m_ulTableSize );

    //
    // allocate a string of length equal to the parent + the length
    // of the current name + 2 more for '\' and NULL. Make sure the 
    // allocation succeeded.
    //
    LPTSTR szEntry = 
        new TCHAR[_tcslen(szPath) + _tcslen(m_rgszPathTable[ulParentIndex]) + 2];
    if( NULL == szEntry )
    {
        return FALSE;
    }

    //
    // construct the full path string
    //
    _stprintf( 
        szEntry, 
        TEXT("%s\\%s"), 
        m_rgszPathTable[ulParentIndex], 
        szPath );

    //
    // add the entry to the table and increment the number of entries
    //
    m_rgszPathTable[m_ulTableSize] = szEntry;
    m_ulTableSize++;

    DEBUGMSG( 1, (TEXT("added: %s"), szEntry) );

    return TRUE;
}
