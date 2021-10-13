//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "surface.h"

BOOL
CSurface::Initialize(TestSuiteInfo *tsi, TSTRING ts, TSTRING tsDefault)
{
    BOOL bRval = TRUE;

    tsi->tsFieldDescription.push_back(ts);
    tsi->tsFieldDescription.push_back(ts + TEXT("bpp"));

    // retrieve list of destinations
    if(!(m_SectionList->GetString(TEXT("Device"), &m_tsDeviceName)))
    {
        m_tsDeviceName = TEXT("Display");
    }

    // retrieve list of destinations
    if(m_nMaxIndex = m_SectionList->GetStringArray(ts, NULL, 0))
    {
        m_tsDCNameArray = new(TSTRING[m_nMaxIndex]);
        m_DC = new(HDC[m_nMaxIndex]);
        m_nBitDepth = new(UINT[m_nMaxIndex]);

        if(m_tsDCNameArray)
            m_SectionList->GetStringArray(ts, m_tsDCNameArray, m_nMaxIndex);
    }
    else
    {
        m_nMaxIndex = 1;
        m_tsDCNameArray = new(TSTRING[m_nMaxIndex]);
        m_DC = new(HDC[m_nMaxIndex]);
        m_nBitDepth = new(UINT[m_nMaxIndex]);

        if(m_tsDCNameArray)
            m_tsDCNameArray[0] = tsDefault;
    }

    if(NULL == m_tsDCNameArray)
    {
        g_pCOtakLog->Log(OTAK_ERROR, TEXT("Name array allocation failed."));
        bRval = FALSE;
    }
    else if(!m_DC)
    {
        g_pCOtakLog->Log(OTAK_ERROR, TEXT("DC array allocation failed."));
        bRval = FALSE;
    }
    else if(!m_nBitDepth)
    {
        g_pCOtakLog->Log(OTAK_ERROR, TEXT("BitDepth array allocation failed."));
        bRval = FALSE;
    }
    else
    {
        // Initialize  DC Array, if the initialization fails then the entry is set to NULL,
        // which is what is needed for cleanup.
        for(m_nIndex = 0; m_nIndex < m_nMaxIndex; m_nIndex++)
        {
            if(NULL == (m_DC[m_nIndex] = myCreateSurface(m_tsDeviceName, m_tsDCNameArray[m_nIndex], &(m_nBitDepth[m_nIndex]))))
            {
                g_pCOtakLog->Log(OTAK_ERROR, TEXT("Failed to create selected surface %s"), m_tsDCNameArray[m_nIndex].c_str());
                bRval = FALSE;
            }
        }
    }

    m_nIndex = 0;

    g_pCOtakLog->Log(OTAK_DETAIL, TEXT("%d %s in use."), m_nMaxIndex, ts.c_str());

    return bRval;
}

BOOL
CSurface::PreRun(TestInfo *tiRunInfo)
{
    tiRunInfo->Descriptions.push_back(m_tsDCNameArray[m_nIndex]);
    tiRunInfo->Descriptions.push_back(itos(m_nBitDepth[m_nIndex]));

    return TRUE;
}

BOOL
CSurface::PostRun()
{
    BOOL bRVal = FALSE;
    m_nIndex++;
    if(m_nIndex >= m_nMaxIndex)
    {
        m_nIndex = 0;
        bRVal = TRUE;
    }

    return bRVal;
}

BOOL
CSurface::Cleanup()
{
    // free the destination dc's
    if(m_DC)
        for(m_nIndex = 0; m_nIndex < m_nMaxIndex; m_nIndex++)
        {
            myDeleteSurface(m_DC[m_nIndex], m_tsDCNameArray[m_nIndex]);
        }
    delete[] m_tsDCNameArray;
    delete[] m_DC;

    return TRUE;
}

HDC
CSurface::GetSurface()
{
    return(m_DC[m_nIndex]);
}
