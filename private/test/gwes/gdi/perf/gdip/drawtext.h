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

#ifndef DRAWTEXT_H
#define DRAWTEXT_H
class CDrawTextTestSuite : public CTestSuite
{
    public:
        CDrawTextTestSuite(CSection * Section) : CTestSuite(Section),
                                            m_Dest(Section), m_Coordinates(Section),
                                            m_Rgn(Section), m_Font(Section), m_String(Section), m_DispPerfData(),
                                            m_hdcDest(NULL), m_nFormat(0), m_tcStringInUse(NULL),
                                            m_dwBottom(NULL), m_nBottomIndex(0), m_nMaxBottomIndex(0),
                                            m_dwCalcrect(NULL), m_nCalcrectIndex(0), m_nMaxCalcrectIndex(0),
                                            m_dwCenter(NULL), m_nCenterIndex(0), m_nMaxCenterIndex(0),
                                            m_dwExpandtabs(NULL), m_nExpandtabsIndex(0), m_nMaxExpandtabsIndex(0),
                                            m_dwInternal(NULL), m_nInternalIndex(0), m_nMaxInternalIndex(0),
                                            m_dwLeft(NULL), m_nLeftIndex(0), m_nMaxLeftIndex(0),
                                            m_dwNoclip(NULL), m_nNoclipIndex(0), m_nMaxNoclipIndex(0),
                                            m_dwNoprefix(NULL), m_nNoprefixIndex(0), m_nMaxNoprefixIndex(0),
                                            m_dwRight(NULL), m_nRightIndex(0), m_nMaxRightIndex(0),
                                            m_dwSingleline(NULL), m_nSinglelineIndex(0), m_nMaxSinglelineIndex(0),
                                            m_dwTabstop(NULL), m_nTabstopIndex(0), m_nMaxTabstopIndex(0),
                                            m_dwTop(NULL), m_nTopIndex(0), m_nMaxTopIndex(0),
                                            m_dwVcenter(NULL), m_nVcenterIndex(0), m_nMaxVcenterIndex(0),
                                            m_dwWordbreak(NULL), m_nWordbreakIndex(0), m_nMaxWordbreakIndex(0)
                                            { info(DETAIL, TEXT("In CDrawTextTestSuite overloaded constructor.")); }
         ~CDrawTextTestSuite() { info(DETAIL, TEXT("In CDrawTextTestSuite destructor.")); }

        virtual BOOL Initialize(TestSuiteInfo *);
        virtual BOOL PreRun(TestInfo *);
        virtual BOOL Run();
        virtual BOOL AddPostRunData(TestInfo *);
        virtual BOOL PostRun();
        virtual BOOL Cleanup();

private:
        CDrawTextTestSuite() { info(DETAIL, TEXT("In CDrawTextTestSuite base constructor.")); }

        int m_nIterationCount;
        class CSurface m_Dest;
        class CRectCoordinates m_Coordinates;
        class CRgn m_Rgn;
        class CFont m_Font;
        class CString m_String;
        class CDispPerfData m_DispPerfData;

        HDC m_hdcDest;
        RECT m_rcRectInUse;
        UINT m_nFormat;
        TCHAR *m_tcStringInUse;

        DWORD *m_dwBottom;
        int m_nBottomIndex, m_nMaxBottomIndex;

        DWORD *m_dwCalcrect;
        int m_nCalcrectIndex, m_nMaxCalcrectIndex;

        DWORD *m_dwCenter;
        int m_nCenterIndex, m_nMaxCenterIndex;

        DWORD *m_dwExpandtabs;
        int m_nExpandtabsIndex, m_nMaxExpandtabsIndex;

        DWORD *m_dwInternal;
        int m_nInternalIndex, m_nMaxInternalIndex;

        DWORD *m_dwLeft;
        int m_nLeftIndex, m_nMaxLeftIndex;

        DWORD *m_dwNoclip;
        int m_nNoclipIndex, m_nMaxNoclipIndex;

        DWORD *m_dwNoprefix;
        int m_nNoprefixIndex, m_nMaxNoprefixIndex;

        DWORD *m_dwRight;
        int m_nRightIndex, m_nMaxRightIndex;

        DWORD *m_dwSingleline;
        int m_nSinglelineIndex, m_nMaxSinglelineIndex;

        DWORD *m_dwTabstop;
        int m_nTabstopIndex, m_nMaxTabstopIndex;

        DWORD *m_dwTop;
        int m_nTopIndex, m_nMaxTopIndex;

        DWORD *m_dwVcenter;
        int m_nVcenterIndex, m_nMaxVcenterIndex;

        DWORD *m_dwWordbreak;
        int m_nWordbreakIndex, m_nMaxWordbreakIndex;


#ifndef UNDER_CE
        DWORD m_dwOldBatchLimit;
#endif

        BOOL InitializeOptions(TestSuiteInfo * tsi);

};
#endif
