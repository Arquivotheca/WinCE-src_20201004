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
#include "dispperfdata.h"

BOOL
CDispPerfData::Initialize(TestSuiteInfo * tsi)
{
    info(DETAIL, TEXT("In CDispPerfData::Initialize"));
    BOOL bRval = TRUE;

#ifdef UNDER_CE
    DWORD Options[] = {
                                     DISPPERF_EXTESC_CLEARTIMING, 
                                     DISPPERF_EXTESC_GETSIZE
                                    };

    m_hdc = GetDC(NULL);

    if(m_hdc)
    {
        m_bDispPerfAvailable = TRUE;

        // verify that the dispperf options we want to use are available.  if any aren't, then don't use dispperf.
        for(int i = 0; i < countof(Options); i++)
        {
            m_dwQuery = Options[i];
            if(ExtEscape(m_hdc, QUERYESCSUPPORT, sizeof(DWORD), (LPCSTR)&m_dwQuery, 0, NULL) <= 0)
            {
                m_bDispPerfAvailable = FALSE;
                break;
            }
        }

        if(m_bDispPerfAvailable)
        {
            // if the extescapes were queried to be available, verify that we can actually clear the data.
            // some drivers seem to return true to the query, but can't do it, this is a workaround for those devices.
            if(ExtEscape(m_hdc, DISPPERF_EXTESC_CLEARTIMING, 0, NULL, sizeof(DWORD), (LPSTR)&m_dwQuery) <= 0)
            {
                m_bDispPerfAvailable = FALSE;
            }
            else
            {
                tsi->tsFieldDescription.push_back(TEXT("cGPE"));
                tsi->tsFieldDescription.push_back(TEXT("cEmul"));
                tsi->tsFieldDescription.push_back(TEXT("cHardware"));
            }
        }
        else
            info(ECHO, TEXT("DispPerf data not available."));
    }
    else
    {
        info(FAIL, TEXT("Failed to retrieve the DC for DispPerf."));
        bRval = FALSE;
    }
#endif

    return bRval;
}

BOOL
CDispPerfData::PreRun(TestInfo * tiRunInfo)
{
    info(DETAIL, TEXT("In CDispPerfData::PreRun"));
    BOOL bRval = TRUE;

#ifdef UNDER_CE
    if(m_bDispPerfAvailable)
    {
        if(ExtEscape(m_hdc, DISPPERF_EXTESC_CLEARTIMING, 0, NULL, sizeof(DWORD), (LPSTR)&m_dwQuery) <= 0)
        {
            info(FAIL, TEXT("DISPPERF_EXTESC_CLEARTIMING failed."));
            bRval = FALSE;
        }
    }
#endif

    return bRval;
}

BOOL
CDispPerfData::AddPostRunData(TestInfo * tiRunInfo)
{
    info(DETAIL, TEXT("In CDispPerfData::AddPostRunData"));
    BOOL bRval = TRUE;
#ifdef UNDER_CE
    PDISPPERF_TIMING    pTimings;
    int cGPE = 0;
    int cEmul = 0;
    int cHardware = 0;


    if(m_bDispPerfAvailable)
    {
        if(ExtEscape(m_hdc, DISPPERF_EXTESC_GETSIZE, 0, NULL, sizeof(DWORD), (LPSTR)&m_dwQuery) >= 0)
        {
            if(m_dwQuery > 0)
            {
                pTimings = (PDISPPERF_TIMING)LocalAlloc (LPTR, m_dwQuery);

                if(pTimings)
                {
                    // clear the memory allocated, m_dwQuery is the # of bytes allocated as returned from the ExtEscape.
                    memset(pTimings, 0, m_dwQuery);
                    if(ExtEscape(m_hdc, DISPPERF_EXTESC_GETTIMING, 0, NULL, m_dwQuery, (LPSTR)pTimings) > 0)
                    {
                        for (int i = 0; i < (m_dwQuery/sizeof(DISPPERF_TIMING)); i++)
                        {
                            cGPE += pTimings[i].cGPE;
                            cEmul += pTimings[i].cEmul;
                            cHardware += pTimings[i].cHardware;
                        }

                        tiRunInfo->Descriptions.push_back(itos(cGPE));
                        tiRunInfo->Descriptions.push_back(itos(cEmul));
                        tiRunInfo->Descriptions.push_back(itos(cHardware));
                    }
                    else
                    {
                        info(FAIL, TEXT("DISPPERF_EXTESC_GETTIMING failed."));
                        bRval = FALSE;
                    }
                    LocalFree(pTimings);
                }
                else
                {
                    info(FAIL, TEXT("Failed to allocate dispperf data buffer."));
                    bRval = FALSE;
                }
            }
            else
            {
                tiRunInfo->Descriptions.push_back(TEXT("0"));
                tiRunInfo->Descriptions.push_back(TEXT("0"));
                tiRunInfo->Descriptions.push_back(TEXT("0"));
            }
        }
        else
        {
            info(FAIL, TEXT("DISPPERF_EXTESC_GETSIZE failed."));
            bRval = FALSE;
        }
    }
#endif
    return bRval;
}

BOOL
CDispPerfData::Cleanup()
{
    info(DETAIL, TEXT("In CDispPerfData::Cleanup"));

#ifdef UNDER_CE
    ReleaseDC(NULL, m_hdc);
#endif
    return TRUE;
}
