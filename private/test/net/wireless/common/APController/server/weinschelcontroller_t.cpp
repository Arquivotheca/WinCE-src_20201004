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
// Implementation of the WeinschelController_t class.
//
// ----------------------------------------------------------------------------

#include "WeinschelController_t.hpp"

#include "APCUtils.hpp"
#include "CommPort_t.hpp"

#include <assert.h>
#include <tchar.h>
#include <strsafe.h>

// Define these if you want LOTS of debug output:
#define EXTRA_DEBUG       1
//#define EXTRA_EXTRA_DEBUG 1

// Weinschel-device attenuation limits (dBs):
static const int MinimumMinAttenuation =   0;
static const int MaximumMinAttenuation =  20;
static const int MinimumMaxAttenuation =  90;
static const int MaximumMaxAttenuation = 110;

// Time (milliseconds) to complete device operations:
static const int SetChannelTimeMS     = 200;
static const int GetAttenuationTimeMS =  75;
static const int SetAttenuationTimeMS =  75;

// Minimum sleep and inter-attenuation-step milliseconds:
static const long MinimumAttenuationSleepTime = 20;
static const long MinimumAttenuationStepTime  = 400;        

using namespace ce::qa;

// ----------------------------------------------------------------------------
//
// Constructor.
//
WeinschelController_t::
WeinschelController_t(
    const TCHAR *pDeviceType,
    const TCHAR *pDeviceName)
    : AttenuatorController_t(pDeviceType, pDeviceName),
      m_pPort(NULL),
      m_PortNumber(-1),
      m_ChannelNumber(-1)
{
    m_State.SetMinimumAttenuation(-1);
    m_State.SetMaximumAttenuation(-1);
}

// ----------------------------------------------------------------------------
//
// Copy constructor.
//
WeinschelController_t::
WeinschelController_t(
    const WeinschelController_t &rhs)
    : AttenuatorController_t(rhs.GetDeviceType(), rhs.GetDeviceName()),
      m_pPort(NULL),
      m_PortNumber(rhs.m_PortNumber),
      m_ChannelNumber(rhs.m_ChannelNumber),
      m_State(rhs.m_State)
{
    // Nothing to do.
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
        if (NO_ERROR != result)
        {
            LogError(TEXT("Can't open Weinschel device \"%s\": %s"),
                     pDevice->GetDeviceName(), Win32ErrorText(result));
            return result;
        }
    }
    return NO_ERROR;
}

// ----------------------------------------------------------------------------
//
// Parse a command response.
//
static DWORD
ParseCommandResponse(
    ce::string& readBuff,
    int*        pValue)
{
    DWORD result = ERROR_READ_FAULT;
    if (readBuff.length() != 0)
    {
        // The Weinschel may echo its command input. Hence, the following
        // scans through the response and ignores all but the last line.
        const char *response  = readBuff.get_buffer();
        const char *respEnd   = response + readBuff.length();
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

        char *pEnd;
        int value = (int)(strtod(lineStart, &pEnd) + 0.5);
        if (NULL != pEnd && isspace((unsigned char)*pEnd))
        {
            *pValue = value;
            result = NO_ERROR;
        }
    }
    return result;
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
    DWORD result = NO_ERROR;
    ce::string command(pCommand);
    
    // Allocate a read-buffer.
    ce::string readBuff;
    if (!readBuff.reserve(command.length() + 50))
    {
        LogError(TEXT("Can't allocate %u bytes for read-buffer"),
                 command.length() + 50);
        return ERROR_OUTOFMEMORY;
    }

    enum State_e { WritingCommand, ReadingResponse, ParsingResponse };
    State_e state;

    for (int resets = 0 ;;)
    {
        result = ReOpen(pDevice, pPort);
        if (NO_ERROR != result)
            return result;

        // Do this a few times in case the Weinschel is slow responding.
        // Allow twice as long for the comm-port response each time.
        state = WritingCommand;
        const int MaxTries = 4;
        for (int tries = 0 ; tries < MaxTries ; ++tries, MaxMillisToWait *= 2)
        {
            // Only wait 1 second for the write to finish.
            const long MaxWriteWaitTime = 1000;
        
            // Write the command.
            state = WritingCommand;
            result = pPort->Write(command, MaxWriteWaitTime);
            if (NO_ERROR != result)
            {
                Sleep(MaxMillisToWait);
                continue;
            }
        
            // Read the response.
            state = ReadingResponse;
            result = pPort->Read(&readBuff, readBuff.capacity(),
                                 MaxMillisToWait/2, MaxMillisToWait);
            if (NO_ERROR != result)
                continue;

            // Parse and validate the response (if any).
            state = ParsingResponse;
            result = ERROR_READ_FAULT;
            int value;
            if (NO_ERROR == ParseCommandResponse(readBuff, &value)
             && MinResponse <= value && value <= MaxResponse)
            {
#ifdef EXTRA_EXTRA_DEBUG
                LogDebug(TEXT("[AC]   %hs = %d"), pFieldName, value);
#endif
                *pResponse = value;
                return NO_ERROR;
            }
        }

        // If we made it to here, it's because there was an error.
        // ERROR_WRITE_FAULT gets one chance at a reset.
        assert(NO_ERROR != result);
        if (2 == ++resets || ERROR_WRITE_FAULT != result)
            break;
        assert(ERROR_WRITE_FAULT == result && 2 > resets);

        // Close the com port, as that will probably fix the problem.
        LogDebug(TEXT("[AC] Force closing %s \"%s\""),
                 pDevice->GetDeviceType(),
                 pDevice->GetDeviceName());
        pPort->Close();
    }

    if (!ErrorExpected)
    {
        switch (state)
        {
        case WritingCommand:
            command[command.length()-1] = '\0';  // remove trailing newline
            LogError(TEXT("Can't write %hs \"%hs\" to %s at \"%s\": %s"),
                     pFieldName,
                     &command[1],                 // ignore leading newline
                     pDevice->GetDeviceType(),
                     pDevice->GetDeviceName(), 
                     Win32ErrorText(result));
            break;

        case ReadingResponse:
            LogError(TEXT("Can't read %hs from %s at \"%s\": %s"),
                     pFieldName,
                     pDevice->GetDeviceType(),
                     pDevice->GetDeviceName(), 
                     Win32ErrorText(result));
            break;

        default:
            LogError(TEXT("Got bad %hs response from %s at \"%s\":")
                TEXT(" \"%hs\" (%d bytes)"),
                     pFieldName,
                     pDevice->GetDeviceType(),
                     pDevice->GetDeviceName(),
                     &readBuff[0], readBuff.length());                
            break;
        }
    }
    
    return result;
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
                                 "\nCHAN %d\nCHAN?\n", channel);
    if (FAILED(hr))
        return HRESULT_CODE(hr);

    return GetCommandResponse(pDevice,
                              pPort,
                             "channel-set",
                              buff,
                              &channel,
                               channel,
                               channel,
                              SetChannelTimeMS);
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
                              "\nATTN?\n",
                              pAttenuation,
                              MinimumMinAttenuation,
                              MaximumMaxAttenuation,
                              GetAttenuationTimeMS);
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
                                 "\nATTN %d\nATTN?\n", *pAttenuation);
    if (FAILED(hr))
        return HRESULT_CODE(hr);

    return GetCommandResponse(pDevice,
                              pPort,
                              "attenuation-set",
                              buff,
                              pAttenuation,
                              *pAttenuation - 1,
                              *pAttenuation + 1,
                              SetAttenuationTimeMS,
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
    RFAttenuatorState_t   *pState)
{
    DWORD result;

    // Get the current attenuation value.
    int curValue;
    result = DoGetAttenuation(pDevice, pPort, &curValue);
    if (NO_ERROR != result)
        return result;

    // If the cached min/max values haven't been set yet, probe the 
    // device to determine the minimum and maximum legal values.
    if (0 > pState->GetMinimumAttenuation() 
     || 0 > pState->GetMaximumAttenuation())
    {        
        int minValue = 0;
#if 1
        for (minValue = MaximumMinAttenuation ; 
             minValue > MinimumMinAttenuation ; --minValue)
        {
            int setValue = minValue;
            result = DoSetAttenuation(pDevice, pPort, &setValue, true);
            if (NO_ERROR != result)
            {
                while (++minValue < MaximumMinAttenuation)
                {
                    setValue = minValue;
                    result = DoSetAttenuation(pDevice, pPort, &setValue, true);
                    if (NO_ERROR == result)
                    {
                        minValue = setValue;
                        break;
                    }
                }
                break;
            }
        }
        if (minValue >= MaximumMinAttenuation)
        {
            LogWarn(TEXT("[AC] Can't get min attenuation for %s at \"%s\": %s"),
                    pDevice->GetDeviceType(),
                    pDevice->GetDeviceName(), 
                    Win32ErrorText(result));
        }
#endif
        pState->SetMinimumAttenuation(minValue);

        int maxValue = 103;
#if 1
        for (maxValue = MinimumMaxAttenuation ; 
             maxValue < MaximumMaxAttenuation ; ++maxValue)
        {
            int setValue = maxValue;
            result = DoSetAttenuation(pDevice, pPort, &setValue, true);
            if (NO_ERROR != result)
            {
                while (--maxValue > MinimumMaxAttenuation)
                {
                    setValue = maxValue;
                    result = DoSetAttenuation(pDevice, pPort, &setValue, true);
                    if (NO_ERROR == result)
                    {
                        maxValue = setValue;
                        break;
                    }
                }
                break;
            }
        }
        if (maxValue <= MinimumMaxAttenuation)
        {
            LogWarn(TEXT("[AC] Can't get max attenuation for %s at \"%s\": %s"),
                    pDevice->GetDeviceType(),
                    pDevice->GetDeviceName(), 
                    Win32ErrorText(result));
        }
#endif
        pState->SetMaximumAttenuation(maxValue);

        // Reset the current attenuation where it belongs.
        result = DoSetAttenuation(pDevice, pPort, &curValue);
        if (NO_ERROR != result)
            return result;
    }
    
    pState->SetCurrentAttenuation(curValue);
    return NO_ERROR;
}

DWORD
WeinschelController_t::
GetAttenuator(
    RFAttenuatorState_t *pResponse)
{
    // Connect and lock the comm port.
    DWORD result = Reconnect();
    if (NO_ERROR != result)
        return result;
    
    ce::gate<ce::critical_section> portLocker(m_pPort->GetLocker());

    // Open the comm port (if necessary).
    result = ReOpen(this, m_pPort);
    if (NO_ERROR != result)
        return result;

    // Get the attenuation values.
    result = DoSwitchChannel(this, m_pPort);
    if (NO_ERROR == result)
        result = DoGetAttenuator(this, m_pPort, &m_State);

    // Close the port if the operation failed.
    if (NO_ERROR != result)
        m_pPort->Close();
    else
    {
       *pResponse = m_State;       
        pResponse->SetFieldFlags(0);
    }

    return result;
}

// ----------------------------------------------------------------------------
//
// Sets the current attenuation for the RF attenuator.
// Returns ERROR_SYNC_FOREGROUND_REFRESH_REQUIRED and updates the current
// state after processing the specified number of attenuation steps. 
//
static DWORD
DoSetAttenuator(
          WeinschelController_t *pDevice,
          CommPort_t            *pPort,
          RFAttenuatorState_t   *pState,
    const RFAttenuatorState_t   &NewState,
          RFAttenuatorState_t   *pAsyncState,
          long                  *pAsyncDelay)
{
    DWORD result;

#ifdef EXTRA_DEBUG
    if (NewState.GetCurrentAttenuation() == NewState.GetFinalAttenuation())
    {
        LogDebug(TEXT("[AC] Setting %s;%s attenuation to %ddBs"),
                 pDevice->GetDeviceType(),
                 pDevice->GetDeviceName(),
                 NewState.GetCurrentAttenuation());
    }
    else
    {
        LogDebug(TEXT("[AC] Adjusting %s;%s attenuation:"),
                 pDevice->GetDeviceType(),
                 pDevice->GetDeviceName());
        LogDebug(TEXT("[AC]   starting attenuation %ddBs"),
                 NewState.GetCurrentAttenuation());
        LogDebug(TEXT("[AC]   final attenuation    %ddBs"),
                 NewState.GetFinalAttenuation());
        LogDebug(TEXT("[AC]   adjusted once every  %.2f second"),
                 (double)NewState.GetStepTimeMS() / 1000.0);
        LogDebug(TEXT("[AC]   over period of       %d.00 seconds"),
                 NewState.GetAdjustTime());
        if (NewState.GetSynchronousSteps())
        {
            LogDebug(TEXT("[AC]   synchronous steps       %d"),
                     NewState.GetSynchronousSteps());
        }
    }
#endif

    // Validate the input parameters.
    long stepTime = NewState.GetStepTimeMS();
    if (stepTime < MinimumAttenuationStepTime)
        stepTime = MinimumAttenuationStepTime;

    // Until the attenuation reaches the desired setting...
    int   newAtten  = NewState.GetCurrentAttenuation();
    int   stopSteps = NewState.GetSynchronousSteps();
    long  stopTime  = (long)NewState.GetAdjustTime() * 1000;
    DWORD startTime = GetTickCount();
    for (int stepNumber = 0 ;; ++stepNumber)
    {
        // Set the attenuation.
        result = DoSetAttenuation(pDevice, pPort, &newAtten);
        if (NO_ERROR != result)
            break;

        // If the cached min/max values have been set, just use those.
        if (0 <= pState->GetMinimumAttenuation() 
         && 0 <= pState->GetMaximumAttenuation())
        {
            pState->SetCurrentAttenuation(newAtten);
        }

        // Otherwise, get all the values.
        else
        {
            result = DoGetAttenuator(pDevice, pPort, pState);
            if (NO_ERROR != result)
                break;
        }

        // Stop if we've reached the desired attenuation setting.
        int remaining = NewState.GetFinalAttenuation() - newAtten;
        if (remaining == 0)
            break;
        
        // Calculate the next attenuation step delay.
        long runTime = WiFUtils::SubtractTickCounts(GetTickCount(), startTime);
        long remainingTime = stopTime - runTime;
        long sleepTime;
        if (remainingTime < MinimumAttenuationStepTime)
        {
            newAtten += remaining;
            sleepTime = 0;
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
            sleepTime = remainingTime / remainingSteps;
        }
        
        // If we've finished the last synchronous attenuation step, update
        // the current state and return the appropriate code.
        if (--stopSteps == 0)
        {
            result = ERROR_SYNC_FOREGROUND_REFRESH_REQUIRED;
           *pAsyncState = NewState;
           *pAsyncDelay = sleepTime;
            pAsyncState->AdjustAttenuation(newAtten,
                                           NewState.GetFinalAttenuation(),
                                          (remainingTime + 500) / 1000,
                                           NewState.GetStepTimeMS());
            break;
        }

        // If necessary, sleep until time for the next step.
        if (MinimumAttenuationSleepTime < sleepTime)
        {
            Sleep(sleepTime);
        }
    }

    return result;
}

// ----------------------------------------------------------------------------
//
// Sets the current attenuation for the RF attenuator by iterating through
// a series of attenuation steps.
// Returns ERROR_SYNC_FOREGROUND_REFRESH_REQUIRED and updates the current
// state after processing the specified number of attenuation steps. 
//
static DWORD
DoStepAttenuator(
          WeinschelController_t *pDevice,
          CommPort_t            *pPort,
          RFAttenuatorState_t   *pState,
    const RFAttenuatorState_t   &NewState,
          RFAttenuatorState_t   *pAsyncState,
          long                  *pAsyncDelay)
{
    DWORD result;

    RFAttenuationStep_t attenSteps[RFAttenuatorState_t::MaximumAttenuationSteps];;
    int                 attenStepCount = COUNTOF(attenSteps);
    NewState.GetAttenuationSteps(attenSteps, &attenStepCount);

#ifdef EXTRA_DEBUG
    LogDebug(TEXT("[AC] Adjusting %s;%s attenuation in following steps:"),
             pDevice->GetDeviceType(),
             pDevice->GetDeviceName());
    int syncSteps = NewState.GetSynchronousSteps();
    if (syncSteps == 0)
    {
        for (int sx = 0 ; sx < attenStepCount ; ++sx)
        {
            LogDebug(TEXT("[AC]   step[%02d]: atten %3ddBs, delay %3dsecs"),
                     sx, attenSteps[sx].StepAttenuation,
                         attenSteps[sx].StepSeconds);
        }
    }
    else
    {
        for (int sx = 0 ; sx < attenStepCount ; ++sx)
        {
            LogDebug(TEXT("[AC]   step[%02d]: atten %3ddBs, delay %3dsecs (%hs)"),
                     sx, attenSteps[sx].StepAttenuation,
                         attenSteps[sx].StepSeconds,
                    (sx < syncSteps)? "synchronous" : "asynchronous");
        }
    }
#endif

    // For each attenuation step...
    int   stopSteps = NewState.GetSynchronousSteps();
    DWORD startTime = GetTickCount();
    long  stepTime  = 0;
    for (int stepNumber = 0 ;; ++stepNumber)
    {
        int newAtten = attenSteps[stepNumber].StepAttenuation;
        stepTime += attenSteps[stepNumber].StepSeconds * 1000;
        
        // Set the attenuation.
        result = DoSetAttenuation(pDevice, pPort, &newAtten);
        if (NO_ERROR != result)
            break;

        // If the cached min/max values have been set, just use those.
        if (0 <= pState->GetMinimumAttenuation() 
         && 0 <= pState->GetMaximumAttenuation())
        {
            pState->SetCurrentAttenuation(newAtten);
        }

        // Otherwise, get all the values.
        else
        {
            result = DoGetAttenuator(pDevice, pPort, pState);
            if (NO_ERROR != result)
                break;
        }

        // Stop if we've reached the desired attenuation setting.
        int stepsLeft = (attenStepCount - stepNumber) - 1;
        if (0 >= stepsLeft)
            break;
        
        // Calculate the next attenuation step delay.
        long runTime = WiFUtils::SubtractTickCounts(GetTickCount(), startTime);
        long sleepTime = stepTime - runTime;
        
        // If we've finished the last synchronous attenuation step, update
        // the current state and return the appropriate code.
        if (--stopSteps == 0)
        {
            result = ERROR_SYNC_FOREGROUND_REFRESH_REQUIRED;
           *pAsyncState = NewState;
           *pAsyncDelay = sleepTime;
            pAsyncState->SetAttenuationSteps(&attenSteps[stepNumber+1], stepsLeft);
            break;
        }

        // If necessary, sleep until time for the next step.
        if (MinimumAttenuationSleepTime < sleepTime)
        {
            Sleep(sleepTime);
        }
    }

    return result;   
}

struct SetAttenuatorProcArgs_t
{
    WeinschelController_t *pDevice;
    RFAttenuatorState_t    NewState;
    long                   InitialDelay;
};

static DWORD WINAPI
SetAttenuatorProc(LPVOID pParameter)
{
    SetAttenuatorProcArgs_t *pArgs = (SetAttenuatorProcArgs_t *)pParameter;

    LogDebug(TEXT("[AC] Starting sub-thread to attenuate %s \"%s\""),
             pArgs->pDevice->GetDeviceType(),
             pArgs->pDevice->GetDeviceName());
    
    // If necessary, delay before starting the attenuation.
    if (0 < pArgs->InitialDelay)
    {
#ifdef EXTRA_DEBUG
        LogDebug(TEXT("[AC] delaying %ldms before starting async attenuation"),
                 pArgs->InitialDelay);
#endif
        Sleep(pArgs->InitialDelay);
    }

    // Attenuate the RF signal.
    RFAttenuatorState_t response;
    DWORD result = pArgs->pDevice->SetAttenuator(pArgs->NewState, &response);

#ifdef EXTRA_DEBUG
    LogDebug(TEXT("[AC] asynchronous attenuation finished: %s"),
             Win32ErrorText(result));
#endif

    // Clean up.
    delete pArgs->pDevice;
    delete pArgs;
    return result;
}

DWORD
WeinschelController_t::
SetAttenuator(
    const RFAttenuatorState_t &NewState,
          RFAttenuatorState_t *pResponse)
{
    RFAttenuatorState_t asyncState;
    long                asyncDelay = 0;
    
    // Connect and lock the comm port.
    DWORD result = Reconnect();
    if (NO_ERROR != result)
        return result;

    ce::gate<ce::critical_section> portLocker(m_pPort->GetLocker());
   
    // Open the comm port (if necessary).
    result = ReOpen(this, m_pPort);
    if (NO_ERROR != result)
        return result;

    // Set the attenuation values.
    result = DoSwitchChannel(this, m_pPort);
    if (NO_ERROR == result)
    {
        if (0 == NewState.GetNumberAttenuationSteps())
        {
            result = DoSetAttenuator(this, m_pPort,
                                     &m_State, NewState,
                                     &asyncState, &asyncDelay);
        }
        else
        {
            result = DoStepAttenuator(this, m_pPort,
                                      &m_State, NewState,
                                      &asyncState, &asyncDelay);
        }
    }

    // If the command finished all the synchronous steps successfully,
    // start a sub-thread to finish the rest asynchronously.
    if (result == ERROR_SYNC_FOREGROUND_REFRESH_REQUIRED)
    {
        SetAttenuatorProcArgs_t *pArgs = new SetAttenuatorProcArgs_t;
        if (NULL == pArgs)
        {
            result = ERROR_OUTOFMEMORY;
            LogError(TEXT("Can't allocate args for %s \"%s\": %s"),
                     GetDeviceType(),
                     GetDeviceName(), 
                     Win32ErrorText(result));
        }
        else
        {
            pArgs->pDevice      = new WeinschelController_t(*this);
            pArgs->NewState     = asyncState;
            pArgs->InitialDelay = asyncDelay;
            pArgs->NewState.SetSynchronousSteps(0);
            
            if (NULL == pArgs->pDevice)
            {
                delete pArgs;
                result = ERROR_OUTOFMEMORY;
                LogError(TEXT("Can't copy device to set attenuator %s \"%s\": %s"),
                         GetDeviceType(),
                         GetDeviceName(), 
                         Win32ErrorText(result));
            }
            else
            {
                HANDLE thred = CreateThread(NULL, 0, SetAttenuatorProc, pArgs, 0, NULL);
                if (NULL != thred)
                {
                    result = NO_ERROR;
                    CloseHandle(thred);
                }
                else
                {
                    delete pArgs->pDevice;
                    delete pArgs;
                    result = GetLastError();
                    LogError(TEXT("Can't create thread to set attenuator %s \"%s\": %s"),
                             GetDeviceType(),
                             GetDeviceName(), 
                             Win32ErrorText(result));
                }
            }
        }
    }

    // Close the port if the operation failed.
    if (NO_ERROR != result)
        m_pPort->Close();
    else
    {
       *pResponse = m_State;       
        pResponse->SetFieldFlags(0);
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
    
    return NO_ERROR;
}

// ----------------------------------------------------------------------------
