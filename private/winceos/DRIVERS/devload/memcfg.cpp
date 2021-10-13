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

//
// This module defines helper routines for determining device driver memory configuration,
// including:
//  DDKReg_GetWindowInfo
//

#include <windows.h>
#include <ceddk.h>
#include <ddkreg.h>
#include <pcibus.h>
#include "ddkreg_i.h"


// This routine reads a list of windows out of the registry.  Windows are specified with a
// value specifying base address(s) and a value specifying length(s).  If the list has only
// one entry the registry values are of type REG_DWORD.  Otherwise they are REG_MULTI_SZ values
// with each base or length represented in hex.  In the REG_MULTI_SZ case, the base and length
// lists must have the same number of entries.
static DWORD
RegReadWindowList(HKEY hk, LPTSTR pszBaseName, LPTSTR pszLenName, DWORD dwMaxLen, LPDWORD pdwListLen, PDEVICEWINDOW ppciw)
{
    DWORD dwStatus, dwBaseType, dwLenType, dwSize;
    union { // Solve Alignment issue.
        WCHAR cBaseList[2 * DEVKEY_LEN];
        DWORD dwBaseValue;
    };
    union {
        WCHAR cLenList[2 * DEVKEY_LEN];
        DWORD dwLenValue;
    };

    // read the two lists -- note that it is possible neither will exist, indicating this
    // kind of window is not present on the device.
    dwSize = sizeof(cBaseList);
    dwStatus = RegQueryValueEx(hk, pszBaseName, NULL, &dwBaseType, (LPBYTE) cBaseList, &dwSize);
    if(dwStatus != ERROR_SUCCESS) {
        dwBaseType = REG_NONE;
    }
    dwSize = sizeof(cLenList);
    dwStatus = RegQueryValueEx(hk, pszLenName, NULL, &dwLenType, (LPBYTE) cLenList, &dwSize);
    if(dwStatus != ERROR_SUCCESS) {
        dwLenType = REG_NONE;
    }

    // now check data types
    if(dwBaseType != dwLenType) {
        dwStatus = ERROR_INVALID_DATA;
        goto done;
    } 
    else if(dwBaseType == REG_NONE) {
        // window information not found
        *pdwListLen = 0;
        dwStatus = ERROR_SUCCESS;
    } 
    else if(dwBaseType == REG_DWORD) {
        // one window present
        *pdwListLen = 1;
        ppciw[0].dwBase = dwBaseValue ;
        ppciw[0].dwLen = dwLenValue;
    } else {
        // multiple windows present, so we have to parse the base and length lists and 
        // sanity check them.  We will ignore extra entries.
        DWORD i;
        PWCHAR bp;
        dwSize = 0;
        cBaseList[_countof(cBaseList) - 1] = cBaseList[_countof(cBaseList) - 2] = 0 ; // add multi string end if it is not.
        cLenList[_countof(cLenList) - 1] = cLenList[ _countof(cLenList) - 2] = 0 ;
        for (bp = cBaseList, i = 0; i < dwMaxLen && *bp != 0; i++) {
            PWCHAR ep;
            // parse the value
            ppciw[i].dwBase = wcstoul(bp, &ep, 16);

            // proceed to the next value if we parsed this one ok
            if (*ep == 0) {
                dwSize++;
                bp = ep + 1;
            }
        }

        // record the list size and read the length list
        *pdwListLen = dwSize;
        dwSize = 0;
        for (bp = cLenList, i = 0; i < dwMaxLen && *bp != 0; i++) {
            PWCHAR ep;
            // parse the value
            ppciw[i].dwLen = wcstoul(bp, &ep, 16);

            // proceed to the next value if we parsed this one ok
            if (*ep == 0) {
                dwSize++;
                bp = ep + 1;
            }
        }

        // make sure the lists are the same length
        if(dwSize != *pdwListLen) {
            dwStatus = ERROR_INVALID_DATA;
        }
    }

done:
    return dwStatus;
}

//
// Fills in the DDKWINDOWINFO structure with information it reads from the registry.  This
// information includes everything a driver needs to map its memory windows.
//
// RETURN VALUE:
//  ERROR_SUCCESS means that the DDKWINDOWINFO structure has been populated successfully.
//  ERROR_INVALID_PARAMETER may indicate a size problem with the PDDKWINDOWINFO structure.
//  ERROR_INVALID_DATA may indicate that a registry value had an unexpected type, or that
//      device window address and length lists did not have the same length
//  Other return codes are defined in winerror.h.
//
DWORD WINAPI
DDKReg_GetWindowInfo(HKEY hk, PDDKWINDOWINFO pwi)
{
    DWORD dwStatus, dwSize, dwType;
    int i;
    REGQUERYPARAMS rvWindowInfo[] = {
        { PCIBUS_BUSNUMBER_VALNAME, PCIBUS_BUSNUMBER_VALTYPE, &pwi->dwBusNumber, sizeof(pwi->dwBusNumber) },
        { PCIBUS_IFCTYPE_VALNAME, PCIBUS_IFCTYPE_VALTYPE, &pwi->dwInterfaceType, sizeof(pwi->dwInterfaceType) },
    };

    // sanity check parameters
    if(hk == NULL || pwi == NULL || pwi->cbSize != sizeof(DDKWINDOWINFO)) {
        dwStatus = ERROR_INVALID_PARAMETER;
        goto done;
    }

    // clear the structure
    dwSize = pwi->cbSize;
    memset(pwi, 0, dwSize);
    pwi->cbSize = dwSize;

    // read memory mapping and instance values
    pwi->dwInterfaceType = (DWORD)InterfaceTypeUndefined;
    pwi->dwBusNumber = 0;
    for(i = 0; i < _countof(rvWindowInfo); i++) {
        dwSize = rvWindowInfo[i].dwValSize;
        dwStatus = RegQueryValueEx(hk, rvWindowInfo[i].pszValName, NULL, &dwType, (LPBYTE) rvWindowInfo[i].pvVal, &dwSize);
        if(dwStatus == ERROR_SUCCESS && dwType != rvWindowInfo[i].dwValType) {
            dwStatus = ERROR_INVALID_DATA;
            goto done;
        }
    }

    // read memory and i/o windows
    dwStatus = RegReadWindowList(hk, PCIBUS_MEMBASE_VALNAME, PCIBUS_MEMLEN_VALNAME, _countof(pwi->memWindows), &pwi->dwNumMemWindows, pwi->memWindows);
    if(dwStatus != ERROR_SUCCESS) {
        goto done;
    }
    dwStatus = RegReadWindowList(hk, PCIBUS_IOBASE_VALNAME, PCIBUS_IOLEN_VALNAME, _countof(pwi->ioWindows), &pwi->dwNumIoWindows, pwi->ioWindows);
    if(dwStatus != ERROR_SUCCESS) {
        goto done;
    }
    
done:
    return dwStatus;
}
