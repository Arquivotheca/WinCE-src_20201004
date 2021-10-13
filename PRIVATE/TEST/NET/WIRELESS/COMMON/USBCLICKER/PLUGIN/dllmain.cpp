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
// ----------------------------------------------------------------------------
//
// Use of this source code is subject to the terms of the Microsoft end-user
// license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
// If you did not accept the terms of the EULA, you are not authorized to use
// this source code. For a copy of the EULA, please see the LICENSE.RTF on your
// install media.
//
// ----------------------------------------------------------------------------
//
// DllMain for the USBClickerController plugin.
//
// ----------------------------------------------------------------------------

#include "USBClickerController_t.hpp"

// Device-access synchronization mutex:
ce::mutex g_USBClickerMutex;

using namespace ce::qa;

// ----------------------------------------------------------------------------
//
// Windows CE specific code:
//
#ifdef UNDER_CE
BOOL WINAPI
DllMain(
    HANDLE hInstance,
    ULONG  dwReason,
    LPVOID lpReserved)
{
   return TRUE;
}
#endif /* ifdef UNDER_CE */

// ----------------------------------------------------------------------------
//
// Creates a new USBClickerController object.
//
DWORD WINAPI
CreateDeviceController(
    const TCHAR         *pDeviceType,
    const TCHAR         *pDeviceName,
    DeviceController_t **ppDevice,
    ce::tstring         *pErrorMessage)
{
    DWORD result;

    // Create the synchronization mutex one time.
    static const TCHAR * const  s_MutexName = TEXT("USBClickerMutex");
    static ce::critical_section s_MutexLocker;
    static bool                 s_MutexCreated = false;
    if (!s_MutexCreated)
    {
        ce::gate<ce::critical_section> locker(s_MutexLocker);
        if (!s_MutexCreated)
        {
            if (g_USBClickerMutex.create(s_MutexName))
            {
                result = GetLastError();
                if (result == ERROR_ALREADY_EXISTS)
                    result =  ERROR_SUCCESS;
            }
            else
            {
                result = GetLastError();
                if (result == ERROR_SUCCESS)
                    result =  ERROR_CREATE_FAILED;
            }
            if (ERROR_SUCCESS == result)
                s_MutexCreated = true;
            else
            {
                WiFUtils::FmtMessage(pErrorMessage,
                         TEXT("Cannot create mutex \"%s\": %s"),
                         s_MutexName, Win32ErrorText(result));
                return result;
            }
        }
    }

    // Create the clicker-controller.
   *ppDevice = new USBClickerController_t(pDeviceType, pDeviceName);
    if (NULL == *ppDevice)
    {
        WiFUtils::FmtMessage(pErrorMessage,
                 TEXT("Cannot allocate \"%s-%s\" USBClicker"),
                 pDeviceType, pDeviceName);
        return ERROR_OUTOFMEMORY;
    }
    return ERROR_SUCCESS;

}

