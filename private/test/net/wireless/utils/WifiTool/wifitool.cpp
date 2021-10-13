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

#include <CmdArgList_t.hpp>
#include <FakeWifiUtil.hpp>
#include <NetMain_t.hpp>
#include <WiFiAccount_t.hpp>
#include <WiFiConfig_t.hpp>
#include <WiFiDevice.hpp>
#include <WiFiRegHelper.hpp>
#include <WiFiService_t.hpp>
#include <WiFUtils.hpp>

using namespace ce::qa;
using namespace litexml;

// ----------------------------------------------------------------------------
//
// External variables for NetMain:
//
WCHAR *gszMainProgName = L"WiFiTool";
DWORD  gdwMainInitFlags = 0;

// -----------------------------------------------------------------------------
//
// Extends NetMain to add our customized run methods.
//
class MyNetMain_t : public NetMain_t
{
private:

    // Run-time configuration objects:
    //
    CmdArgList_t *m_pCmdArgList;
    
    // Connect:
    //
    CmdArgFlag_t    *m_pCmdConnect;
    WiFiConfig_t     m_ConnectConfig;
    CmdArgFlag_t    *m_pNetworkIsAdHoc;
    CmdArgFlag_t    *m_pNetworkIsHidden;
    CmdArgFlag_t    *m_pUseConfigSProfile;
    CmdArgFlag_t    *m_pUseIHVExtProfile;
    CmdArgFlag_t    *m_pDisableConnectMonitor;

    // Query and Profiles:
    //
    CmdArgString_t  *m_pCmdDeleteProfile;
    CmdArgFlag_t    *m_pCmdQueryAdapterInfo;
    CmdArgFlag_t    *m_pCmdResetPreferredList;

    // Miscellaneous:
    //
    CmdArgFlag_t    *m_pCmdEnableWiFiService;
    CmdArgFlag_t    *m_pCmdDisableWiFiService;
    CmdArgFlag_t    *m_pCmdDisableCertDialogs;
    CmdArgFlag_t    *m_pCmdDisableCustFeedback;
    CmdArgFlag_t    *m_pCmdDisableItgProxy;
    CmdArgString_t  *m_pCmdSetAdapterPower;
    CmdArgStringW_t *m_pCmdSetDisplayTimeout;
    CmdArgString_t  *m_pCmdSetIHVExtension;
    CmdArgFlag_t    *m_pCmdUnloadFakeWifi;

public:

    // Constructor/destructor:
    //
    MyNetMain_t(void)
        : m_pCmdArgList(NULL),
          m_pWiFiService(NULL)
    {
        Clear();
    }
  __override virtual
   ~MyNetMain_t(void)
    {
        if (NULL != m_pCmdArgList)
        {
            delete m_pCmdArgList;
            m_pCmdArgList = NULL;
        }
        
        if (NULL != m_pWiFiService)
        {
            delete m_pWiFiService;
            m_pWiFiService = NULL;
        }
        
        Clear();
    }
    void
    Clear(void)
    {
        m_pCmdConnect             = NULL;
        m_pNetworkIsAdHoc         = NULL;
        m_pNetworkIsHidden        = NULL;
        m_pUseConfigSProfile      = NULL;
        m_pUseIHVExtProfile       = NULL;
        m_pDisableConnectMonitor  = NULL;

        m_pCmdDeleteProfile       = NULL;
        m_pCmdQueryAdapterInfo    = NULL;
        m_pCmdResetPreferredList  = NULL;

        m_pCmdEnableWiFiService   = NULL;
        m_pCmdDisableWiFiService  = NULL;
        m_pCmdDisableCertDialogs  = NULL;
        m_pCmdDisableCustFeedback = NULL;
        m_pCmdDisableItgProxy     = NULL;
        m_pCmdSetAdapterPower     = NULL;
        m_pCmdSetDisplayTimeout   = NULL;
        m_pCmdSetIHVExtension     = NULL;
        m_pCmdUnloadFakeWifi      = NULL;
    }

    // Initializes the program:
    // Returns ERROR_ALREADY_EXISTS if the program is already running.
    //
 __override virtual DWORD
    Init(
        int    argc,
        TCHAR *argv[]);

    // The actual program:
    //
  __override virtual DWORD
    Run();

    // Cleans up before program shutdown:
    //
    DWORD
    Cleanup(void);

protected:

    // Displays the program's command-line arguments:
    //
  __override virtual void
    DoPrintUsage(void)
    {
        LogAlways(TEXT("\n"));
        NetMain_t::DoPrintUsage();
        CmdArgList_t *pArgList = GetCmdArgList();
        if (pArgList)
            pArgList->PrintUsage(LogAlways);
        LogAlways(TEXT("\n"));
    }

    // Parses the program's command-line arguments:
    //
  __override virtual DWORD
    DoParseCommandLine(
        int    argc,
        TCHAR *argv[])
    {
        CmdArgList_t *pArgList = GetCmdArgList();
        return (NULL == pArgList)? ERROR_OUTOFMEMORY
                                 : pArgList->ParseCommandLine(argc, argv);
    }

    // Overrides the normal method to allow multiple instances to run
    // at one time:
    //
  __override virtual DWORD
    DoFindPreviousInstance(void)
    {
        return NO_ERROR;
    }

private:

    // Generates or retrieves the command-arg list:
    //
    CmdArgList_t *
    GetCmdArgList(void);

    // Enables or disables the WiFi configuration service:
    //
    DWORD
    DoCmdEnableWiFiService(void);
    DWORD
    DoCmdDisableWiFiService(void);

    // Disables the "confirm certificate installation" UI dialogs:
    //
    DWORD
    DoCmdDisableCertDialogs(void);

    // Disables the "customer feedback requested" UI dialogs:
    //
    DWORD
    DoCmdDisableCustFeedback(void);

    // Disables the Internet Explorer ITG proxy setting:
    //
    DWORD
    DoCmdDisableItgProxy(void);

    // Sets the WiFi adapter power (on or off):
    //
    DWORD
    DoCmdSetAdapterPower(void);

    // Sets the backlight display timeout interval:
    //
    DWORD
    DoCmdSetDisplayTimeout(void);

    // Inserts or removes the IHV Extensions DLL registry:
    //
    DWORD
    DoCmdSetIHVExtension(void);

    // Unloads the Fake WiFi adapter(s):
    //
    DWORD
    DoCmdUnloadFakeWiFi(void);

    // Deletes the specified WiFi profile:
    //
    DWORD
    DoCmdDeleteProfile(void);

    // Logs WiFi adapter state information:
    //
    DWORD
    DoCmdQueryAdapterInfo(void);

    // Resets the WiFi adapter's Preferred Networks List:
    //
    DWORD
    DoCmdResetPreferredList(void);

    // Connects to the specified WiFi network:
    //
    DWORD
    DoCmdConnect(void);

    // Retrieves the name of the WiFi adapter:
    // This is either the name specified by the user or the first adapter
    // on the system.
    //
    TCHAR m_AdapterName[MaxWiFiAdapterNameChars];
    const TCHAR *
    GetAdapterName(void)
    {
        if (TEXT('\0') == m_AdapterName[0])
        {
            WiFiService_t *pWiFi = GetWiFiService();
            if (pWiFi && pWiFi->HasAdapter())
                SafeCopy(m_AdapterName, pWiFi->GetAdapterName(), COUNTOF(m_AdapterName));
        }
        return m_AdapterName;
    }

    // Initializes the WiFi service for controlling the selected adapter:
    //
    WiFiService_t *m_pWiFiService;
    WiFiService_t *
    GetWiFiService(void)
    {
        WiFiService_t *pWiFi = m_pWiFiService;
        
        if (NULL == pWiFi)
            assert(NULL != m_pWiFiService);
        else
        if (!pWiFi->HasAdapter()
         || _tcscmp(pWiFi->GetAdapterName(), m_AdapterName) != 0)
        {
            if (NO_ERROR != pWiFi->SetAdapterName(m_AdapterName))
                pWiFi = NULL;
        }
        
        return pWiFi;
    }

    // Examines the string command-line argument to determine whether it
    // contains a valid on,off value:
    //
    DWORD
    CheckOnOffArgValue(
        const CmdArgString_t &Argument,
        const char           *pRequired,
        bool                 *pValue)
    {
        DWORD result = NO_ERROR;
        const TCHAR *pStrArg = Argument.GetValue();
        if (0 == _tcsicmp(pStrArg, TEXT("1"))
         || 0 == _tcsicmp(pStrArg, TEXT("on"))
         || 0 == _tcsicmp(pStrArg, TEXT("set"))
         || 0 == _tcsicmp(pStrArg, TEXT("yes"))
         || 0 == _tcsicmp(pStrArg, TEXT("true"))
         || 0 == _tcsicmp(pStrArg, TEXT("enable")))
        {
            *pValue = true;
        }
        else
        if (0 == _tcsicmp(pStrArg, TEXT("0"))
         || 0 == _tcsicmp(pStrArg, TEXT("no"))
         || 0 == _tcsicmp(pStrArg, TEXT("off"))
         || 0 == _tcsicmp(pStrArg, TEXT("clear"))
         || 0 == _tcsicmp(pStrArg, TEXT("false"))
         || 0 == _tcsicmp(pStrArg, TEXT("disable")))
        {
            *pValue = false;
        }
        else
        {
            LogError(TEXT("Invalid argument for -%s: %hs required"),
                     Argument.GetArgName(),
                     pRequired);
            result = ERROR_INVALID_PARAMETER;
        }
        return result;
    }
    
};

// -----------------------------------------------------------------------------
//
// Generates or retrieves the command-arg list:
//
CmdArgList_t*
MyNetMain_t::
GetCmdArgList()
{
    if (NULL == m_pCmdArgList)
    {
        //
        // General options:
        //
        
        ce::auto_ptr<CmdArgList_t> pArgList = new CmdArgList_t(TEXT("General options"));
        if (!pArgList.valid())
        {
            LogError(TEXT("Can't allocate CmdArgList"));
            return NULL;
        }

        if (NO_ERROR != pArgList->Add(WiFiService_t::GetCmdArgList()))
            return NULL;

        m_pCmdEnableWiFiService
            = new CmdArgFlag_t(TEXT("EnableWiFiService"),
                               TEXT("enablewifisvc"),
                               TEXT("Enables the WiFi configuration service"));
        if (NO_ERROR != pArgList->Add(m_pCmdEnableWiFiService))
            return NULL;

        m_pCmdDisableWiFiService
            = new CmdArgFlag_t(TEXT("DisableWiFiService"),
                               TEXT("disablewifisvc"),
                               TEXT("Disables the WiFi configuration service"));
        if (NO_ERROR != pArgList->Add(m_pCmdDisableWiFiService))
            return NULL;

        //
        // Connection:
        //
        
        if (NO_ERROR != pArgList->SetSubListName(TEXT("Connect")))
            return NULL;

        m_pCmdConnect
            = new CmdArgFlag_t(TEXT("Connect"),
                               TEXT("connect"),
                               TEXT("Connects to the following WiFi network:"));
        if (NO_ERROR != pArgList->Add(m_pCmdConnect))
            return NULL;

        ce::auto_ptr<CmdArgList_t> pConnConfigArgs
            = m_ConnectConfig.GenerateCmdArgList(TEXT("Connect"),
                                                 TEXT(""),
                                                 TEXT("network"));
        if (NO_ERROR != pArgList->Add(pConnConfigArgs))
            return NULL;

        m_pNetworkIsAdHoc
            = new CmdArgFlag_t(TEXT("AdhocAccessPoint"),
                               TEXT("Adhoc"),
                               TEXT("indicates the network is Adhoc"));
        if (NO_ERROR != pArgList->Add(m_pNetworkIsAdHoc))
            return NULL;

        m_pNetworkIsHidden
            = new CmdArgFlag_t(TEXT("HiddenAccessPoint"),
                               TEXT("Hidden"),
                               TEXT("indicates the network Hidden/Nonbroadcasting"));
        if (NO_ERROR != pArgList->Add(m_pNetworkIsHidden))
            return NULL;

        m_pUseConfigSProfile
            = new CmdArgFlag_t(TEXT("Use ConfigSP Profile"),
                               TEXT("ConfigSP"),
                               TEXT("use Wi-FI Config Service Provider configure connection"));
        if (NO_ERROR != pArgList->Add(m_pUseConfigSProfile))
            return NULL;

        m_pUseIHVExtProfile
            = new CmdArgFlag_t(TEXT("Use IHV Profile"),
                               TEXT("ProfileIHV"),
                               TEXT("use IHV Extensions profile to configure connection"));
        if (NO_ERROR != pArgList->Add(m_pUseIHVExtProfile))
            return NULL;

        m_pDisableConnectMonitor
            = new CmdArgFlag_t(TEXT("Disable Connection Monitor"),
                               TEXT("DisableConnMon"),
                               TEXT("disables Connection Monitor"));
        if (NO_ERROR != pArgList->Add(m_pDisableConnectMonitor))
            return NULL;

        //
        // Query and profile management:
        //

        if (NO_ERROR != pArgList->SetSubListName(TEXT("Profiles")))
            return NULL;

        m_pCmdDeleteProfile
            = new CmdArgString_t(TEXT("DeleteProfile"),
                                 TEXT("delete"),
                                 TEXT("Deletes a WiFi network profile"),
                                 TEXT(""),
                                 TEXT("Deletes the specified WiFi network profile"));
        if (NO_ERROR != pArgList->Add(m_pCmdDeleteProfile))
            return NULL;

        m_pCmdQueryAdapterInfo
            = new CmdArgFlag_t(TEXT("QueryAdapterInfo"),
                               TEXT("query"),
                               TEXT("Queries and displays detailed adapter info"));
        if (NO_ERROR != pArgList->Add(m_pCmdQueryAdapterInfo))
            return NULL;

        m_pCmdResetPreferredList
            = new CmdArgFlag_t(TEXT("ResetPreferredList"),
                               TEXT("reset"),
                               TEXT("Clears the WiFi adapter's Preferred Networks list"));
        if (NO_ERROR != pArgList->Add(m_pCmdResetPreferredList))
            return NULL;

        //
        // EAP Authentication:
        //
        
        if (NO_ERROR != pArgList->SetSubListName(TEXT("EAP Authentication")))
            return NULL;

        if (NO_ERROR != pArgList->Add(WiFiAccount_t::GetCmdArgList()))
            return NULL;


        //
        // Miscelaneous:
        //

        if (NO_ERROR != pArgList->SetSubListName(TEXT("Miscellaneous")))
            return NULL;

        m_pCmdSetAdapterPower
            = new CmdArgString_t(TEXT("Power"),
                                 TEXT("power"),
                                 TEXT("Powers a wifi adapter on or off"),
                                 TEXT(""),
                                 TEXT("Powers a wifi adapter \"on\" or \"off\""));
        if (NO_ERROR != pArgList->Add(m_pCmdSetAdapterPower))
            return NULL;

        m_pCmdUnloadFakeWifi
            = new CmdArgFlag_t(TEXT("EjectFakeWifi"),
                               TEXT("ejectxwifi"),
                               TEXT("Unloads the Fake Wifi drivers"));
        if (NO_ERROR != pArgList->Add(m_pCmdUnloadFakeWifi))
            return NULL;

        m_pCmdDisableCertDialogs
            = new CmdArgFlag_t(TEXT("DisableCertInstall"),
                               TEXT("disablecertinstall"),
                               TEXT("Disables the 'confirm certificate install' prompt"));
        if (NO_ERROR != pArgList->Add(m_pCmdDisableCertDialogs))
            return NULL;

        m_pCmdDisableCustFeedback
            = new CmdArgFlag_t(TEXT("DisableCustFeedback"),
                               TEXT("disablecustfeedback"),
                               TEXT("Disables the 'customer feedback' prompt"));
        if (NO_ERROR != pArgList->Add(m_pCmdDisableCustFeedback))
            return NULL;

        m_pCmdDisableItgProxy
            = new CmdArgFlag_t(TEXT("DisableItgProxy"),
                               TEXT("disableitgproxy"),
                               TEXT("Disables the IE ITG proxy setting"));
        if (NO_ERROR != pArgList->Add(m_pCmdDisableItgProxy))
            return NULL;

        m_pCmdSetDisplayTimeout
            = new CmdArgString_t(TEXT("DisplayTimeout"),
                                 TEXT("displaytimeout"),
                                 TEXT("Sets the display timeout in seconds"),
                                 TEXT(""),
                                 TEXT("Sets the display timeout (0-60 seconds or never=off)"));
        if (NO_ERROR != pArgList->Add(m_pCmdSetDisplayTimeout))
            return NULL;

        m_pCmdSetIHVExtension
            = new CmdArgString_t(TEXT("IHVExtension"),
                                 TEXT("IHVExt"),
                                 TEXT("Sets/Clears Test IHV Extension on an interface"),
                                 TEXT(""),
                                 TEXT("Sets or clears test IHV Extension for the interface (set or clear)"));
        if (NO_ERROR != pArgList->Add(m_pCmdSetIHVExtension))
            return NULL;
        
        m_pCmdArgList = pArgList.release();
    }

    return m_pCmdArgList;
}

// -----------------------------------------------------------------------------
//
// Actual program.
//
DWORD
MyNetMain_t::
Run()
{
    DWORD result = NO_ERROR;

    // Unload the Fake WiFi driver(s) first.
    result |= DoCmdUnloadFakeWiFi();

    // Power up the real WiFi adapter next.
    const TCHAR *pPowerCmd = m_pCmdSetAdapterPower->GetValue();
    if (pPowerCmd && (0 == _tcsicmp(pPowerCmd, TEXT("on"))))
    {
        result |= DoCmdSetAdapterPower();
    }

    // Enable the WiFi configuration service.
    result |= DoCmdEnableWiFiService();

    // Disable the "confirm certificate installation" UI dialogs.
    result |= DoCmdDisableCertDialogs();

    // Disable the "customer feedback requested" UI dialogs.
    result |= DoCmdDisableCustFeedback();

    // Disable the Internet Explorer ITG proxy setting.
    result |= DoCmdDisableItgProxy();

    // Set the backlight display timeout interval.
    result |= DoCmdSetDisplayTimeout();

    // Insert or removes the IHV Extensions DLL registry.
    result |= DoCmdSetIHVExtension();

    // Delete the specified WiFi profile.
    result |= DoCmdDeleteProfile();

    // Reset the WiFi adapter's Preferred Networks List.
    result |= DoCmdResetPreferredList();

    // Connect to the specified WiFi network.
    result |= DoCmdConnect();

    // Log WiFi adapter state information.
    result |= DoCmdQueryAdapterInfo();

    // Disable the WiFi configuration service.
    result |= DoCmdDisableWiFiService();

    // Power down the WiFi adapter last.
    if (pPowerCmd && (0 != _tcsicmp(pPowerCmd, TEXT("on"))))
    {
        result |= DoCmdSetAdapterPower();
    }

    return result;
}

// -----------------------------------------------------------------------------
//
// Enables or disables the WiFi configuration service:
//
DWORD
MyNetMain_t::
DoCmdEnableWiFiService(void)
{
    DWORD result = NO_ERROR;
    
    if (m_pCmdEnableWiFiService->GetValue())
    {
        WiFiService_t *pWiFi = GetWiFiService();
        if (NULL == pWiFi)
            result = ERROR_OPEN_FAILED;
        else
        {
            LogStatus(TEXT("Enabling WiFi service for adapter %s"),
                      GetAdapterName());
            
            result = pWiFi->Enable();
        }

        if (NO_ERROR != result)
        {
            LogError(TEXT("Error enabling WiFi config service: %s"),
                     Win32ErrorText(result));
        }
    }

    return result;
}

DWORD
MyNetMain_t::
DoCmdDisableWiFiService(void)
{
    DWORD result = NO_ERROR;
    
    if (m_pCmdDisableWiFiService->GetValue())
    {
        WiFiService_t *pWiFi = GetWiFiService();
        if (NULL == pWiFi)
            result = ERROR_OPEN_FAILED;
        else
        {
            LogStatus(TEXT("Disabling WiFi service for adapter %s"),
                      GetAdapterName());
            
            result = pWiFi->Disable();
        }

        if (NO_ERROR != result)
        {
            LogError(TEXT("Error disabling WiFi config service: %s"),
                     Win32ErrorText(result));
        }
    }

    return result;
}

// -----------------------------------------------------------------------------
//
// Disables the "confirm certificate installation" UI dialogs:
//
DWORD
MyNetMain_t::
DoCmdDisableCertDialogs(void)
{
    DWORD result = NO_ERROR;

    if (m_pCmdDisableCertDialogs->GetValue())
    {
        LogStatus(TEXT("Disabling 'confirm certificate install' prompt"));

        HRESULT hr = WiFUtils::DisableConfirmInstallPrompt();
        result = HRESULT_CODE(hr);

        if (ERROR_FILE_NOT_FOUND == result)
            result = NO_ERROR;
        else
        if (NO_ERROR != result)
        {
            LogError(TEXT("Error disabling certificate install prompts: %s"),
                     Win32ErrorText(result));
        }
    }

    return result;
}

// -----------------------------------------------------------------------------
//
// Disables the "customer feedback requested" UI dialogs:
//
DWORD
MyNetMain_t::
DoCmdDisableCustFeedback(void)
{
    DWORD result = NO_ERROR;

    if (m_pCmdDisableCustFeedback->GetValue())
    {
        LogStatus(TEXT("Disabling 'customer feedback' prompt"));

        HRESULT hr = WiFUtils::DisableCustomerFeedback();
        result = HRESULT_CODE(hr);

        if (ERROR_FILE_NOT_FOUND == result)
            result = NO_ERROR;
        else
        if (NO_ERROR != result)
        {
            LogError(TEXT("Error disabling customer feedback prompts: %s"),
                     Win32ErrorText(result));
        }
    }

    return result;
}

// -----------------------------------------------------------------------------
//
// Disables the Internet Explorer ITG proxy setting:
//
DWORD
MyNetMain_t::
DoCmdDisableItgProxy(void)
{
    DWORD result = NO_ERROR;

    if (m_pCmdDisableItgProxy->GetValue())
    {
        LogStatus(TEXT("Disabling the ITG Proxy Internet Explorer setting"));

        HRESULT hr = WiFUtils::DisableItgProxy();
        result = HRESULT_CODE(hr);

        if (ERROR_FILE_NOT_FOUND == result)
            result = NO_ERROR;
        else
        if (NO_ERROR != result)
        {
            LogError(TEXT("Error disabling ITG Proxy setting: %s"),
                     Win32ErrorText(result));
        }
    }

    return result;
}

// -----------------------------------------------------------------------------
//
// Sets the WiFi adapter power (on or off):
//
DWORD
MyNetMain_t::
DoCmdSetAdapterPower(void)
{
    DWORD result = NO_ERROR;

    const TCHAR *pPowerCmd = m_pCmdSetAdapterPower->GetValue();
    if (pPowerCmd && pPowerCmd[0])
    {
        bool bEnable;
        result = CheckOnOffArgValue(*m_pCmdSetAdapterPower,
                                    "\"on\" or \"off\"",
                                    &bEnable);               
        if (NO_ERROR == result)
        {
            const TCHAR *pAdapterName = GetAdapterName();
            if (!pAdapterName[0])
            {
                LogError(TEXT("Must specify adapter name for -%s command"),
                         m_pCmdSetAdapterPower->GetArgName());
                result = ERROR_INVALID_PARAMETER;                     
            }
            else
            {
                LogStatus(TEXT("Powering '%ls' %s"), pAdapterName,
                          bEnable ? TEXT("ON") : TEXT("OFF"));

                // Drop current WiFi service connection (if any).
                assert(m_pWiFiService);
                m_pWiFiService->DisconnectAdapter();

                // Change adapter's power state.
                result = WiFiDevice::AdapterPowerOnOff(pAdapterName, bEnable);

                // Wait for WiFi to notice adapter change.
                if (NO_ERROR == result)
                {
                    LogStatus(TEXT("Wait %d seconds for adapter change"),
                              WiFiService_t::GetDefaultStableTimeMS() / 2000);
                    Sleep(WiFiService_t::GetDefaultStableTimeMS() / 2);
                }
            }
        }

        if (NO_ERROR != result)
        {
            LogError(TEXT("Error changing adapter power state: %s"),
                     Win32ErrorText(result));
        }
    }

    return result;
}

// -----------------------------------------------------------------------------
//
// Sets the backlight display timeout interval:
//
DWORD
MyNetMain_t::
DoCmdSetDisplayTimeout(void)
{
    DWORD result = NO_ERROR;

    const WCHAR *pTimeout = m_pCmdSetDisplayTimeout->GetValue();
    if (pTimeout && pTimeout[0])
    {
        DWORD dwTimeout = 0;
        if (_istdigit(pTimeout[0]))
        {
            dwTimeout = _wtoi(pTimeout);
            LogStatus(TEXT("Setting display timeout to %d seconds"),
                      dwTimeout);
        }
        else
        if (0 == _tcsicmp(pTimeout, L"never"))
        {
            dwTimeout = 0xFFFFFFFF;
            LogStatus(TEXT("Setting display timeout to NEVER"));
        }
        else
        {
            LogError(TEXT("Invalid argument for -%s: 0-60 or \"never\" required"),
                     m_pCmdSetDisplayTimeout->GetArgName());
            result = ERROR_INVALID_PARAMETER;
        }

        if (NO_ERROR == result)
        {
            HRESULT hr = WiFUtils::SetDisplayTimeout(dwTimeout);
            result = HRESULT_CODE(hr);
        }

        if (NO_ERROR != result)
        {
            LogError(TEXT("Error setting display timeout: %s"),
                     Win32ErrorText(result));
        }
    }

    return result;
}

// -----------------------------------------------------------------------------
//
// Inserts or removes the IHV Extensions DLL registry:
//
DWORD
MyNetMain_t::
DoCmdSetIHVExtension(void)
{
    HRESULT hr;
    DWORD result = NO_ERROR;

    static const TCHAR s_ExtensibilityKeyFormat[] = TEXT("Comm\\%s\\Parms\\%s");
    static const TCHAR s_ExtKeyName[]             = TEXT("IHVExtensions");
    static const TCHAR s_ExtValueName[]           = TEXT("ExtensibilityDLL");
    static const TCHAR s_OUIValueName[]           = TEXT("AdapterOUI");
    static const TCHAR s_TestExtensionsDll[]      = TEXT("IHVTestExt.dll");

    const TCHAR *pIHVExtCmd = m_pCmdSetIHVExtension->GetValue();
    if (pIHVExtCmd && pIHVExtCmd[0])
    {
        TCHAR ihvKeyBuff[MAX_PATH];
        WiFiRegHelper reghelper;
        bool regAdded = false;
        bool regDeleted = false;

        do
        {
            bool bEnable;
            result = CheckOnOffArgValue(*m_pCmdSetIHVExtension,
                                        "\"set\" or \"clear\"",
                                        &bEnable);        
            if (NO_ERROR != result)
                break;

            const TCHAR *pAdapterName = GetAdapterName();
            if (!pAdapterName[0])
            {
                LogError(TEXT("Must specify adapter name for -%s command"),
                         m_pCmdSetIHVExtension->GetArgName());
                result = ERROR_INVALID_PARAMETER;  
                break;
            }

            LogStatus(TEXT("%hs IHV Extensions plugin for adapter %s"),
                      bEnable? "Setting" : "Clearing",
                      pAdapterName);

            // Adjust the IHV extensions registry.
            hr = StringCchPrintf(ihvKeyBuff,
                         COUNTOF(ihvKeyBuff),
                                 s_ExtensibilityKeyFormat,
                                 pAdapterName,
                                 s_ExtKeyName);
            if (FAILED(hr))
            {
                result = HRESULT_CODE(hr);
                LogError(TEXT("Can't format IHVExtension Registry key: %s"),
                         Win32ErrorText(result));
                break;
            }
            
            if (!reghelper.Init(ihvKeyBuff))
            {
                LogError(TEXT("RegHelper into for IHV Extensions %s failed"), ihvKeyBuff);
                result = ERROR_GEN_FAILURE;
                break;
            }
            
            if (bEnable)
            {
                if (!reghelper.CreateKey(&result)
                 || !reghelper.SetStrValue(s_ExtValueName, s_TestExtensionsDll, &result)
                 || !reghelper.SetDwordValue(s_OUIValueName, 0x123456, &result))
                {
                    LogError(TEXT("Registry setup for IHV Extensions \"%s\" failed"), ihvKeyBuff);
                    break;
                }
                regAdded = true;
            }
            else 
            {
                if (reghelper.OpenKey())   // delete only if it exists currently
                {
                    reghelper.DeleteKey(&result);
                    if (NO_ERROR != result)
                    {
                        LogError(TEXT("Registry deletion for IHV Extensions \"%s\" failed"), ihvKeyBuff);
                        break;
                    }
                    regDeleted = true;
                }
            }

            if (regAdded || regDeleted)
            {        
                // Drop current WiFi service connection (if any).
                assert(m_pWiFiService);
                m_pWiFiService->DisconnectAdapter();

                // Unbind and re-bind the adapter.
                result = WiFiDevice::AdapterBindUnbind(pAdapterName, false);
                if (NO_ERROR != result)
                {
                    LogError(TEXT("Unbind for Adapter %s failed"), pAdapterName);
                    break;
                }
                result = WiFiDevice::AdapterBindUnbind(pAdapterName, true);
                if (NO_ERROR != result)
                {
                    LogError(TEXT("Bind for Adapter %s failed"), pAdapterName);
                    break;
                }

                // Wait for WiFi to notice adapter change.
                LogStatus(TEXT("Wait %d seconds for adapter change"),
                          WiFiService_t::GetDefaultStableTimeMS() / 1000);
                Sleep(WiFiService_t::GetDefaultStableTimeMS());
            }
        }
        while (false);

        if (NO_ERROR != result)
        {
            LogError(TEXT("Error setting IHV Extensions plugin: %s"),
                     Win32ErrorText(result));

            // Clean up if we added the reg entries but failed anyway.
            if (regAdded)
            {
                reghelper.DeleteKey();
            }
        }
    }

    return result;
}

// -----------------------------------------------------------------------------
//
// Unloads the Fake WiFi adapter(s):
//
DWORD
MyNetMain_t::
DoCmdUnloadFakeWiFi(void)
{
    DWORD result = NO_ERROR;

    if (m_pCmdUnloadFakeWifi->GetValue())
    {
        result = EjectFakeWiFi(L"XWIFI11b", 1);
        
        if (NO_ERROR == result)
        {
            result = EjectFakeWiFi(L"VXWIFI", 1);
        }

        if (NO_ERROR != result)
        {
            LogError(TEXT("Error unloading Fake WiFi adapter(s): %s"),
                     Win32ErrorText(result));
        }
    }

    return result;
}

// -----------------------------------------------------------------------------
//
// Deletes the specified WiFi profile:
//
DWORD
MyNetMain_t::
DoCmdDeleteProfile(void)
{
    DWORD result = NO_ERROR;

    const TCHAR *pProfileName = m_pCmdDeleteProfile->GetValue();
    if (pProfileName && pProfileName[0])
    {
        do
        {
            WiFiService_t *pWiFi = GetWiFiService();
            if (NULL == pWiFi)
            {
                result = ERROR_OPEN_FAILED;
                break;
            }
            else
            {
                LogStatus(TEXT("Deleting profile \"%s\" from adapter %s"),
                          pProfileName, GetAdapterName());

                // Use Wi-Fi Configuration Service Provided if indicated.
                if (m_pUseConfigSProfile->GetValue())
                {
                    LogAlways(TEXT(" -%s: Using Config SP profile"),
                              m_pUseConfigSProfile->GetArgName());
                    result = pWiFi->SetServiceType(WiFiServiceConfigSP);
                    if (NO_ERROR != result)
                        break;
                }

                result = pWiFi->RemoveFromPreferredList(pProfileName);
            }
            
            if (NO_ERROR != result)
            {
                LogError(TEXT("Error deleting WiFi network profile: %s"),
                         Win32ErrorText(result));
            }
        }while(FALSE);
    }

    return result;
}

// -----------------------------------------------------------------------------
//
// Logs WiFi adapter state information:
//
DWORD
MyNetMain_t::
DoCmdQueryAdapterInfo(void)
{
    DWORD result = NO_ERROR;

    if (m_pCmdQueryAdapterInfo->GetValue())
    {
        WiFiService_t *pWiFi = GetWiFiService();
        if (NULL == pWiFi)
            result = ERROR_OPEN_FAILED;
        else
        {
            LogStatus(TEXT("Querying state information from adapter %s"),
                      GetAdapterName());
            
            LogAlways(TEXT("\n"));
            result = pWiFi->Log(LogAlways, true);
            if (NO_ERROR == result)
                result = pWiFi->LogDriverInfo(LogAlways, true);
        }

        if (NO_ERROR != result)
        {
            LogError(TEXT("Error logging adapter state information: %s"),
                     Win32ErrorText(result));
        }
    }

    return result;
}

// -----------------------------------------------------------------------------
//
// Resets the WiFi adapter's Preferred Networks List:
//
DWORD
MyNetMain_t::
DoCmdResetPreferredList(void)
{
    DWORD result = NO_ERROR;

    if (m_pCmdResetPreferredList->GetValue())
    {
        do
        {
            WiFiService_t *pWiFi = GetWiFiService();
            if (NULL == pWiFi)
            {
                result = ERROR_OPEN_FAILED;
                break;
            }
            else
            {
                LogAlways(TEXT("Resetting Preferred Networks list for adapter %s"),
                          GetAdapterName());
        
                // Use Wi-Fi Configuration Service Provided if indicated.
                if (m_pUseConfigSProfile->GetValue())
                {
                    LogAlways(TEXT(" -%s: Using Config SP profile"),
                              m_pUseConfigSProfile->GetArgName());
                    result = pWiFi->SetServiceType(WiFiServiceConfigSP);
                    if (NO_ERROR != result)
                        break;
                }

                result = pWiFi->Clear(WiFiService_t::GetDefaultStableTimeMS());
            }
        }while(FALSE);

        if (NO_ERROR != result)
        {
            LogError(TEXT("Error resetting Preferred Networks list: %s"),
                     Win32ErrorText(result));
        }
    }

    return result;
}

// -----------------------------------------------------------------------------
//
// Connects to the specified WiFi network:
//
DWORD
MyNetMain_t::
DoCmdConnect(void)
{
    DWORD result = NO_ERROR;

    if (m_pCmdConnect->GetValue())
    {
        LogStatus(TEXT("Connecting %s:")
                  TEXT(" -SSID %s -Auth %s -Encr %s")
                  TEXT(" -EapAuth %s -KeyIndex %d -Key \"%s\""),
                  GetAdapterName(),
                  m_ConnectConfig.GetSsid(),
                  m_ConnectConfig.GetAuthName(),
                  m_ConnectConfig.GetCipherName(),
                  m_ConnectConfig.GetEapAuthName(),
                  m_ConnectConfig.GetKeyIndex(),
                  m_ConnectConfig.GetKey());
        do
        {
            WiFiService_t *pWiFi = GetWiFiService();
            if (NULL == pWiFi)
            {
                result = ERROR_OPEN_FAILED;
                break;
            }

            // Use Ad Hoc connection if indicated.
            if (m_pNetworkIsAdHoc->GetValue())
            {
                LogAlways(TEXT(" -%s: Configuring Ad Hoc connection"),
                          m_pNetworkIsAdHoc->GetArgName());
                m_ConnectConfig.SetAdHoc(true);
            }

            // Specify hidden network if indicated.
            if (m_pNetworkIsHidden->GetValue())
            {
                LogAlways(TEXT(" -%s: Network is hidden / non-broadcast"),
                          m_pNetworkIsHidden->GetArgName());
                m_ConnectConfig.SetSsidBroadcast(false);
            }

            // Use Wi-Fi Configuration Service Provided if indicated.
            if (m_pUseConfigSProfile->GetValue())
            {
                LogAlways(TEXT(" -%s: Using Config SP profile"),
                          m_pUseConfigSProfile->GetArgName());
                result = pWiFi->SetServiceType(WiFiServiceConfigSP);
                if (NO_ERROR != result)
                    break;
            }

            // Use IHV Extenstions profile if indicated.
            if (m_pUseIHVExtProfile->GetValue())
            {
                LogAlways(TEXT(" -%s: Using IHV Extensions profile"),
                          m_pUseIHVExtProfile->GetArgName());
                m_ConnectConfig.SetIhvProfile(true);
            }

            // If necessary, initialize an EAP-authentication handler.
            ce::auto_ptr<WiFiAccount_t> pEapCredentials;
            bool needEap = false;
            switch (m_ConnectConfig.GetAuthMode())
            {
            case APAuthWEP_802_1X:
            case APAuthWPA:
            case APAuthWPA2:
                needEap = true;
                break;
            }
            if (needEap)
            {
                pEapCredentials = WiFiAccount_t::CloneEapCredentials(m_ConnectConfig.GetEapAuthMode());
                if (!pEapCredentials.valid())
                {
                    LogError(TEXT("Can't generate EAP credentials object"));
                    result = ERROR_OUTOFMEMORY;
                    break;
                }
            }

            // Connect the network.
            result = pWiFi->ConnectNetwork(m_ConnectConfig, pEapCredentials);
            if (NO_ERROR != result)
                break;

            // Wait for the connection if indicated.
            if (!m_pDisableConnectMonitor->GetValue())
            {
                const MACAddr_t *pExpectedBSsid    = NULL;
                const bool       fAcceptTempIPAddr = m_ConnectConfig.IsAdHoc();
                result = pWiFi->MonitorConnection(m_ConnectConfig.GetSsid(),
                                                  pExpectedBSsid,
                                                  fAcceptTempIPAddr);
            }
        } 
        while (false);
        
        if (NO_ERROR != result)
        {
            LogError(TEXT("Error connecting WiFi network: %s"),
                     Win32ErrorText(result));
        }
    }
    
    return result;
}

// -----------------------------------------------------------------------------
//
// Initializes the program:
// Returns ERROR_ALREADY_EXISTS if the program is already running.
//
DWORD
MyNetMain_t::
Init(
    int    argc,
    TCHAR *argv[])
{
    DWORD result = NetMain_t::Init(argc, argv);

    if (NO_ERROR == result)
        result = StartupWinsock(2,2);

    if (NO_ERROR == result)
    {
        SafeCopy(m_AdapterName, WiFiService_t::GetDefaultAdapterName(), COUNTOF(m_AdapterName));

        m_pWiFiService = new WiFiService_t(WiFiService_t::GetDefaultServiceType());
        if (NULL == m_pWiFiService)
        {
            LogError(TEXT("Can't allocate WiFiService instance"));
            result = ERROR_OUTOFMEMORY;
        }
    }

    return result;
}

// -----------------------------------------------------------------------------
//
// Cleans up before program shutdown:
//
DWORD
MyNetMain_t::
Cleanup(void)
{
    if (m_pWiFiService)
    {
        delete m_pWiFiService;
        m_pWiFiService = NULL;
    }
    
    return CleanupWinsock();
}

// -----------------------------------------------------------------------------
//
// Called by netmain. Generates the MyNetMain object and runs it.
//
int
mymain(
    int    argc,
    TCHAR *argv[])
{
    MyNetMain_t realMain;
    DWORD result;

    result = realMain.Init(argc,argv);
    if (NO_ERROR == result)
        result = realMain.Run();

    DWORD cleanupResult = realMain.Cleanup();
    if (NO_ERROR == result)
        result = cleanupResult;

    if (result == NO_ERROR)
    {
        // Ignore error messages.
        NetLogResetMessageCounts();
    }

    return int(result);
}

// -----------------------------------------------------------------------------
