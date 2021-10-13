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
// Definitions and declarations for the RFAttenuatorState_t classes.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_RFAttenuatorState_t_
#define _DEFINED_RFAttenuatorState_t_
#pragma once

#include <APController_t.hpp>
#include <WiFUtils.hpp>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Current and/or requested state of an RF attenuation device.
//
class APControlClient_t;
class RFAttenuatorState_t
{
protected:
    INT8 m_StructSize;
    
private:

    // Attenuator settings:
    INT8 m_CurrentAttenuation;
    INT8 m_MinimumAttenuation;
    INT8 m_MaximumAttenuation;

    // Attenuation-adjustment settings:
    INT8  m_FinalAttenuation; // Attenuation after last adjustment step
    INT16 m_AdjustTime;       // Time, in seconds, to perform adjustment
    INT32 m_StepTimeMS;       // Time, in milliseconds, between each step

    UINT8 m_FieldFlags;

public:

    RFAttenuatorState_t(void) { Clear(); }

    // Clears/initializes the state information:
    void
    Clear(void);

    // Gets or sets the attenuation values:
    int GetCurrentAttenuation(void) const { return m_CurrentAttenuation; }
    int GetMinimumAttenuation(void) const { return m_MinimumAttenuation; }
    int GetMaximumAttenuation(void) const { return m_MaximumAttenuation; }
    void SetCurrentAttenuation(int Value);
    void SetMinimumAttenuation(int Value);
    void SetMaximumAttenuation(int Value);

    // Schedules an attenutation adjustment within the specified range
    // using the specified adjustment steps:
    void
    AdjustAttenuation(
        int  StartAttenuation,    // Attenuation after first adjustment
        int  FinalAttenuation,    // Attenuation after last adjustment
        int  AdjustTime = 30,     // Time, in seconds, to perform adjustment
        long StepTimeMS = 1000L); // Time, in milliseconds, between each step
                                  // (Limited to between 500 (1/2 second) and
                                  // 3,600,000 (1 hour)

    // Gets the current atenuation-adjustment parameters:
    int  GetFinalAttenuation(void) const { return m_FinalAttenuation; }
    int  GetAdjustTime(void) const { return m_AdjustTime; }
    long GetStepTimeMS(void) const { return m_StepTimeMS; }

    // Flags indicating which which values have been set or modified:
    enum FieldMask_e {
        FieldMaskAttenuation    = 0x01
       ,FieldMaskMinAttenuation = 0x02
       ,FieldMaskMaxAttenuation = 0x04
       ,FieldMaskAll            = 0xFF
    };
    int  GetFieldFlags(void) const { return m_FieldFlags; }
    void SetFieldFlags(int Flags) { m_FieldFlags = (UINT8)Flags; }

    // Compares this object to another; compares each of the flagged
    // fields and updates the returns flags indicating which fields differ:
    int CompareFields(int Flags, const RFAttenuatorState_t &peer) const;

    // Gets the size (in bytes) of the structure.
    int GetStructSize(void) const { return m_StructSize; }

  // APControl message-formatting and communication:

    // Message-data type:
    enum { RFAttenuatorStateDataType = 0x8A };

    // Extracts a state object from the specified message.
    DWORD
    DecodeMessage(
        DWORD        Result,
        const BYTE *pMessageData,
        DWORD        MessageDataBytes);
    
    // Allocates a packet and initializes it to hold the current object:
    DWORD
    EncodeMessage(
        DWORD Result,
        BYTE  **ppMessageData,
        DWORD   *pMessageDataBytes) const;

    // Uses the specified client to send the message and await a response:
    DWORD
    SendMessage(
        DWORD CommandType,
        APControlClient_t   *pClient,
        RFAttenuatorState_t *pResponse,
        ce::tstring         *pErrorResponse) const;
};

};
};
#endif /* _DEFINED_RFAttenuatorState_t_ */
// ----------------------------------------------------------------------------
