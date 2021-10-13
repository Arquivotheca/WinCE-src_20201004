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
// Implementation of the USBClickerController_t class.
//
// ----------------------------------------------------------------------------

#include "USBClickerController_t.hpp"

#include <assert.h>
#include <tchar.h>
#include <SCBExAPI.h>

#include <USBClicker_t.hpp>
#include <ClickerState_t.hpp>
#include <WiFUtils.hpp>

#include <inc/sync.hxx>

using namespace ce::qa;

// ----------------------------------------------------------------------------
//
// Constructor.
//
USBClickerController_t::
USBClickerController_t(
    IN const TCHAR *pDeviceType,
    IN const TCHAR *pDeviceName)
    : DeviceController_t(pDeviceType, pDeviceName),
      m_pClicker(NULL)
{
    // nothing to do
}

// ----------------------------------------------------------------------------
//
// Destructor.
//
USBClickerController_t::
~USBClickerController_t(void)
{
    if (NULL != m_pClicker)
    {
        delete m_pClicker;
        m_pClicker = NULL;
    }
}

// ----------------------------------------------------------------------------
//
// (Re)connects the SCB interface.
//
static DWORD
Reconnect(
    OUT USBClicker_t **ppClicker,
    IN  const TCHAR    *pDeviceName)
{
    // Only reconnect if necessaary.
    if (NULL != *ppClicker)
        return ERROR_SUCCESS;

    // Lock the device.
    ce::gate<ce::mutex> locker(g_USBClickerMutex);

    LogDebug(TEXT("[AC] Opening USB-clicker \"%s\""), pDeviceName);

    // Parse the port and speed from the device-name.
    // This must be formatted as two integers separated by a dash
    // (minus sign) optionally preceded by "COM" and "SPEED":
    //     [COM] {port} - [SPEED] {baud-rate}
    int  commPort = -1;
    long commSpeed = -1;
    const TCHAR *pDash = _tcschr(pDeviceName, TEXT('-'));
    if (NULL != pDash)
    {
        const TCHAR *pValue = pDash;
        while (pValue-- != pDeviceName && _istspace(*pValue)) {;} pValue++;
        while (pValue-- != pDeviceName && _istdigit(*pValue)) {;} pValue++;
        WCHAR *pEnd;
        unsigned long portNumber = _tcstoul(pValue, &pEnd, 10);
        if (NULL != pEnd && (pEnd == pDash || _istspace(*pEnd))
         && 256 >= portNumber)
        {
            pValue = pDash;
            while (_istspace(*(++pValue)) || _istalpha(*pValue)) {;}
            unsigned long baudRate = _tcstoul(pValue, &pEnd, 10);
            if (NULL != pEnd && (TEXT('\0') == *pEnd || _istspace(*pEnd))
             && (128*1024) >= baudRate)
            {
                commPort  = (int) portNumber;
                commSpeed = (long)baudRate;
            }
        }
    }
    
    if (1 > commPort || 1200 > commSpeed)
    {
        LogError(TEXT("USB-clicker device-name \"%s\" bad:")
                 TEXT(" expected \"[COM]{port}-[SPEED]{baud-rate}\""),
                 pDeviceName);
        return ERROR_DLL_INIT_FAILED;
    }

    // Initialize the connection.
   *ppClicker = new USBClicker_t;
    if (NULL == *ppClicker)
    {
        LogError(TEXT("[AC] Can't allocate USBClicker object"));
        return ERROR_DLL_INIT_FAILED;
    }

    (*ppClicker)->SetCommPort(commPort);
    (*ppClicker)->SetCommSpeed(commSpeed);
    return ERROR_SUCCESS;

}

// ----------------------------------------------------------------------------
//
// Performs the remote SCB command.
//
DWORD
USBClickerController_t::
DoDeviceCommand(
    IN       DWORD        CommandCode,
    IN const BYTE       *pCommandData,
    IN       DWORD        CommandDataBytes,
    OUT      BYTE      **ppReturnData,
    OUT      DWORD       *pReturnDataBytes,
    OUT      ce::tstring *pErrorMessage)
{
    DWORD result;
    ClickerState_t command;

    // Capture error messages.
    ErrorLock captureErrors(pErrorMessage);
    
    // Create the USB-clicker driver (if necessary).
    result = Reconnect(&m_pClicker, GetDeviceName());
    if (ERROR_SUCCESS != result)
        return result;
    
    // Decode the command.
    result = command.DecodeMessage(result,
                                   pCommandData,
                                    CommandDataBytes);
    if (ERROR_SUCCESS != result)
        return result;

    // Try the command as many times as required.
    const char *commandName = NULL;
    for (int tries = 0 ; tries < command.GetCommandRetries() ; ++tries)
    {
        switch (command.GetCommandCode())
        {
            case ClickerState_t::CommandSetSwitches:
                commandName = "SetSwitches";
                LogDebug(TEXT("[AC] %hs command:"), commandName);
                LogDebug(TEXT("[AC]   BoardNumber  = %d"),
                         command.GetBoardNumber());
                LogDebug(TEXT("[AC]   IsFireWireOn = %hs"),
                         command.IsFireWireOn()? "true" : "false");
                LogDebug(TEXT("[AC]   IsUSBOn      = %hs"),
                         command.IsUSBOn()? "true" : "false");
                LogDebug(TEXT("[AC]   IsPCMCIAOn   = %hs"),
                         command.IsPCMCIAOn()? "true" : "false");
                result = m_pClicker->SetSwitches(command.GetBoardNumber(),
                                                 command.IsFireWireOn(),
                                                 command.IsUSBOn(),
                                                 command.IsPCMCIAOn());
                break;

            case ClickerState_t::CommandDisableCard:
                commandName = "DisableCard";
                LogDebug(TEXT("[AC] %hs command:"), commandName);
                LogDebug(TEXT("[AC]   BoxNumber  = %d"),
                         command.GetBoxNumber());
                LogDebug(TEXT("[AC]   CardNumber = %d"),
                         command.GetCardNumber());
                result = m_pClicker->DisableCard(command.GetBoxNumber(),
                                                 command.GetCardNumber());
                break;

            case ClickerState_t::CommandSelectCard:
                commandName = "SelectCard";
                LogDebug(TEXT("[AC] %hs command:"), commandName);
                LogDebug(TEXT("[AC]   BoxNumber  = %d"),
                         command.GetBoxNumber());
                LogDebug(TEXT("[AC]   CardNumber = %d"),
                         command.GetCardNumber());
                result = m_pClicker->SelectNthCard(command.GetBoxNumber(),
                                                   command.GetCardNumber());
                break;

            default:
                commandName = "ConnectUSB";
                LogDebug(TEXT("[AC] %hs command:"), commandName);
                LogDebug(TEXT("[AC]   BoardNumber = %d"),
                         command.GetBoardNumber());
                LogDebug(TEXT("[AC]   PortNumber  = %d"),
                         command.GetPortNumber());
                result = m_pClicker->ConnectUSBPort(command.GetBoardNumber(),
                                                    command.GetPortNumber());
                break;
        }

        if (ERROR_SUCCESS == result)
            break;

        Sleep(100);
    }
    
    if (ERROR_SUCCESS == result)
    {
        // Encode the results.
        result = command.EncodeMessage(result,
                                       ppReturnData,
                                        pReturnDataBytes);
    }
    else
    {
        // Generate an appropriate error message.
        switch (result)
        {
            case SCBERR_INIT_FAILED:
                result = ERROR_DLL_INIT_FAILED;
                break;
            case SCBERR_INVALIDPARA:
                result = ERROR_INVALID_PARAMETER;
                break;
            case SCBERR_PORTWRITEFAILURE:
                result = ERROR_WRITE_FAULT;
                break;
            case SCBERR_PORTREADFAILURE:
                result = ERROR_READ_FAULT;
                break;
        }
        LogError(TEXT("USB-clicker %s %hs failed: %s"),
                 GetDeviceName(), commandName,
                 Win32ErrorText(result));
    }
    
    return result;
}

// ----------------------------------------------------------------------------
