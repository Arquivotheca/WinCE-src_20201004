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
                info(FAIL, TEXT("Invalid BkMode %s."), m_tsBkModeName[m_nBkModeIndex].c_str());
                bRval = FALSE;
                break;
            }
        }
    }
    else
    {
        info(FAIL, TEXT("Failed to allocate BKMode array."));
        bRval = FALSE;
    }

    info(ECHO, TEXT("%d BkModes in use."), m_nMaxBkModeIndex);

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

