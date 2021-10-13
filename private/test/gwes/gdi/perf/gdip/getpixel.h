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

#ifndef GETPIXEL_H
#define GETPIXEL_H
class CGetPixelTestSuite : public CTestSuite
{
    public:
        CGetPixelTestSuite(CSection * Section) : CTestSuite(Section), 
                                            m_Dest(Section), m_hdcDest(NULL), m_Coordinates(Section), m_Rgn(Section), m_DispPerfData()
                                            { info(DETAIL, TEXT("In CGetPixelTestSuite overloaded constructor.")); }
         ~CGetPixelTestSuite() { info(DETAIL, TEXT("In CGetPixelTestSuite destructor.")); }

        virtual BOOL Initialize(TestSuiteInfo *);
        virtual BOOL PreRun(TestInfo *);
        virtual BOOL Run();
        virtual BOOL AddPostRunData(TestInfo *);
        virtual BOOL PostRun();
        virtual BOOL Cleanup();

private:
        CGetPixelTestSuite() { info(DETAIL, TEXT("In CGetPixelTestSuite base constructor.")); }

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
