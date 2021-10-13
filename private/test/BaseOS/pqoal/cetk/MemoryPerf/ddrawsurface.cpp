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


// ***************************************************************************************************************
//
// These functions create and set up DDraw surfaces for testing their performance.
//
// ***************************************************************************************************************


#include "MemoryPerf.h"
#include "DDrawSurface.h"

#pragma warning( disable : 22110 )      // prefast overly aggressive since WinCE/WM does not have encode pointer


#ifdef BUILD_WITH_DDRAW


#include <ddraw.h>

// ***************************************************************************************************************
// a DDRaw surface should be uncached-bufferable (not strongly ordered)
// in contrast to a user-mode uncached which is strongly ordered.
// Stolen and adapted from DDex3.cpp to get a driver off-screen surface in "video" memory
// ***************************************************************************************************************

//-----------------------------------------------------------------------------
// Local definitions
//-----------------------------------------------------------------------------
#define NAME                TEXT("MemoryPerf")
#define TITLE               TEXT("Memory Performance Test")

//-----------------------------------------------------------------------------
// Global data
//-----------------------------------------------------------------------------
LPDIRECTDRAW                g_pDD = NULL;        // DirectDraw object
LPDIRECTDRAWSURFACE         g_pDDSOne = NULL;    // Off-screen surface 1


//-----------------------------------------------------------------------------
// Name: ReleaseAllObjects()
// Desc: Finished with all objects we use; release them
//-----------------------------------------------------------------------------

static void
ReleaseAllObjects(void)
{
    if (g_pDDSOne != NULL)
    {
        g_pDDSOne->Unlock( NULL ); 
        g_pDDSOne->Release();
        g_pDDSOne = NULL;
    }
    if (g_pDD != NULL)
    {
        g_pDD->SetCooperativeLevel(NULL, DDSCL_NORMAL);
        g_pDD->Release();
        g_pDD = NULL;
    }
}




//-----------------------------------------------------------------------------
// Name: InitFail()
// Desc: This function is called if an initialization function fails
//-----------------------------------------------------------------------------

#define PREFIX      TEXT("MemoryPerf: ")
#define PREFIX_LEN  12

HRESULT
InitFail(HRESULT hRet, LPCTSTR szError,...)
{
    TCHAR                       szBuff[128] = PREFIX;
    va_list                     vl;

    va_start(vl, szError);
    StringCchVPrintf(szBuff + PREFIX_LEN, (128-PREFIX_LEN), szError, vl);
    ReleaseAllObjects();
    LogPrintf( "%S\r\n", szBuff);
    va_end(vl);
    return hRet;
}

#undef PREFIX_LEN
#undef PREFIX

HINSTANCE g_hCoreDll = NULL;
PFN_DirectDrawCreate g_pfnDirectDrawCreate = NULL;


//-----------------------------------------------------------------------------
// Name: GetDDrawFunctionEntries()
// Desc: set up function pointers
//-----------------------------------------------------------------------------

BOOL GetDDrawFunctionEntries()
{
    g_hCoreDll = LoadLibrary(TEXT("ddraw.dll"));
    if ( g_hCoreDll == NULL )
    {
        RETAILMSG(1,(L"LoadLibrary(ddraw.dll) failed!"));
        return FALSE;
    }
    else
    {
        g_pfnDirectDrawCreate = (PFN_DirectDrawCreate)GetProcAddress(g_hCoreDll, TEXT("DirectDrawCreate"));
        if ( g_pfnDirectDrawCreate == NULL )
        {
            RETAILMSG(1,(L"Function entry DirectDrawCreate is not available"));
            return FALSE;
        }
    }
    return TRUE;
}

//-----------------------------------------------------------------------------
// Name: bDDSurfaceInitApp()
// Desc: API to set things up if possible
//-----------------------------------------------------------------------------

bool bDDSurfaceInitApp(int nCmdShow)
{
    DDSURFACEDESC               ddsd;
    HRESULT                     hRet;

    g_pDD = NULL;        // DirectDraw object
    g_pDDSOne = NULL;    // Off-screen surface 1

    if ( !GetDDrawFunctionEntries() )
        return InitFail( S_FALSE, TEXT("GetDDrawFunctionEntries FAILED")) != DD_OK;

    hRet = g_pfnDirectDrawCreate(NULL, &g_pDD, NULL);
    if (hRet != DD_OK)
        return InitFail( hRet, TEXT("DirectDrawCreate FAILED")) != DD_OK;

    // Since we have only off-screen buffers, we don't need a window and can use normal coop level.
    hRet = g_pDD->SetCooperativeLevel(NULL, DDSCL_NORMAL );
    if (hRet != DD_OK)
        return InitFail( hRet, TEXT("SetCooperativeLevel FAILED")) != DD_OK;

    // Create a off-screen bitmap.
    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    ddsd.dwFlags = DDSD_HEIGHT | DDSD_WIDTH;
    ddsd.dwHeight = 800;
    ddsd.dwWidth = 480;
    hRet = g_pDD->CreateSurface(&ddsd, &g_pDDSOne, NULL);
    if (hRet != DD_OK)
        return InitFail( hRet, TEXT("CreateSurface FAILED")) != DD_OK;

    return true;
}


//-----------------------------------------------------------------------------
// Name: DDSurfaceClose()
// Desc: API to close things down
//-----------------------------------------------------------------------------

void DDSurfaceClose() {
    ReleaseAllObjects();
}


//-----------------------------------------------------------------------------
// Name: bDDSurfaceLock()
// Desc: API to actually get the surface as read-only, write-only, or read-write
//       returns a pointer and a size
//-----------------------------------------------------------------------------

bool bDDSurfaceLock( const int eLockStyle, DWORD** ppdwDDSRead, DWORD* pdwDDSReadSize )
{
    if ( NULL != g_pDDSOne )
    {
        DDSURFACEDESC tDDSDesc = { sizeof(DDSURFACEDESC), 0 };   // Off-screen surface description when locked
        DWORD dwFlags = 0;
        switch( eLockStyle )
        {
        case eLockReadWrite:
            break;
        case eLockReadOnly:
            dwFlags = DDLOCK_READONLY;
            break;
        case eLockWriteOnly:
            dwFlags = DDLOCK_WRITEONLY;
            break;
        case eLockWriteDiscard:
            dwFlags = DDLOCK_WRITEONLY | DDLOCK_DISCARD;
            break;
        default:
            break;
        }
        HRESULT hRes = g_pDDSOne->Lock( NULL, &tDDSDesc, dwFlags, NULL );
        if ( DD_OK == hRes )
        {
            if ( gtGlobal.bExtraPrints )
            {
                LogPrintf( "!DDSurface [%d,%d] 0x%08p %x (%d,%d) %u [flag %x]\r\n", 
                    tDDSDesc.dwFlags & DDSD_HEIGHT      ? tDDSDesc.dwHeight : 0,
                    tDDSDesc.dwFlags & DDSD_WIDTH       ? tDDSDesc.dwWidth  : 0,
                    tDDSDesc.dwFlags & DDSD_LPSURFACE   ? tDDSDesc.lpSurface : NULL,
                    tDDSDesc.dwFlags & DDSD_PIXELFORMAT ? tDDSDesc.ddpfPixelFormat.dwFourCC : 0,
                    tDDSDesc.dwFlags & DDSD_PITCH       ? tDDSDesc.lPitch : 0,
                    tDDSDesc.dwFlags & DDSD_XPITCH      ? tDDSDesc.lXPitch : 0,
                    tDDSDesc.dwFlags & DDSD_SURFACESIZE ? tDDSDesc.dwSurfaceSize : 0,
                    dwFlags
                );
            }
            if ( (tDDSDesc.dwFlags & DDSD_LPSURFACE) && (tDDSDesc.dwFlags & DDSD_SURFACESIZE) )
            {
                *ppdwDDSRead = (DWORD*)tDDSDesc.lpSurface;
                *pdwDDSReadSize = tDDSDesc.dwSurfaceSize;
                return true;
            }
            else if (gtGlobal.bWarnings)
            {
                LogPrintf( "Warning: Cannot access DDraw off-screen surface\r\n" );
            }
        }
        else if (gtGlobal.bWarnings)
        {
            LogPrintf( "Warning: Cannot lock DDraw off-screen surface with error 0x%x\r\n", hRes );
        }
    }
    return false;
}


//-----------------------------------------------------------------------------
// Name: DDSurfaceUnLock()
// Desc: unlocks the surface if there is one.
//-----------------------------------------------------------------------------

void DDSurfaceUnLock(void)
{
    if ( NULL != g_pDDSOne )
    {
        g_pDDSOne->Unlock( NULL ); 
    }
}

#endif
