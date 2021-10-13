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
#include "tuxmain.h"
#include "handles.h"
#include "bencheng.h"
#include "helpers.h"
#include "bkmode.h"

#ifndef FONT_H
#define FONT_H

class CFont
{
    public:
        CFont(CSection * Section) : m_SectionList(Section), m_BkMode(Section),
                                                    m_dwHeight(NULL), m_nHeightIndex(0), m_nMaxHeightIndex(0),
                                                    m_dwWidth(NULL), m_nWidthIndex(0), m_nMaxWidthIndex(0),
                                                    m_dwEscapement(NULL), m_nEscapementIndex(0), m_nMaxEscapementIndex(0),
                                                    m_tsWeight(NULL), m_nWeightIndex(0), m_nMaxWeightIndex(0),
                                                    m_dwItalics(NULL), m_nItalicsIndex(0), m_nMaxItalicsIndex(0),
                                                    m_dwUnderline(NULL), m_nUnderlineIndex(0), m_nMaxUnderlineIndex(0),
                                                    m_dwStrikeOut(NULL), m_nStrikeOutIndex(0), m_nMaxStrikeOutIndex(0),
                                                    m_tsCharSet(NULL), m_nCharSetIndex(0), m_nMaxCharSetIndex(0),
                                                    m_tsOutPrecision(NULL), m_nOutPrecisionIndex(0), m_nMaxOutPrecisionIndex(0),
                                                    m_tsClipPrecision(NULL), m_nClipPrecisionIndex(0), m_nMaxClipPrecisionIndex(0),
                                                    m_tsQuality(NULL), m_nQualityIndex(0), m_nMaxQualityIndex(0),
                                                    m_tsPitch(NULL), m_nPitchIndex(0), m_nMaxPitchIndex(0),
                                                    m_tsFamily(NULL), m_nFamilyIndex(0), m_nMaxFamilyIndex(0),
                                                    m_tsFaceName(NULL), m_nFaceNameIndex(0), m_nMaxFaceNameIndex(0)
                        { info(DETAIL, TEXT("In CFont overloaded constructor.")); }
        ~CFont() { info(DETAIL, TEXT("In CFont Destructor.")); }
        CFont() {}

        BOOL Initialize(TestSuiteInfo *);
        BOOL PreRun(TestInfo *, HDC hdc);
        BOOL PostRun(HDC hdc);
        BOOL Cleanup();

    private:

        class CBkMode m_BkMode;

        // attached brush list, with the name of the brush and the current index
        CSection *m_SectionList;
        HFONT m_hFont, m_hStockFont;

        DWORD *m_dwHeight;
        int m_nHeightIndex, m_nMaxHeightIndex;

        DWORD *m_dwWidth;
        int m_nWidthIndex, m_nMaxWidthIndex;

        DWORD *m_dwEscapement;
        int m_nEscapementIndex, m_nMaxEscapementIndex;

        TSTRING *m_tsWeight;
        int m_nWeightIndex, m_nMaxWeightIndex;

        DWORD *m_dwItalics;
        int m_nItalicsIndex, m_nMaxItalicsIndex;

        DWORD *m_dwUnderline;
        int m_nUnderlineIndex, m_nMaxUnderlineIndex;

        DWORD *m_dwStrikeOut;
        int m_nStrikeOutIndex, m_nMaxStrikeOutIndex;

        TSTRING *m_tsCharSet;
        int m_nCharSetIndex, m_nMaxCharSetIndex;

        TSTRING *m_tsOutPrecision;
        int m_nOutPrecisionIndex, m_nMaxOutPrecisionIndex;

        TSTRING *m_tsClipPrecision;
        int m_nClipPrecisionIndex, m_nMaxClipPrecisionIndex;

        TSTRING *m_tsQuality;
        int m_nQualityIndex, m_nMaxQualityIndex;

        TSTRING *m_tsPitch;
        int m_nPitchIndex, m_nMaxPitchIndex;

        TSTRING *m_tsFamily;
        int m_nFamilyIndex, m_nMaxFamilyIndex;

        TSTRING *m_tsFaceName;
        int m_nFaceNameIndex, m_nMaxFaceNameIndex;
};

#endif
