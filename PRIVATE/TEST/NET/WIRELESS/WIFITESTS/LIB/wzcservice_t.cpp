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
// Implementation of the WZCService_t class.
//
// ----------------------------------------------------------------------------

#include "WZCService_t.hpp"

#include <assert.h>

using namespace ce;
using namespace ce::qa;

// ----------------------------------------------------------------------------
//
// Constructor.
//
WZCService_t::
WZCService_t(void)
    : m_AdapterPopulated(false)
{
    m_AdapterGuid[0] = L'\0';
    m_AdapterName[0] = TEXT('\0');
}

// ----------------------------------------------------------------------------
//
// Destructor.
//
WZCService_t::
~WZCService_t(void)
{
    // nothing to do
}

// ----------------------------------------------------------------------------
//
// Retrieves the current list of wireless adapters into the specified buffer.
//
static DWORD
GetWZCAdapterTable(
    INTFS_KEY_TABLE *pIntfsTable)
{
    pIntfsTable->dwNumIntfs = 0;
    pIntfsTable->pIntfs = NULL;

    DWORD result = WZCEnumInterfaces(NULL, pIntfsTable);
    if (ERROR_SUCCESS != result)
    {
        LogError(TEXT("Can't enumerate wireless interfaces: %s"),
                 Win32ErrorText(result));
    }

    return result;
}

// ----------------------------------------------------------------------------
//
// Retrieves the name of first wireless adapter on the system.
//
static DWORD
GetFirstAdapterName(
    __out_ecount(AdapterNameChars) WCHAR *pAdapterGuid,
    __out_ecount(AdapterNameChars) TCHAR *pAdapterName,
                                   size_t  AdapterNameChars)
{
    DWORD result;
    HRESULT hr;

    pAdapterGuid[0] = L'\0';
    pAdapterName[0] = TEXT('\0');

    INTFS_KEY_TABLE intfsTable;
    result = GetWZCAdapterTable(&intfsTable);
    if (ERROR_SUCCESS != result)
        return result;

    if (intfsTable.dwNumIntfs == 0)
    {
        LogError(TEXT("System has no wireless interfaces"));
        return ERROR_INVALID_DATA;
    }

    SafeCopy(pAdapterGuid,
             intfsTable.pIntfs[0].wszGuid,
             AdapterNameChars);
    
    hr = WiFUtils::ConvertString(pAdapterName,
                                 pAdapterGuid,
                                  AdapterNameChars, "wifi adapter");

    // Free memory allocated by WZC.
    LocalFree(intfsTable.pIntfs);
    return HRESULT_CODE(hr);
}

// ----------------------------------------------------------------------------
//
// Determines whether the system has any wireless adapters.
//
bool
WZCService_t::
HasAdapter(void)
{
    if (!m_AdapterPopulated)
    {
        if (ERROR_SUCCESS == GetFirstAdapterName(m_AdapterGuid,
                                                 m_AdapterName,
                                                 MaxAdapterNameChars))
            m_AdapterPopulated = true;
    }
    return m_AdapterPopulated;
}

// ----------------------------------------------------------------------------
//
// Specifies the wireless adapter to be controlled.
// A null or empty name selects the first adapter on the system.
// Returns an error if the specified adapter doesn't exist.
//
DWORD
WZCService_t::
SetAdapterName(
    const TCHAR *pAdapterName)
{
    DWORD result;
    HRESULT hr;

    if (NULL != pAdapterName && m_AdapterPopulated
     && 0 == _tcsncmp(pAdapterName, m_AdapterName, MaxAdapterNameChars))
    {
        return ERROR_SUCCESS;
    }

    m_AdapterGuid[0] = L'\0';
    m_AdapterName[0] = TEXT('\0');
    m_AdapterPopulated = false;

    if (NULL == pAdapterName || TEXT('\0') == pAdapterName[0])
    {
        result = GetFirstAdapterName(m_AdapterGuid,
                                     m_AdapterName,
                                     MaxAdapterNameChars);
        if (ERROR_SUCCESS == result)
            m_AdapterPopulated = true;
        return result;
    }

    INTFS_KEY_TABLE intfsTable;
    result = GetWZCAdapterTable(&intfsTable);
    if (ERROR_SUCCESS != result)
        return result;

    if (0 == intfsTable.dwNumIntfs)
    {
        LogError(TEXT("System has no wireless interfaces"));
        return ERROR_INVALID_DATA;
    }

    hr = WiFUtils::ConvertString(m_AdapterGuid,
                                  pAdapterName,
                                MaxAdapterNameChars, "wifi adapter");
    if (FAILED(hr))
    {
        result = HRESULT_CODE(hr);
    }
    else
    {
        for (DWORD nx = 0 ; ; ++nx)
        {
            if (nx >= intfsTable.dwNumIntfs)
            {
                LogError(TEXT("System has no wireless interface named \"%s\""),
                         pAdapterName);
                m_AdapterGuid[0] = L'\0';
                result = ERROR_INVALID_DATA;
                break;
            }
            else
            if ((NULL != intfsTable.pIntfs[nx].wszGuid)
             && (0 == wcsncmp(intfsTable.pIntfs[nx].wszGuid,
                              m_AdapterGuid,
                              MaxAdapterNameChars)))
            {
                SafeCopy(m_AdapterName, pAdapterName, MaxAdapterNameChars);
                m_AdapterPopulated = true;
                result = ERROR_SUCCESS;
                break;
            }
        }
    }

    // Free memory allocated by WZC.
    LocalFree(intfsTable.pIntfs);
    return result;
}

// ----------------------------------------------------------------------------
//
// Gets the name of the wireless adapter being controlled:
// Returns an empty string if the system has no wireless adapters.
// Otherwise, returns either the adapter selected by SetAdapterName
// or the first adapter on the system.
//
const WCHAR *
WZCService_t::
GetAdapterGuid(void)
{
    if (!m_AdapterPopulated)
    {
        if (ERROR_SUCCESS == GetFirstAdapterName(m_AdapterGuid,
                                                 m_AdapterName,
                                                 MaxAdapterNameChars))
            m_AdapterPopulated = true;
    }
    return m_AdapterGuid;
}

const TCHAR *
WZCService_t::
GetAdapterName(void)
{
    if (!m_AdapterPopulated)
    {
        if (ERROR_SUCCESS == GetFirstAdapterName(m_AdapterGuid,
                                                 m_AdapterName,
                                                 MaxAdapterNameChars))
            m_AdapterPopulated = true;
    }
    return m_AdapterName;
}

// ----------------------------------------------------------------------------
//
// Initializes an empty INTF_ENTRY list.
//
static DWORD
InitIntfEntry(
    WZCService_t *pService,
    WZC_INTF_ENTRY *pIntf)
{
    DWORD result = ERROR_INVALID_DATA;
    if (pService->HasAdapter())
    {
        memset(pIntf, 0, sizeof(WZC_INTF_ENTRY));
        pIntf->wszGuid = const_cast<TCHAR *>(pService->GetAdapterGuid());
        result = ERROR_SUCCESS;
    }
    return result;
}

// ----------------------------------------------------------------------------
//
// Queries current wireless adapter information.
//
DWORD
WZCService_t::
QueryAdapterInfo(
    WZCIntfEntry_t *pIntf)
{
    assert(NULL != pIntf);

    WZC_INTF_ENTRY intf;
    DWORD result = InitIntfEntry(this, &intf);

    if (ERROR_SUCCESS == result)
    {
        const DWORD dataRequested = INTF_ALL
#ifdef INTF_ENTRY_V1
                                  | INTF_PMKCACHE
#endif
                                  ;

        DWORD intfFlags;
        result = WZCQueryInterface(NULL, dataRequested, &intf, &intfFlags);
        if (ERROR_SUCCESS == result)
        {
            pIntf->Assign(&intf, intfFlags);
        }
        else
        {
            LogError(TEXT("WZCQueryInterface failed: %s"),
                     Win32ErrorText(result));
        }
    }

    return result;
}

// ----------------------------------------------------------------------------
//
// Enables the WZC service.
//
DWORD
WZCService_t::
Enable(void)
{
    LogStatus(TEXT("Enabling WZC service"));

    // Initialize an empty INTF_ENTRY list.
    WZC_INTF_ENTRY intf;
    DWORD result = InitIntfEntry(this, &intf);
    if (ERROR_SUCCESS != result)
        return result;

    // Enable WZC.
    DWORD intfFlags = 0;
    intf.dwCtlFlags |= INTFCTL_ENABLED;
    result = WZCSetInterface(NULL, INTF_ENABLED, &intf, &intfFlags);
    if (ERROR_SUCCESS != result)
    {
        LogError(TEXT("WZCSetInterface failed: outFlags=0x%X %s"),
                 intfFlags, Win32ErrorText(result));
        return result;
    }

    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Disables the WZC service.
//
DWORD
WZCService_t::
Disable(void)
{
    LogStatus(TEXT("Disabling WZC service"));

    // Initialize an empty INTF_ENTRY list.
    WZC_INTF_ENTRY intf;
    DWORD result = InitIntfEntry(this, &intf);
    if (ERROR_SUCCESS != result)
        return result;

    // Disable WZC.
    DWORD intfFlags = 0;
    intf.dwCtlFlags &= ~INTFCTL_ENABLED;
    result = WZCSetInterface(NULL, INTF_ENABLED, &intf, &intfFlags);
    if (ERROR_SUCCESS != result)
    {
        LogError(TEXT("WZCSetInterface failed: outFlags=0x%X %s"),
                 intfFlags, Win32ErrorText(result));
        return result;
    }

    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Replaces the specified wireless adapter's Preferred Networks list.
//
static DWORD
ReplacePreferredList(
    WZCService_t *pService,
    const WZCConfigList_t &ConfigList)
{
    // Initialize an empty INTF_ENTRY list.
    WZC_INTF_ENTRY intf;
    DWORD result = InitIntfEntry(pService, &intf);
    if (ERROR_SUCCESS != result)
        return result;

    // Replace the preferred list.
    DWORD intfFlags = 0;
    intf.rdStSSIDList = ConfigList;
    result = WZCSetInterface(NULL, INTF_PREFLIST, &intf, &intfFlags);
    if (ERROR_SUCCESS != result)
    {
        LogError(TEXT("WZCSetInterface failed: outFlags=0x%X %s"),
                 intfFlags, Win32ErrorText(result));
        return result;
    }

    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Clears the current Preferred Networks list.
// This will disconnect everything attached to the wireless adapter.
//
DWORD
WZCService_t::
Clear(void)
{
    LogStatus(TEXT("Clearing WZC Preferred Networks List"));
    WZCConfigList_t emptyList;
    return ReplacePreferredList(this, emptyList);
}

// ----------------------------------------------------------------------------
//
// Refreshes WZC to force a reconnection.
//
DWORD
WZCService_t::
Refresh(void)
{
    LogStatus(TEXT("Refreshing WZC configuration"));

    // Initialize an empty INTF_ENTRY list.
    WZC_INTF_ENTRY intf;
    DWORD result = InitIntfEntry(this, &intf);
    if (ERROR_SUCCESS != result)
        return result;

    // Refresh WZC.
    DWORD intfFlags = 0;
    result = WZCRefreshInterface(NULL, INTF_ALL, &intf, &intfFlags);
    if (ERROR_SUCCESS != result)
    {
        LogError(TEXT("WZCRefreshInterface failed: outFlags=0x%X %s"),
                 intfFlags, Win32ErrorText(result));
        return result;
    }

    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Logs the current WZC context information.
//
DWORD
WZCService_t::
LogWZCContext(
    void (*LogFunc)(const TCHAR *pFormat, ...))
{
    assert(NULL != LogFunc);
    LogFunc(TEXT("\nWZC context parameter settings:"));

    WZC_CONTEXT context;
    DWORD intfFlags;
    DWORD result = WZCQueryContext(NULL, INTF_ALL, &context, &intfFlags);

    if (ERROR_SUCCESS != result)
    {
        LogError(TEXT("WZCQueryContext failed: %s"),
                 Win32ErrorText(result));
        return result;
    }

    LogFunc(TEXT("  outFlags = 0x%x"),
            intfFlags);

    LogFunc(TEXT("  tmTr = %ums (Scan time out)"),
            context.tmTr);
    LogFunc(TEXT("  tmTp = %ums (Association time out)"),
            context.tmTp);
    LogFunc(TEXT("  tmTc = %ums (Periodic scan when connected)"),
            context.tmTc);
    LogFunc(TEXT("  tmTf = %ums (Periodic scan when disconnected)"),
            context.tmTf);
    LogFunc(TEXT("  tmTd = %ums (State Soft Reset (SSr) processing timeout)"),
            context.tmTd);

    return ERROR_SUCCESS;;
}

// ----------------------------------------------------------------------------
//
// Replaces the Preferred Networks List.
// (This will schedule an immediate reconnection.)
//
DWORD
WZCService_t::
SetPreferredList(
    const WZCConfigList_t &ConfigList)
{
    LogDebug(TEXT("Replacing WZC Preferred Networks List"));
    return ReplacePreferredList(this, ConfigList);
}

// ----------------------------------------------------------------------------
//
// Inserts the specified element at the head of the Preferred
// Networks list.
//
DWORD
WZCService_t::
AddToHeadOfPreferredList(
    const WZCConfigItem_t &Config)
{
    DWORD result;

    WZCIntfEntry_t intf;
    result = QueryAdapterInfo(&intf);
    if (ERROR_SUCCESS == result)
    {
        WZCConfigList_t pref;
        result = intf.GetPreferredNetworks(&pref);
        if (ERROR_SUCCESS == result)
        {
            result = pref.AddHead(Config);
            if (ERROR_SUCCESS == result)
            {
                result = ReplacePreferredList(this, pref);
            }
        }
    }

    return result;
}

// ----------------------------------------------------------------------------
//
// Inserts the specified element at the tail of the Preferred
// Networks list.
//
DWORD
WZCService_t::
AddToTailOfPreferredList(
    const WZCConfigItem_t &Config)
{
    DWORD result;

    WZCIntfEntry_t intf;
    result = QueryAdapterInfo(&intf);
    if (ERROR_SUCCESS == result)
    {
        WZCConfigList_t pref;
        result = intf.GetPreferredNetworks(&pref);
        if (ERROR_SUCCESS == result)
        {
            result = pref.AddTail(Config);
            if (ERROR_SUCCESS == result)
            {
                result = ReplacePreferredList(this, pref);
            }
        }
    }

    return result;
}

// ----------------------------------------------------------------------------
//
// Removes the specfied Access Point from the Preferred Networks list.
// If the optional MAC address is not supplied, the method will remove
// the first Access Point with the specified SSID.
// If, in addition, the RemoveAllMatches flag is true, all the matching
// Access Point configuration entries will be removed.
// Returns ERROR_NOT_FOUND if there was no match.
//
DWORD
WZCService_t::
RemoveFromPreferredList(
    const TCHAR     *pSSIDName,
    const MACAddr_t *pMACAddress,
    bool             RemoveAllMatches)
{
    DWORD result;

    WZCIntfEntry_t intf;
    result = QueryAdapterInfo(&intf);
    if (ERROR_SUCCESS == result)
    {
        WZCConfigList_t pref;
        result = intf.GetPreferredNetworks(&pref);
        if (ERROR_SUCCESS == result)
        {
            for (int removed = 0 ;; ++removed)
            {
                WZCConfigItem_t config;
                result = pref.Find(pSSIDName, pMACAddress, &config);
                if (ERROR_SUCCESS == result)
                {
                    result = pref.Remove(config);
                    if (ERROR_SUCCESS == result && RemoveAllMatches)
                    {
                        continue;
                    }
                }

                if (ERROR_NOT_FOUND == result
                 && RemoveAllMatches
                 && 0 != removed)
                {
                    result = ERROR_SUCCESS;
                }

                if (ERROR_SUCCESS == result)
                {
                    result = ReplacePreferredList(this, pref);
                }
                break;
            }
        }
    }

    return result;
}

// ----------------------------------------------------------------------------
//
// Set the interval at which WZCSVC tries to determine available BSSIDs
// Default 60 sec
DWORD
WZCService_t::
SetFailedScanInterval(
   const DWORD IntervalMsec)// in millisec
{
    WZC_CONTEXT wzcContext;
    DWORD result;

    DWORD dummyFlags = 0;       
    result = WZCQueryContext(NULL, dummyFlags, &wzcContext , NULL);   

    if (result != ERROR_SUCCESS)
        return result;
    
    wzcContext.tmTf = IntervalMsec;    
    result = WZCSetContext(NULL, dummyFlags, &wzcContext , NULL);
    
    return result;
}

// Set the interval at which WZCSVC tries to determine available BSSIDs
DWORD
WZCService_t::
GetFailedScanInterval(
   DWORD *pIntervalMsec) // in millisec
{
    DWORD result;
    DWORD dummyFlags = 0;
       
    if(!pIntervalMsec)
    {
        LogError(TEXT("Null pointer passed in for Interval"));
        return ERROR_INVALID_PARAMETER;
    }
        
    WZC_CONTEXT wzcContext;
    
    result = WZCQueryContext(NULL, dummyFlags, &wzcContext , NULL);

    if (result == ERROR_SUCCESS)
           *pIntervalMsec = wzcContext.tmTf;
    
    return result;
}

// ----------------------------------------------------------------------------
//
// Set the interval at which WZCSVC tries to determine available BSSIDs
// Default 60 sec
//
DWORD
WZCService_t::
SetAssociatedScanInterval(
   const DWORD IntervalMsec)// in millisec
{
    WZC_CONTEXT wzcContext;
    DWORD result;
   
    DWORD dummyFlags = 0;       
    result = WZCQueryContext(NULL, dummyFlags, &wzcContext , NULL);   

    if (result != ERROR_SUCCESS)
        return result;
 
    wzcContext.tmTc = IntervalMsec;
    
    result = WZCSetContext(NULL, dummyFlags, &wzcContext , NULL);
    return result;
}

// Set the interval at which WZCSVC tries to determine available BSSIDs
DWORD
WZCService_t::
GetAssociatedScanInterval(
   DWORD *pIntervalMsec) // in millisec
{
    DWORD result;
   DWORD dummyFlags = 0;
         
    if(!pIntervalMsec)
    {
        LogError(TEXT("Null pointer passed in for Interval"));
        return ERROR_INVALID_PARAMETER;
    }
        
    WZC_CONTEXT wzcContext;
    
    result = WZCQueryContext(NULL, dummyFlags, &wzcContext , NULL);

    if (result == ERROR_SUCCESS)
           *pIntervalMsec = wzcContext.tmTc;
    
    return result;
}

// ----------------------------------------------------------------------------
//
// Monitors the adapter's status until the specified time runs out or
// it accociates with the specified Access Point,
// Returns ERROR_TIMEOUT if the association isn't established within the
// time-limit.
//
DWORD
WZCService_t::
MonitorAssociation(
    const TCHAR *pExpectedSSID,
    long         TimeLimit)  // milliseconds
{
    assert(NULL != pExpectedSSID);

    DWORD result;
    TCHAR ssidBuff[MAX_SSID_LEN+1];
    int   ssidChars = MAX_SSID_LEN;

    MACAddr_t macAddr;
    TCHAR     macBuff[COUNTOF(macAddr.m_Addr) * 4];
    int       macChars = COUNTOF(macBuff);

    LogDebug(TEXT("Waiting %ld seconds for association with SSID %s"),
             TimeLimit / 1000, pExpectedSSID);

    DWORD startTime = GetTickCount();
    long runDuration = 0;
    WZCIntfEntry_t intf;
    do
    {
        Sleep(250);
        runDuration = WiFUtils::SubtractTickCounts(GetTickCount(), startTime);

        // Get the current association status.
        result = QueryAdapterInfo(&intf);
        if (ERROR_SUCCESS != result)
            return result;

        // If associated, make sure it's the correct Access Point.
        if (ERROR_SUCCESS == intf.GetConnectedMAC(&macAddr)
         && ERROR_SUCCESS == intf.GetConnectedSSID(ssidBuff, ssidChars))
        {
            macAddr.ToString(macBuff, macChars);
            LogDebug(TEXT("Associated with SSID %s at MAC %s after %.2f secs"),
                     ssidBuff, macBuff, (double)runDuration / 1000.0);

            if (_tcsnicmp(ssidBuff, pExpectedSSID, MAX_SSID_LEN) == 0)
                return ERROR_SUCCESS;
            else
            {
                LogDebug(TEXT("Still associated with SSID %s - want %s"),
                         ssidBuff, pExpectedSSID);
            }
        }
    }
    while (TimeLimit > runDuration);

    LogError(TEXT("Reached limit of %.2f secs for association with SSID %s"),
             (double)TimeLimit / 1000.0, pExpectedSSID);
    return ERROR_TIMEOUT;
}

// ----------------------------------------------------------------------------
//
// Translates the specified authentication-mode into text form.
//
const TCHAR *
WZCService_t::
AuthenticationMode2Text(
    int AuthMode)
{
    static const TCHAR *s_AuthenticationMode[] =
    {
        TEXT("Ndis802_11AuthModeOpen"),
        TEXT("Ndis802_11AuthModeShared"),
        TEXT("Ndis802_11AuthModeAutoSwitch"),
        TEXT("Ndis802_11AuthModeWPA"),
        TEXT("Ndis802_11AuthModeWPAPSK"),
        TEXT("Ndis802_11AuthModeWPANone"),
        TEXT("Ndis802_11AuthModeWPA2"),
        TEXT("Ndis802_11AuthModeWPA2PSK")
    };
    return (0 <= AuthMode && AuthMode < COUNTOF(s_AuthenticationMode))
         ? s_AuthenticationMode[AuthMode]
         : TEXT("<unknown mode>");
}

// ----------------------------------------------------------------------------
//
// Translates the specified infrastructure-mode into text form.
//
const TCHAR *
WZCService_t::
InfrastructureMode2Text(
    int InfraMode)
{
    if (InfraMode == Ndis802_11IBSS)
        return TEXT("NDIS802_11IBSS");
    if (InfraMode == Ndis802_11Infrastructure)
        return TEXT("Ndis802_11Infrastructure");
    if (InfraMode == Ndis802_11AutoUnknown)
        return TEXT("Ndis802_11AutoUnknown");
    return TEXT("<unknown mode>");
}

// ----------------------------------------------------------------------------
//
// Translates the specified network-type into text form.
//
const TCHAR *
WZCService_t::
NetworkType2Text(
    int NetworkType)
{
    if (NetworkType == Ndis802_11FH)
        return   TEXT("NDIS802_11FH");
    if (NetworkType == Ndis802_11DS)
        return   TEXT("NDIS802_11DS");
    return TEXT("<unknown type>");
}

// ----------------------------------------------------------------------------
//
// Translates the specified encryption-mode into text form.
//
const TCHAR *
WZCService_t::
PrivacyMode2Text(
    int PrivacyMode)
{
    static const TCHAR *s_PrivacyMode[] =
    {
        TEXT("Ndis802_11WEPEnabled"),
        TEXT("Ndis802_11WEPDisabled"),
        TEXT("Ndis802_11WEPKeyAbsent"),
        TEXT("Ndis802_11WEPNotSupported"),
        TEXT("Ndis802_11Encryption2Enabled"),
        TEXT("Ndis802_11Encryption2KeyAbsent"),
        TEXT("Ndis802_11Encryption3Enabled"),
        TEXT("Ndis802_11Encryption3KeyAbsent")
    };
    return (0 <= PrivacyMode && PrivacyMode < COUNTOF(s_PrivacyMode))
         ? s_PrivacyMode[PrivacyMode]
         : TEXT("<unknown mode>");
}

// ----------------------------------------------------------------------------
//
// Translates the specified WEP-status code into text form.
//
const TCHAR *
WZCService_t::
WEPStatus2Text(
    int WEPStatus)
{
    static const TCHAR *s_WEPStatus[] =
    {
        TEXT("Ndis802_11WEPEnabled"),
        TEXT("Ndis802_11WEPDisabled"),
        TEXT("Ndis802_11WEPKeyAbsent"),
        TEXT("Ndis802_11WEPNotSupported")
    };
    return (0 <= WEPStatus && WEPStatus < COUNTOF(s_WEPStatus))
         ? s_WEPStatus[WEPStatus]
         : TEXT("<unknown status>");
}

// ----------------------------------------------------------------------------
//
// Formats the specified SSID into text form.
//
HRESULT
WZCService_t::
SSID2Text(
                            const BYTE *pData,
                            DWORD        DataLen,
  __out_ecount(BufferChars) TCHAR      *pBuffer,
                            int          BufferChars)
{
    HRESULT hr = S_OK;

    pBuffer[0] = TEXT('\0');

    if (NULL != pData && 0 != DataLen)
    {
        if (DataLen > BufferChars-1)
            DataLen = BufferChars-1;

        hr = WiFUtils::ConvertString(pBuffer, (const char *)pData,
                                      BufferChars, "SSID", DataLen);
    }
    return hr;
}

// ----------------------------------------------------------------------------
//
// Calculates the 802.11b channel number for given frequency.
// Returns 1-14 based on the given ulFrequency_kHz.
// Returns 0 for invalid frequency range.
//
// 2412 MHz = ch-1
// 2417 MHz = ch-2
// 2422 MHz = ch-3
// 2427 MHz = ch-4
// 2432 MHz = ch-5
// 2437 MHz = ch-6
// 2442 MHz = ch-7
// 2447 MHz = ch-8
// 2452 MHz = ch-9
// 2457 MHz = ch-10
// 2462 MHz = ch-11
// 2467 MHz = ch-12
// 2472 MHz = ch-13
// 2484 MHz = ch-14
//
UINT
WZCService_t::
ChannelNumber2Int(
    ULONG ulFrequency_kHz) // frequency in kHz
{
    ULONG ulFrequency_MHz = ulFrequency_kHz / 1000;
    if ((2412 <= ulFrequency_MHz) && (ulFrequency_MHz < 2484))
        return ((ulFrequency_MHz - 2412) / 5) + 1;
    else
    if (ulFrequency_MHz == 2484)
        return 14;

    // invalid channel number
    return 0;
}

// ----------------------------------------------------------------------------
