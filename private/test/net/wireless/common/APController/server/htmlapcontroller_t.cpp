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
// Implementation of the HtmlAPController_t class.
//
// ----------------------------------------------------------------------------

#include "HtmlAPController_t.hpp"

#include "AccessPointState_t.hpp"
#include "BuffaloBroadcomController_t.hpp"
#include "BuffaloG54sController_t.hpp"
#include "DLinkDI524Controller_t.hpp"
#include "DLinkDWL3200Controller_t.hpp"
#include "HttpPort_t.hpp"

#include <assert.h>
#include <tchar.h>
#include <strsafe.h>

// Define this if you want LOTS of debug output:
#define EXTRA_DEBUG 1

using namespace ce::qa;

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// HtmlAPController implementation class.
//

class HtmlAPControllerImp_t
{
private:

    // Device type and name:
    ce::tstring m_DeviceType;
    ce::tstring m_DeviceName;

    // HTTP connection:
    HttpPort_t *m_pHttp;

    // Current AP configuration:
    AccessPointState_t m_State;

    // Type-specific device interface:
    HtmlAPTypeController_t *m_pType;

public:

    // Constructor and destructor:
    HtmlAPControllerImp_t(
        const TCHAR *        pDeviceType,
        const TCHAR *        pDeviceName);
    virtual
   ~HtmlAPControllerImp_t(void);

    // Gets the current Access Point configuration settings:
    DWORD
    GetAccessPoint(
        AccessPointState_t *pResponse);

    // Updates the Access Point configuration settings:
    DWORD
    SetAccessPoint(
        const AccessPointState_t &NewState,
              AccessPointState_t *pResponse);

private:

    // Ensure the specific derived type controller object has been created:
    DWORD
    CreateTypeController(void);
};
};
};

/* ========================== HtmlAPController_t =========================== */

// ----------------------------------------------------------------------------
//
// Constructor.
//
HtmlAPController_t::
HtmlAPController_t(
    const TCHAR *        pDeviceType,
    const TCHAR *        pDeviceName)
    : AccessPointController_t(pDeviceType, pDeviceName),
      m_pImp(new HtmlAPControllerImp_t(pDeviceType, pDeviceName))
{
    assert(NULL != m_pImp);
}

// ----------------------------------------------------------------------------
//
// Destructor.
//
HtmlAPController_t::
~HtmlAPController_t(void)
{
    if (NULL != m_pImp)
    {
        delete m_pImp;
        m_pImp = NULL;
    }
}

// ----------------------------------------------------------------------------
//
// Gets the current Access Point configuration settings.
//
DWORD
HtmlAPController_t::
GetAccessPoint(
    AccessPointState_t *pResponse)
{
    assert(NULL != m_pImp);
    return (NULL == m_pImp)? ERROR_OUTOFMEMORY
                           : m_pImp->GetAccessPoint(pResponse);
}

// ----------------------------------------------------------------------------
//
// Updates the Access Point configuration settings.
//
DWORD
HtmlAPController_t::
SetAccessPoint(
    const AccessPointState_t &NewState,
          AccessPointState_t *pResponse)
{
    assert(NULL != m_pImp);
    return (NULL == m_pImp)? ERROR_OUTOFMEMORY
                           : m_pImp->SetAccessPoint(NewState, pResponse);
}

/* ======================== HtmlAPControllerImp_t ========================== */

// ----------------------------------------------------------------------------
//
// Constructor.
//
HtmlAPControllerImp_t::
HtmlAPControllerImp_t(
    const TCHAR *        pDeviceType,
    const TCHAR *        pDeviceName)
    : m_DeviceType(pDeviceType),
      m_DeviceName(pDeviceName),
      m_pHttp(NULL),
      m_pType(NULL)
{
    // Parse the device-name.
    ce::tstring lDeviceName(pDeviceName);
    const TCHAR *pHost, *pUser, *pPass;
    pHost = pUser = pPass = NULL;
    TCHAR *nextToken = NULL;  
    for (TCHAR *token = _tcstok_s(lDeviceName.get_buffer(), TEXT(":"),&nextToken) ;
                token != NULL ;
                token = _tcstok_s(NULL, TEXT(":"),&nextToken))
    {
        if      (NULL == pHost) { pHost = WiFUtils::TrimToken(token); }
        else if (NULL == pUser) { pUser = WiFUtils::TrimToken(token); }
        else                    { pPass = WiFUtils::TrimToken(token); break; }
    }

    // Allocate the HTTP controller.
    m_pHttp = new HttpPort_t((NULL == pHost)? TEXT("missing") : pHost);
    if (NULL == m_pHttp)
    {
        assert(NULL != m_pHttp);
    }

    // Insert the admin user and password (if any).
    else
    if (NULL != pUser || NULL != pPass)
    {
        ce::gate<ce::critical_section> locker(m_pHttp->GetLocker());
        if (NULL != pUser) m_pHttp->SetUserName(pUser);
        if (NULL != pPass) m_pHttp->SetPassword(pPass);
    }
}

// ----------------------------------------------------------------------------
//
// Destructor.
//
HtmlAPControllerImp_t::
~HtmlAPControllerImp_t(void)
{
    if (NULL != m_pHttp)
    {
        delete m_pHttp;
        m_pHttp = NULL;
    }
    if (NULL != m_pType)
    {
        delete m_pType;
        m_pType = NULL;
    }
}

// ----------------------------------------------------------------------------
//
// Gets the current Access Point configuration settings.
//
DWORD
HtmlAPControllerImp_t::
GetAccessPoint(
    AccessPointState_t *pResponse)
{
    DWORD result = NO_ERROR;
    if (NULL == m_pHttp)
        return ERROR_OUTOFMEMORY;

    // Lock the HTTP connection.
    ce::gate<ce::critical_section> locker(m_pHttp->GetLocker());

    // Capture error messages.
    ce::tstring capturedErrors;
    ErrorLock errorLock(&capturedErrors);

    // Do this a few times in case the HTML interaction fails.
    for (int tries = 1 ; 2 >= tries ; ++tries)
    {
        capturedErrors.clear();
   
        // Ensure the type controller is created.
        result = CreateTypeController();
        if (NO_ERROR != result)
            break;

        // Use the model interface to load the configuration.
        result = m_pType->LoadSettings();
        if (NO_ERROR == result)
        {
           *pResponse = m_State;
            break;
        }

        // Try resetting the device one time.
        if (1 == tries)
        {
            LogWarn(TEXT("Resetting after error: %s"), &capturedErrors[0]);
            if (NO_ERROR != m_pType->ResetDevice())
                LogWarn(TEXT("AP reset failed: err=%s"), &capturedErrors[0]);
        }

#ifdef EXTRA_DEBUG
        LogDebug(TEXT("++ Error dump"));
        LogWarn(capturedErrors);
        LogDebug(TEXT("-- Error dump"));
#endif
    }

    // Log the captured errors (if any).
    errorLock.StopCapture();
    if (NO_ERROR == result)
         LogDebug(capturedErrors);
    else LogError(capturedErrors);
    
    return result;
}

// ----------------------------------------------------------------------------
//
// Updates the Access Point configuration settings.
//
DWORD
HtmlAPControllerImp_t::
SetAccessPoint(
    const AccessPointState_t &NewState,
          AccessPointState_t *pResponse)
{
    DWORD result = NO_ERROR;
    if (NULL == m_pHttp)
        return ERROR_OUTOFMEMORY;

    // Lock the HTTP connection.
    ce::gate<ce::critical_section> locker(m_pHttp->GetLocker());

    // Capture error messages.
    ce::tstring capturedErrors;
    ErrorLock errorLock(&capturedErrors);
    
    // Do this a few times in case the HTML interaction fails.
    for (int tries = 1 ; 3 >= tries ; ++tries)
    {
        capturedErrors.clear();

        // Ensure the type controller is created.
        result = CreateTypeController();
        if (NO_ERROR != result)
            break;

        // Use the model interface to update the configuration.
        result = m_pType->UpdateSettings(NewState);
        if (NO_ERROR == result)
        {
           *pResponse = m_State;
            break;
        }

        LogDebug(TEXT("UpdateSettings() failed"));

        // Try resetting the device one time.
        if (2 == tries)
        {
            LogWarn(TEXT("Resetting after error"));
            if (NO_ERROR != m_pType->ResetDevice())
                LogWarn(TEXT("AP reset failed"));
        }

#ifdef EXTRA_DEBUG
        LogDebug(TEXT("++ Error dump"));
        LogWarn(capturedErrors);
        LogDebug(TEXT("-- Error dump"));
#endif
    }

    // Log the captured errors (if any).
    errorLock.StopCapture();
    if (NO_ERROR == result)
         LogDebug(capturedErrors);
    else LogError(capturedErrors);
    
    return result;
}

// ----------------------------------------------------------------------------
//
// Ensure the specific derived type controller object has been created.
//
DWORD
HtmlAPControllerImp_t::
CreateTypeController(void)
{
    DWORD result = NO_ERROR;
    if (NULL == m_pType)
    {
        result = HtmlAPTypeController_t::
                    GenerateTypeController(m_DeviceType,
                                           m_DeviceName,
                                           m_pHttp,
                                           &m_State,
                                           &m_pType);
#ifdef DEBUG
        if (NO_ERROR == result)
            assert(NULL != m_pType);
#endif
    }
    return result;
}

/* ======================== HtmlAPTypeController_t ========================= */

// ----------------------------------------------------------------------------
//
// Destructor.
//
HtmlAPTypeController_t::
~HtmlAPTypeController_t(void)
{
    // nothing to do
}

// ----------------------------------------------------------------------------
//
// Gets the basic Access Point configuration settings and uses them
// to generate the appropriate type of HtmlAPTypeController object
// for this device model.
//
/* static */
DWORD
HtmlAPTypeController_t::
GenerateTypeController(
    const TCHAR             *pDeviceType, // vendor name
    const TCHAR             *pDeviceName, // IP address, user and password
    HttpPort_t              *pHttp,       // HTTP controller
    AccessPointState_t      *pState,      // initial device configuration
    HtmlAPTypeController_t **ppType)      // generated controller object
{
    auto_ptr<HtmlAPTypeController_t> pType;
    DWORD result;

    // Lock the HTTP server connection.
    ce::gate<ce::critical_section> locker(pHttp->GetLocker());

    // Buffalo:
    if (0 == _tcsicmp(pDeviceType, TEXT("Buffalo")))
    {
        result = GenerateBuffaloController(pHttp, pState, &pType);
        if (NO_ERROR != result)
            return result;
    }
    // D-Link:
    else
    if (0 == _tcsicmp(pDeviceType, TEXT("DLink")))
    {
        result = GenerateDlinkController(pHttp, pState, &pType);
        if (NO_ERROR != result)
            return result;
    }
    // Unknown AP vendor.
    else
    {
        LogError(TEXT("AP vendor \"%s\" is unrecognized"), pDeviceType);
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
// Create a Buffalo controller object.
//
/* static */
DWORD
HtmlAPTypeController_t::
GenerateBuffaloController(
    HttpPort_t              *pHttp,
    AccessPointState_t      *pState,
    HtmlAPTypeController_t **ppType)
{
    DWORD result;
    ce::string webPage;
    ce::string title;
    const char *pCursor,
             *oldCursor;

    // If necessary, insert the admin user-name.
    if (pHttp->GetUserName()[0] == TEXT('\0') &&
        pHttp->GetPassword()[0] == TEXT('\0'))
    {
        pHttp->SetUserName(TEXT("root"));
    }

    // Load the root page.
    result = pHttp->SendPageRequest(L"/", 0L, &webPage);
    if (NO_ERROR != result)
        return result;

    // Get the model number from the title.
    oldCursor = webPage.get_buffer();
    pCursor = HtmlUtils::FindString(oldCursor, "<title", ">",
                                    "</title>", &title);
    if (oldCursor == pCursor)
    {
        LogError(TEXT("Model number missing from %ls title \"%hs\""),
                 pHttp->GetServerHost(), &title[0]);
        return ERROR_INVALID_PARAMETER;
    }

    // Generate the model-specific device-interface.
    if (strstr(title, "G54S"))
    {
        *ppType = new BuffaloG54sController_t(pHttp, pState);
        LogDebug(TEXT("[AC] Generated Buffalo G54s controller"));
    }
    else
    if (strstr(title, "Broadcom"))
    {
        *ppType = new BuffaloBroadcomController_t(pHttp, pState);
        LogDebug(TEXT("[AC] Generated Buffalo Broadcom controller"));
    }
    else
    {
        LogError(TEXT("Buffalo AP model \"%hs\" is unrecognized in %ls/"),
                 &title[0], pHttp->GetServerHost());
        return ERROR_CALL_NOT_IMPLEMENTED;
    }
    
    return NO_ERROR;
}

// ----------------------------------------------------------------------------
//
// Create a Dlink controller object.
//
/* static */
DWORD
HtmlAPTypeController_t::
GenerateDlinkController(
    HttpPort_t              *pHttp,
    AccessPointState_t      *pState,
    HtmlAPTypeController_t **ppType)
{
    DWORD result;
    ce::string webPage;
    ce::string title;
    const char *pCursor,
             *oldCursor;

    // If necessary, insert the admin user-name.
    if (pHttp->GetUserName()[0] == TEXT('\0') &&
        pHttp->GetPassword()[0] == TEXT('\0'))
    {
        pHttp->SetUserName(TEXT("admin"));
    }

    // Load the root page.
    result = pHttp->SendPageRequest(L"/", 0L, &webPage);
    if (NO_ERROR != result)
        return result;

    // Get the model number from the title.
    oldCursor = webPage.get_buffer();
    pCursor = HtmlUtils::FindString(oldCursor, "<title", ">",
                                    "</title>", &title);
    if (oldCursor == pCursor)
    {
        LogError(TEXT("Model number missing from %ls/ title \"%hs\""),
                 pHttp->GetServerHost(), &title[0]);
        return ERROR_INVALID_PARAMETER;
    }

    // Generate the model-specific device-interface.
    if (strstr(title, "524"))
    {
        *ppType = new DLinkDI524Controller_t(pHttp, pState);
        LogDebug(TEXT("[AC] Generated D-Link DI-524 controller"));
    }
    else
    if (strstr(title, "3200"))
    {
        *ppType = new DLinkDWL3200Controller_t(pHttp, pState, webPage);
        LogDebug(TEXT("[AC] Generated D-Link DWL-3200 controller"));
    }
    else
    {
        LogError(TEXT("D-Link AP model \"%hs\" is unrecognized in %ls/"),
                 &title[0], pHttp->GetServerHost());
        return ERROR_CALL_NOT_IMPLEMENTED;
    }
    
    return NO_ERROR;
}

// ----------------------------------------------------------------------------
//
// Resets the device once it gets into an unknown state.
// (Since not all devices support this, the default implementation
// for this method does nothing.)
//
DWORD
HtmlAPTypeController_t::
ResetDevice(void)
{
    return NO_ERROR;
}

// ----------------------------------------------------------------------------
//
// Uses the specified HTTP port to perform a web-page request and parses
// form information out of the response.
//
DWORD
HtmlAPTypeController_t::
ParsePageRequest(
    HttpPort_t  *pHttp,         // HTTP controller
    const WCHAR *pPageName,     // page to be loaded
    long         SleepMillis,   // wait time before reading response
    ce::string  *pWebPage,      // response from device
    HtmlForms_t *pPageForms,    // HTML forms parser
    int          FormsExpected) // number forms expected (or zero)
{
    DWORD result = pHttp->SendPageRequest(pPageName, SleepMillis, pWebPage);
    if (NO_ERROR != result)
        return result;

    result = pPageForms->ParseWebPage(*pWebPage, pPageName);
    if (NO_ERROR != result)
        return result;

    if (0 != FormsExpected && FormsExpected != pPageForms->Size())
    {
        LogError(TEXT("Expected %d HTML form in %ls - found %d"),
                 FormsExpected, pPageName, pPageForms->Size());
        return ERROR_INVALID_PARAMETER;
    }

    return NO_ERROR;
}

// ----------------------------------------------------------------------------
