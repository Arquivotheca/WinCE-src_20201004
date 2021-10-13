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
#include <windows.h>
#include "diskio.h"
#include "storemgr.h"
#include "strsafe.h"
#include "pm.h"


BOOL SecureWipeAllVolumes()
{
    BOOL Result = FALSE;
    STOREINFO si;
    HANDLE hSearchStore, hStore, hSearchPart;
    PARTINFO pi;
    HANDLE hVolume;
    
    TCHAR szPath[MAX_PATH] = TEXT("");

    si.cbSize = sizeof(STOREINFO);
    hSearchStore = FindFirstStore(&si);    
    if (hSearchStore == INVALID_HANDLE_VALUE) 
    {
        return FALSE;
    }
    
    do 
    {
        hStore = OpenStore(si.szDeviceName);
        if (hStore != INVALID_HANDLE_VALUE) 
        {
            pi.cbSize = sizeof(PARTINFO);
            hSearchPart = FindFirstPartition(hStore, &pi);
            if (hSearchPart != INVALID_HANDLE_VALUE) 
            {
                do 
                {
                    if (FAILED(StringCchPrintf (szPath, MAX_PATH, TEXT("%s\\VOL:"), pi.szVolumeName)))
                    {
                        continue;
                    }

                    hVolume = CreateFile(szPath, 
                                         GENERIC_READ|GENERIC_WRITE, 
                                         0, 
                                         NULL, 
                                         OPEN_EXISTING, 
                                         0, 
                                         NULL); 
                    
                    if (hVolume == INVALID_HANDLE_VALUE)
                    {
                        continue;
                    }

                    if (DeviceIoControl(hVolume,
                                        IOCTL_DISK_SET_SECURE_WIPE_FLAG,
                                        NULL,0,
                                        NULL,0,
                                        NULL,NULL))
                    {
                        Result = TRUE;
                    }
                    
                } while(FindNextPartition(hSearchPart, &pi));

                FindClosePartition(hSearchPart);
            }
            CloseHandle( hStore);                       
        }
    } while(FindNextStore(hSearchStore, &si));
    
    FindCloseStore( hSearchStore);

    // Reboot the device if at least one volume implements setting the flag
    //
    if (Result)
    {
        SetSystemPowerState(NULL, POWER_STATE_RESET, POWER_FORCE);
    }
    
    return Result;
}
