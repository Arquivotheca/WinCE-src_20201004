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
// Implementation of the WZCIntfEntry_t class.
//
// ----------------------------------------------------------------------------

#include "WZCIntfEntry_t.hpp"

#include "WZCService_t.hpp"

#include <assert.h>

using namespace ce;
using namespace ce::qa;

// ----------------------------------------------------------------------------
//
// Constructor.
//
WZCIntfEntry_t::
WZCIntfEntry_t(void)
    : m_IntfPopulated(false)
{
    Assign(NULL, 0);
}

// ----------------------------------------------------------------------------
//
// Destructor.
//
WZCIntfEntry_t::
~WZCIntfEntry_t(void)
{
    Assign(NULL, 0);
}

// ----------------------------------------------------------------------------
//
// Inserts the specified INTF_ENTRY structure.
//
void
WZCIntfEntry_t::
Assign(
    const WZC_INTF_ENTRY *pIntf,
    DWORD                  IntfFlags)
{
    if (m_IntfPopulated)
    {
        WZCDeleteIntfObj(&m_Intf);
    }
    if (pIntf)
    {
        memcpy(&m_Intf, pIntf, sizeof(m_Intf));
        m_IntfFlags = IntfFlags;
        m_IntfPopulated = true;
    }
    else
    {
        memset(&m_Intf, 0, sizeof(m_Intf));
        m_IntfFlags = 0;
        m_IntfPopulated = false;
    }
}

// ----------------------------------------------------------------------------
//
// Determines whether the wireless adapter is connected.
//
bool
WZCIntfEntry_t::
IsConnected(void) const
{
    MACAddr_t mac;
    return (ERROR_SUCCESS == GetConnectedMAC(&mac));
}

// ----------------------------------------------------------------------------
//
// Gets the MAC address or SSID of the connected station:
//
DWORD
WZCIntfEntry_t::
GetConnectedMAC(
    MACAddr_t *pMAC) const
{
    assert(NULL != pMAC);
    if (m_IntfFlags & INTF_BSSID)
    {
        const RAW_DATA &rawMAC = m_Intf.rdBSSID;
        if (NULL != rawMAC.pData)
        {
            pMAC->Assign(rawMAC.pData, rawMAC.dwDataLen);
            if (pMAC->IsValid())
                return ERROR_SUCCESS;
        }
    }
    return ERROR_NOT_FOUND;
}

DWORD
WZCIntfEntry_t::
GetConnectedSSID(
  __out_ecount(SSIDChars) TCHAR *pSSID,
                          int     SSIDChars) const
{
    if (m_IntfFlags & INTF_SSID)
    {
        HRESULT hr = WZCService_t::SSID2Text(m_Intf.rdSSID.pData,
                                             m_Intf.rdSSID.dwDataLen,
                                             pSSID, SSIDChars);
        if (FAILED(hr))
            return HRESULT_CODE(hr);
        if (TEXT('\0') != pSSID[0])
            return ERROR_SUCCESS;
    }
    return ERROR_NOT_FOUND;
}
 
// ----------------------------------------------------------------------------
//
// Checks whether the adapter supports the specified secureity modes:
bool
WZCIntfEntry_t::
IsSecurityModeSupported(
    NDIS_802_11_AUTHENTICATION_MODE AuthMode,
    ULONG                           CipherMode,
    DWORD                           EAPType) const
{
    bool driverSuppliedCapabilities = false;
    
#if 0
    LogDebug(TEXT("Checking WiFi NIC support for:"));
    LogDebug(TEXT("  authMode    = %u = \"%s\""), (unsigned int)AuthMode,
             WZCService_t::AuthenticationMode2Text(AuthMode));
    LogDebug(TEXT("  cipherMode  = %u = \"%s\""), (unsigned int)CipherMode,
             WZCService_t::PrivacyMode2Text(CipherMode));
#endif

#ifdef INTF_ENTRY_V1
    if (m_Intf.rdNicCapabilities.dwDataLen)
    {
        driverSuppliedCapabilities = true;
        
        const INTF_80211_CAPABILITY *pCapability =
              (const INTF_80211_CAPABILITY *) m_Intf.rdNicCapabilities.pData;

        for (DWORD px = 0 ; px < pCapability->dwNumOfAuthEncryptPairs ; ++px)
        {
            const NDIS_802_11_AUTHENTICATION_ENCRYPTION *pPair;
            pPair = &(pCapability->AuthEncryptPair[px]);

            if (AuthMode   == pPair->AuthModeSupported
             && CipherMode == pPair->EncryptStatusSupported)
            {
#if 0
                LogDebug(TEXT("WiFi NIC says it supports this mode"));
#endif
                return true;
            }
        }
    }
#endif /* INTF_ENTRY_V1 */

    if (m_IntfFlags & INTF_CAPABILITIES)
    {
        if (m_Intf.dwCapabilities & INTFCAP_SSN)
        {
            switch (AuthMode)
            {
                case Ndis802_11AuthModeWPA:
                case Ndis802_11AuthModeWPAPSK:
                    if (CipherMode == Ndis802_11Encryption2Enabled)
                    {
#if 0
                        LogDebug(TEXT("WiFi NIC says it supports basic WPA"));
#endif
                        return true;
                    }
            }
        }
#ifdef INTF_ENTRY_V1
        if (m_Intf.dwCapabilities & INTFCAP_80211I)
        {
            switch (AuthMode)
            {
                case Ndis802_11AuthModeWPA:
                case Ndis802_11AuthModeWPAPSK:
                case Ndis802_11AuthModeWPA2:
                case Ndis802_11AuthModeWPA2PSK:
                    if (CipherMode == Ndis802_11Encryption2Enabled
                     || CipherMode == Ndis802_11Encryption3Enabled)
                    {
#if 0
                        LogDebug(TEXT("WiFi NIC says it supports WPA2"));
#endif
                        return true;
                    }
            }
        }
#endif /* INTF_ENTRY_V1 */
    }

    if (!driverSuppliedCapabilities)
    {
        switch (AuthMode)
        {
            case Ndis802_11AuthModeOpen:
            case Ndis802_11AuthModeAutoSwitch:
            case Ndis802_11AuthModeShared:
#if 0
                LogDebug(TEXT("Basic mode supported by all WiFi NICs"));
#endif
                return true;
        }
    }
    
#if 0
    LogDebug(TEXT("WiFi NIC does not support these modes"));
#endif
    return false;
}

// ----------------------------------------------------------------------------
//
// Retrieves a copy of the Available or Preferred list.
//
DWORD
WZCIntfEntry_t::
GetAvailableNetworks(
    WZCConfigList_t *pConfigList) const
{
    assert(NULL != pConfigList);

    RAW_DATA dummyData;
    const RAW_DATA *pListData;
    if (m_IntfPopulated)
    {
        pListData = &m_Intf.rdBSSIDList;
    }
    else
    {
        pListData = &dummyData;
        memset(&dummyData, 0, sizeof(dummyData));
    }

   *pConfigList = *pListData;
    pConfigList->SetListName(TEXT("Available Networks"));
    return pConfigList->Privatize();
}

DWORD
WZCIntfEntry_t::
GetPreferredNetworks(
    WZCConfigList_t *pConfigList) const
{
    assert(NULL != pConfigList);

    RAW_DATA dummyData;
    const RAW_DATA *pListData;
    if (m_IntfPopulated)
    {
        pListData = &m_Intf.rdStSSIDList;
    }
    else
    {
        pListData = &dummyData;
        memset(&dummyData, 0, sizeof(dummyData));
    }

   *pConfigList = *pListData;
    return pConfigList->Privatize();
}

// ----------------------------------------------------------------------------
//
// Logs the current adapter information.
//
void
WZCIntfEntry_t::
Log(
    void (*LogFunc)(const TCHAR *pFormat, ...),
    bool Verbose) const     // logs all available information
{
    assert(NULL != LogFunc);

    LogFunc(TEXT("\nWZC wireless-adapter information:"));

    TCHAR buff[MAX_PATH];
    int   buffChars = COUNTOF(buff);

    MACAddr_t macAddr;
    const TCHAR *desc;

    LogFunc(TEXT("  adapter     = \"%s\""), m_Intf.wszGuid);
    LogFunc(TEXT("  description = \"%s\""), m_Intf.wszDescr);
    LogFunc(TEXT("  outFlags    = 0x%x\n"), m_IntfFlags);

    // BSSID; the MAC address of the AP wifi is connected.
    if (m_IntfFlags & INTF_BSSID)
    {
        if (ERROR_SUCCESS == GetConnectedMAC(&macAddr))
             desc = TEXT("(this wifi card is in associated state)");
        else desc = TEXT("(this wifi card is not associated to any)");
        macAddr.ToString(buff, buffChars);
        LogFunc(TEXT("  BSSID = %s %s"), buff, desc);
    }
    else
    {
        LogFunc(TEXT("  BSSID = <unknown> (not connected)"));
    }

    // Media Type
    if (m_IntfFlags & INTF_NDISMEDIA)
         LogFunc(TEXT("  Media Type          = [%d]"), m_Intf.ulMediaType);
    else LogFunc(TEXT("  Media Type          = <unknown>"));

    // Configuration Mode
    if (m_IntfFlags & INTF_ALL_FLAGS)
    {
        LogFunc(TEXT("  Configuration Mode  = [%08X]"), m_Intf.dwCtlFlags);
        if (m_Intf.dwCtlFlags & INTFCTL_ENABLED)
            LogFunc(TEXT("     zero conf enabled for this interface"));
        if (m_Intf.dwCtlFlags & INTFCTL_FALLBACK)
            LogFunc(TEXT("     attempt to connect to visible")
                    TEXT(      " non-preferred networks also"));
        if (m_Intf.dwCtlFlags & INTFCTL_OIDSSUPP)
            LogFunc(TEXT("     802.11 OIDs are supported by the")
                    TEXT(      " driver/firmware"));
        if (m_Intf.dwCtlFlags & INTFCTL_VOLATILE)
            LogFunc(TEXT("     the service parameters are volatile"));
        if (m_Intf.dwCtlFlags & INTFCTL_POLICY)
            LogFunc(TEXT("     the service parameters policy enforced"));
    }
    else
    {
        LogFunc(TEXT("  Configuration Mode  = <unknown>"));
    }

    // Infrastructure Mode
    if (m_IntfFlags & INTF_INFRAMODE)
    {
        if (m_Intf.nInfraMode == Ndis802_11IBSS)
             desc = TEXT("IBSS net (adhoc net)");
        else
        if (m_Intf.nInfraMode == Ndis802_11Infrastructure)
             desc = TEXT("Infrastructure net")
                    TEXT(" (connected to an Access Point)");
        else desc = TEXT("Ndis802_11AutoUnknown");
        LogFunc(TEXT("  Infrastructure Mode = [%d] %s"),
                m_Intf.nInfraMode, desc);
    }
    else
    {
        LogFunc(TEXT("  Infrastructure Mode = <unknown>"));
    }

    // Authentication Mode
    if (m_IntfFlags & INTF_AUTHMODE)
    {
        desc = WZCService_t::AuthenticationMode2Text(m_Intf.nAuthMode);
        LogFunc(TEXT("  Authentication Mode = [%d] %s"),
                m_Intf.nAuthMode, desc);
    }
    else
    {
        LogFunc(TEXT("  Authentication Mode = <unknown>"));
    }

#ifdef INTF_ENTRY_V1
    // Capabilities
    LogFunc(TEXT("  rdNicCapabilities   = %d bytes"),
            m_Intf.rdNicCapabilities.dwDataLen);

    if (m_Intf.rdNicCapabilities.dwDataLen)
    {
        const INTF_80211_CAPABILITY *pCapability =
                 (const INTF_80211_CAPABILITY *) m_Intf.rdNicCapabilities.pData;

        LogFunc(TEXT("    dwNumOfPMKIDs            : [%d]"),
                pCapability->dwNumOfPMKIDs);
        LogFunc(TEXT("    dwNumOfAuthEncryptPairs  : [%d]"),
                pCapability->dwNumOfAuthEncryptPairs);

        for (DWORD px = 0 ; px < pCapability->dwNumOfAuthEncryptPairs ; ++px)
        {
            const NDIS_802_11_AUTHENTICATION_ENCRYPTION *pPair;
            pPair = &(pCapability->AuthEncryptPair[px]);

            LogFunc(TEXT("       Pair[%d]"), px+1);

            desc = WZCService_t::AuthenticationMode2Text(pPair->AuthModeSupported);
            LogFunc(TEXT("          AuthmodeSupported          [%d] %s"),
                    pPair->AuthModeSupported, desc);

            desc = WZCService_t::PrivacyMode2Text(pPair->EncryptStatusSupported);
            LogFunc(TEXT("          EncryptStatusSupported     [%d] %s"),
                    pPair->EncryptStatusSupported, desc);
        }
    }
#endif /* INTF_ENTRY_V1 */

    // WEP status
    if (m_IntfFlags & INTF_WEPSTATUS)
    {
        desc = WZCService_t::WEPStatus2Text(m_Intf.nWepStatus);
        LogFunc(TEXT("  WEP Status          = [%d] %s"),
                m_Intf.nWepStatus, desc);
    }
    else
    {
        LogFunc(TEXT("  WEP Status          = <unknown>"));
    }

    // SSID status
    if (ERROR_SUCCESS == GetConnectedSSID(buff, buffChars))
         LogFunc(TEXT("  SSID                = %s"), buff);
    else LogFunc(TEXT("  SSID                = <unknown>"));

    if (m_IntfFlags & INTF_CAPABILITIES)
    {
        buff[0] = 0;
        if (m_Intf.dwCapabilities & INTFCAP_SSN)
            SafeAppend(buff, TEXT(" WPA/TKIP capable"), buffChars);
#ifdef INTF_ENTRY_V1
        if (m_Intf.dwCapabilities & INTFCAP_80211I)
            SafeAppend(buff, TEXT(" WPA2/AES capable"), buffChars);
#endif /* INTF_ENTRY_V1 */

        LogFunc(TEXT("  Capabilities        =%s"), buff);
    }

    LogFunc(TEXT("  rdCtrlData length   = %d bytes"),
            m_Intf.rdCtrlData.dwDataLen);

#ifdef INTF_ENTRY_V1
#ifdef UNDER_CE
    // PMK Cache
    if (m_IntfFlags & INTF_PMKCACHE)
    {
        DWORD pmkFlags = m_Intf.PMKCacheFlags;
        LogFunc(TEXT("  PMK Cache:"));
        LogFunc(TEXT("    Enabled           = %s"),
                ((pmkFlags & INTF_ENTRY_PMKCACHE_FLAG_ENABLE)?
                  TEXT("ON") : TEXT("OFF")));
        LogFunc(TEXT("    Preauthentication = %s"),
                ((pmkFlags & INTF_ENTRY_PMKCACHE_FLAG_ENABLE_PREAUTH)?
                  TEXT("ON") : TEXT("OFF")));
        LogFunc(TEXT("    Opportunistic     = %s"),
                ((pmkFlags & INTF_ENTRY_PMKCACHE_FLAG_ENABLE_OPPORTUNISTIC)?
                  TEXT("ON") : TEXT("OFF")));
        if (0 == m_Intf.rdPMKCache.dwDataLen)
        {
            LogFunc(TEXT("    BSSIDInfoCount    = [0]"));
        }
        else
        {
            const NDIS_802_11_PMKID *pCache;
            pCache = (const NDIS_802_11_PMKID *) m_Intf.rdPMKCache.pData;

            LogFunc(TEXT("    BSSIDInfoCount    = [%u]:"),
                    pCache->BSSIDInfoCount);
            for (DWORD kx = 0 ; kx < pCache->BSSIDInfoCount ; ++kx)
            {
                macAddr.Assign(pCache->BSSIDInfo[kx].BSSID,
                        sizeof(pCache->BSSIDInfo[kx].BSSID));
                macAddr.ToString(buff, buffChars);

                const BYTE *pId  = &(pCache->BSSIDInfo[kx].PMKID[0]);
                LogFunc(TEXT("      BSSID=%s PMKID=%02X%02X..%02X%02X"), 
				        buff, pId[0], pId[1], pId[14], pId[15]);
            }
        }
    }
#endif /* UNDER_CE */
#endif /* INTF_ENTRY_V1 */

    WZCConfigList_t availableList(m_Intf.rdBSSIDList);
    availableList.SetListName(TEXT("Available Networks"));
    availableList.Log(LogFunc, Verbose);

    WZCConfigList_t preferredList(m_Intf.rdStSSIDList);
    preferredList.Log(LogFunc, Verbose);

    LogFunc(TEXT(""));
}

// ----------------------------------------------------------------------------
