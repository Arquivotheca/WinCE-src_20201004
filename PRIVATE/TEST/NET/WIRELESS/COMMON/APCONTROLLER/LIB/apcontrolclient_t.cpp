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
// Implementation of the APControlClient_t class.
//
// ----------------------------------------------------------------------------

#include <backchannel.h>

#include <APControlClient_t.hpp>
#include <APControlData_t.hpp>
#include <WiFUtils.hpp>

#include <assert.h>
#include <winsock.h>

// Define this to get LOTS of debug output:
//#define EXTRA_DEBUG 1

using namespace ce::qa;

// ----------------------------------------------------------------------------
//  
// Constructor.
//
APControlClient_t::
APControlClient_t(
    const TCHAR *pServerHost,
    const TCHAR *pServerPort,
    const TCHAR *pDeviceType,
    const TCHAR *pDeviceName)
    : m_ServerHost(pServerHost),
      m_ServerPort(pServerPort),
      m_DeviceType(pDeviceType? pDeviceType : TEXT("")),
      m_DeviceName(pDeviceName? pDeviceName : TEXT("")),
      m_Socket(INVALID_SOCKET)
{
    // nothing to do
}

// ----------------------------------------------------------------------------
//
// Destructor.
//
APControlClient_t::
~APControlClient_t(void)
{
    Disconnect();
}

// ----------------------------------------------------------------------------
//
// Connects to the server.
//
HRESULT
APControlClient_t::
Connect(void)
{
    // Close the existing connection (if any).
    HRESULT hr = Disconnect();
    if (FAILED(hr))
        return hr;

    // Convert the unicode host and port to ASCII.
    char cServerHost[MAX_PATH];
    char cServerPort[20];
    WiFUtils::ConvertString(cServerHost, m_ServerHost, COUNTOF(cServerHost));
    WiFUtils::ConvertString(cServerPort, m_ServerPort, COUNTOF(cServerPort));

    // Connect.
    m_Socket = BackChannel_Connect(cServerHost, cServerPort);
    if (INVALID_SOCKET == m_Socket)
    {
        LogError(TEXT("Can't connect to server \"%s:%s\""),
                 GetServerHost(), GetServerPort());
        return HRESULT_FROM_WIN32(ERROR_CONNECTION_REFUSED);
    }

    // If necessary, tell the server the device-type and -name.
    if (m_DeviceType.length() || m_DeviceName.length())
    {
        ce::tstring command, response;
        command.assign(m_DeviceType);
        command.append(TEXT(","));
        command.append(m_DeviceName);
        DWORD commandCode = APControlData_t::InitializeDeviceCommandCode; 
        DWORD result = SendStringCommand(commandCode, command, &response);
        if (ERROR_SUCCESS != result)
        {
            LogError(TEXT("Can't init device-control for \"%s,%s\": err=%s"),
                     GetDeviceType(), GetDeviceName(), &response[0]); 
            return HRESULT_FROM_WIN32(result);
        }
    }

    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Disconnects from the server.
//
HRESULT
APControlClient_t::
Disconnect(void)
{
    if (IsConnected())
    {
        BOOL result = BackChannel_Disconnect(m_Socket);
        m_Socket = INVALID_SOCKET;

        if (!result)
        {
            LogError(TEXT("Can't disconnect from server \"%s:%s\""),
                     GetServerHost(), GetServerPort());
            return E_FAIL;
        }
    }
    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Sends the specified Unicode-string command.
//
DWORD
APControlClient_t::
SendStringCommand(
    DWORD CommandType,
    const ce::tstring &Command,
          ce::tstring *pResponse)
{
    DWORD result = ERROR_SUCCESS;
    
    BYTE *pCommandData      = NULL;
    DWORD  commandDataBytes = 0;
    BYTE *pReturnData       = NULL;
    DWORD  returnDataBytes  = 0;
    
    pResponse->clear();
    
#ifdef EXTRA_DEBUG
    LogDebug(TEXT("Sending string command %d to \"%s:%s\""),
             CommandType, GetServerHost(), GetServerPort());
#endif

    // Packetize the command data (if any).
    result = APControlData_t::EncodeMessage(result, Command, 
                                                  &pCommandData,
                                                   &commandDataBytes);
    if (ERROR_SUCCESS == result)
    {
        // Send the command and wait for a response.
        DWORD remoteResult;
        result = SendPacket(CommandType, 
                           pCommandData, 
                            commandDataBytes, &remoteResult,
                                             &pReturnData, 
                                              &returnDataBytes);
        if (ERROR_SUCCESS == result)
        {
            // Unpacketize the response message (if any).
            result = APControlData_t::DecodeMessage(remoteResult,
                                                   pReturnData, 
                                                    returnDataBytes,
                                                   pResponse);
        }
    }
    
    if (NULL != pReturnData)
    {
        free(pReturnData);
    }
    if (NULL != pCommandData)
    {
        free(pCommandData);
    }

    if (ERROR_SUCCESS != result && 0 == pResponse->length())
    {
        pResponse->assign(Win32ErrorText(result));
    }
    
    return result;
}

// ----------------------------------------------------------------------------
//
// Sends the specified packet and waits for a response.
//
DWORD
APControlClient_t::
SendPacket(
    DWORD  CommandType,
    BYTE *pCommandData,
    DWORD  CommandDataBytes,
    DWORD *pRemoteResult,
    BYTE **ppReturnData,
    DWORD  *pReturnDataBytes)
{
    // Make sure we're connected.
    if (!IsConnected())
    {
        LogError(TEXT("Can't send command to server \"%s:%s\"")
                 TEXT(" before connection"),
                 GetServerHost(), GetServerPort());
        return ERROR_ONLY_IF_CONNECTED;
    }

    // Send the command.
    if (!BackChannel_SendCommand(m_Socket, CommandType, pCommandData, 
                                                         CommandDataBytes))
    {
        LogError(TEXT("Can't send command type %u to server \"%s:%s\""),
                 CommandType, GetServerHost(), GetServerPort());
        return ERROR_CANTWRITE;
    }

    // Get the result.
    if (!BackChannel_GetResult(m_Socket, pRemoteResult, ppReturnData, 
                                                         pReturnDataBytes))
    {
        LogError(TEXT("Can't get response for command %u from \"%s:%s\""),
                 CommandType, GetServerHost(), GetServerPort());
        return ERROR_CANTREAD;
    }
    
    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
