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
// Implementation of the ManualAttenuatorDevice_t class.
//
// ----------------------------------------------------------------------------

#include "ManualAttenuatorDevice_t.hpp"
#include "APCUtils.hpp"

#include <tchar.h>
#include <strsafe.h>

using namespace ce;
using namespace ce::qa;

// ----------------------------------------------------------------------------
//
// Constructor.
//
ManualAttenuatorDevice_t::
ManualAttenuatorDevice_t(
    const TCHAR *pAPName,
    const TCHAR *pConfigKey,
    const TCHAR *pDeviceType,
    const TCHAR *pDeviceName)
    : AttenuatorDevice_t(pAPName, pConfigKey, pDeviceType, TEXT(""), NULL)
{
    static const TCHAR slash[] = TEXT("/,");
    
    // Get the initial attenuation values from the "name" section of the
    // registry "{type};{name}" parameter.
    ce::tstring lparam(pDeviceName);
    int curAtten, minAtten, maxAtten;
    curAtten = minAtten = maxAtten = -1;
    TCHAR *nextToken = NULL;  
    for (TCHAR *token = _tcstok_s(lparam.get_buffer(), slash, &nextToken) ;
                token != NULL ;
                token = _tcstok_s(NULL, slash, &nextToken))
    {
        if      (0 > curAtten) { curAtten = WiFUtils::GetIntToken(token,  0); }
        else if (0 > minAtten) { minAtten = WiFUtils::GetIntToken(token,  0); }
        else                   { maxAtten = WiFUtils::GetIntToken(token,100); break; }
    }
    if (minAtten > curAtten || curAtten > maxAtten)
    {
        LogError(TEXT("Bad %s attenuator values in \"%s\""),
                 pAPName, pDeviceName);
        curAtten =   0;
        minAtten =   0;
        maxAtten = 100;
    }

    m_State.SetCurrentAttenuation(curAtten);
    m_State.SetMinimumAttenuation(minAtten);
    m_State.SetMaximumAttenuation(maxAtten);
    m_State.SetFieldFlags(0);
}

// ----------------------------------------------------------------------------
//
// Destructor.
//
ManualAttenuatorDevice_t::
~ManualAttenuatorDevice_t(void)
{
    // nothing to do
}

// ----------------------------------------------------------------------------
//
// Generates an attenuator-type parameter for use by CreateAttenuator.
//
HRESULT
ManualAttenuatorDevice_t::
CreateAttenuatorType(
    ce::tstring *pConfigParam) const
{
    TCHAR buff[128];
    DWORD buffChars = COUNTOF(buff);
    
    HRESULT hr = StringCchPrintf(buff, buffChars, TEXT("%s;%d/%d/%d"),
                                 GetDeviceType(),
                                 m_State.GetCurrentAttenuation(),
                                 m_State.GetMinimumAttenuation(),
                                 m_State.GetMaximumAttenuation());
    if (FAILED(hr))
        return hr;

    return pConfigParam->assign(buff)? S_OK : E_OUTOFMEMORY;
}

// ----------------------------------------------------------------------------
//
// Gets the current attenuation settings from the registry.
//
DWORD
ManualAttenuatorDevice_t::
GetAttenuator(
    RFAttenuatorState_t *pResponse)
{
    ce::gate<ce::critical_section> locker(m_Locker);
   *pResponse = m_State;
    pResponse->SetFieldFlags(0);
    return NO_ERROR;
}

// ----------------------------------------------------------------------------
//
// Tells the operator how the attenuator should be configured and
// updates the registry.
//
DWORD
ManualAttenuatorDevice_t::
SetAttenuator(
    const RFAttenuatorState_t &NewState,
          RFAttenuatorState_t *pResponse)
{
    ce::gate<ce::critical_section> locker(m_Locker);

    if (m_State.GetCurrentAttenuation() != NewState.GetCurrentAttenuation())
    {
        RFAttenuatorState_t originalState = m_State;

        // The ONLY field they can manually change is the current value.
        m_State.SetCurrentAttenuation(NewState.GetCurrentAttenuation());

        HRESULT hr = SaveAttenuation();
        if (FAILED(hr))
        {
            if (E_ABORT == hr)
            {
                LogError(TEXT("Operator cancelled attenuator change"));
            }
            else
            {
                LogError(TEXT("Error saving new attenuator settings: %s"),
                         HRESULTErrorText(hr));
            }
            m_State = originalState;
            return HRESULT_CODE(hr);
        }
    }

   *pResponse = m_State;
    pResponse->SetFieldFlags(NewState.GetFieldFlags());
    return NO_ERROR;
}

// ----------------------------------------------------------------------------
//   
// Sends the saved attenuation value to the atenuator device.
//
HRESULT
ManualAttenuatorDevice_t::
SaveAttenuation(void)
{
    HRESULT hr;
    TCHAR buff[128];
    DWORD buffChars = COUNTOF(buff);
    
    // Pop up a dialog box if the attenuation has been changed.
    if (m_State.GetFieldFlags() != 0)
    {
        ce::wstring caption;
        caption.assign(TEXT("Set Attenuator '"));
        caption.append(GetAPName());
        caption.append(TEXT("'"));

        hr = StringCchPrintf(buff, buffChars,
                             TEXT("please set attenuation to %d dbs\n"),
                             m_State.GetCurrentAttenuation());
        if (FAILED(hr))
            return hr; 
        
        if (MessageBox(NULL, buff, caption, MB_OKCANCEL) != IDOK)
            return E_ABORT;
    }

    // Insert the current configuration into the registry.
    hr = AttenuatorDevice_t::SaveAttenuation();
    if (FAILED(hr))
        return hr;
    
    // Reset the field-modification flags.
    m_State.SetFieldFlags(0);
    return S_OK;
}

// ----------------------------------------------------------------------------
