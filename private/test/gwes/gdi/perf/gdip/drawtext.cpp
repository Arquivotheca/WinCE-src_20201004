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
#include "DrawText.h"

BOOL
CDrawTextTestSuite::InitializeOptions(TestSuiteInfo * tsi)
{
    BOOL bRval = TRUE;

    // Fill in the test suite's fields that are varied with each run.
    tsi->tsFieldDescription.push_back(TEXT("DT_BOTTOM"));
    tsi->tsFieldDescription.push_back(TEXT("DT_CALCRECT"));
    tsi->tsFieldDescription.push_back(TEXT("DT_CENTER"));
    tsi->tsFieldDescription.push_back(TEXT("DT_EXPANDTABS"));
    tsi->tsFieldDescription.push_back(TEXT("DT_INTERNAL"));
    tsi->tsFieldDescription.push_back(TEXT("DT_LEFT"));
    tsi->tsFieldDescription.push_back(TEXT("DT_NOCLIP"));
    tsi->tsFieldDescription.push_back(TEXT("DT_NOPREFIX"));
    tsi->tsFieldDescription.push_back(TEXT("DT_RIGHT"));
    tsi->tsFieldDescription.push_back(TEXT("DT_SINGLELINE"));
    tsi->tsFieldDescription.push_back(TEXT("DT_TABSTOP"));
    tsi->tsFieldDescription.push_back(TEXT("DT_TOP"));
    tsi->tsFieldDescription.push_back(TEXT("DT_VCENTER"));
    tsi->tsFieldDescription.push_back(TEXT("DT_WORDBREAK"));

    bRval &= AllocDWArray(TEXT("DT_BOTTOM"), 0, &m_dwBottom, &m_nMaxBottomIndex, m_SectionList, 10);
    bRval &= AllocDWArray(TEXT("DT_CALCRECT"), 0, &m_dwCalcrect, &m_nMaxCalcrectIndex, m_SectionList, 10);
    bRval &= AllocDWArray(TEXT("DT_CENTER"), 0, &m_dwCenter, &m_nMaxCenterIndex, m_SectionList, 10);
    bRval &= AllocDWArray(TEXT("DT_EXPANDTABS"), 0, &m_dwExpandtabs, &m_nMaxExpandtabsIndex, m_SectionList, 10);
    bRval &= AllocDWArray(TEXT("DT_INTERNAL"), 0, &m_dwInternal, &m_nMaxInternalIndex, m_SectionList, 10);
    bRval &= AllocDWArray(TEXT("DT_LEFT"), 0, &m_dwLeft, &m_nMaxLeftIndex, m_SectionList, 10);
    bRval &= AllocDWArray(TEXT("DT_NOCLIP"), 0, &m_dwNoclip, &m_nMaxNoclipIndex, m_SectionList, 10);
    bRval &= AllocDWArray(TEXT("DT_NOPREFIX"), 0, &m_dwNoprefix, &m_nMaxNoprefixIndex, m_SectionList, 10);
    bRval &= AllocDWArray(TEXT("DT_RIGHT"), 0, &m_dwRight, &m_nMaxRightIndex, m_SectionList, 10);
    bRval &= AllocDWArray(TEXT("DT_SINGLELINE"), 0, &m_dwSingleline, &m_nMaxSinglelineIndex, m_SectionList, 10);
    bRval &= AllocDWArray(TEXT("DT_TABSTOP"), 0, &m_dwTabstop, &m_nMaxTabstopIndex, m_SectionList, 10);
    bRval &= AllocDWArray(TEXT("DT_TOP Top"), 0, &m_dwTop, &m_nMaxTopIndex, m_SectionList, 10);
    bRval &= AllocDWArray(TEXT("DT_VCENTER"), 0, &m_dwVcenter, &m_nMaxVcenterIndex, m_SectionList, 10);
    bRval &= AllocDWArray(TEXT("DT_WORDBREAK"), 0, &m_dwWordbreak, &m_nMaxWordbreakIndex, m_SectionList, 10);

    info(ECHO, TEXT("%d bottoms in use."), m_nMaxBottomIndex);
    info(ECHO, TEXT("%d calcrects in use."), m_nMaxCalcrectIndex);
    info(ECHO, TEXT("%d centers in use."), m_nMaxCenterIndex);
    info(ECHO, TEXT("%d expandtabs in use."), m_nMaxExpandtabsIndex);
    info(ECHO, TEXT("%d internalindexes in use."), m_nMaxInternalIndex);
    info(ECHO, TEXT("%d lefts in use."), m_nMaxLeftIndex);
    info(ECHO, TEXT("%d noclips in use."), m_nMaxNoclipIndex);
    info(ECHO, TEXT("%d noprefixes in use."), m_nMaxNoprefixIndex);
    info(ECHO, TEXT("%d rightsin use."), m_nMaxRightIndex);
    info(ECHO, TEXT("%d singlelines in use."), m_nMaxSinglelineIndex);
    info(ECHO, TEXT("%d tabstops use."), m_nMaxTabstopIndex);
    info(ECHO, TEXT("%d tops in use."), m_nMaxTopIndex);
    info(ECHO, TEXT("%d vcenters in use."), m_nMaxVcenterIndex);
    info(ECHO, TEXT("%d wordbreaks in use."), m_nMaxWordbreakIndex);

    m_nBottomIndex = 0;
    m_nCalcrectIndex = 0;
    m_nCenterIndex = 0;
    m_nExpandtabsIndex = 0;
    m_nInternalIndex = 0;
    m_nLeftIndex = 0;
    m_nNoclipIndex = 0;
    m_nNoprefixIndex = 0;
    m_nRightIndex = 0;
    m_nSinglelineIndex = 0;
    m_nTabstopIndex = 0;
    m_nTopIndex = 0;
    m_nVcenterIndex = 0;
    m_nWordbreakIndex = 0;

    return bRval;
}

BOOL
CDrawTextTestSuite::Initialize(TestSuiteInfo * tsi)
{
    info(ECHO, TEXT("In CDrawTextTestSuite::Initialize"));
    BOOL bRval = TRUE;
    
#ifndef UNDER_CE
    m_dwOldBatchLimit = GdiSetBatchLimit(1);
    if(0 == m_dwOldBatchLimit)
        info(FAIL, TEXT("Failed to disable GDI call batching."));
#endif

    tsi->tsTestName = TEXT("DrawText");

    // initialize everything, if anything fails return failure.
    // cleanup is called whether or not there is a failure, which will deallocate 
    // anything that was allocated if there was a failure.

    // the order is string, clipped, opaque, dest, clip rect, position, region, font.
    if(bRval && (bRval = bRval && InitializeOptions(tsi)))
        if(bRval = bRval && m_String.Initialize(tsi))
            if(bRval = bRval && m_Dest.Initialize(tsi, TEXT("Dest")))
                if(bRval = bRval && m_Coordinates.Initialize(tsi, TEXT("FormatCoordinates")))
                    if(bRval = bRval && m_Rgn.Initialize(tsi))
                        if(bRval = bRval && m_Font.Initialize(tsi))
                            bRval = bRval && m_DispPerfData.Initialize(tsi);

    return bRval;
}

BOOL
CDrawTextTestSuite::PreRun(TestInfo *tiRunInfo)
{
    info(DETAIL, TEXT("In CDrawTextTestSuite::PreRun"));

    tiRunInfo->Descriptions.push_back(itos(m_dwBottom[m_nBottomIndex]));
    tiRunInfo->Descriptions.push_back(itos(m_dwCalcrect[m_nCalcrectIndex]));
    tiRunInfo->Descriptions.push_back(itos(m_dwCenter[m_nCenterIndex]));
    tiRunInfo->Descriptions.push_back(itos(m_dwExpandtabs[m_nExpandtabsIndex]));
    tiRunInfo->Descriptions.push_back(itos(m_dwInternal[m_nInternalIndex]));
    tiRunInfo->Descriptions.push_back(itos(m_dwLeft[m_nLeftIndex]));
    tiRunInfo->Descriptions.push_back(itos(m_dwNoclip[m_nNoclipIndex]));
    tiRunInfo->Descriptions.push_back(itos(m_dwNoprefix[m_nNoprefixIndex]));
    tiRunInfo->Descriptions.push_back(itos(m_dwRight[m_nRightIndex]));
    tiRunInfo->Descriptions.push_back(itos(m_dwSingleline[m_nSinglelineIndex]));
    tiRunInfo->Descriptions.push_back(itos(m_dwTabstop[m_nTabstopIndex]));
    tiRunInfo->Descriptions.push_back(itos(m_dwTop[m_nTopIndex]));
    tiRunInfo->Descriptions.push_back(itos(m_dwVcenter[m_nVcenterIndex]));
    tiRunInfo->Descriptions.push_back(itos(m_dwWordbreak[m_nWordbreakIndex]));

    // add clipped info
    m_nFormat = 0;

    if(m_dwBottom[m_nBottomIndex])
        m_nFormat |= DT_BOTTOM;

    if(m_dwCalcrect[m_nCalcrectIndex])
        m_nFormat |= DT_CALCRECT;

    if(m_dwCenter[m_nCenterIndex])
        m_nFormat |= DT_CENTER;

    if(m_dwExpandtabs[m_nExpandtabsIndex])
        m_nFormat |= DT_EXPANDTABS;

    if(m_dwInternal[m_nInternalIndex])
        m_nFormat |= DT_INTERNAL;

    if(m_dwLeft[m_nLeftIndex])
        m_nFormat |= DT_LEFT;

    if(m_dwNoclip[m_nNoclipIndex])
        m_nFormat |= DT_NOCLIP;

    if(m_dwNoprefix[m_nNoprefixIndex])
        m_nFormat |= DT_NOPREFIX;

    if(m_dwRight[m_nRightIndex])
        m_nFormat |= DT_RIGHT;

    if(m_dwSingleline[m_nSinglelineIndex])
        m_nFormat |= DT_SINGLELINE;

    if(m_dwTabstop[m_nTabstopIndex])
        m_nFormat |= DT_TABSTOP;

    if(m_dwTop[m_nTopIndex])
        m_nFormat |= DT_TOP;

    if(m_dwVcenter[m_nVcenterIndex])
        m_nFormat |= DT_VCENTER;

    if(m_dwWordbreak[m_nWordbreakIndex])
        m_nFormat |= DT_WORDBREAK;

    m_String.PreRun(tiRunInfo);
    m_tcStringInUse = m_String.GetString();

    m_Dest.PreRun(tiRunInfo);
    m_hdcDest = m_Dest.GetSurface();

    m_Coordinates.PreRun(tiRunInfo);
    m_rcRectInUse = m_Coordinates.GetCoordinates();

    m_Rgn.PreRun(tiRunInfo, m_hdcDest);

    m_Font.PreRun(tiRunInfo, m_hdcDest);

    m_DispPerfData.PreRun(tiRunInfo);

    return TRUE;
}

BOOL
CDrawTextTestSuite::Run()
{
    // logging here can cause timing issues.
    //info(DETAIL, TEXT("In CDrawTextTestSuite::Run"));
    return DrawText(m_hdcDest, m_tcStringInUse, -1, &m_rcRectInUse, m_nFormat);
}

BOOL
CDrawTextTestSuite::AddPostRunData(TestInfo *tiRunInfo)
{
    info(DETAIL, TEXT("In CDrawTextTestSuite::AddPostRunData"));

    return m_DispPerfData.AddPostRunData(tiRunInfo);
}

BOOL
CDrawTextTestSuite::PostRun()
{
    info(DETAIL, TEXT("In CDrawTextTestSuite::PostRun"));

    m_nIterationCount++;

    // iterate to the next options
    m_nBottomIndex++;
    if(m_nBottomIndex >= m_nMaxBottomIndex)
    {
     m_nBottomIndex = 0;
     m_nCalcrectIndex++;
     if(m_nCalcrectIndex >= m_nMaxCalcrectIndex)
     {
      m_nCalcrectIndex = 0;
      m_nCenterIndex++;
      if(m_nCenterIndex >= m_nMaxCenterIndex)
      {
       m_nCenterIndex = 0;
       m_nExpandtabsIndex++;
       if(m_nExpandtabsIndex >= m_nMaxExpandtabsIndex)
       {
        m_nExpandtabsIndex = 0;
        m_nInternalIndex++;
        if(m_nInternalIndex >= m_nMaxInternalIndex)
        {
         m_nInternalIndex = 0;
         m_nLeftIndex++;
         if(m_nLeftIndex >= m_nMaxLeftIndex)
         {
          m_nLeftIndex = 0;
          m_nNoclipIndex++;
          if(m_nNoclipIndex >= m_nMaxNoclipIndex)
          {
           m_nNoclipIndex = 0;
           m_nNoprefixIndex++;
           if(m_nNoprefixIndex >= m_nMaxNoprefixIndex)
           {
            m_nNoprefixIndex = 0;
            m_nRightIndex++;
            if(m_nRightIndex >= m_nMaxRightIndex)
            {
             m_nRightIndex = 0;
             m_nSinglelineIndex++;
             if(m_nSinglelineIndex >= m_nMaxSinglelineIndex)
             {
              m_nSinglelineIndex = 0;
              m_nTabstopIndex++;
              if(m_nTabstopIndex >= m_nMaxTabstopIndex)
              {
               m_nTabstopIndex = 0;
               m_nTopIndex++;
               if(m_nTopIndex >= m_nMaxTopIndex)
               {
                m_nTopIndex = 0;
                m_nVcenterIndex++;
                if(m_nVcenterIndex >= m_nMaxVcenterIndex)
                {
                 m_nVcenterIndex = 0;
                 m_nWordbreakIndex++;
                 if(m_nWordbreakIndex >= m_nMaxWordbreakIndex)
                 {
                  m_nWordbreakIndex = 0;
                  // iterate to the next options
                  if(m_String.PostRun())
                   if(m_Dest.PostRun())
                    if(m_Coordinates.PostRun())
                     if(m_Rgn.PostRun())
                      if(m_Font.PostRun(m_hdcDest))
                       return FALSE;
                 }
                }
               }
              }
             }
            }
           }
          }
         }
        }
       }
      }
     }
    }

    return TRUE;
}

BOOL
CDrawTextTestSuite::Cleanup()
{
    info(ECHO, TEXT("In CDrawTextTestSuite::Cleanup"));

    delete[] m_dwBottom;
    delete[] m_dwCalcrect;
    delete[] m_dwCenter;
    delete[] m_dwExpandtabs;
    delete[] m_dwInternal;
    delete[] m_dwLeft;
    delete[] m_dwNoclip;
    delete[] m_dwNoprefix;
    delete[] m_dwRight;
    delete[] m_dwSingleline;
    delete[] m_dwTabstop;
    delete[] m_dwTop;
    delete[] m_dwVcenter;
    delete[] m_dwWordbreak;

    // clean up all of the test options
    m_String.Cleanup();
    m_Dest.Cleanup();
    m_Coordinates.Cleanup();
    m_Rgn.Cleanup();
    m_Font.Cleanup();
    m_DispPerfData.Cleanup();
#ifndef UNDER_CE
    if(m_dwOldBatchLimit > 0)
        GdiSetBatchLimit(m_dwOldBatchLimit);
#endif

    return TRUE;
}
