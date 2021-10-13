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
#include "surface.h"
#include "pointcoordinates.h"
#include "rgn.h"
#include "dispperfdata.h"

#ifndef SETPIXEL_H
#define SETPIXEL_H

#define DEFAULT_COLOR_VALUE RGB(0x7f, 0x7f, 0x7f)

class CSetPixelTestSuite : public CTestSuite
{
    public:
        CSetPixelTestSuite(CSection * Section) : CTestSuite(Section), 
                                            m_Dest(Section), m_hdcDest(NULL), m_Coordinates(Section), m_Rgn(Section), m_DispPerfData()
                                            { info(DETAIL, TEXT("In CSetPixelTestSuite overloaded constructor.")); }
         ~CSetPixelTestSuite() { info(DETAIL, TEXT("In CSetPixelTestSuite destructor.")); }

        virtual BOOL Initialize(TestSuiteInfo *);
        virtual BOOL PreRun(TestInfo *);
        virtual BOOL Run();
        virtual BOOL AddPostRunData(TestInfo *);
        virtual BOOL PostRun();
        virtual BOOL Cleanup();

private:
        CSetPixelTestSuite() { info(DETAIL, TEXT("In CSetPixelTestSuite base constructor.")); }

        int m_nIterationCount;
        class CSurface m_Dest;
        class CPointCoordinates m_Coordinates;
        class CRgn m_Rgn;
        class CDispPerfData m_DispPerfData;

        HDC m_hdcDest;
        POINT m_ptPointInUse;
        COLORREF m_crInUse;

#ifndef UNDER_CE
        DWORD m_dwOldBatchLimit;
#endif

};
#endif
