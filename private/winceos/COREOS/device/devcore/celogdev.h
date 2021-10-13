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
#ifndef _CELOGDEV_H_
#define _CELOGDEV_H_

#include <celog.h>

_inline void CELOG_ActivateDevice (LPCWSTR szName)
{
    if (IsCeLogZoneEnabled(CELZONE_BOOT_TIME)) {
        struct {
            union {
                CEL_BOOT_TIME cl;
                BYTE _b;  // Work around compiler warning on zero-length array
            } u;
            WCHAR _sz[MAX_PATH];  // Accessed through cl.szName
        } buf;
        size_t dwLen = 0;

        buf.u.cl.dwAction = BOOT_TIME_DEV_ACTIVATE;

        if (szName) {
            if (SUCCEEDED(StringCchLength(szName, MAX_PATH, &dwLen)) && dwLen > 0) {
#pragma prefast(suppress: 6202, "Buffer overrun")
                VERIFY(SUCCEEDED(StringCchCopyN(buf.u.cl.szName, _countof(buf._sz), szName, dwLen)));
                dwLen++;
            }
        }

        CeLogData(TRUE, CELID_BOOT_TIME, (PVOID) &buf.u.cl,
                  (WORD) (sizeof (CEL_BOOT_TIME) + (dwLen * sizeof (WCHAR))),
                  0, CELZONE_BOOT_TIME, 0, FALSE);
    }
}

_inline void CELOG_DeviceFinished (void)
{
    if (IsCeLogZoneEnabled(CELZONE_BOOT_TIME)) {
        CEL_BOOT_TIME cl;
        cl.dwAction = BOOT_TIME_DEV_FINISHED;

        CeLogData(TRUE, CELID_BOOT_TIME, (PVOID) &cl,
                  (WORD) (sizeof (CEL_BOOT_TIME)),
                  0, CELZONE_BOOT_TIME, 0, FALSE);
    }
}

#endif
