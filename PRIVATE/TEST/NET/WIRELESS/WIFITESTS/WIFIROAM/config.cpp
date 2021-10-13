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
// Implementation of the Config class.
//
// ----------------------------------------------------------------------------

#include "Config.hpp"
#include "WiFiRoam_t.hpp"

#include <Utils.hpp>

#include <assert.h>
#include <auto_xxx.hxx>
#include <APPool_t.hpp>

using namespace ce::qa;

// Synchronization object:
static ce::critical_section s_ConfigLocker;

// APControl server interface:
static APPool_t *s_pAPPool = NULL;

// APControl server connection type:
static DWORD s_ConnectType = 0;

// Program's config-registry keys:
static bool s_ConfigLoaded = false;
const TCHAR Config::RegistryKey      [] = TEXT("Software\\Microsoft\\CETest\\WiFiRoamConfig");
const TCHAR Config::DTPTEnabledKey   [] = TEXT("DTPTEnabled");
const TCHAR Config::DTPTServerHostKey[] = TEXT("DTPTServerHost");
const TCHAR Config::DTPTServerPortKey[] = TEXT("DTPTServerPort");
const TCHAR Config::WiFiServerHostKey[] = TEXT("WiFiServerHost");
const TCHAR Config::WiFiServerPortKey[] = TEXT("WiFiServerPort");

// Specifies whether the mobile device is atteached to the DTPT/USB
// ActiveSync port:
static bool s_fDTPTEnabled = true;

// APControl server host- and port-addresses:
#define DEFAULT_DTPT_SERVER_HOST     TEXT("169.254.2.2")
#define DEFAULT_DTPT_SERVER_PORT     TEXT("33331")
#define DEFAULT_WIFI_SERVER_HOST     TEXT("10.10.0.100")
#define DEFAULT_WIFI_SERVER_PORT     TEXT("33331")
static TCHAR s_DTPTServerHost[MAX_PATH] = DEFAULT_DTPT_SERVER_HOST;
static TCHAR s_DTPTServerPort[30]       = DEFAULT_DTPT_SERVER_PORT;
static TCHAR s_WiFiServerHost[MAX_PATH] = DEFAULT_WIFI_SERVER_HOST;
static TCHAR s_WiFiServerPort[30]       = DEFAULT_WIFI_SERVER_PORT;

// Access Point SSIDs:
#define DEFAULT_HOME_AP_SSID         TEXT("ROAM_HOMEAP")
#define DEFAULT_OFFICE_AP_SSID       TEXT("ROAM_OFFICEAP")
#define DEFAULT_HOTSPOT_AP_SSID      TEXT("ROAM_HOTSPOTAP")
static bool  s_fSSIDNamesUninitialized = true;
static TCHAR s_HomeSSID[MAX_PATH]    = DEFAULT_HOME_AP_SSID;
static TCHAR s_OfficeSSID[MAX_PATH]  = DEFAULT_OFFICE_AP_SSID;
static TCHAR s_HotspotSSID[MAX_PATH] = DEFAULT_HOTSPOT_AP_SSID;


// ----------------------------------------------------------------------------
//
// Loads the program's configuration from the registry.
//
static void
LoadConfig(void)
{
    ce::gate<ce::critical_section> locker(s_ConfigLocker);
    if (!s_ConfigLoaded)
    {
        s_ConfigLoaded = true;

        // Open the registry.
        ce::auto_hkey hkey;
        DWORD result = RegOpenKeyEx(HKEY_CURRENT_USER, Config::RegistryKey, 0,
                                    KEY_READ, &hkey);
        if (ERROR_SUCCESS != result)
        {
            LogError(TEXT("Cannot open config registry \"%s\": %s"),
                     Config::RegistryKey, Win32ErrorText(result));
            hkey.close();
        }

        // Read the "DTPT enabled" flag.
        DWORD enabledFlag = 0;
        HRESULT hr = WiFUtils::ReadRegDword(hkey,
                                            Config::RegistryKey,
                                            Config::DTPTEnabledKey,
                                            &enabledFlag, 0);
        if (SUCCEEDED(hr))
        {
            s_fDTPTEnabled = (enabledFlag > 0);
        }
    
        // Read the server addresses.
        ce::tstring str;
        WiFUtils::ReadRegString(hkey,
                                Config::RegistryKey,
                                Config::DTPTServerHostKey, &str,
                                DEFAULT_DTPT_SERVER_HOST);
        SafeCopy(s_DTPTServerHost, str, COUNTOF(s_DTPTServerHost));
        
        WiFUtils::ReadRegString(hkey,
                                Config::RegistryKey,
                                Config::DTPTServerPortKey, &str,
                                DEFAULT_DTPT_SERVER_PORT);
        SafeCopy(s_DTPTServerPort, str, COUNTOF(s_DTPTServerPort));
        
        WiFUtils::ReadRegString(hkey,
                                Config::RegistryKey,
                                Config::WiFiServerHostKey, &str,
                                DEFAULT_WIFI_SERVER_HOST);
        SafeCopy(s_WiFiServerHost, str, COUNTOF(s_WiFiServerHost));
        
        WiFUtils::ReadRegString(hkey,
                                Config::RegistryKey,
                                Config::WiFiServerPortKey, &str,
                                DEFAULT_WIFI_SERVER_PORT);
        SafeCopy(s_WiFiServerPort, str, COUNTOF(s_WiFiServerPort));
    }
}

// ----------------------------------------------------------------------------
//
// Initializes or cleans up static resources.
//
void
Config::
StartupInitialize(void)
{
    // nothing to do.
}

void
Config::
ShutdownCleanup(void)
{
    ce::gate<ce::critical_section> locker(s_ConfigLocker);
    if (s_pAPPool)
    {
        delete s_pAPPool;
        s_pAPPool = NULL;
        s_ConnectType = 0;
    }
}

// ----------------------------------------------------------------------------
//
// Determines whether the mobile device is connected to the DTPT/USB
// ActiveSync port.
//
bool
Config::
IsDTPTEnabled(void)
{
    if (!s_ConfigLoaded) LoadConfig();
    return s_fDTPTEnabled;
}

// ----------------------------------------------------------------------------
//
// Gets the host- or port-address of the APControl server for connecting
// over the DTPT or WiFi LAN.
//
const TCHAR *
Config::
GetDTPTServerHost(void)
{
    if (!s_ConfigLoaded) LoadConfig();
    return s_DTPTServerHost;
}

const TCHAR *
Config::
GetDTPTServerPort(void)
{
    if (!s_ConfigLoaded) LoadConfig();
    return s_DTPTServerPort;
}

const TCHAR *
Config::
GetWiFiServerHost(void)
{
    if (!s_ConfigLoaded) LoadConfig();
    return s_WiFiServerHost;
}

const TCHAR *
Config::
GetWiFiServerPort(void)
{
    if (!s_ConfigLoaded) LoadConfig();
    return s_WiFiServerPort;
}

// ----------------------------------------------------------------------------
//
// Generates or retrieves an APPool object attached using the 
// specified type of connection.
//
HRESULT
Config::
GetAPPool(
    DWORD      ConnType,
    APPool_t **ppAPPool)
{
    const TCHAR *host, *port;
    if (ConnType & CONN_DTPT)
    {
        ConnType = CONN_DTPT;
        host = GetDTPTServerHost();
        port = GetDTPTServerPort();
    }
    else
    {
        ConnType = CONN_WIFI_LAN;
        port = GetWiFiServerPort();
        host = GetWiFiServerHost();
    }

    ce::gate<ce::critical_section> locker(s_ConfigLocker);

    if (ConnType == s_ConnectType)
    {
       *ppAPPool = s_pAPPool;
        return S_OK;
    }

    APPool_t *pAPPool = s_pAPPool;
    if (NULL == pAPPool)
    {
        s_pAPPool = pAPPool = new APPool_t;
        if (NULL == pAPPool)
        {
            LogError(TEXT("Can't allocate APPool"));
            return E_OUTOFMEMORY;
        }
    }

    s_ConnectType = 0;
    pAPPool->Disconnect();

    LogDebug(TEXT("Doing GetTestStatus from %s:%s"), host, port);
    HRESULT hr = pAPPool->LoadAPControllers(host, port);
    if (FAILED(hr))
    {
        LogError(TEXT("LoadAPControllers Failed: %s"),
                 HRESULTErrorText(hr));
        return hr;
    }

    // If this is the first time we've connected to the server, initialize
    // the connection names from the server's configuration information.
    if (s_fSSIDNamesUninitialized)
    {
        s_fSSIDNamesUninitialized = false;
        for (int apx = 0 ; pAPPool->size() > apx ; ++apx)
        {
            APController_t *pap = pAPPool->GetAP(apx);
            if (pap)
            {
                const TCHAR *nam, *def;
                TCHAR *out;
                if (0 == _tcsicmp(pap->GetAPName(), DEFAULT_HOME_AP_SSID))
                {
                    nam = TEXT("Home");
                    def = DEFAULT_HOME_AP_SSID;
                    out = s_HomeSSID;
                }
                else
                if (0 == _tcsicmp(pap->GetAPName(), DEFAULT_HOTSPOT_AP_SSID))
                {
                    nam = TEXT("Hotspot");
                    def = DEFAULT_HOTSPOT_AP_SSID;
                    out = s_HotspotSSID;
                }
                else
                {
                    nam = TEXT("Office");
                    def = DEFAULT_OFFICE_AP_SSID;
                    out = s_OfficeSSID;
                }
                SafeCopy(out, pap->GetSsid(), MAX_PATH);;
                LogDebug(TEXT("%s AP SSID changed from default \"%s\" to \"%s\""),
                         nam, def, out);
            }
        }
    }

    s_ConnectType = ConnType;
   *ppAPPool = pAPPool;
    return hr;
}

// ----------------------------------------------------------------------------
//
// Gets the SSID of one of the test's Access Points.
//
const TCHAR *
Config::
GetHomeSSID(void)
{
    return s_HomeSSID;
}
const TCHAR *
Config::
GetOfficeSSID(void)
{
    return s_OfficeSSID;
}
const TCHAR *
Config::
GetHotspotSSID(void)
{
    return s_HotspotSSID;
}

// ----------------------------------------------------------------------------
