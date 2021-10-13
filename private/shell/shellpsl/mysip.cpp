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
#include <windows.h>
#include <aygshell.h>

typedef int  (*PFNSIPENUMIM)( IMENUMPROC );
typedef BOOL (*PFNSIPGETCURRENTIM)(CLSID *);
typedef BOOL (*PFNSIPSETCURRENTIM)(CLSID *);
typedef BOOL (*PFNSIPGETINFO)(SIPINFO *);
typedef BOOL (*PFNSIPSHOWIM)(DWORD);
typedef BOOL (*PFNSIPSETINFO)( SIPINFO * );
typedef BOOL (*PFNSIPREGISTERNOTIFICATION)(HWND);
typedef BOOL (*PFNSIPSETDEFAULTRECT)(RECT *);

PFNSIPENUMIM g_pfnSipEnumIM = NULL;
PFNSIPGETCURRENTIM g_pfnSipGetCurrentIM = NULL;
PFNSIPSETCURRENTIM g_pfnSipSetCurrentIM = NULL;
PFNSIPGETINFO g_pfnSipGetInfo = NULL;
PFNSIPSHOWIM g_pfnSipShowIM = NULL;
PFNSIPSETINFO g_pfnSipSetInfo = NULL;
PFNSIPREGISTERNOTIFICATION g_pfnSipRegisterNotification = NULL;
PFNSIPSETDEFAULTRECT g_pfnSipSetDefaultRect = NULL;

extern HINSTANCE g_hCoreDLL;

int pfnSipEnumIM( IMENUMPROC pEnumIMProc )
{
    int iRet = 0;

    if ( g_hCoreDLL != NULL )
    {
        if ( !g_pfnSipEnumIM )
        {
            g_pfnSipEnumIM = (PFNSIPENUMIM)GetProcAddress( g_hCoreDLL, _T( "SipEnumIM" ) );
        }
        if ( g_pfnSipEnumIM != NULL )
        {
            iRet = g_pfnSipEnumIM( pEnumIMProc );
        }
    }

    return iRet;
}

BOOL pfnSipGetCurrentIM( CLSID *pClsid )
{
    BOOL bRet = FALSE;

    if ( g_hCoreDLL != NULL )
    {
        if ( !g_pfnSipGetCurrentIM )
        {
            g_pfnSipGetCurrentIM = (PFNSIPGETCURRENTIM)GetProcAddress( g_hCoreDLL, _T( "SipGetCurrentIM" ) );
        }
        if ( g_pfnSipGetCurrentIM != NULL )
        {
            bRet = g_pfnSipGetCurrentIM( pClsid );
        }
    }

    return bRet;
}

BOOL pfnSipSetCurrentIM( CLSID *pClsid )
{
    BOOL bRet = FALSE;

    if ( g_hCoreDLL != NULL )
    {
        if ( !g_pfnSipSetCurrentIM )
        {
            g_pfnSipSetCurrentIM = (PFNSIPSETCURRENTIM)GetProcAddress( g_hCoreDLL, _T( "SipSetCurrentIM" ) );
        }
        if ( g_pfnSipSetCurrentIM != NULL )
        {
            bRet = g_pfnSipSetCurrentIM( pClsid );
        }
    }

    return bRet;
}

BOOL pfnSipShowIM( DWORD dwFlag )
{
    BOOL bRet = FALSE;

    if ( g_hCoreDLL != NULL )
    {
        if ( !g_pfnSipShowIM )
        {
            g_pfnSipShowIM = (PFNSIPSHOWIM)GetProcAddress( g_hCoreDLL, _T( "SipShowIM" ) );
        }
        if ( g_pfnSipShowIM != NULL )
        {
            bRet = g_pfnSipShowIM( dwFlag );
        }
    }

    return bRet;
}

BOOL pfnSipGetInfo( SIPINFO *pSipInfo )
{
    BOOL bRet = FALSE;

    if ( g_hCoreDLL != NULL )
    {
        if ( !g_pfnSipGetInfo )
        {
            g_pfnSipGetInfo = (PFNSIPGETINFO)GetProcAddress( g_hCoreDLL, _T( "SipGetInfo" ) );
        }
        if ( g_pfnSipGetInfo != NULL )
        {
            bRet = g_pfnSipGetInfo( pSipInfo );
        }
    }

    return bRet;
}


BOOL pfnSipSetInfo( SIPINFO *pSipInfo )
{
    BOOL bRet = TRUE;

    if ( g_hCoreDLL != NULL )
    {
        if ( !g_pfnSipSetInfo )
        {
            g_pfnSipSetInfo = (PFNSIPSETINFO)GetProcAddress( g_hCoreDLL, _T( "SipSetInfo" ) );
        }
        if ( g_pfnSipSetInfo != NULL )
        {
            bRet = g_pfnSipSetInfo( pSipInfo );
        }
    }

    return bRet;
}

BOOL pfnSipRegisterNotification( HWND hWnd )
{
    BOOL bRet = FALSE;
    if ( g_hCoreDLL != NULL )
    {
        if ( !g_pfnSipRegisterNotification )
        {
            g_pfnSipRegisterNotification = (PFNSIPREGISTERNOTIFICATION)GetProcAddress( g_hCoreDLL, _T( "SipRegisterNotification" ) );
        }
        if ( g_pfnSipRegisterNotification != NULL )
        {
            bRet = g_pfnSipRegisterNotification( hWnd );
        }
    }

    return bRet;
}

BOOL pfnSipSetDefaultRect( RECT *pRect )
{
    BOOL bRet = FALSE;
    if ( g_hCoreDLL != NULL )
    {
        if ( !g_pfnSipSetDefaultRect )
        {
            g_pfnSipSetDefaultRect = (PFNSIPSETDEFAULTRECT)GetProcAddress( g_hCoreDLL, _T( "SipSetDefaultRect" ) );
        }
        if ( g_pfnSipSetDefaultRect != NULL )
        {
            bRet = g_pfnSipSetDefaultRect( pRect );
        }
    }

    return bRet;
}
