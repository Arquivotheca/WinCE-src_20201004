//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "font.h"

BOOL
CFont::Initialize(TestSuiteInfo *tsi)
{
    BOOL bRval = TRUE;

    bRval &= m_BkMode.Initialize(tsi);

    tsi->tsFieldDescription.push_back(TEXT("Font Height"));
    tsi->tsFieldDescription.push_back(TEXT("Font Width"));
    tsi->tsFieldDescription.push_back(TEXT("Escapement"));
    tsi->tsFieldDescription.push_back(TEXT("Weight"));
    tsi->tsFieldDescription.push_back(TEXT("Italics"));
    tsi->tsFieldDescription.push_back(TEXT("Underline"));
    tsi->tsFieldDescription.push_back(TEXT("StrikeOut"));
    tsi->tsFieldDescription.push_back(TEXT("CharSet"));
    tsi->tsFieldDescription.push_back(TEXT("OutPrecision"));
    tsi->tsFieldDescription.push_back(TEXT("ClipPrecision"));
    tsi->tsFieldDescription.push_back(TEXT("Quality"));
    tsi->tsFieldDescription.push_back(TEXT("Pitch"));
    tsi->tsFieldDescription.push_back(TEXT("Family"));
    tsi->tsFieldDescription.push_back(TEXT("FaceName"));


    bRval &= AllocDWArray(TEXT("FontHeight"), 0, &m_dwHeight, &m_nMaxHeightIndex, m_SectionList, 10);
    bRval &= AllocDWArray(TEXT("FontWidth"), 0, &m_dwWidth, &m_nMaxWidthIndex, m_SectionList, 10);
    bRval &= AllocDWArray(TEXT("Escapement"), 0, &m_dwEscapement, &m_nMaxEscapementIndex, m_SectionList, 10);
    bRval &= AllocTSArray(TEXT("Weight"), TEXT("FW_NORMAL"), &m_tsWeight, &m_nMaxWeightIndex, m_SectionList);
    bRval &= AllocDWArray(TEXT("Italics"), 0, &m_dwItalics, &m_nMaxItalicsIndex, m_SectionList, 10);
    bRval &= AllocDWArray(TEXT("Underline"), 0, &m_dwUnderline, &m_nMaxUnderlineIndex, m_SectionList, 10);
    bRval &= AllocDWArray(TEXT("StrikeOut"), 0, &m_dwStrikeOut, &m_nMaxStrikeOutIndex, m_SectionList, 10);
    bRval &= AllocTSArray(TEXT("CharSet"), TEXT("DEFAULT_CHARSET"), &m_tsCharSet, &m_nMaxCharSetIndex, m_SectionList);
    bRval &= AllocTSArray(TEXT("OutPrecision"), TEXT("OUT_DEFAULT_PRECIS"), &m_tsOutPrecision, &m_nMaxOutPrecisionIndex, m_SectionList);
    bRval &= AllocTSArray(TEXT("ClipPrecision"), TEXT("CLIP_DEFAULT_PRECIS"), &m_tsClipPrecision, &m_nMaxClipPrecisionIndex, m_SectionList);
    bRval &= AllocTSArray(TEXT("Quality"), TEXT("DEFAULT_QUALITY"), &m_tsQuality, &m_nMaxQualityIndex, m_SectionList);
    bRval &= AllocTSArray(TEXT("Pitch"), TEXT("DEFAULT_PITCH"), &m_tsPitch, &m_nMaxPitchIndex, m_SectionList);
    bRval &= AllocTSArray(TEXT("Family"), TEXT("FF_DONTCARE"), &m_tsFamily, &m_nMaxFamilyIndex, m_SectionList);
    bRval &= AllocTSArray(TEXT("FaceName"), TEXT(""), &m_tsFaceName, &m_nMaxFaceNameIndex, m_SectionList);

    m_nHeightIndex = 0;
    m_nWidthIndex = 0;
    m_nEscapementIndex = 0;
    m_nWeightIndex = 0;
    m_nItalicsIndex = 0;
    m_nUnderlineIndex = 0;
    m_nStrikeOutIndex = 0;
    m_nCharSetIndex = 0;
    m_nOutPrecisionIndex = 0;
    m_nClipPrecisionIndex = 0;
    m_nQualityIndex = 0;
    m_nPitchIndex = 0;
    m_nFamilyIndex = 0;
    m_nFaceNameIndex = 0;

    g_pCOtakLog->Log(OTAK_DETAIL, TEXT("%d FontHeights in use."), m_nMaxHeightIndex);
    g_pCOtakLog->Log(OTAK_DETAIL, TEXT("%d FontWidths in use."), m_nMaxWidthIndex);
    g_pCOtakLog->Log(OTAK_DETAIL, TEXT("%d Escapements in use."), m_nMaxEscapementIndex);
    g_pCOtakLog->Log(OTAK_DETAIL, TEXT("%d Weights in use."), m_nMaxWeightIndex);
    g_pCOtakLog->Log(OTAK_DETAIL, TEXT("%d Italicies in use."), m_nMaxItalicsIndex);
    g_pCOtakLog->Log(OTAK_DETAIL, TEXT("%d Underlines in use."), m_nMaxUnderlineIndex);
    g_pCOtakLog->Log(OTAK_DETAIL, TEXT("%d StrikeOuts in use."), m_nMaxStrikeOutIndex);
    g_pCOtakLog->Log(OTAK_DETAIL, TEXT("%d CharSets in use."), m_nMaxCharSetIndex);
    g_pCOtakLog->Log(OTAK_DETAIL, TEXT("%d OutPrecisions in use."), m_nMaxOutPrecisionIndex);
    g_pCOtakLog->Log(OTAK_DETAIL, TEXT("%d ClipPrecisions in use."), m_nMaxClipPrecisionIndex);
    g_pCOtakLog->Log(OTAK_DETAIL, TEXT("%d Qualities in use."), m_nMaxQualityIndex);
    g_pCOtakLog->Log(OTAK_DETAIL, TEXT("%d Pitches in use."), m_nMaxPitchIndex);
    g_pCOtakLog->Log(OTAK_DETAIL, TEXT("%d Families in use."), m_nMaxFamilyIndex);
    g_pCOtakLog->Log(OTAK_DETAIL, TEXT("%d FaceNames in use."), m_nMaxFaceNameIndex);

    return bRval;
}

BOOL
CFont::PreRun(TestInfo *tiRunInfo, HDC hdc)
{
    m_BkMode.PreRun(tiRunInfo, hdc);

    tiRunInfo->Descriptions.push_back(itos(m_dwHeight[m_nHeightIndex]));
    tiRunInfo->Descriptions.push_back(itos(m_dwWidth[m_nWidthIndex]));
    tiRunInfo->Descriptions.push_back(itos(m_dwEscapement[m_nEscapementIndex]));
    tiRunInfo->Descriptions.push_back(m_tsWeight[m_nWeightIndex]);
    tiRunInfo->Descriptions.push_back(itos(m_dwItalics[m_nItalicsIndex]));
    tiRunInfo->Descriptions.push_back(itos(m_dwUnderline[m_nUnderlineIndex]));
    tiRunInfo->Descriptions.push_back(itos(m_dwStrikeOut[m_nStrikeOutIndex]));
    tiRunInfo->Descriptions.push_back(m_tsCharSet[m_nCharSetIndex]);
    tiRunInfo->Descriptions.push_back(m_tsOutPrecision[m_nOutPrecisionIndex]);
    tiRunInfo->Descriptions.push_back(m_tsClipPrecision[m_nClipPrecisionIndex]);
    tiRunInfo->Descriptions.push_back(m_tsQuality[m_nQualityIndex]);
    tiRunInfo->Descriptions.push_back(m_tsPitch[m_nPitchIndex]);
    tiRunInfo->Descriptions.push_back(m_tsFamily[m_nFamilyIndex]);
    tiRunInfo->Descriptions.push_back(m_tsFaceName[m_nFaceNameIndex]);

    m_hFont = myCreateFont(m_dwHeight[m_nHeightIndex], m_dwWidth[m_nWidthIndex],
                                          m_dwEscapement[m_nEscapementIndex], m_tsWeight[m_nWeightIndex],
                                          m_dwItalics[m_nItalicsIndex], m_dwUnderline[m_nUnderlineIndex],
                                          m_dwStrikeOut[m_nStrikeOutIndex], m_tsCharSet[m_nCharSetIndex],
                                          m_tsOutPrecision[m_nOutPrecisionIndex], m_tsClipPrecision[m_nClipPrecisionIndex],
                                          m_tsQuality[m_nQualityIndex], m_tsPitch[m_nPitchIndex],
                                          m_tsFamily[m_nFamilyIndex], m_tsFaceName[m_nFaceNameIndex]);

    m_hStockFont = (HFONT) SelectObject(hdc, m_hFont);

    return (NULL != m_hFont && NULL != m_hStockFont);
}

BOOL
CFont::PostRun(HDC hdc)
{
    BOOL bRVal = FALSE;

    SelectObject(hdc, m_hStockFont);
    DeleteObject(m_hFont);

    if(m_BkMode.PostRun())
    {
     m_nHeightIndex++;
     if(m_nHeightIndex >= m_nMaxHeightIndex)
     {
      m_nHeightIndex = 0;
      m_nWidthIndex++;
      if(m_nWidthIndex >= m_nMaxWidthIndex)
      {
       m_nWidthIndex = 0;
       m_nEscapementIndex++;
       if(m_nEscapementIndex >= m_nMaxEscapementIndex)
       {
        m_nEscapementIndex = 0;
        m_nWeightIndex++;
        if(m_nWeightIndex >= m_nMaxWeightIndex)
        {
         m_nWeightIndex = 0;
         m_nItalicsIndex++;
         if(m_nItalicsIndex >= m_nMaxItalicsIndex)
         {
          m_nItalicsIndex = 0;
          m_nUnderlineIndex++;
          if(m_nUnderlineIndex >= m_nMaxUnderlineIndex)
          {
           m_nUnderlineIndex = 0;
           m_nStrikeOutIndex++;
           if(m_nStrikeOutIndex >= m_nMaxStrikeOutIndex)
           {
            m_nStrikeOutIndex = 0;
            m_nCharSetIndex++;
            if(m_nCharSetIndex >= m_nMaxCharSetIndex)
            {
             m_nCharSetIndex = 0;
             m_nOutPrecisionIndex++;
             if(m_nOutPrecisionIndex >= m_nMaxOutPrecisionIndex)
             {
              m_nOutPrecisionIndex = 0;
              m_nClipPrecisionIndex++;
              if(m_nClipPrecisionIndex >= m_nMaxClipPrecisionIndex)
              {
               m_nClipPrecisionIndex = 0;
               m_nQualityIndex++;
               if(m_nQualityIndex >= m_nMaxQualityIndex)
               {
                m_nQualityIndex = 0;
                m_nPitchIndex++;
                if(m_nPitchIndex >= m_nMaxPitchIndex)
                {
                 m_nPitchIndex = 0;
                 m_nFamilyIndex++;
                 if(m_nFamilyIndex >= m_nMaxFamilyIndex)
                 {
                  m_nFamilyIndex = 0;
                  m_nFaceNameIndex++;
                  if(m_nFaceNameIndex >= m_nMaxFaceNameIndex)
                  {
                   m_nFaceNameIndex = 0;
                   bRVal = TRUE;
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
    }
    return bRVal;
}

BOOL
CFont::Cleanup()
{
    m_BkMode.Cleanup();

    delete[] m_dwHeight;
    delete[] m_dwWidth;
    delete[] m_dwEscapement;
    delete[] m_tsWeight;
    delete[] m_dwItalics;
    delete[] m_dwUnderline;
    delete[] m_dwStrikeOut;
    delete[] m_tsCharSet;
    delete[] m_tsOutPrecision;
    delete[] m_tsClipPrecision;
    delete[] m_tsQuality;
    delete[] m_tsPitch;
    delete[] m_tsFamily;
    delete[] m_tsFaceName;

    return TRUE;
}

