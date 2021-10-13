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
// Implementation of the AttenuatorDevice_t class.
//
// ----------------------------------------------------------------------------

#include "AttenuatorDevice_t.hpp"

#include "APCUtils.hpp"
#include "AzimuthController_t.hpp"
#include "WeinschelController_t.hpp"
#include "ManualAttenuatorDevice_t.hpp"

#include <tchar.h>
#include <strsafe.h>

using namespace ce::qa;

// ----------------------------------------------------------------------------
//
// Constructor.
//
AttenuatorDevice_t::
AttenuatorDevice_t(
    const TCHAR        *pAPName,
    const TCHAR        *pConfigKey,
    const TCHAR        *pDeviceType,
    const TCHAR        *pDeviceName,
    DeviceController_t *pDevice)
    : AttenuationDriver_t(),
      AttenuatorController_t(pDeviceType, pDeviceName),
      m_APName(pAPName),
      m_ConfigKey(pConfigKey),
      m_pDevice(pDevice)
{
    // nothing to do
}

// ----------------------------------------------------------------------------
//
// Destructor.
//
AttenuatorDevice_t::
~AttenuatorDevice_t(void)
{
    if (NULL != m_pDevice)
    {
        delete m_pDevice;
        m_pDevice = NULL;
    }
}

// ----------------------------------------------------------------------------
//
// Generates a driver object from the registry.
//
HRESULT
AttenuatorDevice_t::
CreateAttenuator(
    const TCHAR         *pRootKey,
    const TCHAR         *pAPName,
    AttenuatorDevice_t **ppAttenuator)
{
    HRESULT hr;
    DWORD result;
    ce::tstring errors;

    auto_ptr<DeviceController_t> pDevice;
    auto_ptr<AttenuatorDevice_t> pAtten;

    LogDebug(TEXT("CreateAttenuator(\"%s\\%s\")"), pRootKey, pAPName);

    // Open the registry.
    ce::wstring configKey;
    if (!configKey.assign(pRootKey)
     || !configKey.append(TEXT("\\"))
     || !configKey.append(pAPName))
        return E_OUTOFMEMORY;

    auto_hkey apHkey;
    result = RegOpenKeyEx(HKEY_CURRENT_USER, configKey, 0, KEY_READ, &apHkey);
    if (NO_ERROR != result)
    {
        LogError(TEXT("Cannot open AP-config \"%s\": %s"),
                 &configKey[0], Win32ErrorText(result));
        return HRESULT_FROM_WIN32(result);
    }

    // Load the configurator-type parameter.
    ce::tstring attenuatorType;
    hr = WiFUtils::ReadRegString(apHkey, configKey, AttenuatorKey,
                                                   &attenuatorType, NULL);
    if (FAILED(hr))
        return hr;

    // Separate the config-type and name/specifications.
    ce::tstring lstring(attenuatorType);
    static const TCHAR semicolon[] = TEXT(";");
    const TCHAR *type, *name;
    type = name = NULL;
    TCHAR *nextToken = NULL;  
    for (TCHAR *token = _tcstok_s(lstring.get_buffer(), semicolon, &nextToken) ;
                token != NULL ;
                token = _tcstok_s(NULL, semicolon, &nextToken))
    {
        if (NULL == type) { type = WiFUtils::TrimToken(token); }
        else              { name = WiFUtils::TrimToken(token); break; }
    }
    if (NULL == type)
    {
        LogError(TEXT("Bad attenuator type \"%s\""), &attenuatorType[0]);
        return E_INVALIDARG;
    }
    if (NULL == name)
        name = TEXT("");

    // Create the device and registry interfaces.
    if (0 == _tcsicmp(type, TEXT("manual")))
    {
        pAtten = new ManualAttenuatorDevice_t(pAPName, configKey,
                                              type, name);
    }
    else
    {
        result = APCUtils::CreateController(type, name, &pDevice);
        if (NO_ERROR != result)
            return HRESULT_FROM_WIN32(result);
        assert(pDevice.valid());
        pAtten = new AttenuatorDevice_t(pAPName, configKey,
                                        type, name, pDevice);
    }
    if (!pAtten.valid())
    {
        LogError(TEXT("Cannot allocate \"%s-%s\" device-controller"),
                 type, name);
        return E_OUTOFMEMORY;
    }
    pDevice.release();

    // Initialize the new attenuation-driver.
    hr = pAtten->LoadAttenuation(apHkey);
    if (FAILED(hr))
        return hr;

   *ppAttenuator = pAtten.release();
    return S_OK;
}

// ----------------------------------------------------------------------------
//
// Loads the initial attenuator configuration from the registry.
//
HRESULT
AttenuatorDevice_t::
LoadAttenuation(HKEY apHkey)
{
    return S_OK;
}

// ----------------------------------------------------------------------------
//
// Generates an attenuator-type parameter for use by CreateAttenuator.
//
HRESULT
AttenuatorDevice_t::
CreateAttenuatorType(ce::tstring *pConfigParam) const
{
    TCHAR buff[64];
    int   buffChars = COUNTOF(buff);

    HRESULT hr = StringCchPrintf(buff, buffChars, TEXT("%s;%s"),
                                 GetDeviceType(), GetDeviceName());
    if (FAILED(hr))
        return hr;

    return pConfigParam->assign(buff)? NO_ERROR : E_OUTOFMEMORY;
}

// ----------------------------------------------------------------------------
//
// Gets the current settings for an RF attenuator.
//
DWORD
AttenuatorDevice_t::
GetAttenuator(
    RFAttenuatorState_t *pResponse)
{
    if (NULL == m_pDevice)
    {
        assert(NULL != m_pDevice);
        return ERROR_INVALID_HANDLE;
    }
    
    ce::gate<ce::critical_section> locker(m_Locker);
    
    DWORD result = m_pDevice->GetAttenuator(pResponse);
    if (NO_ERROR == result)
    {
        m_State = *pResponse;
        m_State.SetFieldFlags(0);
        SaveAttenuation();
    }
    
    return result;
}

// ----------------------------------------------------------------------------
//
// Updates the attenuation settings of an RF attenuator.
//
DWORD
AttenuatorDevice_t::
SetAttenuator(
    const RFAttenuatorState_t &NewState,
          RFAttenuatorState_t *pResponse)
{
    if (NULL == m_pDevice)
    {
        assert(NULL != m_pDevice);
        return ERROR_INVALID_HANDLE;
    }
    
    ce::gate<ce::critical_section> locker(m_Locker);
    
    DWORD result = m_pDevice->SetAttenuator(NewState, pResponse);
    if (NO_ERROR == result)
    {
        m_State = *pResponse;
        m_State.SetFieldFlags(NewState.GetFieldFlags());
        SaveAttenuation();
    }
    
    return result;
}

// ----------------------------------------------------------------------------
//
// Saves the updated attenuation values to the registry.
//
HRESULT
AttenuatorDevice_t::
SaveAttenuation(void)
{
    HRESULT hr;
    LONG result;

    const TCHAR *pConfigKey = GetConfigKey();

    // Open the AP's registry entry.
    auto_hkey apHkey;
    result = RegOpenKeyEx(HKEY_CURRENT_USER, pConfigKey,
                          0, KEY_WRITE, &apHkey);
    if (NO_ERROR != result)
    {
        LogError(TEXT("Cannot open AP-config \"%s\": %s"),
                 pConfigKey, Win32ErrorText(result));
        return HRESULT_FROM_WIN32(result);
    }

    // Initialize the config paramater and write it to the registry.
    ce::tstring attenuatorType;
    hr = CreateAttenuatorType(&attenuatorType);
    if (FAILED(hr))
        return hr;

    ce::wstring wType;
    WiFUtils::ConvertString(&wType, attenuatorType);
    hr = WiFUtils::WriteRegString(apHkey, pConfigKey,
                                  AttenuatorKey, wType);
    if (FAILED(hr))
        return hr;

    return S_OK;
}

// ----------------------------------------------------------------------------
