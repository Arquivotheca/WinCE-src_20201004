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
        m_hWnd = new(HWND[m_nMaxIndex]);
        m_nBitDepth = new(UINT[m_nMaxIndex]);

        if(m_tsDCNameArray)
            m_SectionList->GetStringArray(ts, m_tsDCNameArray, m_nMaxIndex);
    }
    else
    {
        m_nMaxIndex = 1;
        m_tsDCNameArray = new(TSTRING[m_nMaxIndex]);
        m_DC = new(HDC[m_nMaxIndex]);
        m_hWnd = new(HWND[m_nMaxIndex]);
        m_nBitDepth = new(UINT[m_nMaxIndex]);

        if(m_tsDCNameArray)
            m_tsDCNameArray[0] = tsDefault;
    }

    if(NULL == m_tsDCNameArray)
    {
        info(FAIL, TEXT("Name array allocation failed."));
        bRval = FALSE;
    }
    else if(!m_DC)
    {
        info(FAIL, TEXT("DC array allocation failed."));
        bRval = FALSE;
    }
    else if(!m_nBitDepth)
    {
        info(FAIL, TEXT("BitDepth array allocation failed."));
        bRval = FALSE;
    }
    else
    {
        // Initialize  DC Array, if the initialization fails then the entry is set to NULL,
        // which is what is needed for cleanup.
        for(m_nIndex = 0; m_nIndex < m_nMaxIndex; m_nIndex++)
        {
            if(NULL == (m_DC[m_nIndex] = myCreateSurface(m_tsDeviceName, m_tsDCNameArray[m_nIndex], &(m_nBitDepth[m_nIndex]), &(m_hWnd[m_nIndex]))))
            {
                info(FAIL, TEXT("Failed to create selected surface %s"), m_tsDCNameArray[m_nIndex].c_str());
                bRval = FALSE;
            }
        }
    }

    m_nIndex = 0;

    info(ECHO, TEXT("%d %s in use."), m_nMaxIndex, ts.c_str());

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
            myDeleteSurface(m_DC[m_nIndex], m_tsDCNameArray[m_nIndex], m_hWnd[m_nIndex]);
        }
    delete[] m_tsDCNameArray;
    delete[] m_DC;
    delete[] m_hWnd;

    return TRUE;
}

HDC
CSurface::GetSurface()
{
    return(m_DC[m_nIndex]);
}
