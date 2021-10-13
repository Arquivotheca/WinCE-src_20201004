//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "bencheng.h"
#include "otak.h"
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
                                            { g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CTransparentBltTestSuite overloaded constructor.")); }
         ~CTransparentBltTestSuite() { g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CTransparentBltTestSuite destructor.")); }

        virtual BOOL Initialize(TestSuiteInfo *);
        virtual BOOL PreRun(TestInfo *);
        virtual BOOL Run();
        virtual BOOL AddPostRunData(TestInfo *);
        virtual BOOL PostRun();
        virtual BOOL Cleanup();

private:
        CTransparentBltTestSuite() { g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CTransparentBltTestSuite base constructor.")); }

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
