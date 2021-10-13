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
//+---------------------------------------------------------------------------------
//
//
// File:
//      strhash.cpp
//
// Contents:
//
//      Implementation of fixed size hash table that hashes on strings.
//
//----------------------------------------------------------------------------------

#include "headers.h"

#ifdef UNDER_CE
#include "WinCEUtils.h"
#endif


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: ULONG CStrHashTable::Hash(char * pszKey)
//
//  parameters:
//
//  description:
//        Hash function
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
ULONG CStrHashTable::Hash(char * pszKey)
{
    ULONG ulHashValue = 0;

    ASSERT(NULL != pszKey);

    while (*pszKey)
        ulHashValue += *pszKey++;

    return ulHashValue;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CStrHashTable::Insert(char * pszKey, void * pvData)
//
//  parameters:
//
//  description:
//        Insert into hashtable
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CStrHashTable::Insert(char * pszKey, void * pvData)
{
    if (pszKey == NULL || pvData == NULL)
        return E_INVALIDARG;

    ASSERT(Find(pszKey) == NULL);

    char * pKey    = new char[strlen(pszKey) + 1];

    if (pKey)
    {
        ULONG        ulHash = Hash(pszKey);
        ULONG        iEntry = ulHash % HASHTABLE_SIZE;
        SListHashEl* pslEntry = &m_rgHashTable[iEntry];

        while (pslEntry->m_pNext)
            pslEntry = pslEntry->m_pNext;

        if (pslEntry->m_pszKey)
        {
            SListHashEl* pslEntryNew = new SListHashEl();

            if (!pslEntryNew)
            {
#ifndef UNDER_CE
                delete pKey;
#else
                delete [] pKey;
#endif

                goto Cleanup;
            }
            pslEntry->m_pNext = pslEntryNew;
            pslEntry = pslEntryNew;
        }

        strcpy(pKey, pszKey);

        pslEntry->m_pszKey  = pKey;
        pslEntry->m_pvData  = pvData;
        pslEntry->m_ulHash  = ulHash;
		pslEntry->m_pNext = NULL;
        m_ulEntries++;
        return S_OK;
    }

Cleanup:
    return E_OUTOFMEMORY;            
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: void *CStrHashTable::Find(char * pszKey)
//
//  parameters:
//
//  description:
//        Find in the hashtable
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
void *CStrHashTable::Find(char *pszKey)
{
    ULONG         ulHash = Hash(pszKey);
    SListHashEl * pslistEntry = &m_rgHashTable[ulHash % HASHTABLE_SIZE];

    if (pslistEntry->m_pszKey)
    {
        while (pslistEntry)
        {
            if (ulHash == pslistEntry->m_ulHash &&
                !strcmp(pszKey, pslistEntry->m_pszKey))
            {
                return pslistEntry->m_pvData;
            }

            pslistEntry     = pslistEntry->m_pNext;
        }
    }

    return NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: void CStrHashTable::Delete(char *pszKey)
//
//  parameters:
//
//  description:
//        Delete from the hashtable
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStrHashTable::Delete(char *pszKey)
{
    ULONG         ulHash = Hash(pszKey);
    SListHashEl * pslistEntry = &m_rgHashTable[ulHash % HASHTABLE_SIZE];

    if (pslistEntry->m_pszKey)
    {
        SListHashEl * pslistEntryPrev = NULL;
        while (pslistEntry)
        {
            if (ulHash == pslistEntry->m_ulHash &&
                !strcmp(pszKey, pslistEntry->m_pszKey))
            {
                break;
            }
            pslistEntryPrev = pslistEntry;
            pslistEntry     = pslistEntry->m_pNext;
        }

        if (pslistEntry)
        {
            DestroyData(pslistEntry->m_pvData);
            delete[] pslistEntry->m_pszKey;

            if (pslistEntryPrev)
            {
                pslistEntryPrev->m_pNext = pslistEntry->m_pNext;
                delete pslistEntry;
            }
            else
            {
                pslistEntry->Clear();
            }
            m_ulEntries--;
        }
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: void CStrHashTable::DeleteAll()
//
//  parameters:
//
//  description:
//        Delete all elements from the hashtable
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
void CStrHashTable::DeleteAll()
{
    if (m_ulEntries == 0)
        return;

    for (int i = 0; i < HASHTABLE_SIZE; i++)
    {
        bool          fDeleteElem = false;
        SListHashEl * pslistEntry = &m_rgHashTable[i];

        if (pslistEntry->m_pszKey)
        {
            while(pslistEntry)
            {
                SListHashEl * pslistEntryCur = pslistEntry;
                pslistEntry = pslistEntry->m_pNext;

                DestroyData(pslistEntryCur->m_pvData);
                delete[] pslistEntryCur->m_pszKey;
                if (fDeleteElem)
                {
                    delete pslistEntryCur;
                }
                else
                {
                    pslistEntryCur->Clear();
                }
                m_ulEntries--;
                fDeleteElem = true;
            }
        }
    }
}
