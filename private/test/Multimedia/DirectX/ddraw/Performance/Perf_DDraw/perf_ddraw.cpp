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
// =============================================================================
//
//  BlitPerf TUX DLL
//
//
//  Revision History:
//      07-31-2001  GaryDan  Copied from DDRepro
//
// =============================================================================

#include "main.h"

#include <QATestUty/WinApp.h>
#include <QATestUty/TuxDLLUty.h>
#include <DebugStreamUty.h>
#include <DDrawUty.h>
#include <clparse.h>
#include <ddraw.h>
#include "perfCases.h"
#include "TuxFunctionTable.h"
#include "ddrawperftimer.h"

DDrawPerfTimer g_pDDPerfTimer;

#ifndef _countof
#define _countof(x) (sizeof(x)/sizeof(*(x)))
#endif
TCHAR        g_csvFilename[MAX_PATH];

std::tstring g_strTestFilename;

DEFINE_DllMain;
DEFINE_ShellProc;

using namespace com_utilities;
using namespace DebugStreamUty;
using namespace DDrawUty;

// Default window heights for this module 
int CDDPerf::s_nDefaultHeight = 200;
int CDDPerf::s_nDefaultWidth = 300;

CDDPerf g_BlitPerf;
bool g_fVsync = true;
bool g_fRGB = true;

bool CDDPerf::Initialize()
{
    m_dwWindowStyle = WS_VISIBLE | WS_OVERLAPPED;
    m_nHeight = s_nDefaultHeight;
    m_nWidth = s_nDefaultWidth;
        
    return CTuxDLL::Initialize();
}

eShellProcReturnCode CDDPerf::BeginGroup()
{
    HRESULT hr = S_OK;
    hr = g_pDDPerfTimer.BeginPerfSession(
                                        DDRAW_PERF_NAMESPACE,    // Namespace
                                        DDRAW_PERF_SESSION,      // Session
                                        DDRAW_PERF_RESULTFILE,   // Result XML 
                                        guidDDraw                // Unique GUID for the DirectDraw performance tests
                                        );
    if(S_OK != hr)
    {        
        dbgout << "CePerf Session has not been opened. Err Code : " << hr << endl;
    }
    else
    {
        m_IsCePerfAvailable = true;
    }

    if (!m_strPlatformName.empty())
        DebugBeginLevel(0, TEXT("BEGIN GROUP: %s (%s)"), m_pszModuleName, m_strPlatformName.c_str());
    else
        DebugBeginLevel(0, TEXT("BEGIN GROUP: %s"), m_pszModuleName);
    return sprHandled;
}

eShellProcReturnCode CDDPerf::EndGroup()
{   
    if(m_IsCePerfAvailable)
    {
        dbgout << "Closing the CePerf Session." << endl;
        g_pDDPerfTimer.EndPerfSession();

        m_IsCePerfAvailable = false;
    }

    if (!m_strPlatformName.empty())
        DebugEndLevel(TEXT("END GROUP: %s (%s)"), m_pszModuleName, m_strPlatformName.c_str());
    else
        DebugEndLevel(TEXT("END GROUP: %s"), m_pszModuleName);
    return sprHandled;
}

// logging to debug output 
void PerfDbgPrint(LPCTSTR msg)
{
    dbgout << msg << endl;
}

DWORD GetModulePath(HMODULE hMod, TCHAR * tszPath, DWORD dwCchSize)
{
    DWORD fullNameLen = GetModuleFileName(hMod, tszPath, dwCchSize);
    DWORD len = fullNameLen;
    
    while (len > 0 && tszPath[len - 1] != _T('\\'))
        --len;

    // Changing the item at iLen to '\0' will leave a trailing \ on the path.
    if (len >= 0)
    {
        tszPath[len] = 0;
    }
    return fullNameLen;
}


void CDDPerf::ProcessCommandLine(LPCTSTR pszDLLCmdLine)
{
    CClParse clParser(pszDLLCmdLine);
    TCHAR tszTemp[MAX_PATH];
    
    if (clParser.GetOpt(TEXT("novsync")))
    {
        g_fVsync = false;
    }

    if (clParser.GetOpt(TEXT("YUV")))
    {
        g_fRGB = false;
    }

    if (clParser.GetOptString(TEXT("file"), tszTemp, _countof(tszTemp)))
    {
        g_strTestFilename = tszTemp;    
    }
    
    if (clParser.GetOptString(TEXT("platform"), tszTemp, _countof(tszTemp)))
    {
        m_strPlatformName = tszTemp;
    }

    if (!clParser.GetOptVal(TEXT("width"), &m_dwSurfaceWidth))
    {
        m_dwSurfaceWidth = 640;
    }
    if (!clParser.GetOptVal(TEXT("height"), &m_dwSurfaceHeight))
    {
        m_dwSurfaceHeight = 480;
    }    
    return;
}

////////////////////////////////////////////////////////////////////////////////
// InitSurface - creates and initializes a surface for testing.
eTestResult InitSurface( 
    CComPointer<IDirectDraw> * piDD, 
    CComPointer<IDirectDrawSurface> * piDDSurface, 
    DWORD dwInitCaps,
    DWORD dwWidth,
    DWORD dwHeight)
{
    DDBLTFX ddbltfx;
    CDDSurfaceDesc ddsd;
    HRESULT hr;
    
    // Configure the surface
    ddsd.dwSize=sizeof(ddsd);
    ddsd.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
    ddsd.ddsCaps.dwCaps = dwInitCaps;
    ddsd.dwWidth = dwWidth;
    ddsd.dwHeight = dwHeight;
    
    dbgout 
        << "Creating Test Surface: "
        << dwWidth << "x" << dwHeight
        << "(ddsCaps = " << hex(dwInitCaps) << ")"
        << endl;
    

    // create the surface
    hr = (*piDD)->CreateSurface(&ddsd, (*piDDSurface).AsOutParam(), NULL);
    
    if ((DDERR_OUTOFMEMORY == hr) || (DDERR_OUTOFVIDEOMEMORY == hr))
    {
        dbgout 
            << "Not enough memory, cannot create surface. If this format is supported, use /Width and /Height parameters to use smaller surfaces. Skipping" 
            << endl;
        return trSkip;
    }
    // verify the proper creation of the surface
    CheckHRESULT(hr, "CreateSurface", hr);
    // clear the surface
    memset(&ddbltfx, 0, sizeof(ddbltfx));
    ddbltfx.dwSize = sizeof(ddbltfx);
    ddbltfx.dwFillColor = 0;
    hr=(*piDDSurface)->Blt(
        NULL, 
        NULL,
        NULL, 
        DDBLT_WAITNOTBUSY | DDBLT_COLORFILL, 
        &ddbltfx);
    // verify the proper filling of the surface (if we can blit this time, we should be able to test without
    // any errors)
    CheckHRESULT(hr, "ColorFill Blt", trAbort);
    
    return(S_OK);
}

