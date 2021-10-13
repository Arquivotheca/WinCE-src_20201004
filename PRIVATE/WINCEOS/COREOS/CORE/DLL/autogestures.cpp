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

extern "C"
BOOL 
WINAPI 
GetAnimateMessageInfo(
    __in HWND hWnd, 
    __in WPARAM wParam, 
    __in LPARAM lParam, 
    __out LPANIMATEMESSAGEINFO pAnimateMessageInfo
    )
{
    ULONGLONG ullPacked = (((ULONGLONG)lParam) << 32) | wParam;
    const BYTE c_bBitsPerPosition = 28;
    const BYTE c_bBitsPerAnimationId = 8;
    const DWORD c_dwSignMask = (1 << (c_bBitsPerPosition - 1));
    const DWORD c_dwPositionMask = ((1 << c_bBitsPerPosition) - 1);

    if (NULL == pAnimateMessageInfo || sizeof(ANIMATEMESSAGEINFO) != pAnimateMessageInfo->cbSize)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    pAnimateMessageInfo->nVPixelPosition = (c_dwPositionMask & static_cast<int>(ullPacked));
    // Copy the sign bit
    if (0 != (c_dwSignMask & static_cast<int>(ullPacked)))
    {
        pAnimateMessageInfo->nVPixelPosition |= ((-1) << c_bBitsPerPosition);
    }
    ullPacked >>= c_bBitsPerPosition;
    pAnimateMessageInfo->nHPixelPosition = (c_dwPositionMask & static_cast<int>(ullPacked));
    // Copy the sign bit
    if (0 != (c_dwSignMask & static_cast<int>(ullPacked)))
    {
        pAnimateMessageInfo->nHPixelPosition |= ((-1) << c_bBitsPerPosition);
    }
    ullPacked >>= c_bBitsPerPosition;
    pAnimateMessageInfo->dwAnimationID = static_cast<DWORD>(ullPacked);

    return TRUE;
}
