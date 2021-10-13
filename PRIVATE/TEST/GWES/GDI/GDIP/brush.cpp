//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "brush.h"

BOOL
CBrush::Initialize(TestSuiteInfo *tsi)
{
    BOOL bRval = TRUE;

    tsi->tsFieldDescription.push_back(TEXT("Brush"));

    // retrieve number of brushes given in the script
    if(m_nMaxBrushIndex = m_SectionList->GetStringArray(TEXT("Brush"), NULL, 0))
    {
        int nActualBrushIndexReturned;

        m_tsBrushNameArray = new(TSTRING[m_nMaxBrushIndex]);
        m_hBrush = new(HBRUSH[m_nMaxBrushIndex]);

        // fill it in with the actual strings
        if(m_tsBrushNameArray)
        {
            nActualBrushIndexReturned = m_SectionList->GetStringArray(TEXT("Brush"), m_tsBrushNameArray, m_nMaxBrushIndex);
            if(nActualBrushIndexReturned != m_nMaxBrushIndex)
            {
                g_pCOtakLog->Log(OTAK_ERROR, TEXT("Original string count returned %d doesn't match actual string count %d."), m_nMaxBrushIndex, nActualBrushIndexReturned);
                bRval = FALSE;
            }
        }
    }
    else // no brushes in the script, use the default
    {
        m_nMaxBrushIndex = 1;
        m_tsBrushNameArray = new(TSTRING[m_nMaxBrushIndex]);
        m_hBrush = new(HBRUSH[m_nMaxBrushIndex]);
        // set the default
        if(m_tsBrushNameArray)
            m_tsBrushNameArray[0] = TEXT("NULL");
    }

    // if the array's allocated, fill them in.
    if(m_tsBrushNameArray && m_hBrush)
    {
        // Initialize Brush Array
        for(m_nBrushIndex = 0; m_nBrushIndex < m_nMaxBrushIndex; m_nBrushIndex++)
        {
            if(NULL == (m_hBrush[m_nBrushIndex] = myCreateBrush(m_tsBrushNameArray[m_nBrushIndex])))
            {
                g_pCOtakLog->Log(OTAK_ERROR, TEXT("Failed to create selected brush %s"), m_tsBrushNameArray[m_nBrushIndex].c_str());
                bRval = FALSE;
            }
        }
    }
    else
    {
        g_pCOtakLog->Log(OTAK_ERROR, TEXT("Brush array/name array allocation failed."));
        bRval = FALSE;
    }

    m_nBrushIndex = 0;

    g_pCOtakLog->Log(OTAK_DETAIL, TEXT("%d brushes in use."), m_nMaxBrushIndex);

    return bRval;
}

BOOL
CBrush::PreRun(TestInfo *tiRunInfo, HDC hdc)
{
    BOOL bRval = TRUE;

    // if the pointers are invalid, fail the call.
    if(m_hBrush && m_tsBrushNameArray)
    {
        tiRunInfo->Descriptions.push_back(m_tsBrushNameArray[m_nBrushIndex]);
        m_StockBrush = (HBRUSH) SelectObject(hdc, m_hBrush[m_nBrushIndex]);
    }
    else bRval = FALSE;

    return bRval;
}

// PostRun increments 
BOOL
CBrush::PostRun(HDC hdc)
{
    BOOL bRVal = FALSE;

    SelectObject(hdc, m_StockBrush);

    m_nBrushIndex++;

    if(m_nBrushIndex >= m_nMaxBrushIndex)
    {
        m_nBrushIndex = 0;
        bRVal = TRUE;
    }

    return bRVal;
}

BOOL
CBrush::Cleanup()
{

    if(m_hBrush)
        for(m_nBrushIndex = 0; m_nBrushIndex < m_nMaxBrushIndex; m_nBrushIndex++)
        {
            DeleteObject(m_hBrush[m_nBrushIndex]);
        }
    delete[] m_tsBrushNameArray;
    delete[] m_hBrush;

    return TRUE;
}

HBRUSH
CBrush::GetBrush()
{
    return m_hBrush[m_nBrushIndex];
}
