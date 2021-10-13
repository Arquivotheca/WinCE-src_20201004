//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "bkmode.h"

struct NameValPair nvBkModes[] = {
                                                        {OPAQUE, TEXT("OPAQUE")},
                                                        {TRANSPARENT, TEXT("TRANSPARENT")},
                                                        {0, NULL}
                                              };


BOOL
CBkMode::Initialize(TestSuiteInfo *tsi)
{
    BOOL bRval = TRUE;

    tsi->tsFieldDescription.push_back(TEXT("BkMode"));

    bRval &= AllocTSArray(TEXT("BkMode"), TEXT("OPAQUE"), &m_tsBkModeName, &m_nMaxBkModeIndex, m_SectionList);

    m_dwBkModes = new(DWORD[m_nMaxBkModeIndex]);

    if(m_dwBkModes && m_tsBkModeName)
    {
        for(m_nBkModeIndex=0; m_nBkModeIndex < m_nMaxBkModeIndex; m_nBkModeIndex++)
        {
            if(!nvSearch(nvBkModes, m_tsBkModeName[m_nBkModeIndex], &(m_dwBkModes[m_nBkModeIndex])))
            {
                g_pCOtakLog->Log(OTAK_ERROR, TEXT("Invalid BkMode %s."), m_tsBkModeName[m_nBkModeIndex].c_str());
                bRval = FALSE;
                break;
            }
        }
    }
    else
    {
        g_pCOtakLog->Log(OTAK_ERROR, TEXT("Failed to allocate BKMode array."));
        bRval = FALSE;
    }

    g_pCOtakLog->Log(OTAK_DETAIL, TEXT("%d BkModes in use."), m_nMaxBkModeIndex);

    m_nBkModeIndex = 0;

    return bRval;
}

BOOL
CBkMode::PreRun(TestInfo *tiRunInfo, HDC hdc)
{
    tiRunInfo->Descriptions.push_back(m_tsBkModeName[m_nBkModeIndex]);

    SetBkMode(hdc, m_dwBkModes[m_nBkModeIndex]);

    return TRUE;
}

BOOL
CBkMode::PostRun()
{
    BOOL bRVal = FALSE;

    m_nBkModeIndex++;

    if(m_nBkModeIndex >= m_nMaxBkModeIndex)
    {
        m_nBkModeIndex = 0;
        bRVal = TRUE;
    }

    return bRVal;
}

BOOL
CBkMode::Cleanup()
{

    delete[] m_tsBkModeName;
    delete[] m_dwBkModes;

    return TRUE;
}

