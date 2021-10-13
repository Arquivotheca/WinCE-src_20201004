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
// Implementation of the APControlData_t class.
//
// ----------------------------------------------------------------------------

#include <backchannel.h>

#include <APControlData_t.hpp>
#include <WiFUtils.hpp>

#include <assert.h>

// Define this to get LOTS of debug output:
//#define EXTRA_DEBUG 1

using namespace ce::qa;

// ----------------------------------------------------------------------------
//
// External variables for backchannel:
//
int g_ResultTimeoutSecs  =  (6*60); // Time, in seconds, client waits for
                                    // command to finish before assuming
                                    // server is stalled.
int g_CommandTimeoutSecs = (20*60); // Time, in seconds, server waits for
                                    // commands to arrive before dropping
                                    // client connection.

// List of commands known to the backchannel library:
COMMAND_HANDLER_ENTRY g_CommandHandlerArray[] =
{
    { APControlData_t::GetConfigurationCommandName,
      APControlData_t::GetConfigurationCommandCode,
      NULL }
   ,{ APControlData_t::InitializeDeviceCommandName,
      APControlData_t::InitializeDeviceCommandCode,
      NULL }
   ,{ APControlData_t::GetAccessPointCommandName,
      APControlData_t::GetAccessPointCommandCode,
      NULL }
   ,{ APControlData_t::SetAccessPointCommandName,
      APControlData_t::SetAccessPointCommandCode,
      NULL }
   ,{ APControlData_t::GetAttenuatorCommandName,
      APControlData_t::GetAttenuatorCommandCode,
      NULL }
   ,{ APControlData_t::SetAttenuatorCommandName,
      APControlData_t::SetAttenuatorCommandCode,
      NULL }
   ,{ APControlData_t::DoDeviceCommandName,
      APControlData_t::DoDeviceCommandCode,
      NULL }
   ,{ APControlData_t::GetStatusCommandName,
      APControlData_t::GetStatusCommandCode,
      NULL }
   ,{ APControlData_t::SetStatusCommandName,
      APControlData_t::SetStatusCommandCode,
      NULL }
   ,{ APControlData_t::StopCommandName,    // Must be last
      APControlData_t::StopCommandCode,
      NULL } 
};

WCHAR *APControlData_t::GetConfigurationCommandName = L"GetConfiguration";
WCHAR *APControlData_t::InitializeDeviceCommandName = L"InitializeDevice";
WCHAR *APControlData_t::GetAccessPointCommandName   = L"GetAccessPoint";
WCHAR *APControlData_t::SetAccessPointCommandName   = L"SetAccessPoint";
WCHAR *APControlData_t::GetAttenuatorCommandName    = L"GetAttenuator";
WCHAR *APControlData_t::SetAttenuatorCommandName    = L"SetAttenuator";
WCHAR *APControlData_t::DoDeviceCommandName         = L"DoDeviceCommand";
WCHAR *APControlData_t::GetStatusCommandName        = L"GetStatus";
WCHAR *APControlData_t::SetStatusCommandName        = L"SetStatus";
WCHAR *APControlData_t::StopCommandName             = L"Stop";

// ----------------------------------------------------------------------------
//  
// Translates from case-insensitive command-name to code and vice-versa.
//
const WCHAR *
APControlData_t::
GetCommandName(IN DWORD Code)
{
    for (COMMAND_HANDLER_ENTRY *pEntry = g_CommandHandlerArray ;; ++pEntry)
    {
        if (pEntry->dwCommand == Code)
            return pEntry->tszHandlerName;
        if (0 == pEntry->dwCommand)
            return NULL;
    }
}

DWORD        
APControlData_t::
GetCommandCode(IN const WCHAR *pName)
{
    for (COMMAND_HANDLER_ENTRY *pEntry = g_CommandHandlerArray ;; ++pEntry)
    {
        assert(pEntry->tszHandlerName != NULL);
        if (0 == _wcsicmp(pEntry->tszHandlerName, pName))
            return pEntry->dwCommand;
        if (0 == pEntry->dwCommand)
            return (DWORD)-1;
    }
}

// ----------------------------------------------------------------------------
//  
// Specifies the function for processing the specified command-code.
//
void 
APControlData_t::
SetCommandFunction(
    IN DWORD Code, 
    IN DWORD (*pFunction)(
           IN  SOCKET  Sock,
           IN  DWORD   Command,
           IN  BYTE  *pCommandData,
           IN  DWORD   CommandDataBytes,
           OUT BYTE **ppReturnData,
           OUT DWORD  *pReturnDataBytes))
{
    for (COMMAND_HANDLER_ENTRY *pEntry = g_CommandHandlerArray ;; ++pEntry)
    {
        if (pEntry->dwCommand == Code)
        {
            pEntry->pfHandlerFunction = pFunction;
            break;
        }
        if (0 == pEntry->dwCommand)
        {
            assert(NULL == "Tried to set handler function for unknown command");
            break;
        }
    }
}

// ----------------------------------------------------------------------------
//
// Extracts the string data from the specified message.
//
DWORD
APControlData_t::
DecodeMessage(
    DWORD        Result,
    const BYTE *pMessageData,
    DWORD        MessageDataBytes,
    ce::string *pMessage)
{
    ce::wstring wcMessage;
    DWORD result = DecodeMessage(Result, pMessageData,
                                          MessageDataBytes, &wcMessage);
    if (ERROR_SUCCESS == result)
    {
        HRESULT hr = WiFUtils::ConvertString(pMessage,
                                            wcMessage, "response message",
                                             wcMessage.length());
        if (FAILED(hr))
        {
            result = HRESULT_CODE(hr);
        }
    }
    return result;
}
    
DWORD
APControlData_t::
DecodeMessage(
    DWORD         Result,
    const BYTE  *pMessageData,
    DWORD         MessageDataBytes,
    ce::wstring *pMessage)
{
    if (NULL == pMessageData || 0 == MessageDataBytes)
    {
        pMessage->clear();
        return Result;
    }

    int dataBytes = APControlData_t::CalculateDataBytes(MessageDataBytes);
    if (0 > dataBytes)
    {
        LogError(TEXT("Message type unknown: packet size only %d bytes"),
                 MessageDataBytes);
        return ERROR_INVALID_DATA;
    }
    
    const APControlData_t *pPacket = (const APControlData_t *)pMessageData;
    if (APControlData_t::StringMessage != pPacket->m_DataType)
    {
        LogError(TEXT("Unknown string message type 0x%X"), 
                 (unsigned)pPacket->m_DataType);
        return ERROR_INVALID_DATA;
    }

    int dataChars = dataBytes / sizeof(WCHAR);
    if (0 >= dataChars)
    {
        LogError(TEXT("String message truncated: only %d data bytes"),
                 dataBytes);
        return ERROR_INVALID_DATA;
    }            
    
    if (!pMessage->assign(pPacket->m_Data, dataChars))
    {
        LogError(TEXT("Can't allocate %d bytes for string message"),
                 dataBytes);
        return ERROR_OUTOFMEMORY;
    }
    
    return Result;
}

// ----------------------------------------------------------------------------
//      
// Allocates a packet and initializes it to hold the specified string data.
//
DWORD
APControlData_t::
EncodeMessage(
    DWORD             Result,
    const ce::string &Message,
    BYTE          **ppMessageData,
    DWORD           *pMessageDataBytes)
{
    ce::wstring wcMessage;
    HRESULT hr = WiFUtils::ConvertString(&wcMessage,
                                            Message, "text message",
                                            Message.length());
    return FAILED(hr)? HRESULT_CODE(hr)
                     : EncodeMessage(Result, wcMessage, ppMessageData,
                                                         pMessageDataBytes);
}

DWORD
APControlData_t::
EncodeMessage(
    DWORD              Result,
    const ce::wstring &Message,
    BYTE           **ppMessageData,
    DWORD            *pMessageDataBytes)
{
   *ppMessageData = NULL;
    *pMessageDataBytes = 0;
    
    if (0 != Message.length() && L'\0' != Message[0])
    {
        int dataBytes = (Message.length() + 1) * sizeof(WCHAR);
        int messageBytes = APControlData_t::CalculateMessageBytes(dataBytes);

        APControlData_t *pPacket = (APControlData_t *) malloc(messageBytes);
        if (NULL == pPacket)
        {
            LogError(TEXT("Can't allocate %d bytes for string message packet"),
                     messageBytes);
            return ERROR_OUTOFMEMORY;
        }
        
        pPacket->m_DataType = APControlData_t::StringMessage;
        memcpy(pPacket->m_Data, &Message[0], dataBytes);

      *ppMessageData = (BYTE *)pPacket;
       *pMessageDataBytes = messageBytes;
    }
    
    return Result;
}

// ----------------------------------------------------------------------------
