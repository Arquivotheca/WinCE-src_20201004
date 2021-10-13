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
// Implementation of the USBClicker_t class.
//
// ----------------------------------------------------------------------------

#include <USBClicker_t.hpp>
#include <APControlClient_t.hpp>
#include <APControlData_t.hpp>
#include "ClickerState_t.hpp"

#include <assert.h>
#include <inc/auto_xxx.hxx>
#include <SCBExAPI.h>
#include <inc/sync.hxx>
#include <strsafe.h>

using namespace ce::qa;

// Only set this if you want a LOT of debug output:
//#define EXTRA_DEBUG 1

// ----------------------------------------------------------------------------
//  
// Registry keys:
//
const TCHAR * const USBClicker_t::BoardTypeKey   = TEXT("BoardType");
const TCHAR * const USBClicker_t::BoardNumberKey = TEXT("BoardNumber");
const TCHAR * const USBClicker_t::CommPortKey    = TEXT("CommPort");
const TCHAR * const USBClicker_t::CommSpeedKey   = TEXT("CommSpeed");
const TCHAR * const USBClicker_t::ServerHostKey  = TEXT("ServerHost");
const TCHAR * const USBClicker_t::ServerPortKey  = TEXT("ServerPort");

// ----------------------------------------------------------------------------
//
// ClickerClient_t provides a simple, private, data object implementing
// USBClicker's interface to the remote APControl server.
//
namespace ce {
namespace qa {
class ClickerClient_t
{
private:
    
    // Remote server host and port:
    ce::tstring m_ServerHost;
    ce::tstring m_ServerPort;

    // Synchronization object:
    ce::critical_section m_Locker;

    // Client object:
    APControlClient_t *m_pClient;

public:

    // Constructor and destructor:
    ClickerClient_t(
        const TCHAR *ServerHost,
        const TCHAR *ServerPort)
        : m_ServerHost(ServerHost),
          m_ServerPort(ServerPort),
          m_pClient(NULL)
    { }
   ~ClickerClient_t(void)
    {
        Disconnect();
    }

    // Accessors:
    const TCHAR *GetServerHost(void) const { return m_ServerHost; }
    const TCHAR *GetServerPort(void) const { return m_ServerPort; }
    ce::critical_section &GetLocker(void) { return m_Locker; }

    // (Re)connects the remote server:
    APControlClient_t *
    Reconnect(
        const ClickerState_t &State,
        DWORD                *pResult);

    // Disconnects from the server:
    void
    Disconnect(void)
    {
        if (NULL != m_pClient)
        {
            delete m_pClient;
            m_pClient = NULL;
        }
    }
};
};
};

// ----------------------------------------------------------------------------
//  
// Constructor.
//
USBClicker_t::
USBClicker_t(void)
    : m_pState(new ClickerState_t),
      m_pDriver(NULL),
      m_pClient(NULL)
{
    assert(NULL != m_pState);
}

// ----------------------------------------------------------------------------
//
// Destructor.
//
USBClicker_t::
~USBClicker_t(void)
{
    if (NULL != m_pState)
    {
        delete m_pState;
        m_pState = NULL;
    }
    if (NULL != m_pDriver)
    {
        delete m_pDriver;
        m_pDriver = NULL;
    }
    if (NULL != m_pClient)
    {
        delete m_pClient;
        m_pClient = NULL;
    }
}

// ----------------------------------------------------------------------------
//
// Stores the configuration into the registry.
//
DWORD
USBClicker_t::
SaveToRegistry(
    const TCHAR *pRegistryKey)
{
    DWORD result;

    auto_hkey hkey;
    result = RegOpenKeyEx(HKEY_CURRENT_USER, pRegistryKey, 0, KEY_WRITE, &hkey);
    if (ERROR_SUCCESS != result)
    {
        LogError(TEXT("Cannot open registry \"%s\": %s"),
                 pRegistryKey, Win32ErrorText(result));
        return result;
    }

    result = WiFUtils::WriteRegDword(hkey, pRegistryKey,
                                     BoardTypeKey, (int)GetBoardType());
    if (ERROR_SUCCESS != result)
        return result;

    result = WiFUtils::WriteRegDword(hkey, pRegistryKey,
                                     BoardNumberKey, GetBoardNumber());
    if (ERROR_SUCCESS != result)
        return result;

    result = WiFUtils::WriteRegDword(hkey, pRegistryKey,
                                     CommPortKey, GetCommPort());
    if (ERROR_SUCCESS != result)
        return result;

    result = WiFUtils::WriteRegDword(hkey, pRegistryKey,
                                     CommSpeedKey, GetCommSpeed());
    if (ERROR_SUCCESS != result)
        return result;

    if (NULL != m_pClient)
    {
        result = WiFUtils::WriteRegString(hkey, pRegistryKey,
                                          ServerHostKey, GetServerHost());
        if (ERROR_SUCCESS != result)
            return result;

        result = WiFUtils::WriteRegString(hkey, pRegistryKey,
                                          ServerPortKey, GetServerPort());
        if (ERROR_SUCCESS != result)
            return result;
    }

    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Loads the configuration from the registry.
//
DWORD
USBClicker_t::
LoadFromRegistry(
    const TCHAR *pRegistryKey)
{
    DWORD result, iValue;

    auto_hkey hkey;
    result = RegOpenKeyEx(HKEY_CURRENT_USER, pRegistryKey, 0, KEY_READ, &hkey);
    if (ERROR_SUCCESS != result)
    {
        LogError(TEXT("Cannot open registry \"%s\": %s"),
                 pRegistryKey, Win32ErrorText(result));
        return result;
    }

    result = WiFUtils::ReadRegDword(hkey, pRegistryKey,
                                    BoardTypeKey, &iValue, (int)NormalClickerType);
    if (ERROR_SUCCESS != result)
        return result;
    m_pState->SetBoardType((ClickerType_e)iValue);

    result = WiFUtils::ReadRegDword(hkey, pRegistryKey,
                                    BoardNumberKey, &iValue, 1);
    if (ERROR_SUCCESS != result)
        return result;
    m_pState->SetBoardNumber((int)iValue);

    result = WiFUtils::ReadRegDword(hkey, pRegistryKey,
                                    CommPortKey, &iValue, 1);
    if (ERROR_SUCCESS != result)
        return result;
    m_pState->SetCommPort((int)iValue);

    result = WiFUtils::ReadRegDword(hkey, pRegistryKey,
                                    CommSpeedKey, &iValue, 9600);
    if (ERROR_SUCCESS != result)
        return result;
    m_pState->SetCommSpeed((int)iValue);

    ce::wstring host;
    result = WiFUtils::ReadRegString(hkey, pRegistryKey,
                                     ServerHostKey, &host, L"");
    if (ERROR_SUCCESS != result)
        return result;
    if (0 != host.length())
    {
        ce::wstring port;
        result = WiFUtils::ReadRegString(hkey, pRegistryKey,
                                         ServerPortKey, &port, NULL);
        if (ERROR_SUCCESS != result)
            return result;

        result = SetRemoteServer(host,port);
        if (ERROR_SUCCESS != result)
            return result;
    }

    m_pState->SetFieldFlags(0);
    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Gets or sets the type of "clicker" board.
//
USBClicker_t::ClickerType_e
USBClicker_t::
GetBoardType(void) const
{
    m_pState->SetFieldFlags(m_pState->GetFieldFlags()
                           |m_pState->FieldMaskBoardType);
    return m_pState->GetBoardType();
}
void
USBClicker_t::
SetBoardType(ClickerType_e NewType)
{
    m_pState->SetBoardType(NewType);
}

// ----------------------------------------------------------------------------
//
// Gets or sets the board number.
//
int
USBClicker_t::
GetBoardNumber(void) const
{
    return m_pState->GetBoardNumber();
}
void
USBClicker_t::
SetBoardNumber(int BoardNumber)
{
    m_pState->SetFieldFlags(m_pState->GetFieldFlags()
                           |m_pState->FieldMaskBoardNumber);
    m_pState->SetBoardNumber(BoardNumber);
}

// ----------------------------------------------------------------------------
//
// Gets or sets the COM-port number or communication speed.
//
int
USBClicker_t::
GetCommPort(void) const
{
    return m_pState->GetCommPort();
}
void
USBClicker_t::
SetCommPort(int PortNumber)
{
    m_pState->SetFieldFlags(m_pState->GetFieldFlags()
                           |m_pState->FieldMaskCommPort);
    m_pState->SetCommPort(PortNumber);

    // Force a re-connection with new port info.
    if (NULL != m_pDriver)
    {
        delete m_pDriver;
        m_pDriver = NULL;
    }
    if (NULL != m_pClient)
    {
        m_pClient->Disconnect();
    }
}
long
USBClicker_t::
GetCommSpeed(void) const
{
    return m_pState->GetCommSpeed();
}
void
USBClicker_t::
SetCommSpeed(long CommSpeed)
{
    m_pState->SetFieldFlags(m_pState->GetFieldFlags()
                           |m_pState->FieldMaskCommSpeed);
    m_pState->SetCommSpeed(CommSpeed);

    // Force a re-connection with new port info.
    if (NULL != m_pDriver)
    {
        delete m_pDriver;
        m_pDriver = NULL;
    }
    if (NULL != m_pClient)
    {
        m_pClient->Disconnect();
    }
}

// ----------------------------------------------------------------------------
//
// Gets or sets the remote APControl-server host-name or port-number.
//
const WCHAR *
USBClicker_t::
GetServerHost(void) const
{
    return (NULL == m_pClient)? NULL : m_pClient->GetServerHost();
}
const WCHAR *
USBClicker_t::
GetServerPort(void) const
{
    return (NULL == m_pClient)? NULL : m_pClient->GetServerPort();
}
DWORD
USBClicker_t::
SetRemoteServer(
    const TCHAR *HostName,
    const TCHAR *PortNumber)
{
    if (NULL != m_pClient)
    {
        delete m_pClient;
        m_pClient = NULL;
    }

    m_pClient = new ClickerClient_t(HostName, PortNumber);

    return (NULL == m_pClient)? ERROR_OUTOFMEMORY : ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Connects or disconnects the USB interface.
//
DWORD
USBClicker_t::
Insert(void)
{
    DWORD result;

    m_pState->SetCommandRetries(3);
    if (GetBoardType() == ExtendedClickerType)
         result = ConnectUSBPort(GetBoardNumber(), 2);
    else result = SetSwitches(GetBoardNumber(), false, true, false);
    m_pState->SetCommandRetries(1);

    if (ERROR_SUCCESS != result)
    {
        LogError(TEXT("Turning ON clicker board COM%d-%d failed: %s"),
                 GetCommPort(), GetCommSpeed(), Win32ErrorText(result));
        return result;
    }

    return ERROR_SUCCESS;
}
DWORD
USBClicker_t::
Remove(void)
{
    DWORD result;

    m_pState->SetCommandRetries(3);
    if (GetBoardType() == ExtendedClickerType)
         result = ConnectUSBPort(GetBoardNumber(), 0);
    else result = SetSwitches(GetBoardNumber(), false, false, false);
    m_pState->SetCommandRetries(1);

    if (ERROR_SUCCESS != result)
    {
        LogError(TEXT("Turning OFF clicker board COM%d-%d failed: %s"),
                 GetCommPort(), GetCommSpeed(), Win32ErrorText(result));
        return result;
    }

    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Performs the specified command locally.
//
static DWORD
DoLocalCommand(
    const ClickerState_t &Command,
    SCBExAPI            **ppDriver)
{
    DWORD result;

    // If necessary, initialize the switcher port.
    SCBExAPI *pDriver = *ppDriver;
    if (NULL == pDriver)
    {
        LogDebug(TEXT("Opening SCBExAPI(%d,%d)"), Command.GetCommPort(),
                                                  Command.GetCommSpeed());

        pDriver = new SCBExAPI(Command.GetCommPort(),
                               Command.GetCommSpeed(), FALSE);
        if (NULL == pDriver)
        {
            LogError(TEXT("Can't allocate %d bytes for Switcher board at COM%d-%d"),
                     sizeof(SCBExAPI), Command.GetCommPort(),
                                       Command.GetCommSpeed());
            return ERROR_OUTOFMEMORY;
        }


        if (!pDriver->Init())
        {
            LogError(TEXT("Can't initalize Switcher board at COM%d-%d"),
                     Command.GetCommPort(), Command.GetCommSpeed());
            delete pDriver;
            return ERROR_OPEN_FAILED;
        }

       *ppDriver = pDriver;
    }

    // Try this as many times as required.
    result = ERROR_SUCCESS;
    for (int tries = 0 ; tries < Command.GetCommandRetries() ; ++tries)
    {
        switch (Command.GetCommandCode())
        {
            case ClickerState_t::CommandSetSwitches:
                result = pDriver->Clicker_SetSwitches(Command.GetBoardNumber(),
                                                      Command.IsFireWireOn(),
                                                      Command.IsUSBOn(),
                                                      Command.IsPCMCIAOn());
                break;
            case ClickerState_t::CommandDisableCard:
                result = pDriver->SCB_DisableCard(Command.GetBoxNumber(),
                                                  Command.GetCardNumber());
                break;
            case ClickerState_t::CommandSelectCard:
                result = pDriver->SCB_SelectNthCard(Command.GetBoxNumber(),
                                                    Command.GetCardNumber());
                break;
            default:
                result = pDriver->SCB_ConnectUSBPort(Command.GetBoardNumber(),
                                                     Command.GetPortNumber());
                break;
        }

        if (ERROR_SUCCESS == result)
            break;

        Sleep(100);
    }

    return result;
}

// ----------------------------------------------------------------------------
//
// Performs the specified command remotely.
//
static DWORD
DoRemoteCommand(
    const ClickerState_t &Command,
    ClickerClient_t      *pClient)
{
    DWORD result;

    // Lock the remote connection.
    ce::gate<ce::critical_section> locker(pClient->GetLocker());

    // Try this twice in case the connection has dropped.
    ClickerState_t remoteState;
    ce::tstring    errorResponse;
    for (int tries = 0 ; ;)
    {
        // (Re)connect with the server.
        APControlClient_t *pRemote = pClient->Reconnect(Command, &result);
        if (NULL == pRemote)
            return result;

        // Send the command.
        result = Command.SendMessage(APControlData_t::DoDeviceCommandCode,
                                     pRemote,
                                    &remoteState,
                                    &errorResponse);
        if (ERROR_SUCCESS == result)
        {
            return ERROR_SUCCESS;
        }

        if (2 <= ++tries)
        {
            const char *commandName;
            switch (Command.GetCommandCode())
            {
                case ClickerState_t::CommandSetSwitches:
                    commandName = "SetSwitches";
                    break;
                case ClickerState_t::CommandDisableCard:
                    commandName = "DisableCard";
                    break;
                case ClickerState_t::CommandSelectCard:
                    commandName = "SelectCard";
                    break;
                default:
                    commandName = "ConnectUSB";
                    break;
            }
            LogError(TEXT("Error performing %hs command on COM%d-%d at %s:%s: %.128s"),
                     commandName,
                     Command.GetCommPort(),
                     Command.GetCommSpeed(),
                    pRemote->GetServerHost(),
                    pRemote->GetServerPort(),
                    &errorResponse[0]);
            return result;
        }

        // Disconnect and try again.
        pRemote->Disconnect();
    }
}

// ----------------------------------------------------------------------------
//
// Performs the specified command either locally or remotely.
//
inline DWORD
DoCommand(
    const ClickerState_t &Command,
    ClickerClient_t      *pClient,
    SCBExAPI            **ppDriver)
{
    return (NULL == pClient)?  DoLocalCommand(Command, ppDriver)
                            : DoRemoteCommand(Command,  pClient);
}

// ----------------------------------------------------------------------------
//
// These commands are passed through to the local or remote SCB board.
//
DWORD
USBClicker_t::
SetSwitches(
    int  BoardNumber,
    bool fFireWire,     // (dis)connect the FireWire interface
    bool fUSB,          // (dis)connect the USB interface
    bool fPCMCIA)       // (dis)connect the PCMCIA interface
{
    ClickerState_t localState = *m_pState;

    localState.SetCommandCode(ClickerState_t::CommandSetSwitches);
    localState.SetBoardNumber(BoardNumber);
    localState.SetFireWireOn(fFireWire);
    localState.SetUSBOn(fUSB);
    localState.SetPCMCIAOn(fPCMCIA);

    return DoCommand(localState, m_pClient, &m_pDriver);
}
DWORD
USBClicker_t::
DisableCard(
    int BoxNumber,
    int CardNumber)
{
    ClickerState_t localState = *m_pState;

    localState.SetCommandCode(ClickerState_t::CommandDisableCard);
    localState.SetBoxNumber(BoxNumber);
    localState.SetCardNumber(CardNumber);

    return DoCommand(localState, m_pClient, &m_pDriver);
}
DWORD
USBClicker_t::
SelectNthCard(
    int BoxNumber,
    int CardNumber)
{
    ClickerState_t localState = *m_pState;

    localState.SetCommandCode(ClickerState_t::CommandSelectCard);
    localState.SetBoxNumber(BoxNumber);
    localState.SetCardNumber(CardNumber);

    return DoCommand(localState, m_pClient, &m_pDriver);
}
DWORD
USBClicker_t::
ConnectUSBPort(
    int BoardNumber,
    int PortNumber)
{
    ClickerState_t localState = *m_pState;

    localState.SetCommandCode(ClickerState_t::CommandConnectUSB);
    localState.SetBoardNumber(BoardNumber);
    localState.SetPortNumber(PortNumber);

    return DoCommand(localState, m_pClient, &m_pDriver);
}

// ----------------------------------------------------------------------------
//
// (Re)connects the remote server.
//
APControlClient_t *
ClickerClient_t::
Reconnect(
    const ClickerState_t &State,
    DWORD                *pResult)
{
    HRESULT hr;

    if (NULL == m_pClient)
    {
        TCHAR deviceName[64];
        hr = StringCchPrintf(deviceName, sizeof(deviceName) / sizeof(WCHAR),
                             TEXT("COM%d-%d"), State.GetCommPort(),
                                               State.GetCommSpeed());
        if (FAILED(hr))
        {
           *pResult = HRESULT_CODE(hr);
            return NULL;
        }

        m_pClient = new APControlClient_t(m_ServerHost, m_ServerPort,
                                          TEXT("USBClicker"), deviceName);
        if (NULL == m_pClient)
        {
           *pResult = ERROR_OUTOFMEMORY;
            return NULL;
        }
    }

    if (!m_pClient->IsConnected())
    {
         hr = m_pClient->Connect();
         if (FAILED(hr))
         {
            *pResult = HRESULT_CODE(hr);
             LogError(TEXT("Can't connect to server \"%s:%s\": %s"),
                      m_pClient->GetServerHost(), m_pClient->GetServerPort(),
                      HRESULTErrorText(hr));
             return NULL;
         }
    }

   *pResult = ERROR_SUCCESS;
    return m_pClient;
}

// ----------------------------------------------------------------------------
