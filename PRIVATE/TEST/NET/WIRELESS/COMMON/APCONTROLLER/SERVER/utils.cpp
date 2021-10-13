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
// Implementation of the Utils class.
//
// ----------------------------------------------------------------------------

#include "Utils.hpp"
#include "AzimuthController_t.hpp"
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
TCHAR  Utils::DefaultRegistryKey[] = TEXT("Software\\Microsoft\\CETest\\APCTL");
TCHAR *Utils::      pRegistryKey   = Utils::DefaultRegistryKey;

// Server's host name or address:
TCHAR  Utils::DefaultServerHost[] = TEXT("localhost");
TCHAR *Utils::      pServerHost   = Utils::DefaultServerHost;

// Server's port number:
TCHAR  Utils::DefaultServerPort[] = TEXT("33331");
TCHAR *Utils::      pServerPort   = Utils::DefaultServerPort;

// TclSh path name:
TCHAR  Utils::DefaultShellExecName[] = TEXT("tclsh");
TCHAR *Utils::      pShellExecName   = Utils::DefaultShellExecName;

// ----------------------------------------------------------------------------
//
// Initializes or cleans up static resources:
void
Utils::
StartupInitialize(void)
{
}

void
Utils::
ShutdownCleanup(void)
{
}

// ----------------------------------------------------------------------------
//
// Generates the specified type of device-controller.
//
DWORD
Utils::
CreateController(
    const TCHAR         *pDeviceType,
    const TCHAR         *pDeviceName,
    DeviceController_t **ppDevice,
    ce::tstring         *pErrorMessage)
{
    DWORD result = ERROR_SUCCESS;
    auto_ptr<DeviceController_t> pDevice;

    // Generate the build-in devices.
    if (_tcsicmp(pDeviceType, TEXT("stop")) == 0
     || _tcsicmp(pDeviceType, TEXT("status")) == 0)
    {
       *ppDevice = NULL;
        return ERROR_SUCCESS;
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
                    if (ERROR_SUCCESS != result && pErrorMessage->length())
                        return result;
                    break;
            }
            
            pErrorMessage->clear();
            
            // Load the library.
            HMODULE dllHand;
            dllHand = GetModuleHandle(dllName);
            if (NULL == dllHand)
                dllHand = LoadLibrary(dllName);
            if (NULL == dllHand)
            {
                result = GetLastError();
                WiFUtils::FmtMessage(pErrorMessage,
                        TEXT("Cannot load device-controller plugin \"%s\": %s"),
                        &dllName[0], Win32ErrorText(result));
                continue;
            }

            // Look up the procedure.
#ifdef UNDER_CE
            static const WCHAR s_pProcName[] = L"CreateDeviceController";
#else
            static const char  s_pProcName[] =  "CreateDeviceController";
#endif
            DeviceControllerCreator_t *pProc =
            (DeviceControllerCreator_t *)GetProcAddress(dllHand, s_pProcName);
            if (NULL == pProc)
            {
                result = GetLastError();
                WiFUtils::FmtMessage(pErrorMessage,
#ifdef UNDER_CE
                         TEXT("Cannot find proc \"%ls\" in plugin \"%s\": %s"),
#else
                         TEXT("Cannot find proc \"%hs\" in plugin \"%s\": %s"),
#endif
                         s_pProcName, &dllName[0], Win32ErrorText(result));

                // If the proc isn't there, dump the library.
                FreeLibrary(dllHand);
                continue;
            }

            // Create the device.
            result = pProc(pDeviceType, pDeviceName, &pDevice, pErrorMessage);
            if (ERROR_SUCCESS != result)
                return result;

            break;
        }
    }

    if (!pDevice.valid())
    {
        WiFUtils::FmtMessage(pErrorMessage,
                 TEXT("Cannot allocate \"%s-%s\" device-controller"),
                 pDeviceType, pDeviceName);
        return ERROR_OUTOFMEMORY;
    }

   *ppDevice = pDevice.release();
    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Trims spaces from the front and back of the specified token.
//
TCHAR *
Utils::
TrimToken(TCHAR *pToken)
{
    for (; _istspace(*pToken) ; ++pToken) ;
    TCHAR *pEnd = pToken;
    for (; *pEnd != _T('\0') ; ++pEnd) ;
    while (pEnd-- != pToken)
        if (_istspace(*pEnd))
            *pEnd = _T('\0');
        else break;
    return pToken;
}

// ----------------------------------------------------------------------------
//
// Converts the specified token to an integer.
// Returns the default value if the token is empty or invalid.
//
int
Utils::
GetIntToken(TCHAR *pToken, int DefaultValue)
{
    pToken = Utils::TrimToken(pToken);
    long value = _tcstol(pToken, &pToken, 10);
    return (pToken != NULL && *pToken == _T('\0'))? (int)value : DefaultValue;
}

// ----------------------------------------------------------------------------
//
// Runs the specified command and waits for it to finish.
//
DWORD
Utils::
RunCommand(
    const TCHAR *pProgram,
    const TCHAR *pCommand,
    DWORD        WaitTimeMS)
{
    DWORD result = ERROR_SUCCESS;

    LogDebug(TEXT("Starting program \"%s\" with command-line \"%s\""),
             pProgram, pCommand);

    tstring cmdBuff;
    if (!cmdBuff.assign(pCommand))
    {
        LogError(TEXT("Cannot allocate memory to start \"%s\""), pProgram);
        return ERROR_OUTOFMEMORY;
    }

    PROCESS_INFORMATION procInfo;
    memset((void*)&procInfo, 0, sizeof(procInfo));

    long startTime = GetTickCount();

    if (!CreateProcess(pProgram,   // LPCWSTR lpszImageName, 
                       cmdBuff.get_buffer(), // LPWSTR  lpszCmdLine, 
                       NULL,       // LPSECURITY_ATTRIBUTES lpsaProcess, 
                       NULL,       // LPSECURITY_ATTRIBUTES lpsaThread, 
                       FALSE,      // BOOL fInheritHandles,
                       0,          // DWORD fdwCreate, 
                       NULL,       // LPVOID lpvEnvironment, 
                       NULL,       // LPWSTR lpszCurDir, 
                       NULL,       // LPSTARTUPINFOW lpsiStartInfo, 
                      &procInfo))  // LPPROCESS_INFORMATION lppiProcInfo
    {
        result = GetLastError();
        LogError(TEXT("Cannot start \"%s\": %s"),
                 pProgram, Win32ErrorText(result));
    }
    else
    {
        result = WaitForSingleObject(procInfo.hProcess, WaitTimeMS);

        long numTicks = WiFUtils::SubtractTickCounts(GetTickCount(), startTime);
        double duration = (double)numTicks / 1000.0;

        if (WAIT_OBJECT_0 == result)
        {
            result = ERROR_SUCCESS;
            DWORD exitCode = 0;
            GetExitCodeProcess(procInfo.hProcess, &exitCode);
            LogDebug(TEXT("Prog \"%s\" ended in %.02f seconds with exit %u"),
                     pProgram, duration, exitCode);
        }
        else
        {
            if (WAIT_TIMEOUT == result)
            {
                result = ERROR_TIMEOUT;
                LogError(TEXT("Prog \"%s\" did not stop within %.02f seconds"),
                         pProgram, duration);
            }
            else
            {
                result = GetLastError();
                LogError(TEXT("Prog \"%s\" failed after %.02f seconds: %s"),
                         pProgram, duration, Win32ErrorText(result));
            }

            TerminateProcess(procInfo.hProcess, result);
        }

        CloseHandle(procInfo.hProcess);
        CloseHandle(procInfo.hThread);
    }

    return result;
}

// ----------------------------------------------------------------------------
