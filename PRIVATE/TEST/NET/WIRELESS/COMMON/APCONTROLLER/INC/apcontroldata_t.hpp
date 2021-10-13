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
// Definitions and declarations for the APControlData_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_APControlData_t_
#define _DEFINED_APControlData_t_
#pragma once

#include <WiFUtils.hpp>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Defines the protocol for the messages exchanged between the APControl
// clients and server.
//
struct APControlData_t
{
    // APControl commands:
    enum {        GetConfigurationCommandCode = 9 };
    static WCHAR *GetConfigurationCommandName;
    enum {        InitializeDeviceCommandCode = 1 };
    static WCHAR *InitializeDeviceCommandName;
    enum {        GetAccessPointCommandCode   = 2 };
    static WCHAR *GetAccessPointCommandName;
    enum {        SetAccessPointCommandCode   = 3 };
    static WCHAR *SetAccessPointCommandName;
    enum {        GetAttenuatorCommandCode    = 4 };
    static WCHAR *GetAttenuatorCommandName;
    enum {        SetAttenuatorCommandCode    = 5 };
    static WCHAR *SetAttenuatorCommandName;
    enum {        DoDeviceCommandCode         = 6 };
    static WCHAR *DoDeviceCommandName;
    enum {        GetStatusCommandCode        = 7 };
    static WCHAR *GetStatusCommandName;
    enum {        SetStatusCommandCode        = 8 };
    static WCHAR *SetStatusCommandName;
    enum {        StopCommandCode             = 0 };
    static WCHAR *StopCommandName;
    
    // Translates from case-insensitive command-name to code and vice-versa:
    static const WCHAR *GetCommandName(DWORD Code);
    static DWORD        GetCommandCode(const WCHAR *pName);

    // Specifies the function for processing the specified command-code:
    static void SetCommandFunction(
        IN  DWORD Code, 
        IN  DWORD (*pFunction)(
            IN  SOCKET  Sock,
            IN  DWORD   Command,
            IN  BYTE  *pCommandData,
            IN  DWORD   CommandDataBytes,
            OUT BYTE **ppReturnData,
            OUT DWORD  *pReturnDataBytes));

    // Message-data type:
    enum DataType_e
    {
        EmptyMessage   =    0
       ,StringMessage  = 0xA1
       ,UnknownMessage =   -1
    };
    BYTE m_DataType;

    // Message data:
    WCHAR m_Data[1];

    // Calculates the overall size of a message containing the specified
    // amount of data:
    static int
    CalculateMessageBytes(IN int DataBytes) {
        return DataBytes + (sizeof(APControlData_t) - sizeof(WCHAR));
    }

    // Calculates the amount of data contained in a message of the specified
    // overall size:
    static int
    CalculateDataBytes(IN int MessageBytes) {
        return MessageBytes - (sizeof(APControlData_t) - sizeof(WCHAR));
    }
    
    // Extracts string data from the specified message.
    static DWORD
    DecodeMessage(
        IN  DWORD         Result,
        IN  const BYTE  *pMessageData,
        IN  DWORD         MessageDataBytes,
        OUT ce::string  *pMessage);
    static DWORD
    DecodeMessage(
        IN  DWORD         Result,
        IN  const BYTE  *pMessageData,
        IN  DWORD         MessageDataBytes,
        OUT ce::wstring *pMessage);
    
    // Allocates a packet and initializes it to hold the specified string:
    static DWORD
    EncodeMessage(
        IN  DWORD              Result,
        IN  const ce::string  &Message,
        OUT BYTE           **ppMessageData,
        OUT DWORD            *pMessageDataBytes);
    static DWORD
    EncodeMessage(
        IN  DWORD              Result,
        IN  const ce::wstring &Message,
        OUT BYTE           **ppMessageData,
        OUT DWORD            *pMessageDataBytes);
};

};
};
#endif /* _DEFINED_APControlData_t_ */
// ----------------------------------------------------------------------------
