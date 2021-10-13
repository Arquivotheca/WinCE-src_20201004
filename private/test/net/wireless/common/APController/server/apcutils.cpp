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
// Implementation of the APCUtils class.
//
// ----------------------------------------------------------------------------

#include "APCUtils.hpp"

#include <MemBuffer_t.hpp>
#include <PluginController_t.hpp>

#include "AzimuthController_t.hpp"
#include "CiscoAPController_t.hpp"
#include "HtmlAPController_t.hpp"
#include "WeinschelController_t.hpp"

#include <assert.h>
#include <tchar.h>

using namespace ce::qa;

// ----------------------------------------------------------------------------
//
// Configuration parameters:
//

// Configuration registry key:
TCHAR  APCUtils::DefaultRegistryKey[] = TEXT("Software\\Microsoft\\CETest\\APCTL");
TCHAR *APCUtils::      pRegistryKey   = APCUtils::DefaultRegistryKey;

// Server's host name or address:
TCHAR  APCUtils::DefaultServerHost[] = TEXT("localhost");
TCHAR *APCUtils::      pServerHost   = APCUtils::DefaultServerHost;

// Server's port number:
TCHAR  APCUtils::DefaultServerPort[] = TEXT("33331");
TCHAR *APCUtils::      pServerPort   = APCUtils::DefaultServerPort;

// TclSh path name:
TCHAR  APCUtils::DefaultShellExecName[] = TEXT("tclsh");
TCHAR *APCUtils::      pShellExecName   = APCUtils::DefaultShellExecName;

// ----------------------------------------------------------------------------
//
// Generates the specified type of device-controller.
//
DWORD
APCUtils::
CreateController(
    const TCHAR         *pDeviceType,
    const TCHAR         *pDeviceName,
    DeviceController_t **ppDevice)
{
    DWORD result = NO_ERROR;
    auto_ptr<DeviceController_t> pDevice;

    // Generate the build-in devices.
    if (_tcsicmp(pDeviceType, TEXT("stop")) == 0
     || _tcsicmp(pDeviceType, TEXT("status")) == 0)
    {
       *ppDevice = NULL;
        return NO_ERROR;
    }
    else
    if (_tcsicmp(pDeviceType, TEXT("Azimuth")) == 0)
    {
        pDevice = new AzimuthController_t(pDeviceType, pDeviceName);
    }
    else
    if (_tcsicmp(pDeviceType, TEXT("Buffalo")) == 0
     || _tcsicmp(pDeviceType, TEXT("DLink"))   == 0)
    {
        pDevice = new HtmlAPController_t(pDeviceType, pDeviceName);
    }
    else
    if( _tcsnicmp(pDeviceType, TEXT("Cisco"), 5)   == 0)
    {
        pDevice = new CiscoAPController_t(pDeviceType, pDeviceName);
    }
    else
    if (_tcsicmp(pDeviceType, TEXT("Weinschel")) == 0)
    {
        pDevice = new WeinschelController_t(pDeviceType, pDeviceName);
    }

    // If it's not one of the known types, assume it's a plugin name.
    else
    {
        static ce::critical_section s_PluginLocker;
        ce::gate<ce::critical_section> locker(s_PluginLocker);

        // Do this twice; once for the raw name and once after
        // appending "Plugin".
        ce::tstring dllName;
        for (int passno = 1 ;; ++passno)
        {
            if (!dllName.assign(pDeviceType))
                break;
            switch (passno)
            {
                case 2: 
                    if (!dllName.append(TEXT("Plugin")))
                        continue;
                    break;
                case 3:
                    if (NO_ERROR != result)
                        return result;
                    break;
            }
                        
            // Load the library.
            HMODULE dllHand;
            dllHand = GetModuleHandle(dllName);
            if (NULL == dllHand)
                dllHand = LoadLibrary(dllName);
            if (NULL == dllHand)
            {
                result = GetLastError();
                LogError(TEXT("Cannot load device-controller plugin \"%s\": %s"),
                        &dllName[0], Win32ErrorText(result));
                continue;
            }

            // Look up the procedure.
#ifdef UNDER_CE
            static const WCHAR s_pProcName[] = L"CreateDeviceController";
#else
            static const char  s_pProcName[] =  "CreateDeviceController";
#endif
            PluginControllerCreator_t *pProc =
            (PluginControllerCreator_t *)GetProcAddress(dllHand, s_pProcName);
            if (NULL == pProc)
            {
                result = GetLastError();
#ifdef UNDER_CE
                LogError(TEXT("Cannot find proc \"%ls\" in plugin \"%s\": %s"),
#else
                LogError(TEXT("Cannot find proc \"%hs\" in plugin \"%s\": %s"),
#endif
                         s_pProcName, &dllName[0], Win32ErrorText(result));

                // If the proc isn't there, dump the library.
                FreeLibrary(dllHand);
                continue;
            }

            // Create the device.
            MemBuffer_t errorBuffer;
            result = pProc(pDeviceType, pDeviceName, &pDevice, &errorBuffer);
            if (NO_ERROR != result)
            {
                ce::tstring errorMessage;
                errorBuffer.CopyOut(&errorMessage);
                LogError(errorMessage);
                return result;
            }

            break;
        }
    }

    if (!pDevice.valid())
    {
        LogError(TEXT("Cannot allocate \"%s-%s\" device-controller"),
                 pDeviceType, pDeviceName);
        return ERROR_OUTOFMEMORY;
    }

   *ppDevice = pDevice.release();
    return NO_ERROR;
}

// ----------------------------------------------------------------------------
