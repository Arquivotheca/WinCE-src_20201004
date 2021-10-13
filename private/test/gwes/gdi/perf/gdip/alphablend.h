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
#include "stretchcoordinates.h"
#include "rgn.h"
#include "dispperfdata.h"

#ifndef ALHPABLEND_H
#define ALPHABLEND_H

typedef BOOL (WINAPI * PFNALPHABLEND)(HDC tdcDest, int nXOriginDest, int nYOriginDest, int nWidthDest, int nHeightDest, HDC tdcSrc, int nXOriginSrc, int nYOriginSrc, int nWidthSrc, int nHeightSrc, BLENDFUNCTION blendFunction);

class CAlphaBlendTestSuite : public CTestSuite
{
    public:
        CAlphaBlendTestSuite(CSection * Section) : CTestSuite(Section), m_pfnAlphaBlend(NULL),
                                            m_Dest(Section), m_Source(Section), m_StretchCoordinates(Section), m_Rgn(Section), m_DispPerfData(),
                                            m_hdcDest(NULL), m_hdcSource(NULL), m_tsBlendFunctionName(NULL),
                                            m_BlendFunction(NULL), m_nBlendFunctionIndex(0), m_nMaxBlendFunctionIndex(0)
                                            { info(DETAIL, TEXT("In CAlphaBlendTestSuite overloaded constructor.")); }
         ~CAlphaBlendTestSuite() { info(DETAIL, TEXT("In CAlphaBlendTestSuite destructor.")); }

        virtual BOOL Initialize(TestSuiteInfo *);
        virtual BOOL PreRun(TestInfo *);
        virtual BOOL Run();
        virtual BOOL AddPostRunData(TestInfo *);
        virtual BOOL PostRun();
        virtual BOOL Cleanup();

private:
        CAlphaBlendTestSuite() { info(DETAIL, TEXT("In CAlphaBlendTestSuite base constructor.")); }

        HINSTANCE m_hinstCoreDLL;
        PFNALPHABLEND m_pfnAlphaBlend;

        int m_nIterationCount;
        class CSurface m_Dest;
        class CSurface m_Source;
        class CStretchCoordinates m_StretchCoordinates;
        class CRgn m_Rgn;
        class CDispPerfData m_DispPerfData;

        HDC m_hdcDest;
        HDC m_hdcSource;
        struct StretchCoordinates m_sCoordinateInUse;

        BLENDFUNCTION * m_BlendFunction;
        TSTRING * m_tsBlendFunctionName;
        int m_nBlendFunctionIndex, m_nMaxBlendFunctionIndex;

#ifndef UNDER_CE
        DWORD m_dwOldBatchLimit;
#endif

        BOOL InitializeBlendFunction(TestSuiteInfo *);
        BOOL InitializeFunctionPointer();

};
#endif
