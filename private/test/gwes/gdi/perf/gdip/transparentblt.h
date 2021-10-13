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
#include "handles.h"
#include "surface.h"
#include "stretchcoordinates.h"
#include "rgn.h"
#include "dispperfdata.h"

#ifndef TRANSPARENTBLT_H
#define TRANSPARENTBLT_H

class CTransparentBltTestSuite : public CTestSuite
{
    public:
        CTransparentBltTestSuite(CSection * Section) : CTestSuite(Section),
                                            m_Dest(Section), m_Source(Section), m_StretchCoordinates(Section), 
                                            m_Rgn(Section), m_DispPerfData(),
                                            m_hdcDest(NULL), m_hdcSource(NULL), m_crTransparentColor(0)
                                            { info(DETAIL, TEXT("In CTransparentBltTestSuite overloaded constructor.")); }
         ~CTransparentBltTestSuite() { info(DETAIL, TEXT("In CTransparentBltTestSuite destructor.")); }

        virtual BOOL Initialize(TestSuiteInfo *);
        virtual BOOL PreRun(TestInfo *);
        virtual BOOL Run();
        virtual BOOL AddPostRunData(TestInfo *);
        virtual BOOL PostRun();
        virtual BOOL Cleanup();

private:
        CTransparentBltTestSuite() { info(DETAIL, TEXT("In CTransparentBltTestSuite base constructor.")); }

        int m_nIterationCount;

        class CSurface m_Dest;
        class CSurface m_Source;
        class CStretchCoordinates m_StretchCoordinates;
        class CRgn m_Rgn;
        class CDispPerfData m_DispPerfData;

        HDC m_hdcDest;
        HDC m_hdcSource;
        struct StretchCoordinates m_sCoordinateInUse;

        COLORREF m_crTransparentColor;

#ifndef UNDER_CE
        DWORD m_dwOldBatchLimit;
#endif

};
#endif
