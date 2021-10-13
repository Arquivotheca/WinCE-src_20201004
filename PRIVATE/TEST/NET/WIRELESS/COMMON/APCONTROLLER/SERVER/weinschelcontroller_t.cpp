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
// Implementation of the WeinschelController_t class.
//
// ----------------------------------------------------------------------------

#include "WeinschelController_t.hpp"
#include "CommPort_t.hpp"
#include "Utils.hpp"

#include <assert.h>
#include <tchar.h>
#include <strsafe.h>

// Define this if you want LOTS of debug output:
//#define EXTRA_DEBUG 1

using namespace ce::qa;

// ----------------------------------------------------------------------------
//
// Constructor.
//
WeinschelController_t::
WeinschelController_t(
    const TCHAR *pDeviceType,
    const TCHAR *pDeviceName)
    : DeviceController_t(pDeviceType, pDeviceName),
      m_pPort(NULL),
      m_PortNumber(-1),
      m_ChannelNumber(-1)
{
    m_State.SetMinimumAttenuation(-1);
    m_State.SetMaximumAttenuation(-1);
}

// ----------------------------------------------------------------------------
//
// Destructor.
//
WeinschelController_t::
~WeinschelController_t(void)
{
    if (NULL != m_pPort)
    {
        delete m_pPort;
        m_pPort = NULL;
    }
}

// ----------------------------------------------------------------------------
//
// (Re)opens the comm port if necessary.
//
static DWORD
ReOpen(
    const WeinschelController_t *pDevice,
    CommPort_t                  *pPort)
{
    if (!pPort->IsOpened())
    {
        LogDebug(TEXT("[AC] Opening %s \"%s\""), pDevice->GetDeviceType(),
                                                 pDevice->GetDeviceName());
    
        DWORD result = pPort->Open(pDevice->DefaultBaudRate,
                                   pDevice->DefaultByteSize,
                                   pDevice->DefaultParity,
                                   pDevice->DefaultStopBits);
        if (ERROR_SUCCESS != result)
        {
            LogError(TEXT("Can't open Weinschel device \"%s\": %s"),
                     pDevice->GetDeviceName(), Win32ErrorText(result));
            return result;
        }
    }
    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Sends a command and waits for a response.
//
static DWORD
GetCommandResponse(
    const WeinschelController_t *pDevice,
    CommPort_t                  *pPort,
    const char                  *pFieldName,
    const char                  *pCommand,
    int                         *pResponse,
    int                          MinResponse,
    int                          MaxResponse,
    int                          MaxMillisToWait,
    bool                         ErrorExpected = false)
{
    ce::string command(pCommand);
    ce::string readbuff;

    // Do this a few times in case the Weinschel is slow in responding.
    for (int tries = 0 ; tries < 3 ; ++tries, MaxMillisToWait *= 2)
    {
        // Write the command.          
        DWORD result = pPort->Write(command, MaxMillisToWait);
        if (ERROR_SUCCESS != result)
        {
            command[command.length()-1] = '\0';  // remove trailing newline
            LogError(TEXT("Can't write \"%hs\" to %s at \"%s\": %s"),
                          &command[0],
                           pDevice->GetDeviceType(),
                           pDevice->GetDeviceName(), 
                           Win32ErrorText(result));
            return result;
        }

        // Read a response.
        if (!readbuff.reserve(command.length() + 50))
            return ERROR_OUTOFMEMORY;
    
        result = pPort->Read(&readbuff, readbuff.capacity(), MaxMillisToWait);
        if (ERROR_SUCCESS != result)
        {
            LogError(TEXT("Can't read %hs from %s at \"%s\": %s"),
                           pFieldName,
                           pDevice->GetDeviceType(),
                           pDevice->GetDeviceName(), 
                           Win32ErrorText(result));
            return result;
        }

        // The Weinschel may echo its command input. Hence, the following
        // scans through the response and ignores all but the final line.
        const char *response  = readbuff.get_buffer();
        const char *respEnd   = response + readbuff.length();
        const char *lineStart = response;
        while (*response && response != respEnd)
        {
            lineStart = response;
            while (*response && response != respEnd)
            {
                const char *lineEnd = response++;
                if (*lineEnd == '\n')
                {
                    break;
                }
            }
        }

        // Parse and validate the response.
        char *pEnd;
        int value = (int)(strtod(lineStart, &pEnd) + 0.5);
        if (NULL != pEnd && isspace((unsigned char)*pEnd) 
         && MinResponse <= value && value <= MaxResponse)
        {
#ifdef EXTRA_DEBUG
            LogDebug(TEXT("[AC]   %hs = %d"), pFieldName, value);
#endif
           *pResponse = value;
            return ERROR_SUCCESS;
        }
    }

    if (!ErrorExpected)
    {
        LogError(TEXT("Got bad %hs response from %s at \"%s\":")
                 TEXT(" \"%hs\" (%d bytes)"),
                 pFieldName,
                 pDevice->GetDeviceType(),
                 pDevice->GetDeviceName(),
                &readbuff[0], readbuff.length());
    }
    
    return ERROR_READ_FAULT;
}

// ----------------------------------------------------------------------------
//
// Changes to the specified channel.
//
static DWORD
DoSwitchChannel(
    WeinschelController_t *pDevice,
    CommPort_t            *pPort)
{
    int channel = pDevice->GetChannelNumber();

#ifdef EXTRA_DEBUG
    LogDebug(TEXT("[AC] DoSwitchChannel(%d)"), channel);
#endif

    char buff[128];
    int  buffChars = COUNTOF(buff);
    HRESULT hr = StringCchPrintfA(buff, buffChars,
                                 "CHAN %d\nCHAN?\n", channel);
    if (FAILED(hr))
        return HRESULT_CODE(hr);

    return GetCommandResponse(pDevice,
                              pPort,
                             "channel-set",
                              buff,
                              &channel,
                               channel,
                               channel,
                              150);
}

// ----------------------------------------------------------------------------
//
// Gets the current attenuation value.
//
static DWORD
DoGetAttenuation(
    WeinschelController_t *pDevice,
    CommPort_t            *pPort,
    int                   *pAttenuation)
{
#ifdef EXTRA_DEBUG
    LogDebug(TEXT("[AC] DoGetAttenuation()"));
#endif

    return GetCommandResponse(pDevice,
                              pPort,
                              "attenuation",
                              "ATTN?\n",
                              pAttenuation,
                              0,
                              150,
                              50);
}

// ----------------------------------------------------------------------------
//
// Sets the RF attenuation level.
//
static DWORD
DoSetAttenuation(
    WeinschelController_t *pDevice,
    CommPort_t            *pPort,
    int                   *pAttenuation,
    bool                   ErrorExpected = false)
{
#ifdef EXTRA_DEBUG
    LogDebug(TEXT("[AC] DoSetAttenuation(%d)"), *pAttenuation);
#endif
    
    char buff[128];
    int  buffChars = COUNTOF(buff);
    HRESULT hr = StringCchPrintfA(buff, buffChars,
                                 "ATTN %d\nATTN?\n", *pAttenuation);
    if (FAILED(hr))
        return HRESULT_CODE(hr);

    return GetCommandResponse(pDevice,
                              pPort,
                             "attenuation-set",
                              buff,
                              pAttenuation,
                             *pAttenuation - 1,
                             *pAttenuation + 1,
                              75,
                              ErrorExpected);
}

// ----------------------------------------------------------------------------
//
// Gets the current, minimum and maximum RF attenuation.
//
static DWORD
DoGetAttenuator(
    WeinschelController_t *pDevice,
    CommPort_t            *pPort,
    RFAttenuatorState_t   *pState,
    RFAttenuatorState_t   *pResponse)
{
    DWORD result;

    // Change to the appropriate Weinschel channel.
    result = DoSwitchChannel(pDevice, pPort);
    if (ERROR_SUCCESS != result)
        return result;

    // Get the current attenuation value.
    int curValue;
    result = DoGetAttenuation(pDevice, pPort, &curValue);
    if (ERROR_SUCCESS != result)
        return result;

    // If the cached min/max values haven't been set yet, probe the 
    // device to determine the minimum and maximum legal values
    if (0 > pState->GetMinimumAttenuation() 
     || 0 > pState->GetMaximumAttenuation())
    {
        int minValue;
        for (minValue = 0 ; 100 > minValue ; ++minValue)
        {
            int setValue = minValue;
            result = DoSetAttenuation(pDevice, pPort, &setValue, true);
            if (ERROR_SUCCESS == result)
            {
                pState->SetMinimumAttenuation(setValue);
                break;
            }
        }
        if (100 <= minValue)
        {
            LogError(TEXT("[AC] Can't get min attenuation for %s at \"%s\": %s"),
                     pDevice->GetDeviceType(),
                     pDevice->GetDeviceName(), 
                     Win32ErrorText(result));
            pState->SetMinimumAttenuation(0);
        }    

        int maxValue;
        for (maxValue = 110 ; pState->GetMinimumAttenuation() < maxValue ; --maxValue)
        {
            int setValue = maxValue;
            result = DoSetAttenuation(pDevice, pPort, &setValue, true);
            if (ERROR_SUCCESS == result)
            {
                pState->SetMaximumAttenuation(setValue);
                break;
            }
        }
        if (pState->GetMinimumAttenuation() >= maxValue)
        {
            LogError(TEXT("[AC] Can't get max attenuation for %s at \"%s\": %s"),
                     pDevice->GetDeviceType(),
                     pDevice->GetDeviceName(), 
                     Win32ErrorText(result));
            pState->SetMaximumAttenuation(100);
        }

        // Reset the current attenuation where it belongs.
        result = DoSetAttenuation(pDevice, pPort, &curValue);
        if (ERROR_SUCCESS != result)
            return result;
    }
    
    pState->SetCurrentAttenuation(curValue);
   *pResponse = *pState;
    return ERROR_SUCCESS;
}

DWORD
WeinschelController_t::
GetAttenuator(
    RFAttenuatorState_t *pResponse,
    ce::tstring         *pErrorMessage)
{
    // Capture errors.
    ErrorLock captureErrors(pErrorMessage);

    // Connect and lock the comm port.
    DWORD result = Reconnect();
    if (ERROR_SUCCESS != result)
        return result;
    
    ce::gate<ce::critical_section> portLocker(m_pPort->GetLocker());

    // Open the comm port (if necessary).
    result = ReOpen(this, m_pPort);
    if (ERROR_SUCCESS != result)
        return result;

    // Get the attenuation values.
    result = DoGetAttenuator(this, m_pPort, &m_State, pResponse);

    // Close the port if the operation failed.
    if (ERROR_SUCCESS != result)
    {
        m_pPort->Close();
    }

    return result;
}

// ----------------------------------------------------------------------------
//
// Sets the current attenuation for the RF attenuator.
//
static DWORD
DoSetAttenuator(
          WeinschelController_t *pDevice,
          CommPort_t            *pPort,
          RFAttenuatorState_t   *pState,
    const RFAttenuatorState_t   &NewState,
          RFAttenuatorState_t   *pResponse)
{
    DWORD result;
    
    // Change to the appropriate Weinschel channel.
    result = DoSwitchChannel(pDevice, pPort);
    if (ERROR_SUCCESS != result)
        return result;

    // Until the attenuation reaches the desired setting...
    int  newAtten = NewState.GetCurrentAttenuation();
    long stopTime = GetTickCount() + ((long)NewState.GetAdjustTime() * 1000);
    long stepTime = NewState.GetStepTimeMS();
    for (;;)
    {
        // Set the attenuation.
        result = DoSetAttenuation(pDevice, pPort, &newAtten);
        if (ERROR_SUCCESS != result)
            break;

        // If the cached min/max values have been set, just use those.
        if (0 <= pState->GetMinimumAttenuation() 
         && 0 <= pState->GetMaximumAttenuation())
        {
            pState->SetCurrentAttenuation(newAtten);
           *pResponse = *pState;
        }

        // Otherwise, have GetAttenuation get all the values.
        else
        {
            result = DoGetAttenuator(pDevice, pPort, pState, pResponse);
            if (ERROR_SUCCESS != result)
                break;
        }

        // Stop if we've reached the desired attenuation setting.
        int remaining = NewState.GetFinalAttenuation() - newAtten;
        if (remaining == 0)
            break;

        // Calculate the next attenuation step and sleep until it's time.
        long remainingTime = stopTime - GetTickCount();
        if (remainingTime <= 0)
        {
            newAtten += remaining;
        }
        else
        {
            long remainingSteps = (remainingTime + stepTime - 1) / stepTime;
            if (remainingSteps < 2)
                remainingSteps = 1;
            else
            {
                while (remaining / remainingSteps == 0)
                       remainingSteps--;
            }
            newAtten += remaining / remainingSteps;
            Sleep(remainingTime / remainingSteps);
        }
    }

    return result;
}

DWORD
WeinschelController_t::
SetAttenuator(
    const RFAttenuatorState_t &NewState,
          RFAttenuatorState_t *pResponse,
          ce::tstring         *pErrorMessage)
{
    DWORD result = ERROR_SUCCESS;
    ErrorLock captureErrors(pErrorMessage);

    // Open and lock the COM port.
    result = Reconnect();
    if (ERROR_SUCCESS != result)
        return result;

    ce::gate<ce::critical_section> portLocker(m_pPort->GetLocker());
   
    // Open the comm port (if necessary).
    result = ReOpen(this, m_pPort);
    if (ERROR_SUCCESS != result)
        return result;

    // Get the attenuation values.
    result = DoSetAttenuator(this, m_pPort, &m_State, NewState, pResponse);

    // Close the port if the operation failed.
    if (ERROR_SUCCESS != result)
    {
        m_pPort->Close();
    }

    return result;     
}

// ----------------------------------------------------------------------------
//
// (Re)connects the Weinschel device.
//
DWORD
WeinschelController_t::
Reconnect(void)
{
    // Only reconnect if necessary.
    if (NULL == m_pPort)
    {
        LogDebug(TEXT("[AC] Connecting %s \"%s\""), GetDeviceType(), 
                                                    GetDeviceName());

        // Parse the port and channel number from the device-name.
        // This must be formatted as two integers separated by a dash
        // (minus sign) optionally preceded by "COM" and "CHAN":
        //     [COM] {port} - [CHAN] {channel}
        const TCHAR *pDevName = GetDeviceName();
        if (0 > m_PortNumber || 0 > m_ChannelNumber)
        {
            const TCHAR *pDash = _tcschr(pDevName, TEXT('-'));
            if (NULL != pDash)
            {
                const TCHAR *pValue = pDash;
                while (pValue-- != pDevName && _istspace(*pValue)) {;} pValue++;
                while (pValue-- != pDevName && _istdigit(*pValue)) {;} pValue++;
                TCHAR *pEnd;
                unsigned long portNumber = _tcstoul(pValue, &pEnd, 10);
                if (NULL != pEnd && (pEnd == pDash || _istspace(*pEnd))
                 && 256 >= portNumber)
                {
                    pValue = pDash;
                    while (_istspace(*(++pValue)) || _istalpha(*pValue)) {;}
                    unsigned long chanNumber = _tcstoul(pValue, &pEnd, 10);
                    if (NULL != pEnd && (0 == *pEnd || _istspace(*pEnd))
                     && 16 >= chanNumber)
                    {
                        m_PortNumber    = (int)portNumber;
                        m_ChannelNumber = (int)chanNumber;
                    }
                }
            }
        }
        
        if (1 > m_PortNumber || 1 > m_ChannelNumber)
        {
            LogError(TEXT("Weinschel device-name \"%s\" bad:")
                     TEXT(" expected \"[COM]{port}-[CHAN]{channel}\""),
                     pDevName);
            return ERROR_INVALID_PARAMETER;
        }

        // Create a link to the COM port.
        m_pPort = new CommPort_t(m_PortNumber);
        if (NULL == m_pPort)
        {
            LogError(TEXT("[AC] Can't allocate CommPort object"));
            return ERROR_OUTOFMEMORY;
        }
    }
    
    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
