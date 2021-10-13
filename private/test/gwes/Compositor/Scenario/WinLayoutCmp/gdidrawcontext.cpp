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

//================================================================================
//  Include Files
//================================================================================
#include "gdidrawcontext.h"

#include <ehm.h>

#include "gdiwindowvisual.h"

#ifdef PERFCOUNTERS
#include "..\common\performance.h"
#endif // PERFCOUNTERS

//================================================================================
//  Global Variables
//================================================================================
#ifdef PERFCOUNTERS
static CPerfCounter g_PerfCounter;
#endif // PERFCOUNTERS

//================================================================================
//  CDrawContext Class Declaration
//================================================================================
CGdiDrawContext::
CGdiDrawContext()
    : m_hwndCompositor( NULL )
    , m_hdcPrimarySurf( NULL )
    , m_hdcBackbuffer( NULL )
    , m_hdcCompatible( NULL )
    , m_hBackbuffer( NULL )
    , m_hDefault( NULL )
    , m_hrgnPostDraw( NULL )
{
}


CGdiDrawContext::
~CGdiDrawContext()
{
    Deinitialize();
}


BOOL
CGdiDrawContext::
Initialize( HWND hwndCompositor )
{
    HRESULT hr = E_FAIL;

    // Store off the handle to the compositor window.
    m_hwndCompositor = hwndCompositor;
    CBRA( NULL != m_hwndCompositor );

    // Get a handle to the compositor window's DC.
    m_hdcPrimarySurf = GetDC( m_hwndCompositor );
    CBRA( NULL != m_hdcPrimarySurf );

    // Create a compatible DC for the backbuffer.
    m_hdcBackbuffer = CreateCompatibleDC( m_hdcPrimarySurf );
    CBRA( NULL != m_hdcBackbuffer );

    // Create the backbuffer surface.
    m_hBackbuffer = CreateCompatibleBitmap( m_hdcPrimarySurf, m_szDisplay.cx, m_szDisplay.cy );
    CBRA( NULL != m_hBackbuffer );

    // Select the backbuffer surface into the backbuffer DC.
    m_hDefault = (HBITMAP)SelectObject( m_hdcBackbuffer, m_hBackbuffer );
    CBRA( NULL != m_hDefault );

    // Create a compatible DC that scene objects will use
    // to blit their surfaces into the primary.
    m_hdcCompatible = CreateCompatibleDC( m_hdcPrimarySurf );
    CBRA( NULL != m_hdcCompatible );

    // Create the region object that will control blitting from
    // back to front buffers
    m_hrgnPostDraw = CreateRectRgn( 0, 0, 0, 0 );
    CBRA( NULL != m_hrgnPostDraw );

    // Success.
    hr = S_OK;

Error:
    // Cleanup on failure.
    if ( FAILED( hr ) )
    {
        Deinitialize();
    }

    return SUCCEEDED( hr );
}


VOID
CGdiDrawContext::
Deinitialize()
{
    if ( NULL != m_hrgnPostDraw )
    {
        DeleteObject( m_hrgnPostDraw );
        m_hrgnPostDraw = NULL;
    }

    if ( NULL != m_hdcCompatible )
    {
        DeleteObject( m_hdcCompatible );
        m_hdcCompatible = NULL;
    }

    if ( NULL != m_hDefault )
    {
        SelectObject( m_hdcBackbuffer, m_hDefault );
    }

    if ( NULL != m_hBackbuffer )
    {
        DeleteObject( m_hBackbuffer );
        m_hBackbuffer = NULL;
    }

    if ( NULL != m_hdcBackbuffer )
    {
        DeleteObject( m_hdcBackbuffer );
        m_hdcBackbuffer = NULL;
    }

    if ( NULL != m_hdcPrimarySurf )
    {
        ReleaseDC( m_hwndCompositor, m_hdcPrimarySurf );
        m_hdcPrimarySurf = NULL;
    }
}


VOID
CGdiDrawContext::
Predraw()
{
    ASSERT( NULL != m_hdcPrimarySurf );
    ASSERT( NULL != m_hrgnPostDraw );

#ifdef PERFCOUNTERS
    g_PerfCounter.StartTime();
#endif // PERFCOUNTERS

    // Make a copy of the invalid region that we'll use
    // when blitting from the backbuffer into the primary
    // surface.
    CombineRgn( m_hrgnPostDraw, m_hrgnInvalid, NULL, RGN_COPY );
}


VOID
CGdiDrawContext::
Postdraw()
{
    ASSERT( NULL != m_hdcPrimarySurf );
    ASSERT( NULL != m_hdcBackbuffer );
    ASSERT( NULL != m_hrgnPostDraw );

    // Clear any parts of the invalid region that weren't drawn over.
    // by any other objects.
    FillRgn( m_hdcBackbuffer, m_hrgnInvalid, (HBRUSH)GetStockObject( BLACK_BRUSH ) );
    SubtractFromInvalidRegion( m_hrgnInvalid );

    // Set the clip region for the blit since we don't
    // need to redraw the entire primary surface.
    SelectClipRgn( m_hdcPrimarySurf, m_hrgnPostDraw );

    // Blit from the backbuffer into the primary surface.
    BitBlt(
        m_hdcPrimarySurf,
        0, 0, m_szDisplay.cx, m_szDisplay.cy,
        m_hdcBackbuffer,
        0, 0,
        SRCCOPY
        );

    // Unset the clip region.
    SelectClipRgn( m_hdcPrimarySurf, NULL );

#ifdef PERFCOUNTERS
    {
        HDC hdcCompositor = GetDC( m_hwndCompositor );
        TCHAR szPerfInfo[256] = {0};

        const size_t nMaxSamples = 100;
        static size_t nNumSamples = 1;
        static size_t nCurrentSample = 0;
        static DWORD dwDrawTimeSamples[ nMaxSamples ] = { 0 };

        DWORD dwAverageDrawTime = 0;
        DWORD dwAverageFrameRate = 0;
        DWORD dwFrameRate = 0;

        dwDrawTimeSamples[ nCurrentSample ] = (DWORD)g_PerfCounter.GetTimeMs();
        for ( size_t i = 0; i < nNumSamples; ++i )
        {
            dwAverageDrawTime += dwDrawTimeSamples[ i ];
        }

        dwAverageDrawTime /= nNumSamples;
        dwAverageFrameRate = 1000 / (dwAverageDrawTime > 1 ? dwAverageDrawTime : 1);
        dwFrameRate = 1000 / (dwDrawTimeSamples[ nCurrentSample ] > 1 ? dwDrawTimeSamples[ nCurrentSample ] : 1);

        StringCchPrintf(
            szPerfInfo, 256,
            TEXT("FPS: %d, DrawTime: %d ms; AverageFPS: %d, AverageDrawTime: %d ms; Full Set: %s"),
            dwFrameRate,
            dwDrawTimeSamples[ nCurrentSample ],
            dwAverageFrameRate,
            dwAverageDrawTime,
            ( nMaxSamples == nNumSamples ? L"TRUE" : L"FALSE" )
            );

        nNumSamples = ( nNumSamples < nMaxSamples ? nNumSamples + 1: nNumSamples );
        nCurrentSample = ( nCurrentSample < (nMaxSamples - 1) ? nCurrentSample + 1 : 0 );

        RECT rcText = { 0, 0, 800, 150 };
        DrawText( hdcCompositor, szPerfInfo, -1, &rcText, DT_LEFT );

        ReleaseDC( m_hwndCompositor, hdcCompositor );
        hdcCompositor = NULL;
    }
#endif // PERFCOUNTERS
}


VOID
CGdiDrawContext::
StateUpdated(
    CDrawContext::DRAWCONTEXT_STATE eUpdatedState
    )
{
    switch ( eUpdatedState )
    {
    case CDrawContext::DRAWCONTEXT_STATE_SIZE:
    case CDrawContext::DRAWCONTEXT_STATE_POSITION:
    case CDrawContext::DRAWCONTEXT_STATE_ORIENT:
        {
            RECT rc = { 0 };

            // Recreate the backbuffer surface.
            {
                // Select the default bitmap back into the backbuffer DC.
                SelectObject( m_hdcBackbuffer, m_hDefault );
                m_hDefault = NULL;

                // Delete the old backbuffer surface.
                if ( NULL != m_hBackbuffer )
                {
                    DeleteObject( m_hBackbuffer );
                    m_hBackbuffer = NULL;
                }

                // Recreate the backbuffer surface.
                m_hBackbuffer = CreateCompatibleBitmap( m_hdcPrimarySurf, m_szDisplay.cx, m_szDisplay.cy );
                ASSERT( NULL != m_hBackbuffer );

                // Select the new backbuffer into the backbuffer DC.
                m_hDefault = (HBITMAP)SelectObject( m_hdcBackbuffer, m_hBackbuffer );
                ASSERT( NULL != m_hDefault );
            }
        }
        break;
        
    default:
        // Do nothing.
        break;
    }
}
