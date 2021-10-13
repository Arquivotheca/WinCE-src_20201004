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
// translation table
// tt.cpp
#include "diffbin.h"


CTranslationTable::CTranslationTable(): m_cEntries(0), m_pte(NULL)
/*---------------------------------------------------------------------------*\
 * Create the translation table
\*---------------------------------------------------------------------------*/
{

} /* CTranslationTable::CTranslationTable()
   */

CTranslationTable::~CTranslationTable()
/*---------------------------------------------------------------------------*\
 *
\*---------------------------------------------------------------------------*/
{
    LocalFree(m_pte);

} /* CTranslationTable::~CTranslationTable()
   */

HRESULT
CTranslationTable::Insert(UINT32 iPacked, ADDRESS iUnpacked)
/*---------------------------------------------------------------------------*\
 *   Insert a new translation from offset "iPacked" in the
 *   image to address "iUnpacked" in the destination memory
\*---------------------------------------------------------------------------*/
{
    HRESULT         hr       = NOERROR;

    if (m_cEntries % TT_ENTRIES == 0) {
        CHR(SafeRealloc((LPVOID*)&m_pte, (m_cEntries + TT_ENTRIES) * sizeof(TranslationEntry)));
    }
	m_pte[m_cEntries].iPacked = iPacked;
	m_pte[m_cEntries].iUnpacked = iUnpacked;
	m_cEntries++;

Error:
    return hr;

} /* CTranslationTable::Insert()
   */

ADDRESS 
CTranslationTable::Lookup(UINT32 iPacked)
/*---------------------------------------------------------------------------*\
 *   Get the address in the destination memory that
 *   corresponds to offset "iPacked" in the image file
\*---------------------------------------------------------------------------*/
{
	UINT32 iEntry = 0;
	while (iEntry < m_cEntries)
	{
		if (iPacked < m_pte[iEntry].iPacked)
			return AddAddress(m_pte[iEntry - 1].iUnpacked, iPacked - m_pte[iEntry - 1].iPacked);
		++iEntry;
	}
	return AddAddress(m_pte[m_cEntries - 1].iUnpacked, iPacked - m_pte[m_cEntries - 1].iPacked);

} /* CTranslationTable::Lookup()
   */

