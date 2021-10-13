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

    info(ECHO, TEXT("%d FontHeights in use."), m_nMaxHeightIndex);
    info(ECHO, TEXT("%d FontWidths in use."), m_nMaxWidthIndex);
    info(ECHO, TEXT("%d Escapements in use."), m_nMaxEscapementIndex);
    info(ECHO, TEXT("%d Weights in use."), m_nMaxWeightIndex);
    info(ECHO, TEXT("%d Italicies in use."), m_nMaxItalicsIndex);
    info(ECHO, TEXT("%d Underlines in use."), m_nMaxUnderlineIndex);
    info(ECHO, TEXT("%d StrikeOuts in use."), m_nMaxStrikeOutIndex);
    info(ECHO, TEXT("%d CharSets in use."), m_nMaxCharSetIndex);
    info(ECHO, TEXT("%d OutPrecisions in use."), m_nMaxOutPrecisionIndex);
    info(ECHO, TEXT("%d ClipPrecisions in use."), m_nMaxClipPrecisionIndex);
    info(ECHO, TEXT("%d Qualities in use."), m_nMaxQualityIndex);
    info(ECHO, TEXT("%d Pitches in use."), m_nMaxPitchIndex);
    info(ECHO, TEXT("%d Families in use."), m_nMaxFamilyIndex);
    info(ECHO, TEXT("%d FaceNames in use."), m_nMaxFaceNameIndex);

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

