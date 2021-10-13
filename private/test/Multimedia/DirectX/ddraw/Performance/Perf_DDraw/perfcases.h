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
#pragma once

#include <QATestUty/TestBase.h>

extern FUNCTION_TABLE_ENTRY g_lpFTE[];

typedef ITuxDLL::eShellProcReturnCode eShellProcReturnCode;

eTestResult InitSurface( 
    com_utilities::CComPointer<IDirectDraw> * piDD, 
    com_utilities::CComPointer<IDirectDrawSurface> * piDDSurface, 
    DWORD dwInitCaps,
    DWORD dwWidth,
    DWORD dwHeight);

class CDDPerf : public CTuxDLL
{
public:
    CDDPerf() : CTuxDLL(g_lpFTE,false),
                m_strPlatformName(TEXT("")),
                m_IsCePerfAvailable(false)
    {
        m_pszModuleName = TEXT("DDPerf.DLL");
    }

    virtual bool Initialize();    
    virtual eShellProcReturnCode BeginGroup();
    virtual eShellProcReturnCode EndGroup();
    void ProcessCommandLine(LPCTSTR pszDLLCmdLine);

    std::tstring m_strPlatformName;
    int m_StatusFilter;
    DWORD m_dwSurfaceWidth;
    DWORD m_dwSurfaceHeight;

    bool m_IsCePerfAvailable;
    
private:
    static int s_nDefaultHeight,
               s_nDefaultWidth;
};

extern CDDPerf g_BlitPerf;

#define CREATETESTCASE(num) \
class CDDPerf##num : public TestUty::CTest \
{ \
public: \
    virtual eTestResult Test(); \
}

// Create the test cases

CREATETESTCASE(BpsVidVid);
CREATETESTCASE(BpsSysVid);
CREATETESTCASE(BpsSysSys);
CREATETESTCASE(Bps2VidVid);
CREATETESTCASE(Bps2SysVid);
CREATETESTCASE(Bps2SysSys);
CREATETESTCASE(Flip);
CREATETESTCASE(LockVid);
CREATETESTCASE(LockSys);
CREATETESTCASE(CkBlit);

// Marker Definitions
enum Markers
{
    MARK_BLIT_VIDVID_1SURF_FULLSURF = 1,
    MARK_BLIT_VIDVID_1SURF_1PIXEL,
    MARK_BLIT_VIDVID_2SURF_FULLSURF,
    MARK_BLIT_VIDVID_2SURF_1PIXEL,
    MARK_BLIT_SYSVID_2SURF_FULLSURF,
    MARK_BLIT_SYSVID_2SURF_1PIXEL,
    MARK_BLIT_SYSSYS_1SURF_FULLSURF,
    MARK_BLIT_SYSSYS_1SURF_1PIXEL,
    MARK_BLIT_SYSSYS_2SURF_FULLSURF,
    MARK_BLIT_SYSSYS_2SURF_1PIXEL, 

    MARK_BLIT_VIDVID_1SURF,
    MARK_SBLIT_VIDVID_1SURF,
    MARK_BLIT_VIDVID_2SURF,
    MARK_SBLIT_VIDVID_2SURF,
    MARK_BLIT_SYSVID_2SURF,
    MARK_SBLIT_SYSVID_2SURF,
    MARK_BLIT_SYSSYS_1SURF,
    MARK_SBLIT_SYSSYS_1SURF,
    MARK_BLIT_SYSSYS_2SURF,
    MARK_SBLIT_SYSSYS_2SURF,

    MARK_FLIP_1BACK,
    MARK_FLIP_2BACK,
    MARK_FLIP_3BACK,
    MARK_FLIP_4BACK,
    MARK_FLIP_5BACK,

    MARK_LOCK_VID_WRITE,
    MARK_LOCK_VID_READ,
    MARK_LOCK_VID_1PIXEL_WRITE,
    MARK_LOCK_VID_1PIXEL_READ,
    MARK_LOCK_VID_FULLSURF_WRITE,
    MARK_LOCK_VID_FULLSURF_READ,

    MARK_LOCK_SYS_READ,
    MARK_LOCK_SYS_WRITE,
    MARK_LOCK_SYS_1PIXEL_WRITE,
    MARK_LOCK_SYS_1PIXEL_READ,
    MARK_LOCK_SYS_FULLSURF_WRITE,
    MARK_LOCK_SYS_FULLSURF_READ,

    MARK_BLIT_VIDVID_CKBLT, 
};

#define MARK_FLIP_FROM_BACKBUFFER_COUNT(x) (MARK_FLIP_1BACK + x - 1)
