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
