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
// Implementation of the WiFiBase_t class.
//
// ----------------------------------------------------------------------------

#include "WiFiBase_t.hpp"

#include <assert.h>
#include <netcmn.h>

#include <APPool_t.hpp>
#include "IPUtils.hpp"
#include "WiFiAccount_t.hpp"

using namespace ce;
using namespace ce::qa;

// ----------------------------------------------------------------------------

// APPool used for controlling the test's Access Points and RF Attenuators:
static APPool_t *s_pAPPool = NULL;

// WZC (Wireless Zero Config) interface:
static WZCService_t *s_pWZCService = NULL;

// ----------------------------------------------------------------------------
//
// Initializes or cleans up static resources.
//
void
WiFiBase_t::
StartupInitialize(void)
{
    WiFiAccount_t::StartupInitialize();
}

void
WiFiBase_t::
ShutdownCleanup(void)
{
    WiFiAccount_t::ShutdownCleanup();

    if (NULL != s_pAPPool)
    {
        delete s_pAPPool;
        s_pAPPool = NULL;
    }

    if (NULL != s_pWZCService)
    {
        delete s_pWZCService;
        s_pWZCService = NULL;
    }
}

// ----------------------------------------------------------------------------
//
// Configuration parameters:
//

//
// Selected wireless adapter name:
//
static TCHAR s_SelectedAdapter[64] = TEXT("");

const TCHAR *
WiFiBase_t::
GetSelectedAdapter(void)
{
    return s_SelectedAdapter;
}
void
WiFiBase_t::
SetSelectedAdapter(const TCHAR *pValue)
{
    SafeCopy(s_SelectedAdapter, pValue, COUNTOF(s_SelectedAdapter));
    s_pWZCService = NULL;
}

//
// LAN APControl server host-address:
//
const  TCHAR WiFiBase_t::DefaultLANServerHost[]         = TEXT("157.56.97.94");
static TCHAR                  s_LANServerHost[MAX_PATH] = TEXT("157.56.97.94");

const TCHAR *
WiFiBase_t::
GetLANServerHost(void)
{
    return s_LANServerHost;
}
void
WiFiBase_t::
SetLANServerHost(const TCHAR *pValue)
{
    SafeCopy(s_LANServerHost, pValue, COUNTOF(s_LANServerHost));
    s_pAPPool = NULL;
}

//
// LAN APControl server port-number:
//
const  TCHAR WiFiBase_t::DefaultLANServerPort[]   = TEXT("33331");
static TCHAR                  s_LANServerPort[30] = TEXT("33331");

const TCHAR *
WiFiBase_t::
GetLANServerPort(void)
{
    return s_LANServerPort;
}
void
WiFiBase_t::
SetLANServerPort(const TCHAR *pValue)
{
    SafeCopy(s_LANServerPort, pValue, COUNTOF(s_LANServerPort));
    s_pAPPool = NULL;
}

//
// WiFi APControl server host-address:
//
const  TCHAR WiFiBase_t::DefaultWiFiServerHost[]         = TEXT("10.10.0.1");
static TCHAR                  s_WiFiServerHost[MAX_PATH] = TEXT("10.10.0.1");

const TCHAR *
WiFiBase_t::
GetWiFiServerHost(void)
{
    return s_WiFiServerHost;
}
void
WiFiBase_t::
SetWiFiServerHost(const TCHAR *pValue)
{
    SafeCopy(s_WiFiServerHost, pValue, COUNTOF(s_WiFiServerHost));
    s_pAPPool = NULL;
}

//
// WiFi APControl server port-number:
//
const  TCHAR WiFiBase_t::DefaultWiFiServerPort[]   = TEXT("33331");
static TCHAR                  s_WiFiServerPort[30] = TEXT("33331");

const TCHAR *
WiFiBase_t::
GetWiFiServerPort(void)
{
    return s_WiFiServerPort;
}
void
WiFiBase_t::
SetWiFiServerPort(const TCHAR *pValue)
{
    SafeCopy(s_WiFiServerPort, pValue, COUNTOF(s_WiFiServerPort));
    s_pAPPool = NULL;
}

//
// WiFi APControl server Access Point configuration:
//
const WiFiConfig_t WiFiBase_t::DefaultWiFiServerAPConfig(
                                APAuthOpen,         // authentiction
                                APCipherClearText,  // encryption
                                APEapAuthTLS,       // EAP authentication
                                0,                  // WEP key index
                                TEXT(""),           // PSK or WEP key
                                TEXT(""));          // Access Point SSID

static WiFiConfig_t s_WiFiServerAPConfig =
   WiFiBase_t::DefaultWiFiServerAPConfig;
static bool         s_WiFiServerAPConfigured = false;

const WiFiConfig_t &
WiFiBase_t::
GetWiFiServerAPConfig(void)
{
    return s_WiFiServerAPConfig;
}

bool
WiFiBase_t::
IsWiFiServerAPConfigured(void)
{
    return s_WiFiServerAPConfigured;
}

// ----------------------------------------------------------------------------
//
// Displays the command-argument options.
//
void
WiFiBase_t::
PrintUsage(void)
{
    LogAlways(TEXT("\ngeneral options:"));
    LogAlways(TEXT("  -adapter   wireless adapter name (default first available)"));

    LogAlways(TEXT("\nAP configuration server options:"));
    LogAlways(TEXT("  -lHost     LAN server name/address (default \"%s\")"),
              DefaultLANServerHost);

    LogAlways(TEXT("  -lPort     LAN server port (default %s)"),
              DefaultLANServerPort);

    LogAlways(TEXT("\n  -wHost     WiFi server name/address (default \"%s\")"),
              DefaultWiFiServerHost);

    LogAlways(TEXT("  -wPort     WiFi server port (default %s)"),
              DefaultWiFiServerPort);

    LogAlways(TEXT("  -wSSID     WiFi server SSID (default \"%s\")"),
              DefaultWiFiServerAPConfig.GetSSIDName());
    LogAlways(TEXT("             If the SSID is specified, the LAN server\n")
              TEXT("             will not be used. Instead, all server\n")
              TEXT("             communication will use this Access Point.\n")
              TEXT("             Note that the following options must be\n")
              TEXT("             specified to enable associate with the\n")
              TEXT("             Access Point:"));

    LogAlways(TEXT("  -wAuth     WiFi server authentication (default \"%s\")"),
              DefaultWiFiServerAPConfig.GetAuthName());
    LogAlways(TEXT("             options:"));
    for (int ax = 0 ; ax < NumberAPAuthModes ; ++ax)
    {
        APAuthMode_e authMode = (APAuthMode_e) ax;
        LogAlways(TEXT("               %s"), APAuthMode2String(authMode));
    }

    LogAlways(TEXT("  -wEncr     WiFi server encryption-cipher (default \"%s\")"),
              DefaultWiFiServerAPConfig.GetCipherName());
    LogAlways(TEXT("             options:"));
    for (int cx = 0 ; cx < NumberAPCipherModes ; ++cx)
    {
        APCipherMode_e cipherMode = (APCipherMode_e) cx;
        LogAlways(TEXT("               %s"), APCipherMode2String(cipherMode));
    }

    LogAlways(TEXT("  -wEap      WiFi server EAP auth-mode (default \"%s\")"),
              DefaultWiFiServerAPConfig.GetEapAuthName());
    LogAlways(TEXT("             options:"));
    for (int cx = 0 ; cx < NumberAPEapAuthModes ; ++cx)
    {
        APEapAuthMode_e eapAuthMode = (APEapAuthMode_e) cx;
#ifndef EAP_MD5_SUPPORTED
        if (APEapAuthMD5 == eapAuthMode) continue;
#endif
        LogAlways(TEXT("               %s"), APEapAuthMode2String(eapAuthMode));
    }

    LogAlways(TEXT("  -wIndex    WiFi server WEP-key index (default %d)"),
              DefaultWiFiServerAPConfig.GetKeyIndex());

    LogAlways(TEXT("  -wKey      WiFi server encryption-key (default \"%s\")"),
              DefaultWiFiServerAPConfig.GetKey());
    LogAlways(TEXT("             Because the wireless server will be used to\n")
              TEXT("             retrieve authentication certificates, only\n")
              TEXT("             Open, Shared (WEP) and PSK can be used.\n")
              TEXT("             Examples: \n")
              TEXT("               (WEP-40) 01.02.03.04.05\n")
              TEXT("               (WEP-104) 12.34.56.78.90.AB.CD.EF.12.34.56.78.90\n")
              TEXT("               (PSK) qwertyuiopasdfghjklzxcvbnm\n")
              TEXT("               (PSK) 0123456789"));

    WiFiAccount_t::PrintUsage();
}

// ----------------------------------------------------------------------------
//
// Parses the DLL's command-line arguments.
//
DWORD
WiFiBase_t::
ParseCommandLine(int argc, TCHAR *argv[])
{
    TCHAR *pOptionStr;
    TCHAR *pOptionEnd;
    ULONG   optionIValue;

    APAuthMode_e    authMode;
    APCipherMode_e  cipherMode;
    APEapAuthMode_e eapAuthMode;

    // Define the array of accepted options.
    TCHAR *optionsArray[] = {
        TEXT("adapter"),
        TEXT("lHost"),
        TEXT("lPort"),
        TEXT("wHost"),
        TEXT("wPort"),
        TEXT("wSSID"),
        TEXT("wAuth"),
        TEXT("wEncr"),
        TEXT("wEap"),
        TEXT("wIndex"),
        TEXT("wKey"),
    };
    int numberOptions = COUNTOF(optionsArray);

    // Check each of the options.
    bool lanHostSelected  = false;
    for (int opx = 0 ; opx < numberOptions ; ++opx)
    {
        int wasOpt = WasOption(argc, argv, optionsArray[opx]);
        if (wasOpt < 0)
        {
            if (wasOpt < -1)
            {
                LogError(TEXT("Error parsing command line for option \"-%s\""),
                         optionsArray[opx]);
                return ERROR_INVALID_PARAMETER;
            }
            continue;
        }

        pOptionStr = NULL;
        int wasArg = GetOption(argc, argv, optionsArray[opx], &pOptionStr);

        switch (opx)
        {
            case 0: // -adapter - wirless adapter name
                if (wasArg < 0 || !pOptionStr || !pOptionStr[0])
                {
                    LogError(TEXT("No wireless adapter after \"-%s\""),
                             optionsArray[opx]);
                    return ERROR_INVALID_PARAMETER;
                }
                SetSelectedAdapter(pOptionStr);
                break;

            case 1: // -lHost - LAN server host
                if (wasArg < 0 || !pOptionStr || !pOptionStr[0])
                {
                    LogError(TEXT("No LAN server host-address after \"-%s\""),
                             optionsArray[opx]);
                    return ERROR_INVALID_PARAMETER;
                }
                SetLANServerHost(pOptionStr);
                lanHostSelected = true;
                break;

            case 2: // -lPort - LAN server port
                if (wasArg < 0 || !pOptionStr || !pOptionStr[0])
                {
                    LogError(TEXT("No LAN server port-number after \"-%s\""),
                             optionsArray[opx]);
                    return ERROR_INVALID_PARAMETER;
                }
                SetLANServerPort(pOptionStr);
                break;

            case 3: // -wHost - WiFi server host
                if (wasArg < 0 || !pOptionStr || !pOptionStr[0])
                {
                    LogError(TEXT("No WiFi server host-address after \"-%s\""),
                             optionsArray[opx]);
                    return ERROR_INVALID_PARAMETER;
                }
                SetWiFiServerHost(pOptionStr);
                break;

            case 4: // -wPort - WiFi server port
                if (wasArg < 0 || !pOptionStr || !pOptionStr[0])
                {
                    LogError(TEXT("No WiFi server port-number after \"-%s\""),
                             optionsArray[opx]);
                    return ERROR_INVALID_PARAMETER;
                }
                SetWiFiServerPort(pOptionStr);
                break;

            case 5: // -wSSID - WiFi server SSID
                if (wasArg < 0 || !pOptionStr || !pOptionStr[0])
                {
                    LogError(TEXT("No WiFi server SSID after \"-%s\""),
                             optionsArray[opx]);
                    return ERROR_INVALID_PARAMETER;
                }
                s_WiFiServerAPConfig.SetSSIDName(pOptionStr);
                s_WiFiServerAPConfigured = true;
                break;

            case 6: // -wAuth - WiFi server authentication
                if (wasArg < 0 || !pOptionStr || !pOptionStr[0])
                {
                    LogError(TEXT("No WiFi server auth-mode after \"-%s\""),
                             optionsArray[opx]);
                    return ERROR_INVALID_PARAMETER;
                }
                authMode = String2APAuthMode(pOptionStr);
                if (authMode == UnknownAPAuthMode)
                {
                    LogError(TEXT("WiFi server auth-mode \"%s\" unknown"),
                             pOptionStr);
                    return ERROR_INVALID_PARAMETER;
                }
                s_WiFiServerAPConfig.SetAuthMode(authMode);
                break;

            case 7: // -wEncr - WiFi server encryption-cipher
                if (wasArg < 0 || !pOptionStr || !pOptionStr[0])
                {
                    LogError(TEXT("No WiFi server cipher-mode after \"-%s\""),
                             optionsArray[opx]);
                    return ERROR_INVALID_PARAMETER;
                }
                cipherMode = String2APCipherMode(pOptionStr);
                if (cipherMode == UnknownAPCipherMode)
                {
                    LogError(TEXT("WiFi server cipher-mode \"%s\" unknown"),
                             pOptionStr);
                    return ERROR_INVALID_PARAMETER;
                }
                s_WiFiServerAPConfig.SetCipherMode(cipherMode);
                break;

            case 8: // -wEap - WiFi server EAP auth-mode
                if (wasArg < 0 || !pOptionStr || !pOptionStr[0])
                {
                    LogError(TEXT("No WiFi server EAP mode after \"-%s\""),
                             optionsArray[opx]);
                    return ERROR_INVALID_PARAMETER;
                }
                eapAuthMode = String2APEapAuthMode(pOptionStr);
                if (eapAuthMode == UnknownAPEapAuthMode)
                {
                    LogError(TEXT("WiFi server EAP auth-mode \"%s\" unknown"),
                             pOptionStr);
                    return ERROR_INVALID_PARAMETER;
                }
                s_WiFiServerAPConfig.SetEapAuthMode(eapAuthMode);
                break;

            case 9: // -wIndex - WiFi server WEP key index
                if (wasArg < 0 || !pOptionStr || !pOptionStr[0])
                {
                    LogError(TEXT("No WiFi server WEP key index after \"-%s\""),
                             optionsArray[opx]);
                    return ERROR_INVALID_PARAMETER;
                }
                optionIValue = _tcstoul(pOptionStr, &pOptionEnd, 10);
                if (NULL == pOptionEnd || TEXT('\0') != pOptionEnd[0]
                 || 0 > (int)optionIValue || (int)optionIValue > 4)
                {
                    LogError(TEXT("Bad WiFi server WEP-key index \"%s\"")
                             TEXT(" (min=1, max=4)"),
                             pOptionStr);
                    return ERROR_INVALID_PARAMETER;
                }
                if (optionIValue > 0)
                    optionIValue--;
                s_WiFiServerAPConfig.SetKeyIndex((int)optionIValue);
                break;

            case 10: // -wKey - WiFi server encryption-key
                if (wasArg < 0 || !pOptionStr || !pOptionStr[0])
                {
                    LogError(TEXT("No WiFi server cipher-key after \"-%s\""),
                             optionsArray[opx]);
                    return ERROR_INVALID_PARAMETER;
                }
                s_WiFiServerAPConfig.SetKey(pOptionStr);
                break;
        }
    }

    // Warn that the selected LAN-host will be ignored if they also
    // selected an SSID.
    if (lanHostSelected && IsWiFiServerAPConfigured())
    {
        LogWarn(TEXT("#####################################################\n")
                TEXT("Selected LAN server \"%s:%s\" will be ignored because\n")
                TEXT("all AP configuration will be handled by the wireless\n")
                TEXT("server \"%s:%s\" associated with SSID \"%s\".\n")
                TEXT("#####################################################"),
                GetLANServerHost(),  GetLANServerPort(),
                GetWiFiServerHost(), GetWiFiServerPort(),
                GetWiFiServerAPConfig().GetSSIDName());
    }

    return WiFiAccount_t::ParseCommandLine(argc, argv);
}

// ----------------------------------------------------------------------------
//
// Constructor.
//
WiFiBase_t::
WiFiBase_t(void)
{
    // nothing to do
}

// ----------------------------------------------------------------------------
//
// Destructor.
//
WiFiBase_t::
~WiFiBase_t(void)
{
    // nothing to do
}

// ----------------------------------------------------------------------------
//
// Initializes and retrieves the APPool object used for controlling
// the test's Access Points and RF Attenuators.
//
DWORD
WiFiBase_t::
InitAPPool(void)
{
    if (NULL == s_pAPPool)
    {
        DWORD result;
        const TCHAR *pServerHost,
                    *pServerPort;

        auto_ptr<APPool_t> pAPPool = new APPool_t;
        if (!pAPPool.valid())
        {
            LogError(TEXT("Can't allocate an APPool object"));
            return ERROR_OUTOFMEMORY;
        }

        // If they specified a WiFi Access Point SSID, connect to the
        // WiFi APControl server.
        if (IsWiFiServerAPConfigured())
        {
            pServerHost = GetWiFiServerHost();
            pServerPort = GetWiFiServerPort();

            const TCHAR *pSSIDName = s_WiFiServerAPConfig.GetSSIDName();
            LogDebug(TEXT("\nConnecting to WiFi AP-control server at %s"),
                     pSSIDName);

            result = InitWZCService();
            if (ERROR_SUCCESS != result)
                return result;

            WZCService_t *pWZCService = GetWZCService();
            assert(NULL != pWZCService);

            result = s_WiFiServerAPConfig.ConnectWiFi(pWZCService, NULL);
            if (ERROR_SUCCESS != result)
                return result;

            result = IPUtils::MonitorConnection(pWZCService, pSSIDName);
            if (ERROR_SUCCESS != result)
                return result;
        }

        // Otherwise, connect to the LAN APControl server.
        else
        {
            pServerHost = GetLANServerHost();
            pServerPort = GetLANServerPort();
        }

        // Get the list of the Access Points under control.
        SYSTEMTIME serverTime;
        HRESULT hr = pAPPool->LoadAPControllers(pServerHost,
                                                pServerPort,
                                                &serverTime);
        if (FAILED(hr))
        {
            LogError(TEXT("Can't get APControllers from \"%s:%s\": %s"),
                     pServerHost, pServerPort,
                     HRESULTErrorText(hr));
            return HRESULT_CODE(hr);
        }

        s_pAPPool = pAPPool.release();

        // If this is the first time we connected to the server, use its
        // time to set ours.
        static bool s_InitializedSystemTime = false;
        const _int64 MaximumAllowedTimeDiff = 5*60*1000; // 5 minutes
        if (!s_InitializedSystemTime)
        {
            s_InitializedSystemTime = true;

            FILETIME fileTime;
            ULARGE_INTEGER itime;
            SystemTimeToFileTime(&serverTime, &fileTime);
            memcpy(&itime, &fileTime, sizeof(itime));

           _int64 serverMS = _int64(itime.QuadPart) / _int64(10000);
           _int64 systemMS = Utils::GetWallClockTime();
           _int64   diffMS = abs(serverMS - systemMS);

            if (MaximumAllowedTimeDiff >= diffMS)
            {
                int diffSec = (int)(diffMS / 1000);
                LogDebug(TEXT("Server time only differs by %d:%02d"),
                         (int)(diffSec / 60),
                         (int)(diffSec % 60));
            }
            else
            if (SetSystemTime(&serverTime))
            {
                LogWarn(TEXT("Synchronized local system time to server time")
                        TEXT(" %04d/%02d/%02d %02d:%02d:%02d"),
                         (int)serverTime.wYear,
                         (int)serverTime.wMonth,
                         (int)serverTime.wDay,
                         (int)serverTime.wHour,
                         (int)serverTime.wMinute,
                         (int)serverTime.wSecond);
            }
            else
            {
                LogWarn(TEXT("Cannot set local system time to server time")
                        TEXT(" %04d/%02d/%02d %02d:%02d:%02d: %s"),
                         (int)serverTime.wYear,
                         (int)serverTime.wMonth,
                         (int)serverTime.wDay,
                         (int)serverTime.wHour,
                         (int)serverTime.wMinute,
                         (int)serverTime.wSecond,
                         Win32ErrorText(GetLastError()));
            }
        }
    }

    return ERROR_SUCCESS;
}

APPool_t *
WiFiBase_t::
GetAPPool(void)
{
    return s_pAPPool;
}

// ----------------------------------------------------------------------------
//
// Dissociates the WiFi APControl server.
// If the server was connected using a LAN connection, this does nothing.
// Otherwise, this disconnects the WiFi server and dissociates from the
// WiFi-server Access Point.
//
DWORD
WiFiBase_t::
DissociateAPPool(void)
{
    DWORD result = ERROR_SUCCESS;

    if (NULL != s_pAPPool && IsWiFiServerAPConfigured())
    {
        const TCHAR *pSSIDName = s_WiFiServerAPConfig.GetSSIDName();
        LogDebug(TEXT("\nClosing WiFi AP-control server connection at %s"),
                 pSSIDName);

        delete s_pAPPool;
        s_pAPPool = NULL;

        // Remove the SSID from the WZC Preferred Networks list.
        WZCService_t *pWZCService = GetWZCService();
        assert(NULL != pWZCService);

        const bool RemoveAllInstancesOfSSID = true;
        result = pWZCService->RemoveFromPreferredList(pSSIDName, NULL,
                                                      RemoveAllInstancesOfSSID);
        if (ERROR_NOT_FOUND == result)
        {
            LogWarn(TEXT("WiFi AP-control Access Point %s")
                    TEXT(" already dissociated from adapter %s"),
                    pSSIDName, pWZCService->GetAdapterName());
            result = ERROR_SUCCESS;
        }
    }

    return result;
}

// ----------------------------------------------------------------------------
//
// Initializes and retrieves the WZCService object used for accessing
// the WZC (Wireless Zero Config) facilities.
//
DWORD
WiFiBase_t::
InitWZCService(void)
{
    if (NULL == s_pWZCService)
    {
        auto_ptr<WZCService_t> pWZCService = new WZCService_t;
        if (!pWZCService.valid())
        {
            LogError(TEXT("Can't allocate a WZCService object"));
            return ERROR_OUTOFMEMORY;
        }

        // Select the wireless adapter.
        DWORD result = pWZCService->SetAdapterName(GetSelectedAdapter());
        if (ERROR_SUCCESS != result)
        {
            delete pWZCService;
            return result;
        }

        s_pWZCService = pWZCService.release();
    }

    return ERROR_SUCCESS;
}

WZCService_t *
WiFiBase_t::
GetWZCService(void)
{
    return s_pWZCService;
}

// ----------------------------------------------------------------------------
//
// Compares the specified AP's current security-mode settings with those
// specified and returns:
//     -2 - the the security modes are invalid
//     -1 - the AP can't handle those security modes
//      0 - the AP's current modes don't match
//     +1 - the AP's current modes match
//
static int
MatchAPSecurityModes(
    const APController_t &AP,
    const WiFiConfig_t   &Config)
{
    if (!ValidateSecurityModes(Config.GetAuthMode(),
                               Config.GetCipherMode(),
                               AP.GetCapabilitiesEnabled()))
        return ValidateSecurityModes(Config.GetAuthMode(),
                                     Config.GetCipherMode())? -1 : -2;

    int match = 0;
    if (AP.GetAuthMode()   == Config.GetAuthMode()
     && AP.GetCipherMode() == Config.GetCipherMode())
        match++;

    return match;
}

// ----------------------------------------------------------------------------
//
// Gets the APController with the current security mode configuration
// most closely matching that specified. If the optional list of AP
// names is supplied, limits the search to those Access Points.
// Returns ERROR_CALL_NOT_IMPLEMENTED if there are no Access Points
// which can handle the specified (valid) authentication modes.
//
DWORD
WiFiBase_t::
GetAPController(
    const WiFiConfig_t &Config,
    APController_t    **ppControl,
    const TCHAR * const pAPNames[],
    int                 NumberAPNames)
{
    assert(NULL != ppControl);

    // Make sure there are APControllers to choose from.
    APPool_t *pAPPool = GetAPPool();
    assert(NULL != pAPPool);
    if (pAPPool->size() == 0)
    {
        LogError(TEXT("No AP-Controllers found"));
        return ERROR_DEVICE_NOT_AVAILABLE;
    }

    // Scan each controller and select the "best".
    APController_t *pBest = NULL;
    APController_t *pBadModes = NULL;
    APController_t *pUnsupported = NULL;
    int             bestMatch = 0;

    // If they supplied a list of names, select from those APs.
    if (0 != NumberAPNames)
    {
        for (int nx = 0 ; nx < NumberAPNames ; ++nx)
        {
            assert(NULL != pAPNames[nx] && TEXT('\0') != pAPNames[nx][0]);

            APController_t *pAP = NULL;
            for (int apx = 0 ; apx < pAPPool->size() ; ++apx, pAP = NULL)
            {
                pAP = pAPPool->GetAP(apx);
                if (_tcsicmp(pAP->GetAPName(), pAPNames[nx]) == 0
                 && pAP->IsAccessPointValid())
                    break;
            }

            if (NULL == pAP)
            {
                LogWarn(TEXT("Can't find AP-name \"%s\" in AP-server's list"),
                        pAPNames[nx]);
            }
            else
            {
                // If they've asked for a security mode the AP can't
                // handle, skip it.
                int match = MatchAPSecurityModes(*pAP, Config);
                if (match < 0)
                {
                    if (match < -1)
                    {
                        if (NULL == pBadModes)
                            pBadModes = pAP;
                    }
                    else
                    {
                        if (NULL == pUnsupported)
                            pUnsupported = pAP;
                    }
                }

                // Otherwise, if it's the first or the best AP, use it.
                else
                if (pBest == NULL || bestMatch < match)
                {
                    pBest = pAP;
                    bestMatch = match;
                }
            }
        }
    }

    // Otherwise, select from the complete list.
    else
    {
        for (int apx = 0 ; apx < pAPPool->size() ; ++apx)
        {
            APController_t *pAP = pAPPool->GetAP(apx);

            // Ignore bad Access Points.
            if (!pAP->IsAccessPointValid())
                continue;

            // If they've asked for a security mode the AP can't
            // handle, skip it.
            int match = MatchAPSecurityModes(*pAP, Config);
            if (match < 0)
            {
                if (match < -1)
                {
                    if (NULL == pBadModes)
                        pBadModes = pAP;
                }
                else
                {
                    if (NULL == pUnsupported)
                        pUnsupported = pAP;
                }
            }

            // Otherwise, if it's the first or the best AP, use it.
            else
            if (pBest == NULL || bestMatch < match)
            {
                pBest = pAP;
                bestMatch = match;
            }
        }
    }

    // Issue an error message if there are no matching APs.
    if (pBest == NULL)
    {
        if (pUnsupported)
        {
            LogWarn(TEXT("No AP-Controllers can handle auth %s and cipher %s"),
                    Config.GetAuthName(),
                    Config.GetCipherName());
            return ERROR_CALL_NOT_IMPLEMENTED;
        }
        else
        if (pBadModes)
        {
            LogWarn(TEXT("Auth %s and cipher %s is an invalid combination"),
                    Config.GetAuthName(),
                    Config.GetCipherName());
            LogWarn(TEXT("Using AP \"%s\" with auth %s and cipher %s instead"),
                    pBadModes->GetAPName(),
                    APAuthMode2String(pBadModes->GetAuthMode()),
                    APCipherMode2String(pBadModes->GetCipherMode()));
            pBest = pBadModes;
        }
        else
        {
            TCHAR buff[MAX_PATH];
            buff[0] = TEXT('\0');
            for (int nx = 0 ; nx < NumberAPNames ; ++nx)
            {
                if (nx > 0) SafeAppend(buff, TEXT(","), MAX_PATH);
                SafeAppend(buff, pAPNames[nx], MAX_PATH);
            }
            LogError(TEXT("No matching APs in AP-Controllers list \"%s\""),
                     buff);
            return ERROR_DEVICE_NOT_AVAILABLE;
        }
    }

   *ppControl = pBest;
    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
