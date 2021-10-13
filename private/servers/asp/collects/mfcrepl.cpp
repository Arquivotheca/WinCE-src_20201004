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
/*--
Abstract:   MFC Replacement, indexes a list of void ptrs with a string
--*/


#include "aspmain.h"
#include <intsafe.h>


CPtrMapper::CPtrMapper(int iInitial)
{
    ZEROMEM(this);
    m_iModAlloc = (iInitial > 0) ? iInitial : VALUE_GROW_SIZE;        // default = 10 

    if (iInitial){

       UINT uiTemp = 0;
       if (FAILED(UIntMult(iInitial, sizeof(MAPINFO), &uiTemp)))
       {
           ASSERT(FALSE);
       }else{

           m_pMapInfo = MyRgAllocZ(MAPINFO, iInitial);
           
       }
    }
}

CPtrMapper::~CPtrMapper()
{
    int i;
    for (i =0; i < m_nEntries; i++)
    {
        MyFree(m_pMapInfo[i].wszKey);

        // To make this inheritiable by other objects again, remove these lines
        ((CRequestStrList*) m_pMapInfo[i].pbData)->Release();
    }
    MyFree(m_pMapInfo);
}

#define MAX_PTRMAPPR_ENTRIES     100000

BOOL CPtrMapper::Set(WCHAR *wszKey, PVOID pbInData)
{
    PREFAST_ASSERT(m_nEntries <= MAX_PTRMAPPR_ENTRIES);
    PREFAST_ASSERT(m_iModAlloc <= 100);

    // Needs to do initial allocation
    if (m_nEntries == 0)
    {
        m_pMapInfo = MyRgAllocZ(MAPINFO, m_iModAlloc);
        if (NULL == m_pMapInfo)
            return FALSE;
    }

    else if (m_nEntries % m_iModAlloc == 0)
    {
        if (NULL == (m_pMapInfo = MyRgReAlloc(MAPINFO,m_pMapInfo,m_nEntries,m_nEntries + m_iModAlloc)))
            return FALSE;
    }

    PREFAST_ASSERT(m_nEntries <= MAX_PTRMAPPR_ENTRIES);
    PREFAST_ASSERT(m_iModAlloc <= 100);

    m_pMapInfo[m_nEntries].wszKey = wszKey;
    m_pMapInfo[m_nEntries].pbData = pbInData;
    m_nEntries++;
    
    return TRUE;
}


BOOL CPtrMapper::Lookup(WCHAR * wszKey, PVOID *ppbOutData)
{
    int i;

    if (NULL == m_pMapInfo)
        return FALSE;
    
    for (i = 0; i < m_nEntries; i++)
    {
        if (0 == wcsicmp(wszKey,m_pMapInfo[i].wszKey))
        {
            *ppbOutData = m_pMapInfo[i].pbData;
            return TRUE;
        }
    }
    *ppbOutData = NULL;
    return FALSE;
}


BOOL CPtrMapper::Lookup(int iIndex, PVOID *ppbOutData)
{
    if (iIndex < 0 || iIndex > m_nEntries)
    {
        return FALSE;
    }
    *ppbOutData = m_pMapInfo[iIndex].pbData;
    return TRUE;
}
