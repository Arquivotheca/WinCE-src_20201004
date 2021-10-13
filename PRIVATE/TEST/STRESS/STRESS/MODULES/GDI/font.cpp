//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>
#include <stressutils.h>
#include "resource.h"
#include "stressrun.h"
#include "gdistress.h"


extern LPTSTR g_rgszStr[];
extern INT STR_COUNT;


_stressfont_t::_stressfont_t(HINSTANCE hInstance, HWND hWnd, _stressrun_t *psr): m_Surfaces(NULL)
{
    _hInstance = hInstance;

    if (psr)
    {
        _psr = psr;
    }

    // window

    if (!IsWindow(hWnd))
    {
        LogFail(_T("#### _stressfont_t::_stressfont_t - IsWindow failed - Error: 0x%x ####\r\n"),
            GetLastError());
        _psr->fail();
        return;
    }

    _hWnd = hWnd;

    if (!GetClientRect(hWnd, &_rc))
    {
        LogFail(_T("#### _stressfont_t::_stressfont_t - GetClientRect failed - Error: 0x%x ####\r\n"),
            GetLastError());
        _psr->fail();
        return;
    }

    m_Surfaces = new _surfaces(hWnd, _rc, hInstance, psr);

    if (!m_Surfaces)
    {
        LogFail(_T("#### _stressfont_t::_stressfont_t failed to allocate surfaces class - Error: 0x%x ####\r\n"),
            GetLastError());
        _psr->fail();
        return;
    }

}

_stressfont_t::~_stressfont_t()
{
    if(m_Surfaces)
        delete m_Surfaces;
}

DWORD _stressfont_t::drawtext()
{
    _stressrun_t obj(L"_stressfont_t::drawtext");

    FLAG_DETAILS rgfd[] = {
        NAMEVALENTRY(DT_BOTTOM),
        NAMEVALENTRY(DT_CALCRECT),
        NAMEVALENTRY(DT_CENTER),
        NAMEVALENTRY(DT_EXPANDTABS),
        NAMEVALENTRY(DT_INTERNAL),
        NAMEVALENTRY(DT_LEFT),
        NAMEVALENTRY(DT_NOCLIP),
        NAMEVALENTRY(DT_NOPREFIX),
        NAMEVALENTRY(DT_RIGHT),
        NAMEVALENTRY(DT_SINGLELINE),
        NAMEVALENTRY(DT_TABSTOP),
        NAMEVALENTRY(DT_TOP),
        NAMEVALENTRY(DT_VCENTER),
        NAMEVALENTRY(DT_WORDBREAK)
    };
    INT LOOP_COUNT = RANDOM_INT(LOOPCOUNT_MAX, LOOPCOUNT_MIN);
    HDC hDest;    

    hDest = m_Surfaces->GetRandomSurface(TRUE);

    for (INT i = 0; i < LOOP_COUNT; i++)
    {
        INT j = rand() % ARRAY_SIZE(rgfd);
        INT isz = rand() % STR_COUNT;
        RECT rc;

        SetRectEmpty(&rc);

        LogVerbose(_T("_stressfont_t::drawtext - Calling RandomRect...\r\n"));

        if (!RandomRect(&rc, &_rc))
        {
            LogWarn2(_T("#### _stressfont_t::drawtext - RandomRect failed ####\r\n"));
            continue;
        }

        LogVerbose(_T("_stressfont_t::drawtext - Calling DrawText(%s, %s)...\r\n"),
            g_rgszStr[isz], rgfd[j].lpszFlag);

        if(0x80000000 == SetTextCharacterExtra(hDest, (rand() % 32) - 16))
        {
            LogWarn2(_T("#### _stressfont_t::drawtext - SetTextCharacterExtra failed ####\r\n"));
            continue;
        }

        SetLastError(ERROR_SUCCESS);

        if (!DrawText(hDest, (LPTSTR)g_rgszStr[isz], -1, &rc, rgfd[j].dwFlag))
        {
            if(GetLastError() == ERROR_NOT_ENOUGH_MEMORY)
            {
                LogWarn1(_T("_stressfont_::drawtext - insufficient memory.\r\n"));
                obj.warning1();
            }
            else
            {
                LogFail(_T("#### _stressfont_t::drawtext - DrawText(%s, %s) failed - Error: 0x%x ####\r\n"),
                    g_rgszStr[isz], rgfd[j].lpszFlag, GetLastError());
                DumpFailRECT(rc);
                obj.fail();
            }
        }
    }

    m_Surfaces->ReleaseSurface(hDest);

    return obj.status();
}


DWORD _stressfont_t::exttextout()
{
    _stressrun_t obj(L"_stressfont_t::exttextout");

    FLAG_DETAILS rgfd[] = {
        NAMEVALENTRY(ETO_CLIPPED),
        NAMEVALENTRY(ETO_OPAQUE),
        NAMEVALENTRY(ETO_OPAQUE | ETO_CLIPPED)
    };
    const INT LOOP_COUNT = RANDOM_INT(LOOPCOUNT_MAX, LOOPCOUNT_MIN);
    HDC hDest;    
    int *lpDx = NULL, nDx[100];

    hDest = m_Surfaces->GetRandomSurface(TRUE);

    for (INT i = 0; i < LOOP_COUNT; i++)
    {
        INT j = rand() % ARRAY_SIZE(rgfd);
        INT isz = rand() % STR_COUNT;
        RECT rc;
        POINT pt;

        SetRectEmpty(&rc);
        memset(&pt, 0, sizeof pt);

        LogVerbose(_T("_stressfont_t::exttextout - Calling RandomRect...\r\n"));

        if (!RandomRect(&rc, &_rc))
        {
            LogWarn2(_T("#### _stressfont_t::drawtext - RandomRect failed ####\r\n"));
            continue;
        }
        else
        {
            if (RANDOM_CHOICE)
            {
                pt = RandomPoint(&rc);    // within random rect only
            }
            else
            {
                pt = RandomPoint(&_rc);    // within window client area
            }
        }

        LogVerbose(_T("_stressfont_t::exttextout - Calling ExtTextOut(%s, %s)...\r\n"),
            g_rgszStr[isz], rgfd[j].lpszFlag);

        if(0x80000000 == SetTextCharacterExtra(hDest, (rand() % 32) - 16))
        {
            LogWarn2(_T("#### _stressfont_t::drawtext - SetTextCharacterExtra failed ####\r\n"));
            continue;
        }

        if(RANDOM_CHOICE)
        {
            if(_tcslen(g_rgszStr[isz]) < 100)
            {
                lpDx = nDx;
                // set it to 1 or -1 (moving the characters closer or further apart by 1
                for(int k = 0; k < (int) _tcslen(g_rgszStr[isz]); k++)
                    nDx[k] = (rand() % 3) - 1;
            }
            else
            {
                LogWarn2(_T("#### _stressfont_t::drawtext - Insufficient space in nDx ####\r\n"));
                continue;
            }
        }

        SetLastError(ERROR_SUCCESS);

        if (!ExtTextOut(hDest, pt.x, pt.y, rgfd[j].dwFlag, &rc, (LPTSTR)g_rgszStr[isz],
            _tcslen(g_rgszStr[isz]), NULL))
        {
            if(GetLastError() == ERROR_NOT_ENOUGH_MEMORY)
            {
                LogWarn1(_T("_stressfont_::exttextout - insufficient memory.\r\n"));
                obj.warning1();
            }
            else
            {
                LogFail(_T("#### _stressfont_t::exttextout - ExtTextOut(%s, %s) failed - Error: 0x%x ####\r\n"),
                    g_rgszStr[isz], rgfd[j].lpszFlag, GetLastError());
                DumpFailPOINT(pt);
                DumpFailRECT(rc);
                obj.fail();
            }
        }
    }

    m_Surfaces->ReleaseSurface(hDest);

    return obj.status();
}

