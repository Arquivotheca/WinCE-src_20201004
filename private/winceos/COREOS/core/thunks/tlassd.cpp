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
#include <../../../comm/security/lassd/lassd_ioctl.h>
#include <lass_private.h>
#include <lass.h>
#include <assert.h>
#include <MsgWaitHelper.hxx>

static HANDLE  v_hLASS=INVALID_HANDLE_VALUE;

bool LoadDriver()
{
    // We could leak by caling CreateFile twice under a race.
    // However this leak is cleaned up since services.exe makes sure
    // Close is called for every app calling CreateFile.

    if (v_hLASS == INVALID_HANDLE_VALUE)
    {
        v_hLASS = CreateFile(SYSTEM_LASSD_HANDLE_NAME,0,0,NULL,CREATE_ALWAYS,0,0);
    }

    if (v_hLASS  == INVALID_HANDLE_VALUE)
    {
        SetLastError(ERROR_SERVICE_NOT_ACTIVE);
        return false;
    }
    return true;
}

BOOL LASSClose(HLASS h)
{
    if (!LoadDriver())  return FALSE;

    lassd_srv_proxy lp(v_hLASS);

    return lp.LASSClose(h);
}

BOOL LASSGetResult(HLASS h)
{
    if (!LoadDriver())  return FALSE;

    lassd_srv_proxy lp(v_hLASS);

    return lp.LASSGetResult(h);
}

HLASS VerifyUserAsync (const GUID *AEKey, /* Authenication Event Identifier */
                LPCWSTR pwszAEDisplayText, /*Text Plugin will display, if null use from registry.*/
                HWND   hWndParent, /*Parent Window if Available-else use desktop window*/
                DWORD  dwOptions, /*Bitmask of possible options.*/
                PVOID pExtended /*Reserved, must be 0*/
                )

{
    if (!LoadDriver())  return NULL;
    lassd_srv_proxy lp(v_hLASS);

    HLASS  hVerifyUserRequest = lp.LASSVerifyUserAsync(
            AEKey,
            pwszAEDisplayText,
            hWndParent,
            dwOptions,
            pExtended
            );

    return hVerifyUserRequest;
}

BOOL VerifyUser(const GUID *AEKey, /* Authenication Event Identifier */
                LPCWSTR pwszAEDisplayText, /*Text Plugin will display, if null use from registry.*/
                HWND   hWndParent, /*Parent Window if Available-else use desktop window*/
                DWORD  dwOptions, /*Bitmask of possible options.*/
                PVOID pExtended /*Reserved, must be 0*/
                )
{

    // call verify 
    const HLASS hVerifyUserRequest  = VerifyUserAsync(
            AEKey,
            pwszAEDisplayText,
            hWndParent,
            dwOptions,
            pExtended
            );

    if (hVerifyUserRequest == NULL)
    {
        assert(GetLastError() != NULL); 
        return FALSE;
    }

    // wait for completion.
    const DWORD ret  = WaitForCompletionAndPumpMessages(hVerifyUserRequest);
    if (ret != WAIT_OBJECT_0)
    {
        LASSClose(hVerifyUserRequest);
        return FALSE;
    }

    // Pick up results
    const BOOL bRet = LASSGetResult(hVerifyUserRequest);

    // Make sure we're not picking up results too soon.
    assert (bRet || (!bRet && GetLastError() != ERROR_NOT_READY));

    // Cleanup
    const BOOL bRetClose=LASSClose(hVerifyUserRequest);
    assert(bRetClose);
    
    return bRet;

}

BOOL  LASSReloadConfig()
{
    if (!LoadDriver())  return FALSE;

    lassd_srv_proxy lp(v_hLASS);

    return lp.LASSReloadConfig();

}

BOOL  CreateEnrollmentConfigDialog(HWND hwndParent)
{

    if (!LoadDriver())  return FALSE;

    lassd_srv_proxy lp(v_hLASS);

    return lp.LASSEnroll(hwndParent);

}

BOOL LASSGetValue(DWORD ValueId,PVOID lpvOutBuffer, DWORD cbOutBuffer, DWORD* pcbReturned)
{
    if (!LoadDriver())  return FALSE;

    lassd_srv_proxy lp(v_hLASS);

    return lp.LASSGetValue(ValueId,lpvOutBuffer,cbOutBuffer,pcbReturned);

}

