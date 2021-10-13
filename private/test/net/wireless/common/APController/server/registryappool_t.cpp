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
// Implementation of the RegistryAPPool_t class.
//
// ----------------------------------------------------------------------------

#include "RegistryAPPool_t.hpp"
#include "APConfigDevice_t.hpp"
#include "APCUtils.hpp"
#include "AttenuatorDevice_t.hpp"

#include <assert.h>
#include <intsafe.h>
#include <tchar.h>
#include <strsafe.h>

using namespace ce;
using namespace ce::qa;

// ----------------------------------------------------------------------------
// 
// "Nodes" for associating unique device keys with APs and Attenuators.
//
namespace ce {
namespace qa {

class APConfigNode_t
{
public:

    APConfigDevice_t *m_pDevice;
    ce::tstring       m_DeviceKey;

    APConfigNode_t(
        APConfigDevice_t *pDevice,
        const TCHAR      *pDeviceKey)
        : m_pDevice(pDevice),
          m_DeviceKey(pDeviceKey)
    { }
   ~APConfigNode_t(void)
    { }
};

class AttenuatorNode_t
{
public:

    AttenuatorDevice_t *m_pDevice;
    ce::tstring         m_DeviceKey;

    AttenuatorNode_t(
        AttenuatorDevice_t *pDevice,
        const TCHAR        *pDeviceKey)
        : m_pDevice(pDevice),
          m_DeviceKey(pDeviceKey)
    { }
   ~AttenuatorNode_t(void)
    { }
};

};
};

// ----------------------------------------------------------------------------
// 
// Constructor.
//
RegistryAPPool_t::
RegistryAPPool_t(void)
    : m_pConfigList(NULL),
      m_ConfigListSize(0),
      m_ConfigListAlloc(0),
      m_pAttenList(NULL),
      m_AttenListSize(0),
      m_AttenListAlloc(0)
{
    // nothing to do
}

// ----------------------------------------------------------------------------
//
// Destructor.
//
RegistryAPPool_t::
~RegistryAPPool_t(void)
{
    Clear();
}

// ----------------------------------------------------------------------------
//
// Inserts the specified APController into the list.
//
HRESULT
RegistryAPPool_t::
InsertAPConfigDevice(
    APConfigDevice_t   *pConfig,
    AttenuatorDevice_t *pAttenuator)
{
    HRESULT hr;

    auto_ptr<APConfigNode_t>   pConfigNode;
    auto_ptr<AttenuatorNode_t> pAttenNode;

    TCHAR nameBuff[64];
    int   nameChars = COUNTOF(nameBuff);

    // Allocate space for the AP list.
    if (m_ConfigListSize >= m_ConfigListAlloc)
    {
        size_t newAlloc; void *newList;
        hr = SizeTAdd(m_ConfigListAlloc, 16, &newAlloc);
        if (FAILED(hr))
             newList = NULL;
        else newList = calloc(newAlloc, sizeof(APConfigNode_t *));
        if (NULL == newList)
        {
            size_t allocSize = newAlloc * sizeof(APConfigNode_t *);
            LogError(TEXT("Can't allocate %u bytes for APConfigNode list"),
                     allocSize);
            return E_OUTOFMEMORY;
        }

        if (NULL != m_pConfigList)
        {
            if (0 < m_ConfigListAlloc)
            {
                size_t allocSize = m_ConfigListAlloc * sizeof(APConfigNode_t *);
                memcpy(newList, m_pConfigList, allocSize);
            }
            free(m_pConfigList);
        }

        m_ConfigListAlloc = (int) newAlloc;
        m_pConfigList = (APConfigNode_t **) newList;
    }

    // Allocate space for the attenuator list.
    if (m_AttenListSize >= m_AttenListAlloc)
    {
        size_t newAlloc; void *newList;
        hr = SizeTAdd(m_AttenListAlloc, 16, &newAlloc);
        if (FAILED(hr))
             newList = NULL;
        else newList = calloc(newAlloc, sizeof(AttenuatorNode_t *));
        if (NULL == newList)
        {
            size_t allocSize = newAlloc * sizeof(AttenuatorNode_t *);
            LogError(TEXT("Can't allocate %u bytes for AttenuatorNode list"),
                     allocSize);
            return E_OUTOFMEMORY;
        }

        if (NULL != m_pAttenList)
        {
            if (0 < m_AttenListAlloc)
            {
                size_t allocSize = m_AttenListAlloc * sizeof(AttenuatorNode_t*);
                memcpy(newList, m_pAttenList, allocSize);
            }
            free(m_pAttenList);
        }

        m_AttenListAlloc = (int) newAlloc;
        m_pAttenList = (AttenuatorNode_t **) newList;
    }

    // Generate a unique name for the AP.
    const TCHAR *apName = pConfig->GetAPName();
    if (apName[0])
    {
        bool isUnique = false;
        for (int uniqueId = 0 ; !isUnique ; ++uniqueId)
        {
            isUnique = true;

            if (0 == uniqueId)
                 hr = StringCchPrintf(nameBuff, nameChars, TEXT("%s"), apName);
            else hr = StringCchPrintf(nameBuff, nameChars, TEXT("%s%d"),
                                      apName, uniqueId);
            if (FAILED(hr))
            {
                LogError(TEXT("Can't generate unique AP name: %s"),
                         HRESULTErrorText(hr));
                return hr;
            }

            for (int apx = m_ConfigListSize ; 0 <= --apx ;)
            {
                if (0 == _tcscmp(nameBuff, m_pConfigList[apx]->m_DeviceKey))
                {
                    isUnique = false;
                    break;
                }
            }
        }
    }
    else
    {
        hr = StringCchPrintf(nameBuff, nameChars,
                             TEXT("AP%d"), m_ConfigListSize+1);
        if (FAILED(hr))
        {
            LogError(TEXT("Can't generate unique AP name: %s"),
                     HRESULTErrorText(hr));
            return hr;
        }
    }

    // Allocate the nodes.
    pConfigNode = new APConfigNode_t(pConfig, nameBuff);
    if (!pConfigNode.valid())
    {
        LogError(TEXT("Can't allocate APConfigNode"));
        return E_OUTOFMEMORY;
    }
        
    pAttenNode = new AttenuatorNode_t(pAttenuator, nameBuff);
    if (!pAttenNode.valid())
    {
        LogError(TEXT("Can't allocate AttenuatorNode"));
        return E_OUTOFMEMORY;
    }
        
    // Insert into the lists.
    hr = InsertAPController(pConfig, pAttenuator);
    if (SUCCEEDED(hr))
    {
        m_pConfigList[m_ConfigListSize++] = pConfigNode.release();
        m_pAttenList [m_AttenListSize++]  = pAttenNode.release();
    }
    return hr;
}

// ----------------------------------------------------------------------------
//
// Initializes the list of APControllers.
//
HRESULT
RegistryAPPool_t::
LoadAPControllers(const TCHAR *pRootKey)
{
    HRESULT hr;
    LONG    result;

    auto_ptr<APConfigDevice_t>   pConfig;
    auto_ptr<AttenuatorDevice_t> pAtten;
        
    LogDebug(TEXT("RegistryAPPool::LoadAPControllers(\"%s\")"), pRootKey);
    Clear();
        
    // Open the top-level registry directory.
    auto_hkey rootHkey;
    result = RegOpenKeyEx(HKEY_CURRENT_USER, pRootKey, 0, KEY_READ, &rootHkey);
    if (ERROR_SUCCESS != result)
    {
        LogError(TEXT("Cannot open AP-config root-key \"%s\": %s"),
                 pRootKey, Win32ErrorText(result));
        return HRESULT_FROM_WIN32(result);
    }

    // For each AP section...
    TCHAR apName[64];
    for (int apx = 0 ; ; ++apx)
    {
        // Read the AP name and open the AP's sub-section.
        LogDebug(TEXT("Reading AP %d"), apx);
        DWORD apNameChars = COUNTOF(apName);
        result = RegEnumKeyEx(rootHkey, apx,
                              apName, &apNameChars,
                              NULL, NULL, NULL, NULL);
        switch (result)
        {
            case ERROR_SUCCESS:
                break;
            case ERROR_NO_MORE_ITEMS:
                return ERROR_SUCCESS;
            default:
                LogError(TEXT("Cannot enumerate AP #%d in \"%s\": %s"),
                         apx, pRootKey, Win32ErrorText(result));
                return HRESULT_FROM_WIN32(result);
        }

        // Generate and initialize the configurator.
        hr = APConfigDevice_t::CreateConfigurator(pRootKey, apName, &pConfig);
        if (FAILED(hr))
            return hr;

        // Generate and initialize the attenuator.
        hr = AttenuatorDevice_t::CreateAttenuator(pRootKey, apName, &pAtten);
        if (FAILED(hr))
            return hr;

        // Store the initial AP configuration in the list.
        hr = InsertAPConfigDevice(pConfig, pAtten);
        if (FAILED(hr))
            return hr;
        pConfig.release();
        pAtten.release();
    }
}

// ----------------------------------------------------------------------------
//
// Clears all the APControllers from the list.
//
void
RegistryAPPool_t::
Clear(void)
{
    if (m_pConfigList)
    {
        for (int apx = m_ConfigListSize ; 0 <= --apx ;)
        {
            if (m_pConfigList[apx])
            {
                delete m_pConfigList[apx];
                m_pConfigList[apx] = NULL;
            }
        }
        free(m_pConfigList);
        m_pConfigList = NULL;
    }
    m_ConfigListSize = m_ConfigListAlloc = 0;

    if (m_pAttenList)
    {
        for (int apx = m_AttenListSize ; 0 <= --apx ;)
        {
            if (m_pAttenList[apx])
            {
                delete m_pAttenList[apx];
                m_pAttenList[apx] = NULL;
            }
        }
        free(m_pAttenList);
        m_pAttenList = NULL;
    }
    m_AttenListSize = m_AttenListAlloc = 0;

    APPool_t::Clear();
}

// ----------------------------------------------------------------------------
//
// Gets the unique key identifying the specified Access Point or
// Attenuator.
//
const TCHAR *
RegistryAPPool_t::
GetAPKey(int Index) const
{
    return m_pConfigList[Index]->m_DeviceKey;
}
const TCHAR *
RegistryAPPool_t::
GetATKey(int Index) const
{
    return m_pAttenList[Index]->m_DeviceKey;
}

// ----------------------------------------------------------------------------
//
// Gets the Access Point or Attenuator device-controller identified
// by the specified unique key.
//
DeviceController_t *
RegistryAPPool_t::
FindAPConfigDevice(const TCHAR *pDeviceKey)
{
    for (int apx = m_ConfigListSize ; 0 <= --apx ;)
    {
        if (0 == _tcscmp(m_pConfigList[apx]->m_DeviceKey, pDeviceKey))
            return m_pConfigList[apx]->m_pDevice;
    }
    return NULL;
}
DeviceController_t *
RegistryAPPool_t::
FindAttenuatorDevice(const TCHAR *pDeviceKey)
{
    for (int apx = m_AttenListSize ; 0 <= --apx ;)
    {
        if (0 == _tcscmp(m_pAttenList[apx]->m_DeviceKey, pDeviceKey))
            return m_pAttenList[apx]->m_pDevice;
    }
    return NULL;
}

// ----------------------------------------------------------------------------
