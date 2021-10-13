//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this sample source code is subject to the terms of the Microsoft
// license agreement under which you licensed this sample source code. If
// you did not accept the terms of the license agreement, you are not
// authorized to use this sample source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the LICENSE.RTF on your install media or the root of your tools installation.
// THE SAMPLE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//

#include "stdafx.h"

HRESULT MusicRpcConnection::GetPlayerName(LPWSTR pszName, DWORD* length)
{
    HRESULT hr = S_OK;
    CBREx((*length) > 0, E_FAIL);
    CPREx(pszName, E_FAIL);
    CHR(ReadRegistry(MID_MUSIC_PLAYER_NAME, pszName, length));
Error:
    return hr;
}

HRESULT MusicRpcConnection::GetPlayerEndpoint(LPWSTR pszName, DWORD* length)
{
    HRESULT hr = S_OK;
    CBREx((*length) > 0, E_FAIL);
    CPREx(pszName, E_FAIL);
    CHR(ReadRegistry(MID_MUSIC_PLAYER_ENDPOINT, pszName, length));
Error:
    return hr;
}

HRESULT VideoRpcConnection::GetPlayerName(LPWSTR pszName, DWORD* length)
{
    HRESULT hr = S_OK;
    CBREx((*length) > 0, E_FAIL);
    CPREx(pszName, E_FAIL);
    CHR(ReadRegistry(MID_VIDEO_PLAYER_NAME, pszName, length));
Error:
    return hr;
}

HRESULT VideoRpcConnection::GetPlayerEndpoint(LPWSTR pszName, DWORD* length)
{
    HRESULT hr = S_OK;
    CBREx((*length) > 0, E_FAIL);
    CPREx(pszName, E_FAIL);
    CHR(ReadRegistry(MID_VIDEO_PLAYER_ENDPOINT, pszName, length));
Error:
    return hr;
}

HRESULT PhotoRpcConnection::GetPlayerName(LPWSTR pszName, DWORD* length)
{
    HRESULT hr = S_OK;
    CBREx((*length) > 0, E_FAIL);
    CPREx(pszName, E_FAIL);
    CHR(ReadRegistry(MID_PHOTO_VIEWER_NAME, pszName, length));
Error:
    return hr;
}

HRESULT PhotoRpcConnection::GetPlayerEndpoint(LPWSTR pszName, DWORD* length)
{
    HRESULT hr = S_OK;
    CBREx((*length) > 0, E_FAIL);
    CPREx(pszName, E_FAIL);
    CHR(ReadRegistry(MID_PHOTO_VIEWER_ENDPOINT, pszName, length));
Error:
    return hr;
}