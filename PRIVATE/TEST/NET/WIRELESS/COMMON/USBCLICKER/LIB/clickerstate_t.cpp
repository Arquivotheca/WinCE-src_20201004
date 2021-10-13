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
// Implementation of the ClickerState_t classes.
//
// ----------------------------------------------------------------------------

#include "ClickerState_t.hpp"
#include <APControlClient_t.hpp>
#include <APControlData_t.hpp>

#include <assert.h>

using namespace ce::qa;

// ----------------------------------------------------------------------------
//  
// Clears/initializes the state information.
//
void
ClickerState_t::
Clear(void)
{
    memset(this, 0, sizeof(*this));
    m_StructSize = sizeof(*this);

    SetBoardType  (USBClicker_t::NormalClickerType);
    SetBoardNumber(1);
    SetCommPort   (0);
    SetCommSpeed  (CBR_9600);
    
    SetCommandRetries(1);
}

// ----------------------------------------------------------------------------
//
// Extracts a state object from the specified message.
//
DWORD
ClickerState_t::
DecodeMessage(
    IN  DWORD        Result,
    IN  const BYTE *pMessageData,
    IN  DWORD        MessageDataBytes)
{
    int dataBytes = APControlData_t::CalculateDataBytes(MessageDataBytes);
    if (0 > dataBytes)
    {
        LogError(TEXT("Message type unknown: packet size only %d bytes"),
                 MessageDataBytes);
        return ERROR_INVALID_DATA;
    }
    
    APControlData_t *pPacket = (APControlData_t *)pMessageData;
    if (ClickerStateDataType != pPacket->m_DataType)
    {
        LogError(TEXT("Unknown USB-clicker message type 0x%X"), 
                 (unsigned)pPacket->m_DataType);
        return ERROR_INVALID_DATA;
    }

    if (sizeof(*this) < dataBytes)
    {
        LogError(TEXT("USB-clicker message too long: %d data bytes"),
                 dataBytes);
        return ERROR_INVALID_DATA;
    }            

    Clear();
    memcpy(this, pPacket->m_Data, dataBytes);
    
    return Result;
}

// ----------------------------------------------------------------------------
//      
// Allocates a packet and initializes it to hold the current object.
//
DWORD
ClickerState_t::
EncodeMessage(
    IN  DWORD    Result,
    OUT BYTE **ppMessageData,
    OUT DWORD  *pMessageDataBytes) const
{
   *ppMessageData = NULL;
    *pMessageDataBytes = 0;

    int messageBytes = APControlData_t::CalculateMessageBytes(GetStructSize());

    APControlData_t *pPacket = (APControlData_t *) malloc(messageBytes);
    if (NULL == pPacket)
    {
        LogError(TEXT("Can't allocate %d bytes for USB-clicker message packet"),
                 messageBytes);
        return ERROR_OUTOFMEMORY;
    }
              
    pPacket->m_DataType = ClickerStateDataType;
    memcpy(pPacket->m_Data, this, GetStructSize());

  *ppMessageData = (BYTE *)pPacket;
   *pMessageDataBytes = messageBytes;
       
    return Result;
}

// ----------------------------------------------------------------------------
//      
// Uses the specified client to send the message and await a response.
//
DWORD
ClickerState_t::
SendMessage(
    IN  DWORD              CommandCode,
    IN  APControlClient_t *pClient,
    OUT ClickerState_t    *pResponse,
    OUT ce::tstring       *pErrorResponse) const
{
    DWORD result = ERROR_SUCCESS;
    
    BYTE *pCommandData      = NULL;
    DWORD  commandDataBytes = 0;
    BYTE *pReturnData       = NULL;
    DWORD  returnDataBytes  = 0;
    
    pErrorResponse->clear();
    
#ifdef EXTRA_DEBUG
    LogDebug(TEXT("Sending USB-clicker command %d to \"%s:%s\""),
             CommandType, pClient->GetServerHost(),
                          pClient->GetServerPort());
#endif

    // Packetize the command data.
    result = EncodeMessage(result, &pCommandData,
                                    &commandDataBytes);
    if (ERROR_SUCCESS == result)
    {
        // Send the command and wait for a response.
        DWORD remoteResult;
        result = pClient->SendPacket(CommandCode, 
                                    pCommandData, 
                                     commandDataBytes, &remoteResult,
                                                      &pReturnData, 
                                                       &returnDataBytes);
        if (ERROR_SUCCESS == result)
        {
            // Unpacketize the response message.
            if (ERROR_SUCCESS == remoteResult)
            {
                result = pResponse->DecodeMessage(remoteResult,
                                                 pReturnData, 
                                                  returnDataBytes); 
            }
            else
            {
                result = APControlData_t::DecodeMessage(remoteResult,
                                                       pReturnData, 
                                                        returnDataBytes, 
                                                       pErrorResponse);
            }
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
        
    if (ERROR_SUCCESS != result && 0 == pErrorResponse->length())
    {
        pErrorResponse->assign(Win32ErrorText(result));
    }
    
    return result;
}

// ----------------------------------------------------------------------------
