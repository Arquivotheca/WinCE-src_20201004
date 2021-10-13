#pragma once

#include <windows.h>
#include <CePerfTimer.h>
#include "perfCases.h"

CEPERFTIMER_BEGIN( DDrawPerfTimer )
    CEPERFTIMER_BASIC_DURATION( MARK_BLIT_VIDVID_1SURF_FULLSURF,     _T("Blit Vid_Vid 1 Surf FullSurf") )
    CEPERFTIMER_BASIC_DURATION( MARK_BLIT_VIDVID_1SURF_1PIXEL,       _T("Blit Vid_Vid 1 Surf 1 Pixel") )
    CEPERFTIMER_BASIC_DURATION( MARK_BLIT_VIDVID_2SURF_FULLSURF,     _T("Blit Vid_Vid 2 Surf FullSurf") )
    CEPERFTIMER_BASIC_DURATION( MARK_BLIT_VIDVID_2SURF_1PIXEL,       _T("Blit Vid_Vid 2 Surf 1 Pixel") )
    CEPERFTIMER_BASIC_DURATION( MARK_BLIT_SYSVID_2SURF_FULLSURF,     _T("Blit Sys_Vid 2 Surf FullSurf") )
    CEPERFTIMER_BASIC_DURATION( MARK_BLIT_SYSVID_2SURF_1PIXEL,       _T("Blit Sys_Vid 2 Surf 1 Pixel") )
    CEPERFTIMER_BASIC_DURATION( MARK_BLIT_SYSSYS_1SURF_FULLSURF,     _T("Blit Sys_Sys 1 Surf FullSurf") )
    CEPERFTIMER_BASIC_DURATION( MARK_BLIT_SYSSYS_1SURF_1PIXEL,       _T("Blit Sys_Sys 1 Surf 1 Pixel") )
    CEPERFTIMER_BASIC_DURATION( MARK_BLIT_SYSSYS_2SURF_FULLSURF,     _T("Blit Sys_Sys 2 Surf FullSurf") )
    CEPERFTIMER_BASIC_DURATION( MARK_BLIT_SYSSYS_2SURF_1PIXEL,       _T("Blit Sys_Sys 2 Surf 1 Pixel") )
    CEPERFTIMER_BASIC_DURATION( MARK_BLIT_VIDVID_1SURF,              _T("SBlit Vid_Vid 1 Surf FullSurf") )
    CEPERFTIMER_BASIC_DURATION( MARK_SBLIT_VIDVID_1SURF,             _T("SBlit Vid_Vid 1 Surf Stretch") )
    CEPERFTIMER_BASIC_DURATION( MARK_BLIT_VIDVID_2SURF,              _T("SBlit Vid_Vid 2 Surf FullSurf") )
    CEPERFTIMER_BASIC_DURATION( MARK_SBLIT_VIDVID_2SURF,             _T("SBlit Vid_Vid 2 Surf Stretch") )
    CEPERFTIMER_BASIC_DURATION( MARK_BLIT_SYSVID_2SURF,              _T("SBlit Sys_Vid 2 Surf FullSurf") )
    CEPERFTIMER_BASIC_DURATION( MARK_SBLIT_SYSVID_2SURF,             _T("SBlit Sys_Vid 2 Surf Stretch") )
    CEPERFTIMER_BASIC_DURATION( MARK_BLIT_SYSSYS_1SURF,              _T("SBlit Sys_Sys 1 Surf FullSurf") )
    CEPERFTIMER_BASIC_DURATION( MARK_SBLIT_SYSSYS_1SURF,             _T("SBlit Sys_Sys 1 Surf Stretch") )
    CEPERFTIMER_BASIC_DURATION( MARK_BLIT_SYSSYS_2SURF,              _T("SBlit Sys_Sys 2 Surf FullSurf") )
    CEPERFTIMER_BASIC_DURATION( MARK_SBLIT_SYSSYS_2SURF,             _T("SBlit Sys_Sys 2 Surf Stretch") )
    CEPERFTIMER_BASIC_DURATION( MARK_FLIP_1BACK,                     _T("Flip 1 Backbuffer") )
    CEPERFTIMER_BASIC_DURATION( MARK_FLIP_2BACK,                     _T("Flip 2 Backbuffer") )
    CEPERFTIMER_BASIC_DURATION( MARK_FLIP_3BACK,                     _T("Flip 3 Backbuffer") )
    CEPERFTIMER_BASIC_DURATION( MARK_FLIP_4BACK,                     _T("Flip 4 Backbuffer") )
    CEPERFTIMER_BASIC_DURATION( MARK_FLIP_5BACK,                     _T("Flip 5 Backbuffer") )
    CEPERFTIMER_BASIC_DURATION( MARK_LOCK_VID_WRITE,                 _T("Lock Vid WriteAccess") )
    CEPERFTIMER_BASIC_DURATION( MARK_LOCK_VID_READ,                  _T("Lock Vid ReadAccess") )
    CEPERFTIMER_BASIC_DURATION( MARK_LOCK_VID_1PIXEL_WRITE,          _T("Lock Vid Write One Pixel") )
    CEPERFTIMER_BASIC_DURATION( MARK_LOCK_VID_1PIXEL_READ,           _T("Lock Vid Read One Pixel") )
    CEPERFTIMER_BASIC_DURATION( MARK_LOCK_VID_FULLSURF_WRITE,        _T("Lock Vid Write Full Surf") )
    CEPERFTIMER_BASIC_DURATION( MARK_LOCK_VID_FULLSURF_READ,         _T("Lock Vid Read Full Surf") )
    CEPERFTIMER_BASIC_DURATION( MARK_LOCK_SYS_WRITE,                 _T("Lock Sys WriteAccess") )
    CEPERFTIMER_BASIC_DURATION( MARK_LOCK_SYS_READ,                  _T("Lock Sys ReadAccess") )
    CEPERFTIMER_BASIC_DURATION( MARK_LOCK_SYS_1PIXEL_WRITE,          _T("Lock Sys Write One Pixel") )
    CEPERFTIMER_BASIC_DURATION( MARK_LOCK_SYS_1PIXEL_READ,           _T("Lock Sys Read One Pixel") )
    CEPERFTIMER_BASIC_DURATION( MARK_LOCK_SYS_FULLSURF_WRITE,        _T("Lock Sys Write Full Surf") )
    CEPERFTIMER_BASIC_DURATION( MARK_LOCK_SYS_FULLSURF_READ,         _T("Lock Sys Read Full Surf") )
    CEPERFTIMER_BASIC_DURATION( MARK_BLIT_VIDVID_CKBLT,              _T("Blit Vid-Vid Colorkey Blt") )
CEPERFTIMER_END()

// {258CB580-BEFA-40cf-979C-EBB68BEB1B23}
static const GUID guidDDraw =
{ 0x258cb580, 0xbefa, 0x40cf, { 0x97, 0x9c, 0xeb, 0xb6, 0x8b, 0xeb, 0x1b, 0x23 } };

#define DDRAW_PERF_NAMESPACE _T("DirectDraw\\Performance")
#define DDRAW_PERF_SESSION _T("Performance")
#define DDRAW_PERF_RESULTFILE _T("CePerf_DDrawResults.xml")

