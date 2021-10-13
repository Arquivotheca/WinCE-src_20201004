//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef _CELOGDEV_H_
#define _CELOGDEV_H_

#include <celog.h>

_inline void CELOG_ActivateDevice (LPCWSTR szName)
{
    if (IsCeLogZoneEnabled(CELZONE_BOOT_TIME)) {
        BYTE pTmp [MAX_PATH + sizeof (CEL_BOOT_TIME)];
        PCEL_BOOT_TIME pcl = (PCEL_BOOT_TIME) pTmp;
        WORD wLen = 0;

        pcl->dwAction = BOOT_TIME_DEV_ACTIVATE;

        if (szName) {
            wLen = wcslen (szName) + 1;
            wcscpy (pcl->szName, szName);        
        }

        CeLogData(TRUE, CELID_BOOT_TIME, (PVOID) pcl,
                  (WORD) (sizeof (CEL_BOOT_TIME) + (wLen * sizeof (WCHAR))),
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
