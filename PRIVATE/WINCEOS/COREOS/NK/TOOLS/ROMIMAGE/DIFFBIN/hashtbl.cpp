// hashtbl.cpp

#include "diffbin.h"

CHashTable::CHashTable() : m_pHashBuckets(NULL), m_cBuckets(0), m_cMaxBucketSize(0)
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
	for (int i = 0; i < 256; ++i) {
		m_hbOneChar[i].piOffsets = NULL;
		m_hbOneChar[i].cCount = 0;
	}

} /* CHashTable::CHashTable()
   */

CHashTable::~CHashTable()
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
    Cleanup();

} /* CHashTable::~CHashTable()
   */

void
CHashTable::Cleanup()
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
	int i;
    for (i = 0; i < m_cBuckets; ++i) {
		LocalFree(m_pHashBuckets[i].piOffsets);
    }
    for (i = 0; i < 256; ++i) {
		LocalFree(m_hbOneChar[i].piOffsets);
        m_hbOneChar[i].piOffsets = NULL;
		m_hbOneChar[i].cCount = 0;
    }
	LocalFree(m_pHashBuckets);
    m_pHashBuckets = NULL;
    
} /* CHashTable::Cleanup()
   */

HRESULT
CHashTable::Initialize(DWORD cBuckets)
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
    HRESULT             hr          = NOERROR;
    int                 i;

    Cleanup(); 

    m_cBuckets = cBuckets;
    CPR(m_pHashBuckets = (HashBucket *)LocalAlloc(LMEM_FIXED, m_cBuckets * sizeof(HashBucket)));

    for(i = 0; i < m_cBuckets; i++) {
        m_pHashBuckets[i].piOffsets = NULL;
        m_pHashBuckets[i].cCount = 0;
    }

Error:
    return hr;

} /* CHashTable::Initialize()
   */

UINT32 
CHashTable::Hash(UCHAR* pbKey)
/*---------------------------------------------------------------------------*\
 * Hash "pbKey" to a value of LOG_NUM_BUCKETS bits.
 * Returns ONE_CHAR if all characters are the same.
\*---------------------------------------------------------------------------*/
{
	UINT32 iKey = 0;
	UINT32 i;
	BOOL fOneChar = TRUE;

    for (i = 0; i <= HASH_BYTES - sizeof(UINT32); i += 3) {
		iKey += *(UINT32*)(pbKey + i);
		fOneChar = fOneChar && (*(UINT32*)(pbKey + i) == *(UINT32*)pbKey);
	}

	iKey += *(UINT32*)(pbKey + HASH_BYTES - sizeof(UINT32));
	i = 1;

	while (fOneChar && (i < HASH_BYTES)) {
		fOneChar = fOneChar && (pbKey[i] == pbKey[0]);
		++i;
	}

	return fOneChar ? ONE_CHAR : iKey & ((1 << LOG_NUM_BUCKETS) - 1);

} /* CHashTable::Hash()
   */

HashBucketExt 
CHashTable::GetBucket(UCHAR* pbKey)
/*---------------------------------------------------------------------------*\
 * Return a pointer to the bucket into which "pbKey" hashes.  The bucket 
 * returned should NOT be modified.
 * Returns NULL if all characters are the same.
\*---------------------------------------------------------------------------*/
{
	HashBucketExt hbe;

	UINT32 iBucket = Hash(pbKey);

	if (iBucket == ONE_CHAR) {
		hbe.phb = &(m_hbOneChar[pbKey[0]]);
		hbe.fOneChar = TRUE;

    } else {
		hbe.phb = &(m_pHashBuckets[iBucket]);
		hbe.fOneChar = FALSE;
	}
	return hbe;

} /* CHashTable::GetBucket()
   */

HRESULT
CHashTable::Insert(UCHAR* pbKey, UINT32 iData)
/*---------------------------------------------------------------------------*\
 * Insert "pbKey" and "iData" into the hash table
\*---------------------------------------------------------------------------*/
{
    HRESULT         hr          = NOERROR;
    
    HashBucketExt hbe = GetBucket(pbKey);

	HashBucket* phb = hbe.phb;
    if (!hbe.fOneChar && (phb->cCount % BUCKET_SIZE == 0)) {
		CHR(SafeRealloc((LPVOID*)&phb->piOffsets, (phb->cCount + BUCKET_SIZE) * sizeof(UINT32)));
    }
    if (hbe.fOneChar && (phb->cCount % ONE_CHAR_BUCKET_SIZE == 0)) {
		CHR(SafeRealloc((LPVOID*)&phb->piOffsets, (phb->cCount + ONE_CHAR_BUCKET_SIZE) * sizeof(UINT32)));
    }

	phb->piOffsets[phb->cCount++] = iData;
	m_cMaxBucketSize = max(m_cMaxBucketSize, phb->cCount);

	ASSERT(phb->cCount != 0);

	if (phb->cCount == 0) {
		printf("phb->cCount is zero\n\n\n");
	}

Error:
    return hr;

} /* CHashTable::Insert()
   */
