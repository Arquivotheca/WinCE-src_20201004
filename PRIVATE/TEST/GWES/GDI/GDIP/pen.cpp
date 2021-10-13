//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "pen.h"

BOOL
CPen::Initialize(TestSuiteInfo *tsi)
{
    BOOL bRval = TRUE;

    tsi->tsFieldDescription.push_back(TEXT("Pen"));

    bRval &= AllocTSArray(TEXT("Pen"), TEXT("SOLID"), &m_tsPenNameArray, &m_nMaxPenIndex, m_SectionList);

    // allocate the array for pens.
    m_hPen = new(HPEN[m_nMaxPenIndex]);

    if(m_hPen && m_tsPenNameArray)
    {
        // Initialize Pen Array
        for(m_nPenIndex = 0; m_nPenIndex < m_nMaxPenIndex; m_nPenIndex++)
        {
            if(NULL == (m_hPen[m_nPenIndex] = myCreatePen(m_tsPenNameArray[m_nPenIndex])))
            {
                g_pCOtakLog->Log(OTAK_ERROR, TEXT("Failed to create selected Pen %s"), m_tsPenNameArray[m_nPenIndex].c_str());
                bRval = FALSE;
            }
        }
    }
    else
    {
        g_pCOtakLog->Log(OTAK_ERROR, TEXT("Pen array/name array allocation failed."));
        bRval = FALSE;
    }

    m_nPenIndex = 0;

    g_pCOtakLog->Log(OTAK_DETAIL, TEXT("%d Pens in use."), m_nMaxPenIndex);

    bRval &= m_BkMode.Initialize(tsi);

    return bRval;
}

BOOL
CPen::PreRun(TestInfo *tiRunInfo, HDC hdc)
{
    tiRunInfo->Descriptions.push_back(m_tsPenNameArray[m_nPenIndex]);

    m_StockPen = (HPEN) SelectObject(hdc, m_hPen[m_nPenIndex]);

    m_BkMode.PreRun(tiRunInfo, hdc);

    return TRUE;
}

BOOL
CPen::PostRun(HDC hdc)
{
    BOOL bRVal = FALSE;

    SelectObject(hdc, m_StockPen);

    m_nPenIndex++;

    if(m_nPenIndex >= m_nMaxPenIndex)
    {
        m_nPenIndex = 0;
        if(m_BkMode.PostRun())
            bRVal = TRUE;
    }

    return bRVal;
}

BOOL
CPen::Cleanup()
{

    if(m_hPen)
        for(m_nPenIndex = 0; m_nPenIndex < m_nMaxPenIndex; m_nPenIndex++)
        {
            DeleteObject(m_hPen[m_nPenIndex]);
        }
    delete[] m_tsPenNameArray;
    delete[] m_hPen;

    m_BkMode.Cleanup();

    return TRUE;
}

