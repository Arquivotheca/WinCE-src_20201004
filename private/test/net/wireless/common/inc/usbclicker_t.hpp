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
// Definitions and declarations for the USBClicker_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_USBClicker_t_
#define _DEFINED_USBClicker_t_
#pragma once

#ifndef DEBUG
#ifndef NDEBUG
#define NDEBUG
#endif
#endif

#include <windows.h>

class SCBExAPI;

namespace ce {
namespace qa {

class ClickerClient_t;
class ClickerState_t;

// ----------------------------------------------------------------------------
//
// Provides a mechanism for remote or local control of USB "Clicker"
// SCB devices. These devices are used for electronically controlling
// a computer's USB connections. During a test, for example, the clicker
// can be used to electronically disconnect a USB port to simulate the
// physical removal/insertion of a mobile device from/into its USB-enabled
// base-station.
// 
// The normal usage pattern for these objects requires the creation of a
// USBClicker object using one of the following methods:
//
// Method 1 - manual configuration:
//
//      using namespace ce::qa;
//
//      USBClicker clicker;
//      clicker.SetBoardType(clicker.ExtendedClickerType);
//      clicker.SetBoardNumber(1);
//      clicker.SetCommPort(1);
//      clicker.SetCommSpeed(CBR_9600);
//      clicker.SetRemoteServer(L"192.168.92.1",L"33331");  // optional
//
// Method 2 - configuration via registry:
//
//      using namespace ce::qa;
//
//      USBClicker clicker;
//      DWORD result = clicker.LoadFromRegistry(L"Software\\Microsoft\\CETest\\TEST27\\USBClicker");
//      if (ERROR_SUCCESS != result)
//          printf(L"Eek!");
//
// The latter method loads all the configuration information from the
// specified registry entry.
//
// Note the use of RemoteServer at the end of the manual-configuration
// section. This can be done to send the clicker commands to the specified
// APControl server.
//
// Once a "clicker" object has been created, the operation is simple:
//
//      DWORD result = clicker.Insert()
//      if (ERROR_SUCCESS != result)
//          printf(L"Eek!");
//
class USBClicker_t
{
private:

    // Current configuration state:
    ClickerState_t *m_pState;

    // Local USB-clicker interface (optional):
    SCBExAPI *m_pDriver;

    // Remote APControl server interface (optional):
    ClickerClient_t *m_pClient; 

    // Copy and assignment are deliberately disabled:
    USBClicker_t(const USBClicker_t &src);
    USBClicker_t &operator = (const USBClicker_t &src);
    
public:

    // Registry-keys:
    static const TCHAR * const BoardTypeKey;
    static const TCHAR * const BoardNumberKey;
    static const TCHAR * const CommPortKey;
    static const TCHAR * const CommSpeedKey;
    static const TCHAR * const ServerHostKey;
    static const TCHAR * const ServerPortKey;

    // Constructor and destructor:
    USBClicker_t(void);
    virtual
   ~USBClicker_t(void);

    // Stores or loads the configuration to or from the registry:
    DWORD
    SaveToRegistry(const TCHAR *pRegistryKey);
    DWORD
    LoadFromRegistry(const TCHAR *pRegistryKey);
 
    // Gets or sets the type of "clicker" board:
    enum ClickerType_e { NormalClickerType = 0, ExtendedClickerType = 1 };
    ClickerType_e GetBoardType(void) const;
    void          SetBoardType(ClickerType_e NewType);

    // Gets or sets the board number:
    int  GetBoardNumber(void) const;
    void SetBoardNumber(int BoardNumber);

    // Gets or sets the COM-port number or communication speed:
    int  GetCommPort(void) const;
    void SetCommPort(int PortNumber);
    long GetCommSpeed(void) const;
    void SetCommSpeed(long CommSpeed);

    // Gets or sets the remote APControl-server host-name or port-number:
    const TCHAR *GetServerHost(void) const;
    const TCHAR *GetServerPort(void) const;
    DWORD
    SetRemoteServer(
        const TCHAR *HostName = TEXT("localhost"),
        const TCHAR *PortNumber = TEXT("33331"));

    // Connects or disconnects the USB interface:
    DWORD Insert(void);
    DWORD Remove(void);

    // These commands are passed through to the local or remote SCB board:
    DWORD
    SetSwitches(
        int  BoardNumber,
        bool fFireWire,     // (dis)connect the FireWire interface
        bool fUSB,          // (dis)connect the USB interface
        bool fPCMCIA);      // (dis)connect the PCMCIA interface
    DWORD
    DisableCard(
        int BoxNumber,
        int CardNumber);
    DWORD
    SelectNthCard(
        int BoxNumber,
        int CardNumber);
    DWORD
    ConnectUSBPort(
        int BoardNumber,
        int PortNumber);
};

};
};
#endif /* _DEFINED_USBClicker_t_ */
// ----------------------------------------------------------------------------
