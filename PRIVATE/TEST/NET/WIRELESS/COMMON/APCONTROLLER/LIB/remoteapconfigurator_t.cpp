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
// Implementation of the RemoteAPConfigurator_t class.
//
// ----------------------------------------------------------------------------

#include "RemoteAPConfigurator_t.hpp"
#include "APControlData_t.hpp"

#include <assert.h>

using namespace ce;
using namespace ce::qa;

// ----------------------------------------------------------------------------
//
// Constructor.
//
RemoteAPConfigurator_t::
RemoteAPConfigurator_t(
    const TCHAR *pServerHost, 
    const TCHAR *pServerPort,
    const TCHAR *pDeviceType,
    const TCHAR *pDeviceName)
    : APConfigurator_t(pDeviceName),
      m_Client(pServerHost,
               pServerPort,
               pDeviceType,
               pDeviceName)
{
    // nothing to do
}

// ----------------------------------------------------------------------------
//
// Destructor.
//
RemoteAPConfigurator_t::
~RemoteAPConfigurator_t(void)
{
    // nothing to do
}

// ----------------------------------------------------------------------------
//
// Sends the saved configuration values to the atenuator device.
//
HRESULT
RemoteAPConfigurator_t::
SaveConfiguration(void)
{
    HRESULT hr = S_OK;
    DWORD   result;

    // Send the configuration settings if they have changed.
    if (m_State.GetFieldFlags() != 0)
    {
        ce::gate<ce::critical_section> locker(m_Locker);

        // Copy the state in case a reconnection resets it.
        AccessPointState_t localState = m_State;
        
        // Try this twice in case the connection has dropped.
        for (int tries = 1 ;; ++tries)
        {
            // (Re)connect with the server.
            hr = Reconnect();
            if (FAILED(hr))
                break;

            // Update the configuration values.
            AccessPointState_t remoteState;
            ce::tstring        errorResponse;
            DWORD command = APControlData_t::SetAccessPointCommandCode;
            result = localState.SendMessage(command, 
                                           &m_Client,
                                           &remoteState, 
                                           &errorResponse);
            if (ERROR_SUCCESS == result)
            {
                m_State = remoteState;
                m_State.SetFieldFlags(0);
                break;
            }

            // Disconnect and, possibly, try again.
            m_Client.Disconnect();
            
            if (tries > 1)
            {
                locker.leave();
                
                hr = HRESULT_FROM_WIN32(result);
                LogError(TEXT("Error setting configuration")
                         TEXT(" for %s,%s at %s:%s: %.512s"),
                         m_Client.GetDeviceType(),
                         m_Client.GetDeviceName(),
                         m_Client.GetServerHost(),
                         m_Client.GetServerPort(),
                        &errorResponse[0]);
                break;
            }
        }
    }

    return hr;
}

// ----------------------------------------------------------------------------
//
// (Re)connects to the APControl control server and gets the current
// configuration values.
//
HRESULT
RemoteAPConfigurator_t::
Reconnect(void)
{
    HRESULT hr = S_OK;
    DWORD   result;
    
    if (!m_Client.IsConnected())
    {
        ce::gate<ce::critical_section> locker(m_Locker);
        if (!m_Client.IsConnected())
        {
            hr = m_Client.Connect();
            if (SUCCEEDED(hr))
            {
                AccessPointState_t remoteState;
                ce::tstring        errorResponse;
                DWORD command = APControlData_t::GetAccessPointCommandCode;
                result = m_State.SendMessage(command, 
                                             &m_Client,
                                             &remoteState, 
                                             &errorResponse);
                if (ERROR_SUCCESS == result)
                {   
                    m_State = remoteState;
                    m_State.SetFieldFlags(0);
                }
                else
                {
                    m_Client.Disconnect();
                    locker.leave();
                    
                    hr = HRESULT_FROM_WIN32(result);
                    LogError(TEXT("Error getting configuration")
                             TEXT(" for %s,%s at %s:%s: %.512s"),
                             m_Client.GetDeviceType(),
                             m_Client.GetDeviceName(),
                             m_Client.GetServerHost(),
                             m_Client.GetServerPort(),
                            &errorResponse[0]);
                }
            }
        }
    }
    
    return hr;
}

// ----------------------------------------------------------------------------
