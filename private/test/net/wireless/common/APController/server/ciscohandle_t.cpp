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
// Implementation of the CiscoPort_t class.
//
// ----------------------------------------------------------------------------

#include "CiscoHandle_t.hpp"

#include <assert.h>
#include <intsafe.h>
#include <tchar.h>
#include <strsafe.h>

#include <auto_xxx.hxx>

using namespace ce::qa;


// ----------------------------------------------------------------------------
//
// List of the Cisco CLI command-mode prompts we care about:
//
struct CommandModePrompt_t
{
    CiscoPort_t::CommandMode_e Mode;
    int                        Depth;
    const char               *pExpect;
    size_t                     ExpectChars;
    const char               *pExpectName;
};
static const CommandModePrompt_t s_CommandModePrompts[] =
{
    // Note that the order of this table must exactly
    // match the CommandMode_e enums.

    { CiscoPort_t::CommandModeUserExec, 0,
      ">", 1,
      "user-exec-mode command prompt" },

    { CiscoPort_t::CommandModePrivileged, 1,
      "#", 1,
      "privileged-mode command prompt" },

    { CiscoPort_t::CommandModeConfig, 2,
      "(config)#", 9,
      "Configuration-mode command prompt" },

    { CiscoPort_t::CommandModeInterface, 3,
      "(config-if)#", 12,
      "interface-mode command prompt" },

    { CiscoPort_t::CommandModeRadius, 3,
      "(config-sg-radius)#", 19,
      "radius-server-group command prompt" },

    { CiscoPort_t::CommandModeSsid, 3,
      "(config-ssid)#", 14,
      "SSID-mode command prompt" },
};
static const int s_NumberCommmandModePrompts = COUNTOF(s_CommandModePrompts);


// ----------------------------------------------------------------------------
//
// Constructor.
//
CiscoHandle_t::
CiscoHandle_t(
    const TCHAR *pServerHost,
    DWORD         ServerPort)
    : TelnetHandle_t(pServerHost, ServerPort),
      m_CommandMode(CiscoPort_t::CommandModeUserExec),
      m_RadioIntf(0),
      m_NumberGlobalSsids(0)
{
    m_Ssid[0] = '\0';

    // Insert default Cisco credentials.
    SetUserName(TEXT("Cisco"));
    SetPassword(TEXT("Cisco"));
}

// ----------------------------------------------------------------------------
//
// Destructor.
//
CiscoHandle_t::
~CiscoHandle_t(void)
{
    // nothing to do
}

// ----------------------------------------------------------------------------
//
// Connects to the device's telnet-server to access the specified page.
//
DWORD
CiscoHandle_t::
Connect(long MaxWaitTimeMS /* = TelnetPort_t::DefaultConnectTimeMS */)
{
    return TelnetHandle_t::Connect(MaxWaitTimeMS);
}

// ----------------------------------------------------------------------------
//
// Disconnects the current connection.
//
void
CiscoHandle_t::
Disconnect(void)
{
    TelnetHandle_t::Disconnect();
    m_CommandMode = CiscoPort_t::CommandModeUserExec;
    m_RadioIntf = 0;
    m_Ssid[0] = '\0';
}

// ----------------------------------------------------------------------------
//
// Changes the current command mode:
//
DWORD
CiscoHandle_t::
SetCommandMode(
    CiscoPort_t::CommandMode_e NewMode,
    int                        RadioIntf,
    const char                *pSsid)
{
    DWORD result = NO_ERROR;
    
    char msgBuff[256];
    const char *pMessage = "";

    ce::string response;
    int reconnects = 0,
        modeResets = 0;
    while (NO_ERROR == result)
    {
        const CommandModePrompt_t *pOldPrompt;
        const CommandModePrompt_t *pNewPrompt;
        pOldPrompt = &s_CommandModePrompts[(int)m_CommandMode];
        pNewPrompt = &s_CommandModePrompts[(int)NewMode];

        // Check to make sure we're in the mode we think.
        // After the first time, this will usualy send a mode-change
        // command then wait for the new prompt.
        result = SendExpect(pOldPrompt->pExpect,
                            pOldPrompt->ExpectChars,
                            pOldPrompt->pExpectName,
                           &response,
                            pMessage);

        // If we got a different kind of prompt than we expected,
        // see if we can determine which mode we're actually in.
        // If not, disconnect, re-connect and try again.
        // Only do this a few times to avoid a loop.
        if (NO_ERROR != result)
        {
            if (ERROR_NOT_FOUND != result)
                break;
            
            static const int MaxModeResets = 3;
            if (++modeResets < MaxModeResets)
            {
                int modeIX = s_NumberCommmandModePrompts;
                while (--modeIX >= 0)
                {
                    pNewPrompt = &s_CommandModePrompts[modeIX];
                    if (response.length() >= pNewPrompt->ExpectChars
                     && _strnicmp(&response[response.length() - pNewPrompt->ExpectChars],
                                   pNewPrompt->pExpect,
                                   pNewPrompt->ExpectChars) == 0)
                        break;
                }

                if (modeIX >= 0)
                {
                    m_CommandMode = pNewPrompt->Mode;
                    result = NO_ERROR;
                    pMessage = "";
                    continue;
                }
            }

            static const int MaxReconnections = 2;
            if (++reconnects < MaxReconnections)
            {
                Disconnect();
                result = Connect();
                pMessage = "";
                continue;
            }

            LogError(TEXT("Received incorrect prompt \"%hs\"")
                     TEXT(" from %s:%lu - expected \"%hs\""),
                    &response[0],
                     GetServerHost(),
                     GetServerPort(),
                     pOldPrompt->pExpectName);
            break;
        }

        // Compare the depths of the old and new modes. If the modes are
        // the same depth in the hierarchy, make sure they're the same
        // modes. If not, ascend the hierarchy and come back down another
        // leg of the tree.
        int direction = pNewPrompt->Depth - pOldPrompt->Depth;
        if (direction == 0 && m_CommandMode != NewMode)
            direction = -1;
 
        // If we're already in the correct mode, check to make sure the
        // mode's parameters (if any) are correct.
        if (direction == 0)
        {
            switch (m_CommandMode)
            {
            case CiscoPort_t::CommandModeInterface:  // Interface config mode
                if (m_RadioIntf == RadioIntf)
                    return NO_ERROR;
                break;

            case CiscoPort_t::CommandModeSsid:       // SSID config mode
                if (strncmp(m_Ssid, pSsid, CiscoPort_t::MaxSsidChars) == 0)
                    return NO_ERROR;
                break;

            default:
                return NO_ERROR;
            }

            // Ascend to the next-higher mode and come back down with the
            // correct parameters.
            direction = -1;
        }

        pMessage = "";
        switch (m_CommandMode)
        {
        case CiscoPort_t::CommandModeUserExec:   // User EXEC mode
            if (direction < 0)
            {
                assert(!"can't ascend past highest level");
                return ERROR_INVALID_STATE;
            }
            else
            {
                pMessage = msgBuff;
                StringCchPrintfA(msgBuff, COUNTOF(msgBuff), "enable\r\n%ls", GetPassword());
                m_CommandMode = CiscoPort_t::CommandModePrivileged;
            }
            break;

        case CiscoPort_t::CommandModePrivileged: // Privileged EXEC mode
            if (direction < 0)
            {
                pMessage = "disable";
                m_CommandMode = CiscoPort_t::CommandModeUserExec;
            }
            else
            {
                pMessage = "configure terminal";
                m_CommandMode = CiscoPort_t::CommandModeConfig;
            }
            break;

        case CiscoPort_t::CommandModeConfig:     // Global config mode
            if (direction < 0)
            {
                pMessage = "exit";
                m_CommandMode = CiscoPort_t::CommandModePrivileged;
            }
            else
            {
                // The interface, radius and SSID modes are parallel
                // in the hierarchy tree. (I.e., you can't get to SSID
                // from interface, but have to ascend to config mode
                // first.) Hence, we have multiple ways to descend from
                // config mode; interface, radius, and SSID.
                if (NewMode == CiscoPort_t::CommandModeInterface)
                {
                    m_RadioIntf = RadioIntf;
                    StringCchPrintfA(msgBuff, COUNTOF(msgBuff), "interface dot11radio %d", RadioIntf);
                    pMessage = msgBuff;
                    m_CommandMode = CiscoPort_t::CommandModeInterface;
                }
                else
                if (NewMode == CiscoPort_t::CommandModeRadius)
                {
                    pMessage = "aaa new-model" "\r\n"
                               "aaa group server radius rad_eap";
                    m_CommandMode = CiscoPort_t::CommandModeRadius;
                }
                else
                {
                    SafeCopy(m_Ssid, pSsid, COUNTOF(m_Ssid));
                    StringCchPrintfA(msgBuff, COUNTOF(msgBuff), "dot11 ssid %hs", pSsid);
                    pMessage = msgBuff;
                    m_CommandMode = CiscoPort_t::CommandModeSsid;
                }
            }
            break;

        case CiscoPort_t::CommandModeInterface:  // Interface config mode
        case CiscoPort_t::CommandModeRadius:     // Radius server group mode
        case CiscoPort_t::CommandModeSsid:       // SSID config mode
            if (direction < 0)
            {
                pMessage = "exit";
                m_CommandMode = CiscoPort_t::CommandModeConfig;
            }
            else
            {
                assert(!"can't descend past lowest level" );
                return ERROR_INVALID_STATE;
            }
            break;
        }
    }

    return result;
}

// ----------------------------------------------------------------------------
//
// Retrieves the device's current running configuration:
//
DWORD
CiscoHandle_t::
ReadRunningConfig(void)
{
    DWORD result = SendCommand(CiscoPort_t::CommandModePrivileged,
                               m_RadioIntf, m_Ssid,
                              "show running-config",
                              &m_RunningConfig,
                               TelnetPort_t::DefaultReadTimeMS);
    if (NO_ERROR != result)
    {
        LogError(TEXT("Can't read running configuration from %s:%lu: %s"),
                 GetServerHost(), GetServerPort(),
                 Win32ErrorText(result));
        return result;
    }

    return NO_ERROR;
}

// ----------------------------------------------------------------------------
//
// Sends the specified command and retrieves the response (if any):
// If necessary, changes to the specified command-mode before sending
// the command.
//
DWORD
CiscoHandle_t::
SendCommandV(
    CiscoPort_t::CommandMode_e CommandMode,
    int                        RadioIntf,
    const char                *pSsid,
    const char                *pFormat,
    va_list                    pArgList,
    long                       MaxWaitTimeMS /* = TelnetPort_t::DefaultReadTimeMS */)
{
    // Format the command.
    char command[1024];
  __try
    {
        HRESULT hr = StringCchVPrintfExA(command, COUNTOF(command),
                                         NULL, NULL, STRSAFE_IGNORE_NULLS,
                                         pFormat, pArgList);
        if (FAILED(hr))
        {
            LogError(TEXT("Can't format command \"%.64hs...\" for %s:%lu: %s"),
                     pFormat, GetServerHost(), GetServerPort(),
                     HRESULTErrorText(hr));
            return HRESULT_CODE(hr);
        }
    }
  __except(1)
    {
        LogError(TEXT("Exception formatting \"%.64hs...\" for %s:%lu"),
                 pFormat, GetServerHost(), GetServerPort());
        return ERROR_INVALID_PARAMETER;
    }

    // Send the command to the server.
    return SendCommand(CommandMode,
                       RadioIntf,
                       pSsid,
                       command,
                      &m_Response,
                       MaxWaitTimeMS);
}

// ----------------------------------------------------------------------------
//
// Sends the specified command and retrieves the response (if any):
// If necessary, changes to the specified command-mode before sending
// the command.
//
DWORD
CiscoHandle_t::
SendCommand(
    CiscoPort_t::CommandMode_e CommandMode,
    int                        RadioIntf,
    const char                *pSsid,
    const char                *pCommand,
    TelnetLines_t             *pResponse,
    long                       MaxWaitTimeMS)
{
    DWORD result;
    
    // In we're already in the desired command mode, flush the I/O buffers
    // before sending the command. Otherwise, change the command mode.
    if (m_CommandMode == CommandMode
     && m_RadioIntf == RadioIntf
     && strncmp(m_Ssid, pSsid, CiscoPort_t::MaxSsidChars) == 0)
    {
        Flush();
    }
    else
    {
        result = SetCommandMode(CommandMode, RadioIntf, pSsid);
        if (NO_ERROR != result)
            return result;
    }

    // Determine the expected command-prompt response.
    const CommandModePrompt_t *pExpectPrompt;
    pExpectPrompt = &s_CommandModePrompts[(int)m_CommandMode];

    // Send the command and wait for a response.
    ce::string serverPrompt;
    result = SendExpect(pExpectPrompt->pExpect,
                        pExpectPrompt->ExpectChars,
                        pExpectPrompt->pExpectName,
                       &serverPrompt,
                        pCommand,
                        pResponse,
                        MaxWaitTimeMS);

    // Make sure we got back the correct command-prompt.
    if (ERROR_NOT_FOUND == result)
    {
        LogError(TEXT("Received incorrect prompt \"%hs\"")
                 TEXT(" from %s:%lu - expected \"%hs\""),
                &serverPrompt[0],
                 GetServerHost(),
                 GetServerPort(),
                 pExpectPrompt->pExpectName);
        result = ERROR_INVALID_STATE;
    }

    return result;
}

// ----------------------------------------------------------------------------
//
// Adds or deletes the specified global SSID:
//
DWORD
CiscoHandle_t::
AddGlobalSsid(
    const char *pSsid)
{
    HRESULT hr;
    DWORD result = NO_ERROR;
    DWORD newSize, capacity;

    // Look up the SSID in case it's already there.
    CiscoSsid_t *pGlobalSsid = GetGlobalSsid(pSsid);
    if (pGlobalSsid)
        return NO_ERROR;

    // Grow the list capacity in multiples of 8 nodes.
    newSize = (m_NumberGlobalSsids + 0x08) & ~0x7;

    hr = DWordMult(newSize, sizeof(CiscoSsid_t), &capacity);
    if (FAILED(hr))
        result = HRESULT_CODE(hr);
    else
    if (capacity > m_GlobalSsids.Capacity())
        result = m_GlobalSsids.Reserve(capacity);

    if (NO_ERROR == result)
    {
        CiscoSsid_t *pSsids = (CiscoSsid_t *)(m_GlobalSsids.GetPrivate());
        pGlobalSsid = &pSsids[m_NumberGlobalSsids];
        new(pGlobalSsid) CiscoSsid_t(pSsid);
        m_NumberGlobalSsids++;
    }
    else
    {
        LogError(TEXT("Can't allocate %lu bytes for global-SSID list"), capacity);
    }

    return result;
}

DWORD
CiscoHandle_t::
DeleteGlobalSsid(
    const char *pSsid)
{
    CiscoSsid_t *pGlobalSsid = GetGlobalSsid(pSsid);
    if (pGlobalSsid)
    {
        CiscoSsid_t *pSsids = (CiscoSsid_t *)(m_GlobalSsids.GetPrivate());
        CiscoSsid_t *pSsidsEnd = &pSsids[m_NumberGlobalSsids];
        for (pSsids = pGlobalSsid ; ++pGlobalSsid < pSsidsEnd ;)
           *(pSsids++) = *pGlobalSsid;
        m_NumberGlobalSsids--;
    }
    return NO_ERROR;
}

// ----------------------------------------------------------------------------
//
// Looks up a global SSID by number or name:
//
CiscoSsid_t *
CiscoHandle_t::
GetGlobalSsid(int IX)
{
    if (!(0 <= IX && IX < m_NumberGlobalSsids))
    {
        assert(0 <= IX && IX < m_NumberGlobalSsids);
        return NULL;
    }
    
    CiscoSsid_t *pSsids = (CiscoSsid_t *)(m_GlobalSsids.GetShared());
    return &pSsids[IX];
}

const CiscoSsid_t *
CiscoHandle_t::
GetGlobalSsid(int IX) const
{
    if (!(0 <= IX && IX < m_NumberGlobalSsids))
    {
        assert(0 <= IX && IX < m_NumberGlobalSsids);
        return NULL;
    }
    
    const CiscoSsid_t *pSsids = (const CiscoSsid_t *)(m_GlobalSsids.GetShared());
    return &pSsids[IX];
}

CiscoSsid_t *
CiscoHandle_t::
GetGlobalSsid(const char *pSsid)
{
    CiscoSsid_t *pSsids = (CiscoSsid_t *)(m_GlobalSsids.GetPrivate());
    for (int gx = 0 ; gx < m_NumberGlobalSsids ; ++gx)
    {
        if (strncmp(pSsids[gx].m_Ssid, pSsid, CiscoPort_t::MaxSsidChars) == 0)
            return &pSsids[gx];
    }
    return NULL;
}

const CiscoSsid_t *
CiscoHandle_t::
GetGlobalSsid(const char *pSsid) const
{
    const CiscoSsid_t *pSsids = (const CiscoSsid_t *)(m_GlobalSsids.GetShared());
    for (int gx = 0 ; gx < m_NumberGlobalSsids ; ++gx)
    {
        if (strncmp(pSsids[gx].m_Ssid, pSsid, CiscoPort_t::MaxSsidChars) == 0)
            return &pSsids[gx];
    }
    return NULL;
}

// ----------------------------------------------------------------------------
