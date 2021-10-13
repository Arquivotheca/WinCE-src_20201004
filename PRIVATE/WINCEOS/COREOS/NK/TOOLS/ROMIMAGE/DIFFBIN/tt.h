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
