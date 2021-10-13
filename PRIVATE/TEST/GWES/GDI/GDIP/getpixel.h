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
#include "surface.h"
#include "pointcoordinates.h"
#include "rgn.h"
#include "dispperfdata.h"

#ifndef GETPIXEL_H
#define GETPIXEL_H
class CGetPixelTestSuite : public CTestSuite
{
    public:
        CGetPixelTestSuite(CSection * Section) : CTestSuite(Section), 
                                            m_Dest(Section), m_hdcDest(NULL), m_Coordinates(Section), m_Rgn(Section), m_DispPerfData()
                                            { g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CGetPixelTestSuite overloaded constructor.")); }
         ~CGetPixelTestSuite() { g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CGetPixelTestSuite destructor.")); }

        virtual BOOL Initialize(TestSuiteInfo *);
        virtual BOOL PreRun(TestInfo *);
        virtual BOOL Run();
        virtual BOOL AddPostRunData(TestInfo *);
        virtual BOOL PostRun();
        virtual BOOL Cleanup();

private:
        CGetPixelTestSuite() { g_pCOtakLog->Log(OTAK_VERBOSE, TEXT("In CGetPixelTestSuite base constructor.")); }

        int m_nIterationCount;
        class CSurface m_Dest;
        class CPointCoordinates m_Coordinates;
        class CRgn m_Rgn;
        class CDispPerfData m_DispPerfData;

        HDC m_hdcDest;
        POINT m_ptPointInUse;

#ifndef UNDER_CE
        DWORD m_dwOldBatchLimit;
#endif

};
#endif
