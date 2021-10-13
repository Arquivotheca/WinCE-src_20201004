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
//+----------------------------------------------------------------------------
//
//
// File:
//      dynarray.cpp
//
// Contents:
//
//      CDynArray class implementation
//
//-----------------------------------------------------------------------------

#include "Headers.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CDynArray::CDynArray()
//
//  parameters:
//          
//  description:
//          CDynArray constructor
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
CDynArray::CDynArray()
{
    memset(this, 0, sizeof(CDynArray)); 
    m_pCurrentBlock = &m_aStartBlock;
    m_critSect.Initialize();
    m_uiSize = 0; 
}
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CDynArray::~CDynArray()
//
//  parameters:
//          
//  description:
//          
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
CDynArray::~CDynArray()
{
    m_critSect.Delete();
}
////////////////////////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: UINT   CDynArray::Size() const;
//
//  parameters:
//          
//  description:
//     size is calculated by the number of arrayblocks...
//      that's done when we create a block and add, so the number is 
//      precalculated
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
UINT CDynArray::Size() const
{
    return(m_uiSize);
}
////////////////////////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: UINT   Add(CDynArrayEntry *pEntry)
//
//  parameters:
//          
//  description:
//     size is calculated by the number of arrayblocks...
//      that's done when we create a block and add, so the number is 
//      precalculated
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CDynArray::Add(CDynArrayEntry *pEntry)
{
    HRESULT hr = S_OK;
    CDynArrayBlock *pNew; 

    CCritSectWrapper csw(&m_critSect);
    
    CHK (csw.Enter());
    
    if (m_pCurrentBlock->GetSize() == g_blockSize)
    {
        // the guy is full
        pNew = new CDynArrayBlock();
        CHK_BOOL(pNew, E_OUTOFMEMORY);
        
        InterlockedExchangePointer(&(m_pCurrentBlock->m_pNext), pNew);
        InterlockedExchangePointer(&(m_pCurrentBlock), pNew);        
    }
    
    m_pCurrentBlock->Add(pEntry); 
    m_uiSize++; 
    
Cleanup:
    return(hr);
}
////////////////////////////////////////////////////////////////////////////////////////////////////




////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT   ReplaceAt(UINT uiPos, CDynArrayEntry *pEntry)
//
//  parameters:
//          
//  description:
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CDynArray:: ReplaceAt(UINT uiPos, CDynArrayEntry *pEntry)
{
    HRESULT hr = S_OK;
    
    CDynArrayBlock *pNew = &m_aStartBlock; 
    if (uiPos >= m_uiSize)
        return(E_INVALIDARG);

    for (; uiPos>=g_blockSize;uiPos-=g_blockSize )
    {
        pNew = pNew->m_pNext;
    }    
    
    pNew->m_aEntries[uiPos] = pEntry; 
    
    return(hr);
    
}
////////////////////////////////////////////////////////////////////////////////////////////////////




////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CDynArrayEntry * GetAt(UINT uiPos)
//
//  parameters:
//          
//  description:
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
CDynArrayEntry *CDynArray:: GetAt(UINT uiPos)
{
    CDynArrayBlock *pNew = &m_aStartBlock; 
    if (uiPos >= m_uiSize)
        return(0);

    for (; uiPos>=g_blockSize;uiPos-=g_blockSize )
    {
        pNew = pNew->m_pNext;
    }    
    
    return (pNew->m_aEntries[uiPos]);
}
////////////////////////////////////////////////////////////////////////////////////////////////////


