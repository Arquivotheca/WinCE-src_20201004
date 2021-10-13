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
#include "CiscoPort_t.hpp"

#include <assert.h>
#include <tchar.h>
#include <strsafe.h>

using namespace ce::qa;

/* ========================================================================= */
/* ============================== CiscoPort ================================ */
/* ========================================================================= */

// ----------------------------------------------------------------------------
//
// Constructor.
//
static TelnetHandle_t *
CreateCiscoHandle(
    const TCHAR *pServerHost,
    DWORD         ServerPort)
{
    return new CiscoHandle_t(pServerHost, ServerPort);
}

CiscoPort_t::
CiscoPort_t(
    const TCHAR *pServerHost,
    DWORD         ServerPort,
    int           RadioIntf)
    : TelnetPort_t(pServerHost, ServerPort, CreateCiscoHandle),
      m_RadioIntf(abs(RadioIntf) % NumberRadioInterfaces),
      m_RadioIntfType(InterfaceTypeRadioG)
{
    assert(0 <= RadioIntf && RadioIntf < NumberRadioInterfaces);
    StringCchPrintfA(m_Ssid, COUNTOF(m_Ssid), "CiscoRadio%d", RadioIntf);
}

// ----------------------------------------------------------------------------
//
// Destructor.
//
CiscoPort_t::
~CiscoPort_t(void)
{
    // Nothing to do.
}

// ----------------------------------------------------------------------------
//
// Gets or sets the current SSID:
//
void
CiscoPort_t::
SetSsid(const char *pNewSsid)
{
    if (NULL == pNewSsid || '\0' == pNewSsid[0])
    {
        assert(pNewSsid && pNewSsid[0]);
        return;
    }
    
    SafeCopy(m_Ssid, pNewSsid, COUNTOF(m_Ssid));
}

// ----------------------------------------------------------------------------
//
// Gets or changes the current command mode:
//
CiscoPort_t::CommandMode_e
CiscoPort_t::
GetCommandMode(void) const
{
    if (NULL == m_pHandle)
        return CommandModeUserExec;

    return ((CiscoHandle_t *)m_pHandle)->GetCommandMode();
}

DWORD
CiscoPort_t::
SetCommandMode(
    CiscoPort_t::CommandMode_e NewMode)
{
    if (NULL == m_pHandle)
        return ERROR_INVALID_HANDLE;

    return ((CiscoHandle_t *)m_pHandle)->SetCommandMode(NewMode, m_RadioIntf, m_Ssid);
}

// ----------------------------------------------------------------------------
//
// Retrieves the device's current running configuration:
//
DWORD
CiscoPort_t::
ReadRunningConfig(void)
{
    if (NULL == m_pHandle)
        return ERROR_INVALID_HANDLE;

    return ((CiscoHandle_t *)m_pHandle)->ReadRunningConfig();
}

TelnetLines_t *
CiscoPort_t::
GetRunningConfig(void)
{
    if (NULL == m_pHandle)
        return NULL;

    return ((CiscoHandle_t *)m_pHandle)->GetRunningConfig();
}

// ----------------------------------------------------------------------------
//
// Sends the specified command and retrieves the response (if any):
// If necessary, changes to the specified command-mode before sending
// the command.
//
DWORD
CiscoPort_t::
SendCommand(
    const char    *pFormat, ...)
{
    if (NULL == m_pHandle)
        return ERROR_INVALID_HANDLE;
    CiscoHandle_t *pHandle = (CiscoHandle_t *)m_pHandle;

    va_list pArgList;
    va_start(pArgList, pFormat);
    DWORD result = pHandle->SendCommandV(pHandle->GetCommandMode(),
                                         m_RadioIntf,
                                         m_Ssid,
                                         pFormat,
                                         pArgList);
    va_end(pArgList);
    return result;
}

DWORD
CiscoPort_t::
SendCommand(
    CiscoPort_t::CommandMode_e CommandMode,
    const char                *pFormat, ...)
{
    if (NULL == m_pHandle)
        return ERROR_INVALID_HANDLE;
    CiscoHandle_t *pHandle = (CiscoHandle_t *)m_pHandle;

    va_list pArgList;
    va_start(pArgList, pFormat);
    DWORD result = pHandle->SendCommandV(CommandMode,
                                         m_RadioIntf,
                                         m_Ssid,
                                         pFormat,
                                         pArgList);
    va_end(pArgList);
    return result;
}

DWORD
CiscoPort_t::
SendCommandV(
    CiscoPort_t::CommandMode_e CommandMode,
    const char                *pFormat,
    va_list                    pArgList,
    long                       MaxWaitTimeMS /* = TelnetPort_t::DefaultReadTimeMS */)
{
    if (NULL == m_pHandle)
        return ERROR_INVALID_HANDLE;
    CiscoHandle_t *pHandle = (CiscoHandle_t *)m_pHandle;

    return pHandle->SendCommandV(CommandMode,
                                 m_RadioIntf,
                                 m_Ssid,
                                 pFormat,
                                 pArgList,
                                 MaxWaitTimeMS);
}

TelnetLines_t *
CiscoPort_t::
GetCommandResponse(void)
{
    if (NULL == m_pHandle)
        return NULL;

    return ((CiscoHandle_t *)m_pHandle)->GetCommandResponse();
}

// ----------------------------------------------------------------------------
//
// Checks the server command-response for an error message:
// (Any response is assumed to be an error message.)
// If appropriate, issues an error message containing the specified
// operation name and the error message sent by the server.
//
static DWORD
VerifyCommandResponse(
    TelnetLines_t *pResponse,
    const char    *pOperation)
{
    char *pErrorMsg = const_cast<char *>(pResponse->GetText());

    // Until we find there's an actual error message...
    while (pErrorMsg && pErrorMsg[0])
    {
        // Remove leading spaces.
        while (pErrorMsg[0] == ' ')
             ++pErrorMsg;

        // If it's a "pointer" ignore the line.
        if (pErrorMsg[0] == '^')
        {
            pErrorMsg++;
            while (pErrorMsg[0] == ' ')
                 ++pErrorMsg;
            while (pErrorMsg[0])
                if (pErrorMsg[0] == '\r' && pErrorMsg[1] == '\n')
                    break;
            if (pErrorMsg[0] == '\r' && pErrorMsg[1] == '\n')
                pErrorMsg += 2;
            continue;
        }
        
        // Remove everything after the newline.
        char *pErrorEnd = pErrorMsg;
        for (; pErrorEnd[0] ; ++pErrorEnd)
            if (pErrorEnd[0] == '\r' && pErrorEnd[1] == '\n')
            {
                pErrorEnd[0] = '\0';
                break;
            }

        if (pErrorEnd != pErrorMsg)
        {
            LogError(TEXT("%hs failed: %hs"), pOperation, pErrorMsg);
            return ERROR_INVALID_COMMAND_LINE;
        }
        break;
    }

    return NO_ERROR;
}

// ----------------------------------------------------------------------------
//
// Sends the specified command checks the response for an error message:
// (Any response is assumed to be an error message.)
// If necessary, changes to the specified command-mode before sending
// the command.
// If appropriate, issues an error message containing the specified
// operation name and the error message sent by the server.
//
DWORD
CiscoPort_t::
VerifyCommand(
    const char *pOperation,
    const char *pFormat, ...)
{
    if (NULL == m_pHandle)
        return ERROR_INVALID_HANDLE;
    CiscoHandle_t *pHandle = (CiscoHandle_t *)m_pHandle;
   
    va_list pArgList;
    va_start(pArgList, pFormat);
    DWORD result = pHandle->SendCommandV(pHandle->GetCommandMode(),
                                         m_RadioIntf,
                                         m_Ssid,
                                         pFormat,
                                         pArgList);
    va_end(pArgList);

    if (NO_ERROR == result)
        result = VerifyCommandResponse(pHandle->GetCommandResponse(),
                                       pOperation);
    return result;
}

DWORD
CiscoPort_t::
VerifyCommand(
    CiscoPort_t::CommandMode_e CommandMode,
    const char                *pOperation,
    const char                *pFormat, ...)
{
    if (NULL == m_pHandle)
        return ERROR_INVALID_HANDLE;
    CiscoHandle_t *pHandle = (CiscoHandle_t *)m_pHandle;
    
    va_list pArgList;
    va_start(pArgList, pFormat);
    DWORD result = pHandle->SendCommandV(CommandMode,
                                         m_RadioIntf,
                                         m_Ssid,
                                         pFormat,
                                         pArgList);
    va_end(pArgList);

    if (NO_ERROR == result)
        result = VerifyCommandResponse(pHandle->GetCommandResponse(),
                                       pOperation);
    return result;
}

DWORD
CiscoPort_t::
VerifyCommandV(
    CiscoPort_t::CommandMode_e CommandMode,
    const char                *pOperation,
    const char                *pFormat,
    va_list                    pArgList,
    long                       MaxWaitTimeMS /* = TelnetPort_t::DefaultReadTimeMS */)
{
    if (NULL == m_pHandle)
        return ERROR_INVALID_HANDLE;
    CiscoHandle_t *pHandle = (CiscoHandle_t *)m_pHandle;

    DWORD result = pHandle->SendCommandV(CommandMode,
                                         m_RadioIntf,
                                         m_Ssid,
                                         pFormat,
                                         pArgList,
                                         MaxWaitTimeMS);

    if (NO_ERROR == result)
        result = VerifyCommandResponse(pHandle->GetCommandResponse(),
                                       pOperation);
    return result;
}

// ----------------------------------------------------------------------------
//
// Gets the count of global SSIDs:
//
int
CiscoPort_t::
GetNumberGlobalSsids(void) const
{
    if (NULL == m_pHandle)
        return 0;

    return ((const CiscoHandle_t *)m_pHandle)->GetNumberGlobalSsids();
}

// ----------------------------------------------------------------------------
//
// Adds or deletes the specified global SSID:
//
DWORD
CiscoPort_t::
AddGlobalSsid(
    const char *pSsid)
{
    if (NULL == pSsid || '\0' == pSsid[0])
    {
        assert(pSsid && pSsid[0]);
        return ERROR_INVALID_PARAMETER;
    }
    
    if (NULL == m_pHandle)
        return ERROR_INVALID_HANDLE;

    return ((CiscoHandle_t *)m_pHandle)->AddGlobalSsid(pSsid);
}

DWORD
CiscoPort_t::
DeleteGlobalSsid(
    const char *pSsid)
{
    if (NULL == pSsid || '\0' == pSsid[0])
    {
        assert(pSsid && pSsid[0]);
        return ERROR_INVALID_PARAMETER;
    }
    
    if (NULL == m_pHandle)
        return ERROR_INVALID_HANDLE;

    return ((CiscoHandle_t *)m_pHandle)->DeleteGlobalSsid(pSsid);
}

// ----------------------------------------------------------------------------
//
// Looks up the specified global SSID by name or number:
//
CiscoSsid_t *
CiscoPort_t::
GetGlobalSsid(int IX)
{
    if (NULL == m_pHandle)
        return NULL;

    return ((CiscoHandle_t *)m_pHandle)->GetGlobalSsid(IX);
}

const CiscoSsid_t *
CiscoPort_t::
GetGlobalSsid(int IX) const
{
    if (NULL == m_pHandle)
        return NULL;

    return ((const CiscoHandle_t *)m_pHandle)->GetGlobalSsid(IX);
}

CiscoSsid_t *
CiscoPort_t::
GetGlobalSsid(const char *pSsid)
{
    if (NULL == m_pHandle)
        return NULL;

    return ((CiscoHandle_t *)m_pHandle)->GetGlobalSsid(pSsid);
}

const CiscoSsid_t *
CiscoPort_t::
GetGlobalSsid(const char *pSsid) const
{
    if (NULL == m_pHandle)
        return NULL;

    return ((const CiscoHandle_t *)m_pHandle)->GetGlobalSsid(pSsid);
}

/* ========================================================================= */
/* ============================== CiscoSsid ================================ */
/* ========================================================================= */

// ----------------------------------------------------------------------------
//
// Constructor:
//
CiscoSsid_t::
CiscoSsid_t(const char *pSsid)
{
    memset(this, 0, sizeof(*this));
    SafeCopy(m_Ssid, pSsid, COUNTOF(m_Ssid));
}

// ----------------------------------------------------------------------------
//
// Associates or disassociates the global SSID to/from a radio interface:
// The Disassociate method will optionally retrieve the count of radio 
// interfaces still associated with the global SSID.
//
DWORD
CiscoSsid_t::
AssociateRadioInterface(
    int RadioIntf)
{
    if (!(0 <= RadioIntf && RadioIntf < CiscoPort_t::NumberRadioInterfaces))
    {
        assert(0 <= RadioIntf && RadioIntf < CiscoPort_t::NumberRadioInterfaces);
        return ERROR_INVALID_PARAMETER;
    }
    
    m_Interface[RadioIntf] = 1;
    return NO_ERROR;
}

DWORD
CiscoSsid_t::
DisassociateRadioInterface(
    int  RadioIntf,
    int *pNumberAssociations /* = NULL */)
{
    if (!(0 <= RadioIntf && RadioIntf < CiscoPort_t::NumberRadioInterfaces))
    {
        assert(0 <= RadioIntf && RadioIntf < CiscoPort_t::NumberRadioInterfaces);
        return ERROR_INVALID_PARAMETER;
    }
    
    m_Interface[RadioIntf] = 0;

    if (pNumberAssociations)
    {
        int numberLeft = 0;
        for (int ix = 0 ; ix < CiscoPort_t::NumberRadioInterfaces ; ++ix)
            if (m_Interface[ix] != 0)
                numberLeft++;
       *pNumberAssociations = numberLeft;
    }

    return NO_ERROR;
}

// ----------------------------------------------------------------------------
//
// Determines whether the global SSID is associated with the specified
// radio interface:
//
bool
CiscoSsid_t::
IsAssociated(
    int RadioIntf) const
{
    if (!(0 <= RadioIntf && RadioIntf < CiscoPort_t::NumberRadioInterfaces))
    {
        assert(0 <= RadioIntf && RadioIntf < CiscoPort_t::NumberRadioInterfaces);
        return false;
    }

    return m_Interface[RadioIntf] != 0;
}

// ----------------------------------------------------------------------------
