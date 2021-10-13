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
#include "helpers.h"
#include "bkmode.h"

#ifndef PEN_H
#define PEN_H

class CPen
{
    public:
        CPen(CSection * Section) : m_SectionList(Section), m_BkMode(Section), m_hPen(NULL),
                                                      m_StockPen(NULL), m_tsPenNameArray(NULL), 
                                                      m_nPenIndex(0), m_nMaxPenIndex(0)
                        { info(DETAIL, TEXT("In CPen overloaded constructor.")); }
        ~CPen() { info(DETAIL, TEXT("In CPen Destructor.")); }
        CPen() {}

        BOOL Initialize(TestSuiteInfo *);
        BOOL PreRun(TestInfo *, HDC hdc);
        BOOL PostRun(HDC hdc);
        BOOL Cleanup();

    private:
        // pens are affected by the BKMode
        class CBkMode m_BkMode;

        // attached brush list, with the name of the brush and the current index
        CSection *m_SectionList;
        HPEN *m_hPen;
        HPEN m_StockPen;
        TSTRING *m_tsPenNameArray;
        int m_nPenIndex, m_nMaxPenIndex;
};

#endif
