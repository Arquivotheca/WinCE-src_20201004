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
