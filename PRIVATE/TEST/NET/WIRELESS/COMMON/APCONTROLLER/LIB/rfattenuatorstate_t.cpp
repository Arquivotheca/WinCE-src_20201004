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
// Implementation of the RFAttenuatorState_t classes.
//
// ----------------------------------------------------------------------------

#include "RFAttenuatorState_t.hpp"
#include "APControlClient_t.hpp"
#include "APControlData_t.hpp"

#include <assert.h>

// Define this if you want LOTS of debug output:
//#define EXTRA_DEBUG 1

using namespace ce::qa;

// ----------------------------------------------------------------------------
//  
// Clears/initializes the state information.
//
void
RFAttenuatorState_t::
Clear(void)
{
    memset(this, 0, sizeof(*this));
    m_StructSize = sizeof(*this);
}

// ----------------------------------------------------------------------------
//
// Sets the atenuation values.
//
void
RFAttenuatorState_t::
SetCurrentAttenuation(int Value)
{
    if      (Value < _I8_MIN) Value = _I8_MIN;
    else if (Value > _I8_MAX) Value = _I8_MAX;

    m_CurrentAttenuation =
    m_FinalAttenuation = static_cast<INT8>(Value);
    m_FieldFlags |= FieldMaskAttenuation;
}
void
RFAttenuatorState_t::
SetMinimumAttenuation(int Value)
{
    if      (Value < _I8_MIN) Value = _I8_MIN;
    else if (Value > _I8_MAX) Value = _I8_MAX;

    m_MinimumAttenuation = static_cast<INT8>(Value);
    m_FieldFlags |= FieldMaskMinAttenuation;
}
void
RFAttenuatorState_t::
SetMaximumAttenuation(int Value)
{
    if      (Value < _I8_MIN) Value = _I8_MIN;
    else if (Value > _I8_MAX) Value = _I8_MAX;

    m_MaximumAttenuation = static_cast<INT8>(Value);
    m_FieldFlags |= FieldMaskMaxAttenuation;
}

// ----------------------------------------------------------------------------
//
// Schedules an attenutation adjustment within the specified range
// using the specified adjustment steps.
//
void
RFAttenuatorState_t::
AdjustAttenuation(
    int  StartAttenuation, // Attenuation after first adjustment
    int  FinalAttenuation, // Attenuation after last adjustment
    int  AdjustTime,       // Time, in seconds, to perform adjustment
    long StepTimeMS)       // Time, in milliseconds, between each step
                           // (Limited to between 500 (1/2 second) and
                           // 3,600,000 (1 hour)
{
    SetCurrentAttenuation(StartAttenuation);

    if      (FinalAttenuation < _I8_MIN) FinalAttenuation = _I8_MIN;
    else if (FinalAttenuation > _I8_MAX) FinalAttenuation = _I8_MAX;
    m_FinalAttenuation = static_cast<INT8>(FinalAttenuation);

    if      (AdjustTime < 0)        AdjustTime = 0;
    else if (AdjustTime > _I16_MAX) AdjustTime = _I16_MAX;
    m_AdjustTime = static_cast<INT16>(AdjustTime);

    if      (StepTimeMS <     250) StepTimeMS =     250;
    else if (StepTimeMS > 3600000) StepTimeMS = 3600000;
    m_StepTimeMS = static_cast<INT32>(StepTimeMS);

    m_FieldFlags |= FieldMaskAttenuation;
}

// ----------------------------------------------------------------------------
//
// Compares this object to another; compares each of the flagged
// fields and updates the returns flags indicating which fields differ:
//
int
RFAttenuatorState_t::
CompareFields(int Flags, const RFAttenuatorState_t &peer) const
{
    if (Flags & (int)FieldMaskAttenuation)
    {
        if (m_CurrentAttenuation == peer.m_CurrentAttenuation
         && m_FinalAttenuation   == peer.m_FinalAttenuation)
            Flags &= ~(int)FieldMaskAttenuation;
    }

    if (Flags & (int)FieldMaskMinAttenuation)
    {
        if (m_MinimumAttenuation == peer.m_MinimumAttenuation)
            Flags &= ~(int)FieldMaskMinAttenuation;
    }

    if (Flags & (int)FieldMaskMaxAttenuation)
    {
        if (m_MaximumAttenuation == peer.m_MaximumAttenuation)
            Flags &= ~(int)FieldMaskMaxAttenuation;
    }

    return Flags;
}

// ----------------------------------------------------------------------------
//
// Extracts a state object from the specified message.
//
DWORD
RFAttenuatorState_t::
DecodeMessage(
    DWORD        Result,
    const BYTE *pMessageData,
    DWORD        MessageDataBytes)
{
    int dataBytes = APControlData_t::CalculateDataBytes(MessageDataBytes);
    if (0 > dataBytes)
    {
        LogError(TEXT("Message type unknown: packet size only %d bytes"),
                 MessageDataBytes);
        return ERROR_INVALID_DATA;
    }
    
    const APControlData_t *pPacket = (const APControlData_t *)pMessageData;
    if (RFAttenuatorStateDataType != pPacket->m_DataType)
    {
        LogError(TEXT("Unknown RF-atenuator message type 0x%X"), 
                 (unsigned)pPacket->m_DataType);
        return ERROR_INVALID_DATA;
    }

    if (sizeof(*this) < dataBytes)
    {
        LogDebug(TEXT("%d-byte RF=attenuator message truncated to %d bytes"),
                 dataBytes, sizeof(*this));
        dataBytes = sizeof(*this);
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
RFAttenuatorState_t::
EncodeMessage(
    DWORD    Result,
    BYTE **ppMessageData,
    DWORD  *pMessageDataBytes) const
{
   *ppMessageData = NULL;
    *pMessageDataBytes = 0;

    int messageBytes = APControlData_t::CalculateMessageBytes(GetStructSize());

    APControlData_t *pPacket = (APControlData_t *) malloc(messageBytes);
    if (NULL == pPacket)
    {
        LogError(TEXT("Can't allocate %d bytes for RF-attenuator message"),
                 messageBytes);
        return ERROR_OUTOFMEMORY;
    }
              
    pPacket->m_DataType = RFAttenuatorStateDataType;
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
RFAttenuatorState_t::
SendMessage(
    DWORD CommandType,
    APControlClient_t   *pClient,
    RFAttenuatorState_t *pResponse,
    ce::tstring         *pErrorResponse) const
{
    DWORD result = ERROR_SUCCESS;
    
    BYTE *pCommandData      = NULL;
    DWORD  commandDataBytes = 0;
    BYTE *pReturnData       = NULL;
    DWORD  returnDataBytes  = 0;
    
    pErrorResponse->clear();
    
#ifdef EXTRA_DEBUG
    LogDebug(TEXT("Sending attenuator command %d to \"%s:%s\""),
             CommandType, pClient->GetServerHost(),
                          pClient->GetServerPort());
#endif

    // Packetize the command data (if any).
    result = EncodeMessage(result, &pCommandData,
                                    &commandDataBytes);
    if (ERROR_SUCCESS == result)
    {
        // Send the command and wait for a response.
        DWORD remoteResult;
        result = pClient->SendPacket(CommandType, 
                                    pCommandData, 
                                     commandDataBytes, &remoteResult,
                                                      &pReturnData, 
                                                       &returnDataBytes);
        if (ERROR_SUCCESS == result)
        {
            // Unpacketize the response message (if any).
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
