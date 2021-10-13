//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//+---------------------------------------------------------------------------------
//
//
// File:
//      strhash.h
//
// Contents:
//
//      This module defines hash table that hashes on strings.
//
//----------------------------------------------------------------------------------

#ifndef __STRHASH_H_INCLUDED__
#define __STRHASH_H_INCLUDED__

#define HASHTABLE_SIZE 31       // This is arbitrary (must be prime number)

typedef void ( *PDESTROYFUNC )(IN PVOID pvData );

typedef struct _SListHashEl
{
    void Clear() { m_pNext = NULL; m_pszKey = NULL; m_pvData = NULL; m_ulHash = 0; }

    struct _SListHashEl * m_pNext;
    char                * m_pszKey;
    void                * m_pvData;
    ULONG                 m_ulHash;
} SListHashEl;

class CStrHashTable
{
public:
    CStrHashTable()     { memset(this, 0, sizeof (CStrHashTable)); }
    ~CStrHashTable()    { }

    HRESULT Insert(char * m_pszKey, void * pvData);
    void *  Find (char * m_pszKey);
    void    Passivate() { DeleteAll(); }
    void    Delete(char * pszKey);
    void    DeleteAll();
    // Used in the case of an AV. Cannot delete memory since it may be corrupted.
    // We have to live with memory leak in case of an AV.
    void    ClearHashTable() { memset((char *)m_rgHashTable, 0, sizeof (m_rgHashTable)); m_ulEntries = 0; }

protected:
    virtual void DestroyData(void * pvData) = 0;

private:
    SListHashEl     m_rgHashTable[HASHTABLE_SIZE];
    ULONG           m_ulEntries;

    ULONG   Hash(char * pszHash);
};

#endif  // __STRHASH_H_INCLUDED__
