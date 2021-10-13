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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
// hashtbl.h

#pragma once

//////////////////////////////////////////////////
// Hash table structures
//////////////////////////////////////////////////

#define BUCKET_SIZE                 10
#define ONE_CHAR_BUCKET_SIZE        16
#define LOG_NUM_BUCKETS             19
#define HASH_BYTES                  10

typedef struct _HashBucket
{
	UINT32* piOffsets;
	UINT32 cCount;
} HashBucket;

typedef struct _HashBucketExt
{
	HashBucket* phb;
	BOOL fOneChar;
} HashBucketExt;



class CHashTable
{
public:
    CHashTable();
    ~CHashTable();

    HRESULT         Initialize(DWORD cBuckets);

    UINT32          Hash(UCHAR* pbKey);
    HashBucketExt   GetBucket(UCHAR* pbKey);
    HRESULT         Insert(UCHAR* pbKey, UINT32 iData);
    DWORD           GetMaxBucketSize() { return m_cMaxBucketSize; }

private:
    void            Cleanup();

    HashBucket *        m_pHashBuckets;
    DWORD               m_cBuckets;
    DWORD               m_cMaxBucketSize;

    HashBucket          m_hbOneChar[256];
};
