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
#include "AlphaBlend.h"

BOOL
CAlphaBlendTestSuite::InitializeBlendFunction(TestSuiteInfo *tsi)
{
    BOOL bRval = TRUE;
    int nSCAValue;

    tsi->tsFieldDescription.push_back(TEXT("BlendFunction"));

    // retrieve list of destinations
    if(m_nMaxBlendFunctionIndex = m_SectionList->GetStringArray(TEXT("BlendFunction"), NULL, 0))
    {
        m_tsBlendFunctionName = new(TSTRING[m_nMaxBlendFunctionIndex]);
        m_BlendFunction = new(BLENDFUNCTION[m_nMaxBlendFunctionIndex]);

        if(m_tsBlendFunctionName)
            m_SectionList->GetStringArray(TEXT("BlendFunction"), m_tsBlendFunctionName, m_nMaxBlendFunctionIndex);
    }
    // if there's no destinations, use the default.
    else
    {
        m_nMaxBlendFunctionIndex = 1;
        m_tsBlendFunctionName = new(TSTRING[m_nMaxBlendFunctionIndex]);
        m_BlendFunction = new(BLENDFUNCTION[m_nMaxBlendFunctionIndex]);

        // default to a source constant alpha, but not 0xFF because it's a special value to do nothing.
        if(m_tsBlendFunctionName)
            m_tsBlendFunctionName[0] = TEXT("SCA_0x77");
    }

    // if the destinations are available, use them
    if(m_tsBlendFunctionName && m_BlendFunction)
    {
        for(m_nBlendFunctionIndex = 0; m_nBlendFunctionIndex < m_nMaxBlendFunctionIndex && bRval; m_nBlendFunctionIndex++)
        {
            m_BlendFunction[m_nBlendFunctionIndex].BlendOp = AC_SRC_OVER;
            m_BlendFunction[m_nBlendFunctionIndex].BlendFlags = 0;
            m_BlendFunction[m_nBlendFunctionIndex].SourceConstantAlpha = 0;
            m_BlendFunction[m_nBlendFunctionIndex].AlphaFormat = 0;

            // PPA&SCA must go first because it contains SCA and PPA
            if (m_tsBlendFunctionName[m_nBlendFunctionIndex].npos != m_tsBlendFunctionName[m_nBlendFunctionIndex].find(TEXT("PPA&SCA")))
            {
                if(1 != _stscanf_s(m_tsBlendFunctionName[m_nBlendFunctionIndex].c_str(), TEXT("PPA&SCA_0x%x"), &nSCAValue))
                {   
                    nSCAValue = 0x77;
                    m_tsBlendFunctionName[m_nBlendFunctionIndex]=TEXT("PPA&SCA_0x77");
                }

                m_BlendFunction[m_nBlendFunctionIndex].SourceConstantAlpha = nSCAValue;
                m_BlendFunction[m_nBlendFunctionIndex].AlphaFormat = AC_SRC_ALPHA;
            }
            else if(m_tsBlendFunctionName[m_nBlendFunctionIndex].npos != m_tsBlendFunctionName[m_nBlendFunctionIndex].find(TEXT("SCA")))
            {
                if(1 != _stscanf_s(m_tsBlendFunctionName[m_nBlendFunctionIndex].c_str(), TEXT("SCA_0x%x"), &nSCAValue))
                {   
                    nSCAValue = 0x77;
                    m_tsBlendFunctionName[m_nBlendFunctionIndex]=TEXT("SCA_0x77");
                }

                m_BlendFunction[m_nBlendFunctionIndex].SourceConstantAlpha = nSCAValue;
            }
            else if (TEXT("PPA") == m_tsBlendFunctionName[m_nBlendFunctionIndex])
            {
                m_BlendFunction[m_nBlendFunctionIndex].SourceConstantAlpha = 0xFF;
                m_BlendFunction[m_nBlendFunctionIndex].AlphaFormat = AC_SRC_ALPHA;
            }
            else
            {
                info(FAIL, TEXT("Unknown BlendFunction method %s."), m_tsBlendFunctionName[m_nBlendFunctionIndex]);
                bRval = FALSE;
            }
        }
    }
    else
    {
        info(FAIL, TEXT("Failed to allocate BlendFunction name/value array."));
        bRval = FALSE;
    }
    
    m_nBlendFunctionIndex = 0;

    return bRval;
}

BOOL
CAlphaBlendTestSuite::InitializeFunctionPointer()
{
    BOOL bRval = TRUE;

    // function pointers
    m_hinstCoreDLL = LoadLibrary(TEXT("\\coredll.dll"));

    if(m_hinstCoreDLL)
    {
        // this mail fail, indicating that GWES doesn't support alpha blending.
        // desktop always has alphablend.
#ifdef UNDER_CE
        m_pfnAlphaBlend = (PFNALPHABLEND) GetProcAddress(m_hinstCoreDLL, TEXT("AlphaBlend"));
#else
        m_pfnAlphaBlend = &AlphaBlend;
#endif

        if(!m_pfnAlphaBlend)
        {
            info(FAIL, TEXT("Unknown BlendFunction method %s."), m_tsBlendFunctionName[m_nBlendFunctionIndex]);
            bRval = FALSE;
        }
    }
    else
    {
        info(FAIL, TEXT("Unable to load CoreDLL."));
        bRval = FALSE;
    }

    return bRval;
}

BOOL
CAlphaBlendTestSuite::Initialize(TestSuiteInfo * tsi)
{
    info(ECHO, TEXT("In CAlphaBlendTestSuite::Initialize"));
    BOOL bRval = TRUE;
    
#ifndef UNDER_CE
    m_dwOldBatchLimit = GdiSetBatchLimit(1);
    if(0 == m_dwOldBatchLimit)
        info(FAIL, TEXT("Failed to disable GDI call batching."));
#endif

    tsi->tsTestName = TEXT("AlphaBlend");

    // initialize everything, if anything fails return failure.
    // cleanup is called whether or not there is a failure, which will deallocate 
    // anything that was allocated if there was a failure.
    if(bRval && (bRval = bRval && InitializeFunctionPointer()))
        if(bRval = bRval && m_StretchCoordinates.Initialize(tsi))
            if(bRval = bRval && InitializeBlendFunction(tsi))
                if(bRval = bRval &&m_Source.Initialize(tsi, TEXT("Source"), TEXT("DIB32_RGB8888")))
                    if(bRval = bRval && m_Dest.Initialize(tsi, TEXT("Dest")))
                        if(bRval = bRval && m_Rgn.Initialize(tsi))
                            // the disp perf data entries correspond to data added by the 
                            // add post run data function, thus this needs to be done last.
                            m_DispPerfData.Initialize(tsi);

    return bRval;
}

BOOL
CAlphaBlendTestSuite::PreRun(TestInfo *tiRunInfo)
{
    info(DETAIL, TEXT("In CAlphaBlendTestSuite::PreRun"));

    m_StretchCoordinates.PreRun(tiRunInfo);
    m_sCoordinateInUse = m_StretchCoordinates.GetCoordinates();

    tiRunInfo->Descriptions.push_back(m_tsBlendFunctionName[m_nBlendFunctionIndex].c_str());

    m_Source.PreRun(tiRunInfo);
    m_hdcSource = m_Source.GetSurface();

    m_Dest.PreRun(tiRunInfo);
    m_hdcDest = m_Dest.GetSurface();

    m_Rgn.PreRun(tiRunInfo, m_hdcDest);

    m_DispPerfData.PreRun(tiRunInfo);

    return TRUE;
}

BOOL
CAlphaBlendTestSuite::Run()
{
    // logging here can cause timing issues.
    //info(DETAIL, TEXT("In CAlphaBlendTestSuite::Run"));
    return(m_pfnAlphaBlend(m_hdcDest,
                        m_sCoordinateInUse.nDestLeft,
                        m_sCoordinateInUse.nDestTop,
                        m_sCoordinateInUse.nDestWidth,
                        m_sCoordinateInUse.nDestHeight,
                        m_hdcSource,
                        m_sCoordinateInUse.nSrcLeft,
                        m_sCoordinateInUse.nSrcTop,
                        m_sCoordinateInUse.nSrcWidth,
                        m_sCoordinateInUse.nSrcHeight,
                        m_BlendFunction[m_nBlendFunctionIndex]));
}

BOOL
CAlphaBlendTestSuite::AddPostRunData(TestInfo *tiRunInfo)
{
    info(DETAIL, TEXT("In CAlphaBlendTestSuite::AddPostRunData"));

    return m_DispPerfData.AddPostRunData(tiRunInfo);
}

BOOL
CAlphaBlendTestSuite::PostRun()
{
    info(DETAIL, TEXT("In CAlphaBlendTestSuite::PostRun"));

    m_nIterationCount++;

    // iterate to the next options
    if(m_StretchCoordinates.PostRun())
    {
        m_nBlendFunctionIndex++;
        if(m_nBlendFunctionIndex >= m_nMaxBlendFunctionIndex)
        {
            m_nBlendFunctionIndex=0;
            if(m_Source.PostRun())
                if(m_Dest.PostRun())
                    if(m_Rgn.PostRun())
                        return FALSE;
        }
    }
    return TRUE;
}

BOOL
CAlphaBlendTestSuite::Cleanup()
{
    info(ECHO, TEXT("In CAlphaBlendTestSuite::Cleanup"));

    // free the coredll library loaded when retrieving the function pointer.
    FreeLibrary(m_hinstCoreDLL);

    // free the blend functions
    delete[] m_BlendFunction;
    delete[] m_tsBlendFunctionName;

    // clean up all of the test options
    m_StretchCoordinates.Cleanup();
    m_Source.Cleanup();
    m_Dest.Cleanup();
    m_Rgn.Cleanup();
    m_DispPerfData.Cleanup();

#ifndef UNDER_CE
    if(m_dwOldBatchLimit > 0)
        GdiSetBatchLimit(m_dwOldBatchLimit);
#endif

    return TRUE;
}
