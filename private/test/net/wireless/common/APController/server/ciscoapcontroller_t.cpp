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
// Implementation of the CiscoAPController_t class.
//
// ----------------------------------------------------------------------------

#include "CiscoAPController_t.hpp"

#include "AccessPointState_t.hpp"
#include "Cisco1200Controller_t.hpp"

#include <assert.h>
#include <tchar.h>
#include <strsafe.h>

#ifdef _PREFAST_
#pragma warning(disable:26020)
#else
#pragma warning(disable:4068)
#endif

using namespace ce::qa;


/* ========================================================================= */
/* ========================== CiscoAPController ============================ */
/* ========================================================================= */

// ----------------------------------------------------------------------------
//
// Constructor.
//
CiscoAPController_t::
CiscoAPController_t(
    const TCHAR *pDeviceType,
    const TCHAR *pDeviceName)
    : AccessPointController_t(pDeviceType, pDeviceName),
      m_pCLI(NULL),
      m_Intf(-1),   // defaults to all intfs
      m_pType(NULL)
{
    // Parse the device-name.
    ce::tstring lDeviceName(pDeviceName);
    const TCHAR *pHost, *pIntf, *pUser, *pPass;
    pHost = pIntf = pUser = pPass = NULL;
    TCHAR *nextToken = NULL;  
    
    for (TCHAR *token = _tcstok_s(lDeviceName.get_buffer(), TEXT(":"),&nextToken) ;
                token != NULL ;
                token = _tcstok_s(NULL, TEXT(":"), &nextToken))
    {
        if      (NULL == pHost) { pHost = WiFUtils::TrimToken(token); }
        else if (NULL == pIntf) { pIntf = WiFUtils::TrimToken(token); }
        else if (NULL == pUser) { pUser = WiFUtils::TrimToken(token); }
        else                    { pPass = WiFUtils::TrimToken(token); break; }
    }

    if (NULL == pHost) pHost = TEXT("missing");

    // Parse the interface number.
    int intf = 0;   // defaults to B/G intf
    if (NULL != pIntf)
    {
        ULONG ulValue = ULONG_MAX;
        if (pIntf[1] == TEXT('\0') && (pIntf[0] == TEXT('A')))
            m_Intf = intf = 1;   // assumes intf 1 is an A-band radio
        else
        if (pIntf[1] == TEXT('\0') && (pIntf[0] == TEXT('B')
                                    || pIntf[0] == TEXT('G')))
            m_Intf = intf = 0;   // assumes intf 0 is a B/G-band radio
        else
        if (_istdigit(pIntf[0]))
        {
            TCHAR *pEnd = NULL;
            ulValue = _tcstoul(pIntf, &pEnd, 10);
            if (pEnd && !pEnd[0] && ulValue < CiscoPort_t::NumberRadioInterfaces)
                m_Intf = intf = (int)ulValue;
            else
            {
                ulValue = ULONG_MAX;
                if (pPass)
                {
                    LogError(TEXT("Bad interface number \"%s\" in Cisco device-name \"%s\""),
                             pIntf, pDeviceName);
                }
            }
        }

        // If it's actually the user-name, shift it.
        if (ulValue >= 10 && !pPass)
        {
            pPass = pUser;
            pUser = pIntf;
            pIntf = NULL;
        }
    }

    // Allocate the telnet/CLI controller.
    m_pCLI = new CiscoPort_t(pHost, TelnetPort_t::DefaultTelnetPort, intf);
    if (NULL == m_pCLI)
    {
        LogError(TEXT("Can't allocate telnet interface for \"%s:%lu\""),
                 pHost, CiscoPort_t::DefaultTelnetPort);
        assert(NULL != m_pCLI);
    }

    // Insert the admin user and password (if any).
    else
    if (NULL != pUser || NULL != pPass)
    {
        ce::gate<ce::critical_section> locker(m_pCLI->GetLocker());
        if (NULL != pUser) m_pCLI->SetUserName(pUser);
        if (NULL != pPass) m_pCLI->SetPassword(pPass);
    }
}

// ----------------------------------------------------------------------------
//
// Destructor.
//
CiscoAPController_t::
~CiscoAPController_t(void)
{
    if (NULL != m_pCLI)
    {
        delete m_pCLI;
        m_pCLI = NULL;
    }
    if (NULL != m_pType)
    {
        delete m_pType;
        m_pType = NULL;
    }
}

// ----------------------------------------------------------------------------
//
// Provides an object for initializing the device's state:
//
void
CiscoAPController_t::
SetInitialState(
    const AccessPointState_t &InitialState)
{
    m_State = InitialState;
}

// ----------------------------------------------------------------------------
//
// Gets the current Access Point configuration settings.
//
DWORD
CiscoAPController_t::
GetAccessPoint(
    AccessPointState_t *pResponse)
{
    DWORD result;
    if (NULL == m_pCLI)
        return ERROR_OUTOFMEMORY;

    // Lock the HTTP connection.
    ce::gate<ce::critical_section> locker(m_pCLI->GetLocker());

    // Capture error messages.
    ce::tstring capturedErrors;
    ErrorLock errorLock(&capturedErrors);

    // Do this a few times in case the telnet interaction fails.
    static const int MaxLoadSettingsErrors = 2;
    for (int tries = 1 ;; ++tries)
    {
        capturedErrors.clear();

        // (Re)connect the telnet interface.
        result = m_pCLI->Connect();
        if (NO_ERROR != result)
            break;

        // Create and/or retrieve the type controller.
        CiscoAPTypeController_t *pType = NULL;
        result = CreateTypeController(&pType);
        if (NO_ERROR != result)
            break;

        // Use the type controller to load the configuration.
        result = pType->LoadSettings();
        if (NO_ERROR == result)
        {
           *pResponse = m_State;
            break;
        }

        // Try resetting the device one time.
        if (tries >= MaxLoadSettingsErrors)
            break;
        else
        {
            LogWarn(TEXT("Resetting after error: %s"), &capturedErrors[0]);
            if (NO_ERROR != pType->ResetDevice())
                LogWarn(TEXT("AP reset failed: err=%s"), &capturedErrors[0]);
        }
    }

    // Log the captured errors (if any).
    errorLock.StopCapture();
    if (capturedErrors.length() > 0)
    {
        if (NO_ERROR == result)
             LogWarn (capturedErrors);
        else LogError(capturedErrors);
    }

    return result;
}

// ----------------------------------------------------------------------------
//
// Updates the Access Point configuration settings.
//
DWORD
CiscoAPController_t::
SetAccessPoint(
    const AccessPointState_t &NewState,
          AccessPointState_t *pResponse)
{
    DWORD result;
    if (NULL == m_pCLI)
        return ERROR_OUTOFMEMORY;

    // Lock the HTTP connection.
    ce::gate<ce::critical_section> locker(m_pCLI->GetLocker());

    // Capture error messages.
    ce::tstring capturedErrors;
    ErrorLock errorLock(&capturedErrors);

    // Do this a few times in case the telnet interaction fails.
    static const int MaxUpdateSettingsErrors = 3;
    for (int tries = 1 ;; ++tries)
    {
        capturedErrors.clear();

        // (Re)connect the telnet interface.
        result = m_pCLI->Connect();
        if (NO_ERROR != result)
            break;

        // Create and/or retrieve the type controller.
        CiscoAPTypeController_t *pType = NULL;
        result = CreateTypeController(&pType);
        if (NO_ERROR != result)
            break;

        // Use the type controller to update the configuration.
        result = pType->UpdateSettings(NewState);
        if (NO_ERROR == result)
        {
           *pResponse = m_State;
            break;
        }

        // Try resetting the device a few times.
        if (tries >= MaxUpdateSettingsErrors)
            break;
        else
        {
            LogWarn(TEXT("Resetting after error: %s"), &capturedErrors[0]);
            if (NO_ERROR != pType->ResetDevice())
                LogWarn(TEXT("AP reset failed: err=%s"), &capturedErrors[0]);
        }
    }

    // Log the captured errors (if any).
    errorLock.StopCapture();
    if (capturedErrors.length() > 0)
    {
        if (NO_ERROR == result)
             LogWarn (capturedErrors);
        else LogError(capturedErrors);
    }

    return result;
}

// ----------------------------------------------------------------------------
//
// Ensure the specific derived type controller object has been created.
//
DWORD
CiscoAPController_t::
CreateTypeController(CiscoAPTypeController_t **ppType)
{
    DWORD result = NO_ERROR;
    CiscoAPTypeController_t *pType = m_pType;
    
    if (NULL == pType)
    {
        result = CiscoAPTypeController_t::GenerateTypeController(GetDeviceType(),
                                                                 GetDeviceName(),
                                                                 m_pCLI,
                                                                 m_Intf,
                                                                &m_State,
                                                                &pType);
        if (NO_ERROR == result)
        {
            assert(NULL != pType);
            m_pType = pType;
        }
    }
    
   *ppType = pType;
    return result;
}

/* ========================================================================= */
/* ========================= CiscoAPTypeController ========================= */
/* ========================================================================= */

// ----------------------------------------------------------------------------
//
// Constructor:
//
CiscoAPTypeController_t::
CiscoAPTypeController_t(
    CiscoPort_t         *pCLI,        // Primary Telnet/CLI interface
    int                  Intf,        // Radio intf or -1 if all intfs
    const CiscoAPInfo_t &APInfo,      // Basic configuration information
    AccessPointState_t  *pState)      // Initial device configuration  
    : m_pCLI(pCLI),
      m_APInfo(APInfo),
      m_pState(pState)
{ 
    memset(m_pIntfCLIs, 0, sizeof(m_pIntfCLIs));
    if (0 > Intf)
        m_pIntfCLIs[0] = pCLI;
}

// ----------------------------------------------------------------------------
//
// Destructor.
//
CiscoAPTypeController_t::
~CiscoAPTypeController_t(void)
{
    // Delete the extra radio-interface controllers (if any).
    for (int ix = 1 ; ix < COUNTOF(m_pIntfCLIs) ; ++ix)
        if (NULL != m_pIntfCLIs[ix])
        {
            delete m_pIntfCLIs[ix];
            m_pIntfCLIs[ix] = NULL;
        }
}

// ----------------------------------------------------------------------------
//
// Gets the basic Access Point configuration settings and uses them
// to generate the appropriate type of CiscoAPTypeController object
// for this device model.
//
DWORD
CiscoAPTypeController_t::
GenerateTypeController(
    const TCHAR              *pDeviceType, // vendor name
    const TCHAR              *pDeviceName, // IP address, user and password
    CiscoPort_t              *pCLI,        // Telnet (CLI) controller
    int                       Intf,        // Radio intf or -1 if all intfs
    AccessPointState_t       *pState,      // initial device configuration
    CiscoAPTypeController_t **ppType)      // generated controller object
{
    auto_ptr<CiscoAPTypeController_t> pType;
    DWORD result;

    // Lock the Telnet/CLI connection.
    ce::gate<ce::critical_section> locker(pCLI->GetLocker());

    // Initialize the AP-info.
    CiscoAPInfo_t apInfo;
    memset(&apInfo, 0, sizeof(apInfo));

    // Read version information from the AP.
    result = pCLI->SendCommand(CiscoPort_t::CommandModeUserExec,
                               "show version");
    if (NO_ERROR != result)
    {
        LogError(TEXT("Can't read model information from AP at %s: %s"),
                 pCLI->GetServerHost(),
                 Win32ErrorText(result));
        return result;
    }

    TelnetLines_t *pResponse = pCLI->GetCommandResponse();
    assert(NULL != pResponse);
    
    for (int lx = 0 ; pResponse
          && lx < pResponse->Size() && (!apInfo.ModelName[0] || !apInfo.IosVersionName[0]) ;
           ++lx)
    {
        const TelnetWords_t &words = pResponse->GetLine(lx);
        if (words.Size() <= 1)
            continue;

        // Parse the model number.
        static const char *ModelPrefixes[] = { "AIR-AP", "AIR-LAP" };
        if (apInfo.ModelName[0] == '\0')
        {
            for (int px = 0 ; px < COUNTOF(ModelPrefixes) ; ++px)
            {
                if (_strnicmp(words[1], ModelPrefixes[px],
                                 strlen(ModelPrefixes[px])) == 0)
                {
                    SafeCopy(apInfo.ModelName, words[1], COUNTOF(apInfo.ModelName));
                    apInfo.ModelNumber = atoi(words[1] + strlen(ModelPrefixes[px]));
                    break;
                }
            }
        }

        // Parse the IOS version number.
        if (apInfo.IosVersionName[0] != '\0')
            continue;

        int versIX = -1;
        for (int wx = words.Size() - 3 ; wx >= 0 ; --wx)
        {
            if (_stricmp(words[wx], "IOS") == 0)
            {
                for (int vx = words.Size() - 2 ; vx > wx ; --vx)
                {
                    if (_stricmp(words[vx], "Version") == 0)
                    {
                        versIX = vx;
                        break;
                    }
                }
                break;
            }
        }
        
        if (versIX > 0)
        {
            char *vp, version [COUNTOF(apInfo.IosVersionName)];
            char *dp, digiVers[COUNTOF(apInfo.IosVersionName)*2];
            const char *vpEnd = &version [COUNTOF(version) -1];
            const char *dpEnd = &digiVers[COUNTOF(digiVers)-3];

            SafeCopy(version, words[versIX+1], COUNTOF(version));

            // Remove the terminating comma (if any),
            vp = version;
            for (; vp < vpEnd && vp[0] ; ++vp)
                if (vp[0] == ',' && vp[1] == '\0')
                    break;
            vp[0] = '\0';
            vpEnd = vp;
            
            memcpy(apInfo.IosVersionName, version, sizeof(version));

            // Remove non-digits and convert to a float.
            // Convert alphas to two decimal digits each.
            vp = version;
            dp = digiVers;
            while (vp < vpEnd && dp < dpEnd)
            {
                UINT ch = (UINT)*(vp++);
                if (isdigit(ch) ||  ch == '.')
                   *(dp++) = (char)ch;
                else
                if (isalpha(ch))
                {
                    if (islower(ch)) ch -= ('a' - 1);
                    else             ch -= ('A' - 1);
                   *(dp++) = '0' + ((ch / 10) % 10);
                   *(dp++) = '0' +  (ch       % 10);
                }
            }
            dp[0] = '\0';
            
            apInfo.IosVersionNumber = atof(digiVers);
        }
    }

    if (apInfo.ModelName[0])
        LogDebug(TEXT("[AC] Cisco Model: %lu (%hs)"),
                 apInfo.ModelNumber, apInfo.ModelName);
    else
        LogWarn(TEXT("[AC] Can't read Cisco model number from %s"),
                pCLI->GetServerHost());

    if (apInfo.IosVersionName[0])
        LogDebug(TEXT("[AC] IOS Version: %2.9f (%hs)"),
                 apInfo.IosVersionNumber, apInfo.IosVersionName);
    else
        LogWarn(TEXT("[AC] Can't read Cisco version number from %s"),
                pCLI->GetServerHost());

    // 1200-series:
    if (1200 <= apInfo.ModelNumber && apInfo.ModelNumber <= 1299)  // <== Extend this as new models added
    {
        result = Generate1200Controller(pCLI, Intf, apInfo, pState, &pType);
        if (NO_ERROR != result)
            return result;
    }
    else
    {
        LogError(TEXT("Cisco AP model \"%hs\" is unrecognized at %s"),
                 apInfo.ModelName,
                 pCLI->GetServerHost());
        return ERROR_CALL_NOT_IMPLEMENTED;
    }

    // Make sure the allocation succeeded.
    if (!pType.valid())
    {
        LogError(TEXT("Can't generate %s AP controller"), pDeviceType);
        return ERROR_OUTOFMEMORY;
    }

    // Initialize the controller.
    result = pType->InitializeDevice();
    if (NO_ERROR == result)
    {
        *ppType = pType.release();
    }

    return result;
}

// ----------------------------------------------------------------------------
//
// Creates a 1200 series controller object.
//
DWORD
CiscoAPTypeController_t::
Generate1200Controller(
    CiscoPort_t              *pCLI,        // Telnet/CLI interface
    int                       Intf,        // Radio intf or -1 if all intfs
    const CiscoAPInfo_t      &APInfo,      // Basic configuration information
    AccessPointState_t       *pState,      // Initial device configuration
    CiscoAPTypeController_t **ppType)      // Output pointer
{
    // Generate the model-specific device-interface.
   *ppType = new Cisco1200Controller_t(pCLI, Intf, APInfo, pState);
    LogDebug(TEXT("[AC] Generated Cisco 1200-series controller"));
    return NO_ERROR;
}

// ----------------------------------------------------------------------------
//
// Resets the device once it gets into an unknown state.
//
DWORD
CiscoAPTypeController_t::
ResetDevice(void)
{
    m_pCLI->Disconnect();
    return NO_ERROR;
}

// ----------------------------------------------------------------------------
//
// Sends the specified command and stores the response in m_Response:
// If necessary, changes to the specified command-mode before sending
// the command.
// If the command is being directed at the radio interface, sends it
// to each the Access Point's interfaces in turn.
//
DWORD
CiscoAPTypeController_t::
SendCommand(
    CiscoPort_t::CommandMode_e CommandMode,
    const char                *pFormat, ...)
{
    va_list pArgList;
    va_start(pArgList, pFormat);
    DWORD result = SendCommandV(CommandMode,
                                pFormat,
                                pArgList);
    va_end(pArgList);
    return result;
}

DWORD
CiscoAPTypeController_t::
SendCommandV(
    CiscoPort_t::CommandMode_e CommandMode,
    const char                *pFormat,
    va_list                    pArgList)
{
    DWORD result = NO_ERROR;
    if (CiscoPort_t::CommandModeInterface != CommandMode
     || NULL == m_pIntfCLIs[0])
    {
        result = m_pCLI->SendCommandV(CommandMode,
                                      pFormat,
                                      pArgList);
    }
    else
    {
        for (int ix = 0 ; ix < CiscoPort_t::NumberRadioInterfaces ; ++ix)
            if (NULL == m_pIntfCLIs[ix])
                break;
            else
            {
                result = m_pIntfCLIs[ix]->SendCommandV(CommandMode,
                                                       pFormat,
                                                       pArgList);
                if (NO_ERROR != result)
                    break;
            }
    }
    return result;
}

// ----------------------------------------------------------------------------
//
// Sends the specified command, stores the response in m_Response and
// and checks it for an error message:
// (Any response is assumed to be an error message.)
// If necessary, changes to the specified command-mode before sending
// the command.
// If the command is being directed at the radio interface, verifies
// it on each the Access Point's interfaces in turn.
// If appropriate, issues an error message containing the specified
// operation name and the error message sent by the server.
//
DWORD
CiscoAPTypeController_t::
VerifyCommand(
    CiscoPort_t::CommandMode_e CommandMode,
    const char                *pOperation,
    const char                *pFormat, ...)
{
    va_list pArgList;
    va_start(pArgList, pFormat);
    DWORD result = VerifyCommandV(CommandMode,
                                  pOperation,
                                  pFormat,
                                  pArgList);
    va_end(pArgList);
    return result;
}

DWORD
CiscoAPTypeController_t::
VerifyCommandV(
    CiscoPort_t::CommandMode_e CommandMode,
    const char                *pOperation,
    const char                *pFormat,
    va_list                    pArgList)
{
    DWORD result = NO_ERROR;
    if (CiscoPort_t::CommandModeInterface != CommandMode
     || NULL == m_pIntfCLIs[0])
    {
        result = m_pCLI->VerifyCommandV(CommandMode,
                                        pOperation,
                                        pFormat,
                                        pArgList);
    }
    else
    {
        for (int ix = 0 ; ix < CiscoPort_t::NumberRadioInterfaces ; ++ix)
            if (NULL == m_pIntfCLIs[ix])
                break;
            else
            {
                result = m_pIntfCLIs[ix]->VerifyCommandV(CommandMode,
                                                         pOperation,
                                                         pFormat,
                                                         pArgList);
                if (NO_ERROR != result)
                    break;
            }
    }
    return result;
}

// ----------------------------------------------------------------------------
