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
#include "rectcoordinates.h"
#include "pointcoordinates.h"
#include "rgn.h"
#include "font.h"
#include "strings.h"
#include "dispperfdata.h"

#ifndef EXTTEXTOUT_H
#define EXTTEXTOUT_H
class CExtTextOutTestSuite : public CTestSuite
{
    public:
        CExtTextOutTestSuite(CSection * Section) : CTestSuite(Section),
                                            m_Dest(Section), m_Coordinates(Section), m_ptCoordinates(Section),
                                            m_Rgn(Section), m_Font(Section), m_String(Section), m_DispPerfData(),
                                            m_hdcDest (NULL), m_fuOptionsInUse(0), m_nStringLengthInUse(0),
                                            m_tcStringInUse(NULL), m_dwClipped(0), m_nClippedIndex(0),
                                            m_nMaxClippedIndex(0), m_dwOpaque(0), m_nOpaqueIndex(0),
                                            m_nMaxOpaqueIndex(0)
                                            { info(DETAIL, TEXT("In CExtTextOutTestSuite overloaded constructor.")); }
         ~CExtTextOutTestSuite() { info(DETAIL, TEXT("In CExtTextOutTestSuite destructor.")); }

        virtual BOOL Initialize(TestSuiteInfo *);
        virtual BOOL PreRun(TestInfo *);
        virtual BOOL Run();
        virtual BOOL AddPostRunData(TestInfo *);
        virtual BOOL PostRun();
        virtual BOOL Cleanup();

private:
        CExtTextOutTestSuite() { info(DETAIL, TEXT("In CExtTextOutTestSuite base constructor.")); }

        int m_nIterationCount;
        class CSurface m_Dest;
        class CRectCoordinates m_Coordinates;
        class CRgn m_Rgn;
        class CFont m_Font;
        class CPointCoordinates m_ptCoordinates;
        class CString m_String;
        class CDispPerfData m_DispPerfData;

        HDC m_hdcDest;
        RECT m_rcRectInUse;
        POINT m_ptPosInUse;
        UINT m_fuOptionsInUse;
        UINT m_nStringLengthInUse;
        TCHAR *m_tcStringInUse;

        DWORD *m_dwClipped;
        int m_nClippedIndex, m_nMaxClippedIndex;

        DWORD *m_dwOpaque;
        int m_nOpaqueIndex, m_nMaxOpaqueIndex;

#ifndef UNDER_CE
        DWORD m_dwOldBatchLimit;
#endif

        BOOL InitializeOptions(TestSuiteInfo * tsi);

};
#endif
