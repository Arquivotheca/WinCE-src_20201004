//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#pragma once

#include "bucket.h"
#include "tchar.h"
#include "tstring.h"
#include <set>
#include <string>
#include <vector>

#define MAX_ARGUMENT_LENGTH 512
#define MAX_LENGTH 4503599627370495  // 2^52 -1 (max mantissa value for a double precision number)

typedef struct
{
    DWORD dwProperties;
    DWORD dwRandomSeed;
    DWORD dwIteration;
}FileBucketConfig;

typedef enum FileBucketProperties
{
    FB_ASCENDING = 1,       // Fetch in ascending order
    FB_DESCENDING = 2,      // Fetch in descending order
    FB_RANDOM = 4,          // Fetch randomly
    FB_FINITE = 8,          // Items leave bucket when fetched
    FB_INFINITE = 16,       // Items never leave the bucket
    FB_FROMFILE = 128       // Items are read directly from a file

    // Note: The following combinations of bucket properties are not supported:
    //    (FB_ASCENDING | FB_DESCENDING) & FB_RANDOM
    //    FB_FROMFILE & FB_FINITE & FB_RANDOM
    //    FB_ASCENDING & FB_DESCENDING
    //    FB_RANDOM & FB_INFINITE
};

class InsensitiveStringComparer
{
public:
    bool operator() (const tstring szLeft, const tstring szRight) const
    {
        return ( 0 < _tcsicmp(szLeft.c_str(), szRight.c_str()));
    }
};

// Bucket object containing filenames
class CFileNameBucket:CBucket
{
public:
    CFileNameBucket();
    virtual ~CFileNameBucket();

    // Perform any configuration steps for the bucket
    virtual HRESULT Initialize(void* pConfig);

    // Add items to the bucket from a source path or config file
    virtual HRESULT Add(void* pSource);

    // Get the current item in the bucket without advancing
    virtual HRESULT Peek(void* pItem);

    // Get the current item and advance to the next one
    virtual HRESULT Next(void* pItem);

    // Fetch the number of items left in the bucket
    virtual HRESULT Count(DWORD* pdwCount);

    // Fetch the current iteration of the bucket
    virtual HRESULT Iteration(LONGLONG* pllIteration);

    // Fetch the random seed used in the bucket
    virtual HRESULT RandomSeed(DWORD* pdwRandomSeed);

    // Do not add items whose extensions match entries in this space-delimited list
    virtual HRESULT Exclude(TCHAR* pszExclusions);

    // Restrict bucket to contain items whose extensions match entries in this space-delimited list
    virtual HRESULT Restrict(TCHAR* pszRestrictions);

    // Save the contents of the bucket to a config file
    virtual HRESULT Save(TCHAR* pszFileName);

private:

    // Add filenames recursively
    HRESULT AddInternal(TCHAR* pszPath);

    // Parse an exsiting formatted config file and use filenames from it
    HRESULT ParseConfigFile(TCHAR* pszFileName);

    // Utility func to verify correct property combinations
    HRESULT VerifyProperties(DWORD dwProps);

    // Conditionally add found file against restrictions & exclusions - only works when storing items in-memory
    HRESULT Filter(WIN32_FIND_DATA* pFD, TCHAR* pszDirectory);

    // Calculate the next index based on properties
    void NextIndex();

    // 64 bit random number generator
    ULONGLONG rand64();
    
    
    ULONGLONG m_Properties;         // Defines file bucket behavior
    ULONGLONG m_Iteration;          // Current iteration
    ULONGLONG m_DesiredIteration;
    DWORD m_RandomSeed;             // Random Seed

    ULONGLONG m_NextIndex;          // Index of next entry coming from the bucket
    
    std::vector<tstring> m_Exclusions;     // exclude files with extensions in this list     
    std::vector<tstring> m_Restrictions;   // limit list to files with these extensions
    std::vector<tstring> m_Items;          // paths to files

    ULONGLONG m_fpFirstItem;        // When reading directly from a config file, points to the first item
    ULONGLONG m_ItemLength;         // Length of each item within the config file.
    ULONGLONG m_ItemCount;          // Total number of items within the config file
    FILE * m_pFile;                 // Pointer to the config file
    
};

