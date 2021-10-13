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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
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

#ifndef _countof
#define _countof(x) (sizeof(x)/sizeof(*(x)))
#endif
PerfLogStub *g_pLog = NULL;
CEPerfLog    g_CEPerfLog;
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
    if (!m_strPlatformName.empty())
        DebugBeginLevel(0, TEXT("BEGIN GROUP: %s (%s)"), m_pszModuleName, m_strPlatformName.c_str());
    else
        DebugBeginLevel(0, TEXT("BEGIN GROUP: %s"), m_pszModuleName);
    return sprHandled;
}

eShellProcReturnCode CDDPerf::EndGroup()
{
    // release it if necessary
    if (g_pLog != &g_CEPerfLog && g_pLog != NULL) {
        delete g_pLog;
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
    enum { LOG_PERFLOG, LOG_DBG, LOG_CSV};
    CClParse clParser(pszDLLCmdLine);
    TCHAR tszTemp[MAX_PATH];
    DWORD logType = LOG_PERFLOG;
    
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

    if (clParser.GetOptString(TEXT("log"), tszTemp, _countof(tszTemp))) 
    {
        if (_tcsnicmp(tszTemp, TEXT("dbg"), 3) == 0) 
        {
            logType = LOG_DBG;
        }
        else if (_tcsnicmp(tszTemp, TEXT("csv"), 3) == 0) 
        {
            logType = LOG_CSV;
        }
    }
    if (clParser.GetOptString(TEXT("csvfile"), tszTemp, _countof(tszTemp))) 
    {
        logType = LOG_CSV;
        HRESULT hr = StringCchCopy(g_csvFilename, _countof(g_csvFilename), tszTemp);
        if (FAILED(hr))
        {
            g_csvFilename[0] = _T('\0');
        }
    }    

    // init log object
    switch (logType) {
    case LOG_PERFLOG:
        g_pLog = &g_CEPerfLog;
        dbgout << "Logging to [perflog]" << endl;
        break;
        
    case LOG_DBG:
        g_pLog = new DebugOutput(PerfDbgPrint);
        dbgout << "Logging to [Debug Out]" << endl;
        if (g_pLog == NULL) 
        {
            dbgout << "*** OOM creating DebugOutput object***" << endl;
        }        
        break;
        
    case LOG_CSV:
        if (g_csvFilename[0] == _T('\0'))
        {
            if (GetModulePath(GetModuleHandle(NULL), g_csvFilename, countof(g_csvFilename)) > 0)
            {
                HRESULT hr = StringCchCat(g_csvFilename, countof(g_csvFilename), _T("Perf_ddraw.csv"));
                if (FAILED(hr))
                {
                    break;
                }
            }
        }
        g_pLog = new CsvFile(g_csvFilename, 0);
        dbgout << "Logging to [CSV file " << g_csvFilename << "]" << endl; 
        if (g_pLog == NULL) 
        {
            dbgout << "*** OOM creating CsvFile object ***" << endl;
        }        
        break;
    }
    
    if (g_pLog == NULL) 
    {
        g_pLog = &g_CEPerfLog;
        dbgout << "Using default [Perflog]" << endl; 
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

