// tt.h

#pragma once

////////////////////////////////////////////////////////////
// AddAddress
//   Add an offset to an ADDRESS struct.
////////////////////////////////////////////////////////////
__inline ADDRESS 
AddAddress(ADDRESS addr, UINT32 iOffset)
{
    if (addr.iOffset == NO_COMPRESS) {
		addr.iAddr += iOffset;
    } else {
		addr.iOffset += iOffset;
    }
	return addr;
}

//////////////////////////////////////////////////
// Translation table structures
//////////////////////////////////////////////////

#define TT_ENTRIES 128

typedef struct _TranslationEntry
{
	UINT32 iPacked;
	ADDRESS iUnpacked;
} TranslationEntry;

class CTranslationTable
{
public:
    CTranslationTable();
    ~CTranslationTable();

    HRESULT     Insert(UINT32 iPacked, ADDRESS iUnpacked);
    ADDRESS     Lookup(UINT32 iPacked);

private:
    DWORD               m_cEntries;
    TranslationEntry *  m_pte;

};
