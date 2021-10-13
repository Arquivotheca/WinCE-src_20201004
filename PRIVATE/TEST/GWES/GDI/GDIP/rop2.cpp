//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "rop2.h"

BOOL
CRop2::Initialize(TestSuiteInfo *tsi)
{
    BOOL bRval = TRUE;
    TSTRING string;

    tsi->tsFieldDescription.push_back(TEXT("ROP"));

    // retrieve list of rop's, if no list exists default to SRCCOPY, rop's are base 16
    // when entered into the list.
    if(m_SectionList->GetString(TEXT("ROP"), &string) && string == TEXT("All"))
    {
        m_nMaxRopIndex=16;
        m_dwRop = new(DWORD[m_nMaxRopIndex]);

        if(m_dwRop)
        {
            for(int i = 0; i < m_nMaxRopIndex; i++)
                m_dwRop[i] = i;
        }
    }
    else
        bRval &= AllocDWArray(TEXT("ROP"), R2_COPYPEN, &m_dwRop, &m_nMaxRopIndex, m_SectionList, 16);

    m_nRopIndex = 0;

    if(NULL == m_dwRop)
    {
        g_pCOtakLog->Log(OTAK_ERROR, TEXT("ROP2 allocation failed."));
        bRval = FALSE;
    }

    g_pCOtakLog->Log(OTAK_DETAIL, TEXT("%d rop2's in use"), m_nMaxRopIndex);

    return bRval;
}

BOOL
CRop2::PreRun(TestInfo *tiRunInfo, HDC hdc)
{
    BOOL bRval = TRUE;

    tiRunInfo->Descriptions.push_back(itohs(m_dwRop[m_nRopIndex]));
    if(m_dwRop)
        m_dwOldRop2 = SetROP2(hdc, m_dwRop[m_nRopIndex]);
    else bRval = FALSE;

    return bRval;
}

BOOL
CRop2::PostRun(HDC hdc)
{
    BOOL bRVal = FALSE;

    SetROP2(hdc, m_dwOldRop2);

    m_nRopIndex++;
    if(m_nRopIndex >= m_nMaxRopIndex)
    {
        m_nRopIndex = 0;
        bRVal = TRUE;
    }
    return bRVal;
}

BOOL
CRop2::Cleanup()
{
    // free the ROP's
    delete[] m_dwRop;

    return TRUE;
}

DWORD
CRop2::GetRop()
{
    if(m_dwRop)
        return m_dwRop[m_nRopIndex];
    else return -1;
}

