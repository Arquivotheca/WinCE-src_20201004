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

#include <winsock2.h>
#include <windows.h>
#include <netmain.h>
#include <cmnprint.h>

#include <APPool_t.hpp>
#include <WiFUtils.hpp>

WCHAR *gszMainProgName = L"ManConf";
DWORD  gdwMainInitFlags = 0;

using namespace ce::qa;

// Main application
int 
mymain(int argc, TCHAR **argv) 
{
    HRESULT hr;
    DWORD result;
    APPool_t apList;
        
    NetLogInitWrapper(gszMainProgName, LOG_DETAIL, NETLOG_DEF, FALSE);
    CmnPrint_Initialize();
	CmnPrint_RegisterPrintFunctionEx(PT_LOG,     NetLogDebug,   FALSE);
 	CmnPrint_RegisterPrintFunctionEx(PT_WARN1,   NetLogWarning, FALSE);
 	CmnPrint_RegisterPrintFunctionEx(PT_WARN2,   NetLogWarning, FALSE);
	CmnPrint_RegisterPrintFunctionEx(PT_FAIL,    NetLogError,   FALSE);
	CmnPrint_RegisterPrintFunctionEx(PT_VERBOSE, NetLogMessage, FALSE);

    // Initialize winsock.
    WSADATA wsaData;
	result = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (0 != result)
    {
        LogError(TEXT("WSAStartup failed: %s"), Win32ErrorText(result));
        return -1;
    }
    
    // tell the pool to read the initial configuration from registry
    hr = apList.LoadAPControllers(TEXT("localhost"),TEXT("33331"));
    if (FAILED(hr))
    {
        LogError(TEXT("Initial configuration failed: %s"),
                 HRESULTErrorText(hr));
        goto Cleanup;
    }

    // enumerate APs
    APController_t *firstAP = NULL;
    for (int apx = 0 ; apx < apList.size() ; ++apx)
    {
        APController_t *ap = apList.GetAP(apx);
    
        LogDebug(TEXT("Found AP \"%s\": SSID %s"),
                 &ap->GetAPName()[0], 
                 &ap->GetSsid()[0]);
        
        if (firstAP == NULL)
            firstAP = ap; // save a pointer to the first AP
    }

    // make sure there's at least one AP
    if (firstAP == NULL)
    {
        LogError(TEXT("No APs configured"));
        goto Cleanup;
    }

    // change configuration of firstAP
    LogDebug(TEXT("Re-configuring AP %s"), firstAP->GetAPName());
    if (FAILED(hr = firstAP->SetSecurityMode(APAuthWPA, APCipherTKIP))
     || FAILED(hr = firstAP->SetSsid(TEXT("MY_SSID")))
     || FAILED(hr = firstAP->SetRadius(TEXT("11.22.33.44"), 1213,
                                       TEXT("GuessThisPassword")))
     || FAILED(hr = firstAP->SetPassphrase(TEXT("My cool passphrase")))
     || FAILED(hr = firstAP->SaveConfiguration()))
    {
        LogError(TEXT("AP re-configuration failed: %s"),
                 HRESULTErrorText(hr));
        goto Cleanup;
    }

    for (int atten = 23 ; atten <= firstAP->GetMaxAttenuation() ; atten *= 2)
    {
        LogDebug(TEXT("Changing attenuation to %ddb"), atten);
        if (FAILED(hr = firstAP->SetAttenuation(atten))
         || FAILED(hr = firstAP->SaveConfiguration()))
        {
            LogError(TEXT("Change to %ddb attenuation failed: %s"), atten,
                     HRESULTErrorText(hr));
            goto Cleanup;
        }
    }
    
    LogDebug(TEXT("Test Application succeeded..."));

Cleanup:

    // Close the APControllers.
    apList.Clear();
        
    // Shut down WinSock.
	WSACleanup();
	return (ERROR_SUCCESS == result)? 0 : -1;
    NetLogCleanupWrapper();
}
