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
// Implementation of the WZCConfigList_t class.
//
// ----------------------------------------------------------------------------

#include "WZCConfigList_t.hpp"

#include "WZCService_t.hpp"

#include <assert.h>

using namespace ce;
using namespace ce::qa;

// ----------------------------------------------------------------------------
//
// Copies or deallocates the specified WZC network-configuration list
// structure.
//
DWORD
WZCConfigList_t::
CopyConfigList(RAW_DATA *pDest, const RAW_DATA &Source)
{
    assert(NULL != pDest);

    // Copy the list itself.
    DWORD result = WZCConfigItem_t::CopyRawData(pDest, Source);

    // Copy the list elements (if any).
    if (ERROR_SUCCESS == result && pDest->pData && pDest->dwDataLen)
    {
        const WZC_802_11_CONFIG_LIST *pOldList;
              WZC_802_11_CONFIG_LIST *pNewList;
        pOldList = (const WZC_802_11_CONFIG_LIST *)Source.pData;
        pNewList =       (WZC_802_11_CONFIG_LIST *)pDest->pData;

        for (ULONG cx = 0 ; cx < pOldList->NumberOfItems ; ++cx)
        {
            result = WZCConfigItem_t::CopyConfigItem(&(pNewList->Config[cx]),
                                                       pOldList->Config[cx]);

            if (ERROR_SUCCESS != result)
            {
                pNewList->NumberOfItems = cx;
                break;
            }
        }
    }

    return result;
}

void
WZCConfigList_t::
FreeConfigList(RAW_DATA *pData)
{
    assert(NULL != pData);

    if (pData->pData && pData->dwDataLen)
    {
        // Deallocate the list elements. 
        WZC_802_11_CONFIG_LIST *pList = (WZC_802_11_CONFIG_LIST *)pData->pData;
        for (ULONG cx = 0 ; cx < pList->NumberOfItems ; ++cx)
        {
            WZCConfigItem_t::FreeConfigItem(&(pList->Config[cx]));
        }

        // Deallocate the list itself.
        WZCConfigItem_t::FreeRawData(pData);
    }
}

// ----------------------------------------------------------------------------
//
// Constructor.
//
WZCConfigList_t::
WZCConfigList_t(void)
{
    // Nothing to do.
}

// ----------------------------------------------------------------------------
//
// Destructor.
//
WZCConfigList_t::
~WZCConfigList_t(void)
{
    // Nothing to do.
}

// ----------------------------------------------------------------------------
//
// Copy constructors.
//
WZCConfigList_t::
WZCConfigList_t(const WZCConfigList_t &Source)
    : m_pData(Source.m_pData)
{
    memcpy(m_ListName, Source.m_ListName, sizeof(m_ListName));
}

WZCConfigList_t::
WZCConfigList_t(const RAW_DATA &Source)
    : m_pData(Source)
{
    SetListName(TEXT("Preferred Networks"));
}

// ----------------------------------------------------------------------------
//
// Assignment operators.
//
WZCConfigList_t &
WZCConfigList_t::
operator = (const WZCConfigList_t &Source)
{
    if (&Source != this)
    {
        m_pData = Source.m_pData;
        memcpy(m_ListName, Source.m_ListName, sizeof(m_ListName));
    }
    return *this;
}

WZCConfigList_t &
WZCConfigList_t::
operator = (const RAW_DATA &Source)
{
    if (&Source != m_pData)
    {
        m_pData = Source;
        SetListName(TEXT("Preferred Networks"));
    }
    return *this;
}

// ----------------------------------------------------------------------------
//
// Makes sure this object contains a private copy of the WZC data.
//
DWORD
WZCConfigList_t::
Privatize(void)
{
    DWORD result = m_pData.Privatize();

    if (ERROR_SUCCESS != result)
    {
        LogError(TEXT("Can't allocate memory to copy RAW_DATA config list"));
    }

    return result;
}

// ----------------------------------------------------------------------------
//
// Gets the count of network configurations in the list.
//
int
WZCConfigList_t::
Size(void) const
{
    if (m_pData->pData && m_pData->dwDataLen)
    {
        const WZC_802_11_CONFIG_LIST *pList;
        pList = (const WZC_802_11_CONFIG_LIST *)m_pData->pData;
        return static_cast<int>(pList->NumberOfItems);
    }
    return 0;
}

// ----------------------------------------------------------------------------
//
// Gets or sets the name of the list.
//
const TCHAR *
WZCConfigList_t::
GetListName(void) const
{
    return m_ListName;
}

void
WZCConfigList_t::
SetListName(const TCHAR *pListName)
{
    SafeCopy(m_ListName, pListName, COUNTOF(m_ListName));
}

// ----------------------------------------------------------------------------
//
// Searches the list for the specified configuration item.
// Returns the index of the matching element or -1.
//
static int
LookupConfigItem(
    const TCHAR     *pSSIDName,
    const MACAddr_t *pMACAddress,
    const RAW_DATA  &ConfigData,
    int              StartAt = 0)
{
    HRESULT hr;
    TCHAR ssidBuff[MAX_SSID_LEN+1];

    if (NULL != pSSIDName && TEXT('\0') == pSSIDName[0])
        pSSIDName = NULL;

    if (NULL != pMACAddress && !pMACAddress->IsValid())
        pMACAddress = NULL;

    if (NULL != ConfigData.pData && 0 != ConfigData.dwDataLen)
    {
        const WZC_802_11_CONFIG_LIST *pConfigList;
        pConfigList = (const WZC_802_11_CONFIG_LIST *) ConfigData.pData;
        for (ULONG cx = StartAt ; cx < pConfigList->NumberOfItems ; ++cx)
        {
            const WZC_WLAN_CONFIG *pConfig = &(pConfigList->Config[cx]);
            
            if (NULL != pSSIDName)
            {
                hr = WZCService_t::SSID2Text(pConfig->Ssid.Ssid,
                                             pConfig->Ssid.SsidLength,
                                             ssidBuff, MAX_SSID_LEN);
                if (SUCCEEDED(hr)
                 && 0 == _tcsnicmp(ssidBuff, pSSIDName, MAX_SSID_LEN))
                {
                    return cx;
                }
            }

            if (NULL != pMACAddress)
            {
                if (0 == memcmp(pMACAddress->m_Addr, pConfig->MacAddress,
                                              sizeof(pConfig->MacAddress)))
                {
                    return cx;
                }
            }
        }
    }

    return -1;
}

// ----------------------------------------------------------------------------
//
// Retrieves the WZC configuration element describing the specified
// SSID and/or MAC address from the adapter's Available Networks list.
// Returns ERROR_NOT_FOUND if there is no match.
// Note that when only selecting based on the SSID, more than one
// list element may contain a matching SSID. In that case, the method
// will retrieve the first match.
//
DWORD
WZCConfigList_t::
Find(
    const TCHAR     *pSSIDName,   // optional
    const MACAddr_t *pMACAddress, // optional
    WZCConfigItem_t *pConfig) const
{
    assert(NULL != pConfig);
    int foundAt = LookupConfigItem(pSSIDName, pMACAddress, *m_pData);
    if (foundAt < 0)
    {
        return ERROR_NOT_FOUND;
    }
    else
    {
        const WZC_802_11_CONFIG_LIST *pList;
        pList = (const WZC_802_11_CONFIG_LIST *) m_pData->pData;
       *pConfig = pList->Config[foundAt];
        return ERROR_SUCCESS;
    }
}

// ----------------------------------------------------------------------------
//
// Inserts the specified element at the head or tail of the List.
//
static DWORD
AddConfigToList(
    const WZCConfigItem_t &Config,
    RAW_DATA              *pRawList,
    const TCHAR           *pListName,
    bool                   InsertAtHead)
{
    WZC_802_11_CONFIG_LIST *pOldList, *pNewList;
    DWORD result;

    // Get the current list.
    if (NULL != pRawList->pData && 0 != pRawList->dwDataLen)
         pOldList = (WZC_802_11_CONFIG_LIST *) pRawList->pData;
    else pOldList = NULL;

    // Determine whether there's anything in the current list.
    ULONG oldListItems;
    if (NULL == pOldList)
    {
        oldListItems = 0;
        if (pListName[0])
        {
            LogDebug(TEXT("Creating new %s containing:"), pListName);
            Config.Log(LogDebug, TEXT("  "), false);
        }
    }
    else
    {
        oldListItems = pOldList->NumberOfItems;
        if (pListName[0])
        {
            LogDebug(TEXT("Adding config item at %s of %lu-item %s:"),
                     InsertAtHead? TEXT("head") : TEXT("tail"),
                     oldListItems, pListName);
            Config.Log(LogDebug, TEXT("  "), false);
        }
    }

    // Allocate memory for the new list.
    DWORD dataLen = sizeof(WZC_802_11_CONFIG_LIST)
                  + sizeof(WZC_WLAN_CONFIG) * oldListItems;
    pNewList = (WZC_802_11_CONFIG_LIST *) LocalAlloc(LPTR, dataLen);
    if (NULL == pNewList)
    {
        LogError(TEXT("Can't allocate %u bytes for WZC network config list"),
                 dataLen);
        return ERROR_OUTOFMEMORY;
    }

    // Insert the new config entry.
    pNewList->NumberOfItems = oldListItems+1;
    pNewList->Index = InsertAtHead? 0 : oldListItems;

    WZC_WLAN_CONFIG *pNewConfig = &(pNewList->Config[pNewList->Index]);
    result = WZCConfigItem_t::CopyConfigItem(pNewConfig, Config);
    if (ERROR_SUCCESS != result)
        return result;

    pRawList->pData = (BYTE *) pNewList;
    pRawList->dwDataLen = dataLen;

    // Copy the old config items (if any) before or after the new one
    // and deallocate the old list.
    if (NULL != pOldList)
    {
        if (0 < oldListItems)
        {
            dataLen = sizeof(WZC_WLAN_CONFIG) * oldListItems;
            memcpy(&pNewList->Config[InsertAtHead? 1 : 0],
                   &pOldList->Config[0], dataLen);
        }
        LocalFree(pOldList);
    }

    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Inserts the specified element at the head of the List.
//
DWORD
WZCConfigList_t::
AddHead(
    const WZCConfigItem_t &Config)
{
    DWORD result = Privatize();
    if (ERROR_SUCCESS == result)
        result = AddConfigToList(Config, m_pData, m_ListName, true);
    return result;
}

// ----------------------------------------------------------------------------
//
// Inserts the specified element at the tail of the List.
//
DWORD
WZCConfigList_t::
AddTail(
    const WZCConfigItem_t &Config)
{
    DWORD result = Privatize();
    if (ERROR_SUCCESS == result)
        result = AddConfigToList(Config, m_pData, m_ListName, false);
    return result;
}

// ----------------------------------------------------------------------------
//
// Removes the specified element from the list.
// Returns ERROR_NOT_FOUND if there is no match.
//
DWORD
WZCConfigList_t::
Remove(
    const WZCConfigItem_t &Config)
{
    WZC_802_11_CONFIG_LIST *pOldList, *pNewList;
    const WZC_WLAN_CONFIG &rawConfig = Config;
    HRESULT hr;

    // Get the key values.
    TCHAR ssidBuff[MAX_SSID_LEN+1];
    hr = WZCService_t::SSID2Text(rawConfig.Ssid.Ssid,
                                 rawConfig.Ssid.SsidLength,
                                 ssidBuff, MAX_SSID_LEN);
    if (FAILED(hr))
        ssidBuff[0] = TEXT('\0');

    MACAddr_t macAddr;
    macAddr.Assign((BYTE *)rawConfig.MacAddress,
                    sizeof(rawConfig.MacAddress));

    // Look up the keys in the list.
    int foundAt = LookupConfigItem(ssidBuff, &macAddr, *m_pData);
    if (foundAt < 0)
        return ERROR_NOT_FOUND;

    // Get the current list.
    pOldList = (WZC_802_11_CONFIG_LIST *) m_pData->pData;
    ULONG oldListItems = pOldList->NumberOfItems;
    if (m_ListName[0])
    {
        LogDebug(TEXT("Removing config item from %lu-item %s:"),
                 oldListItems, m_ListName);
        Config.Log(LogDebug, TEXT("  "), false);
    }

    // If the new list is empty, just deallocate the current one.
    if (1 >= oldListItems)
    {
        this->~WZCConfigList_t();
        new (this) WZCConfigList_t();
        return ERROR_SUCCESS;
    }

    // Get a private copy of the list.
    DWORD result = Privatize();
    if (ERROR_SUCCESS != result)
        return result;

    pOldList = (WZC_802_11_CONFIG_LIST *) m_pData->pData;
    
    // Allocate space for the new list.
    DWORD dataLen = sizeof(WZC_802_11_CONFIG_LIST)
                  + sizeof(WZC_WLAN_CONFIG) * (oldListItems-2);
    pNewList = (WZC_802_11_CONFIG_LIST *) LocalAlloc(LPTR, dataLen);
    if (NULL == pNewList)
    {
        LogError(TEXT("Can't allocate %u bytes for WZC network config list"),
                 dataLen);
        return ERROR_OUTOFMEMORY;
    }

    pNewList->NumberOfItems = oldListItems-1;
    if (pOldList->Index < foundAt)
         pNewList->Index = pOldList->Index;
    else pNewList->Index = pOldList->Index-1;

    m_pData->pData = (BYTE *) pNewList;
    m_pData->dwDataLen = dataLen;

    // Copy the remaining items from the old list to the new.
    if (foundAt > 0)
    {
        dataLen = sizeof(WZC_WLAN_CONFIG) * foundAt;
        memcpy(&pNewList->Config[0],
               &pOldList->Config[0], dataLen);
    }
    if (foundAt < (oldListItems-1))
    {
        dataLen = sizeof(WZC_WLAN_CONFIG) * ((oldListItems-1) - foundAt);
        memcpy(&pNewList->Config[foundAt],
               &pOldList->Config[foundAt+1], dataLen);
    }

    // Deallocate the deleted element and the old list.
    WZCConfigItem_t::FreeConfigItem(&(pOldList->Config[foundAt]));
    LocalFree(pOldList);
    
    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Logs the list preceded by the specified header.
//
void
WZCConfigList_t::
Log(
    void (*LogFunc)(const TCHAR *pFormat, ...),
    bool   Verbose) const
{
    if (m_pData->pData == NULL || m_pData->dwDataLen == 0)
    {
        LogFunc(TEXT("\n  %s <empty>"), m_ListName);
    }
    else
    {
        const WZC_802_11_CONFIG_LIST * pList =
       (const WZC_802_11_CONFIG_LIST *)m_pData->pData;

        LogFunc(TEXT("\n**** %d %s:"), pList->NumberOfItems, m_ListName);

        for (UINT px = 0 ; px < pList->NumberOfItems ; ++px)
        {
            LogFunc(TEXT("\n  %s[%u] *****"), m_ListName, px);
            WZCConfigItem_t config(pList->Config[px]);
            config.Log(LogFunc, TEXT("    "), Verbose);
        }
    }
}

// ----------------------------------------------------------------------------
