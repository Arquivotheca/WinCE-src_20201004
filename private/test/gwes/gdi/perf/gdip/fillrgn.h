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
#include "bencheng.h"
#include "brush.h"
#include "surface.h"
#include "rgn.h"
#include "dispperfdata.h"

#ifndef FILLRGN_H
#define FILLRGN_H

class CFillRgnTestSuite : public CTestSuite
{
    public:
        CFillRgnTestSuite(CSection * Section) : CTestSuite(Section),
                                            m_Brush(Section), m_Dest(Section), m_Rgn(Section), m_DispPerfData(),
                                            m_hdcDest(NULL), m_hRgn(NULL), m_hBrush(NULL)
                                            { info(DETAIL, TEXT("In CFillRgnTestSuite overloaded constructor.")); }
         ~CFillRgnTestSuite() { info(DETAIL, TEXT("In CFillRgnTestSuite destructor.")); }

        virtual BOOL Initialize(TestSuiteInfo *);
        virtual BOOL PreRun(TestInfo *);
        virtual BOOL Run();
        virtual BOOL AddPostRunData(TestInfo *);
        virtual BOOL PostRun();
        virtual BOOL Cleanup();

private:
        CFillRgnTestSuite() { info(DETAIL, TEXT("In CFillRgnTestSuite base constructor.")); }

        int m_nIterationCount;
        class CBrush m_Brush;
        class CSurface m_Dest;
        class CRgn m_Rgn;
        class CDispPerfData m_DispPerfData;

        HDC m_hdcDest;
        HRGN m_hRgn;
        HBRUSH m_hBrush;

#ifndef UNDER_CE
        DWORD m_dwOldBatchLimit;
#endif
};
#endif
