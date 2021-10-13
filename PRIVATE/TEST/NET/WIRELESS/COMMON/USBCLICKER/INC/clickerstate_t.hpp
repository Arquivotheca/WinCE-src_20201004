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
// Definitions and declarations for the ClickerState_t classes.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_ClickerState_t_
#define _DEFINED_ClickerState_t_
#pragma once

#include <WiFUtils.hpp>
#include <USBClicker_t.hpp>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Current and/or requested state of a USB "clicker" device.
//
class APControlClient_t;
class ClickerState_t
{
protected:
    INT8 m_StructSize;

private:

    // Type of board:
    INT8 m_BoardType;

    // Board and port number:
    INT8 m_BoardNumber;
    INT8 m_PortNumber;

    // COM-port number and communication speed:
    INT8  m_CommPort;
    INT32 m_CommSpeed;

    // Command-code:
    INT8 m_CommandCode;
    INT8 m_CommandRetries;

    // SetSwitches port flags:
    INT8 m_fFireWire;
    INT8 m_fUSB;
    INT8 m_fPCMCIA;

    // Disable/SelectCard arguments:
    INT8 m_BoxNumber;
    INT8 m_CardNumber;

    // Flags indicating which which values have been set or modified:
    UINT8 m_FieldFlags;

public:

    ClickerState_t(void) { Clear(); }
    
    // Clears/initializes the state information:
    void
    Clear(void);

    // Type of board:
    USBClicker_t::ClickerType_e
    GetBoardType(void) const {
        return static_cast<USBClicker_t::ClickerType_e>(m_BoardType);
    }
    void
    SetBoardType(USBClicker_t::ClickerType_e NewType) {
        m_BoardType = static_cast<INT8>(NewType);
    }

    // Board or port number:
    int
    GetBoardNumber(void) const {
        return m_BoardNumber;
    }
    void
    SetBoardNumber(int NewNumber) {
        m_BoardNumber = static_cast<INT8>(NewNumber);
    }
    int
    GetPortNumber(void) const {
        return m_PortNumber;
    }
    void
    SetPortNumber(int NewNumber) {
        m_PortNumber = static_cast<INT8>(NewNumber);
    }

    // COM-port number and communication speed:
    int
    GetCommPort(void) const {
        return m_CommPort;
    }
    void
    SetCommPort(int NewPort) {
        m_CommPort = static_cast<INT8>(NewPort);
    }
    long
    GetCommSpeed(void) const {
        return m_CommSpeed;
    }
    void
    SetCommSpeed(long NewSpeed) {
        m_CommSpeed = static_cast<INT32>(NewSpeed);
    }

    // Remote-command code and maximum-retries:
    enum CommandCode_e {
         CommandSetSwitches  = 1
        ,CommandDisableCard  = 2
        ,CommandSelectCard   = 3
        ,CommandConnectUSB   = 4
    };
    CommandCode_e
    GetCommandCode(void) const {
        return static_cast<CommandCode_e>(m_CommandCode);
    }
    void
    SetCommandCode(CommandCode_e Command) {
        m_CommandCode = static_cast<INT8>(Command);
    }
    int
    GetCommandRetries(void) const {
        return m_CommandRetries;
    }
    void
    SetCommandRetries(int Retries) {
        m_CommandRetries = static_cast<INT8>(Retries);
    }

    // SetSwitches port flags:
    bool
    IsFireWireOn(void) const {
        return m_fFireWire != 0;
    }
    void
    SetFireWireOn(bool NewState) {
        m_fFireWire = NewState? 1 : 0;
    }
    bool
    IsUSBOn(void) const {
        return m_fUSB != 0;
    }
    void
    SetUSBOn(bool NewState) {
        m_fUSB = NewState? 1 : 0;
    }
    bool
    IsPCMCIAOn(void) const {
        return m_fPCMCIA != 0;
    }
    void
    SetPCMCIAOn(bool NewState) {
        m_fPCMCIA = NewState? 1 : 0;
    }

    // DisableCard/SelectCard arguments:
    int
    GetBoxNumber(void) const {
        return m_BoxNumber;
    }
    void
    SetBoxNumber(int NewNumber) {
        m_BoxNumber = static_cast<INT8>(NewNumber);
    }
    int
    GetCardNumber(void) const {
        return m_CardNumber;
    }
    void
    SetCardNumber(int NewNumber) {
        m_CardNumber = static_cast<INT8>(NewNumber);
    }
    
    // Flags indicating which which values have been set or modified:
    enum FieldMask_e
    {
        FieldMaskBoardType   = 0x01
       ,FieldMaskBoardNumber = 0x02
       ,FieldMaskPortNumber  = 0x04
       ,FieldMaskCommPort    = 0x08
       ,FieldMaskCommSpeed   = 0x10
       ,FieldMaskAll         = 0xFF
    };
    int  GetFieldFlags(void) const { return m_FieldFlags; }
    void SetFieldFlags(int Flags) { m_FieldFlags = Flags; }

    // Gets the size (in bytes) of the structure.
    int GetStructSize(void) const { return m_StructSize; }

  // APControl message-formatting and communication:

    // Message-data type:
    enum { ClickerStateDataType = 0xC1 };

    // Extracts a state object from the specified message.
    DWORD
    DecodeMessage(
        IN DWORD        Result,
        IN const BYTE *pMessageData,
        IN DWORD        MessageDataBytes);
    
    // Allocates a packet and initializes it to hold the current object:
    DWORD
    EncodeMessage(
        IN  DWORD    Result,
        OUT BYTE  **ppMessageData,
        OUT DWORD   *pMessageDataBytes) const;

    // Uses the specified client to send the message and await a response:
    DWORD
    SendMessage(
        IN  DWORD              CommandCode,
        IN  APControlClient_t *pClient,
        IN  ClickerState_t    *pResponse,
        OUT ce::tstring       *pErrorResponse) const;
};

};
};
#endif /* _DEFINED_ClickerState_t_ */
// ----------------------------------------------------------------------------
