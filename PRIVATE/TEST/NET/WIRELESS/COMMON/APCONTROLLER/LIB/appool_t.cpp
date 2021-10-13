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
// Implementation of the APPool_t class.
//
// ----------------------------------------------------------------------------

#include <APPool_t.hpp>
#include "APControlClient_t.hpp"
#include "APControlData_t.hpp"
#include "RemoteAPConfigurator_t.hpp"
#include "RemoteAttenuationDriver_t.hpp"

#include <assert.h>
#include <intsafe.h>
#include <netmain.h>

#include <inc/auto_xxx.hxx>
#include <inc/sync.hxx>

using namespace ce::qa;

// APController server syncronization object:
static ce::critical_section APControlLocker;

// ----------------------------------------------------------------------------
// 
// Constructor.
//
APPool_t::
APPool_t(void)
    : m_pAPList(NULL),
      m_APListSize(0),
      m_APListAlloc(0),
      m_pClient(NULL),
      m_Status(0)
{
    // nothing to do
}

// ----------------------------------------------------------------------------
//
// Destructor.
//
APPool_t::
~APPool_t(void)
{
    Clear();
    if (NULL != m_pClient)
    {
        delete m_pClient;
        m_pClient = NULL;
    }
}

// ----------------------------------------------------------------------------
//
// Inserts the specified APController into the list.
//
HRESULT
APPool_t::
InsertAPController(
    APConfigurator_t    *pConfig,
    AttenuationDriver_t *pAttenuator)   
{
    // Allocate space in the list.
    if (m_APListSize >= m_APListAlloc)
    {
        size_t newAlloc; void *newList;
        HRESULT hr = SizeTAdd(m_APListAlloc, 16, &newAlloc);
        if (FAILED(hr))
             newList = NULL;
        else newList = calloc(newAlloc, sizeof(APController_t *));
        if (NULL == newList)
        {
            size_t allocSize = (m_APListAlloc + 16) * sizeof(APController_t *);
            LogError(TEXT("Can't allocate %u bytes for APController list"),
                     allocSize);
            return E_OUTOFMEMORY;
        }

        if (NULL != m_pAPList)
        {
            if (0 < m_APListAlloc)
            {
                size_t allocSize = m_APListAlloc * sizeof(APController_t *);
                memcpy(newList, m_pAPList, allocSize);
            }
            free(m_pAPList);
        }

        m_APListAlloc = (int) newAlloc;
        m_pAPList = (APController_t **) newList;
    }

    // Allocate and insert the controller.
    m_pAPList[m_APListSize] = new APController_t(pConfig, pAttenuator);
    if (NULL == m_pAPList[m_APListSize])
    {
        LogError(TEXT("Can't allocate APController object"));
        return E_OUTOFMEMORY;
    }

    m_APListSize++;
    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Loads the list from those APControllers managed by the specified
// APControl server. If the optional server-time argument is supplied,
// also returns the current system-time on the server.
//
HRESULT
APPool_t::
LoadAPControllers(
    IN  const TCHAR *pServerHost,
    IN  const TCHAR *pServerPort,
    OUT SYSTEMTIME  *pServerTime)
{
    HRESULT hr;
    DWORD result;

    ce::auto_ptr<APConfigurator_t>    pConfig;
    ce::auto_ptr<AttenuationDriver_t> pAtten;

    Clear();

    ce::gate<ce::critical_section> locker(APControlLocker);

    // Do all this twice in case the connection drops.
    ce::tstring response;
    for (int tries = 0 ;; ++tries)
    {
        // If the connection host or port has changed, close it.
        if (NULL != m_pClient)
        {
            if (_tcscmp(pServerHost, m_pClient->GetServerHost()) != 0
             || _tcscmp(pServerPort, m_pClient->GetServerPort()) != 0)
            {
                delete m_pClient;
                m_pClient = NULL;
            }
        }

        // (Re)connect if necessary.
        if (NULL == m_pClient)
        {
            m_pClient = new APControlClient_t(pServerHost, pServerPort);
            if (NULL == m_pClient)
            {
                LogError(TEXT("Can't allocate APControl client"));
                return E_OUTOFMEMORY;
            }
        }
        if (!m_pClient->IsConnected())
        {
            hr = m_pClient->Connect();
            if (FAILED(hr))
                return hr;
        }

        // Send the command and wait for a response.
        DWORD commandCode = APControlData_t::GetConfigurationCommandCode;
        ce::tstring commandText; // empty
        result = m_pClient->SendStringCommand(commandCode,
                                              commandText,
                                             &response);

        // Close the connection immediately.
        m_pClient->Disconnect();

        if (ERROR_SUCCESS == result)
            break;
        if (tries > 1)
        {
            LogError(TEXT("Error getting configuration from %s:%s: err=%.128s"),
                     pServerHost, pServerPort, &response[0]); 
            return HRESULT_FROM_WIN32(result);
        }
    }

    // Parse the list of APConfigurators and AttenuationDrivers from the
    // remote response, initialize the interface objects and create AP
    // controllers.
    const TCHAR *pAPType, *pAPName, *pATType, *pATName;
    pAPType = pAPName = pATType = pATName = NULL;
    ce::tstring parseBuffer(response);
    for (TCHAR *token = _tcstok(parseBuffer.get_buffer(), TEXT(",")) ;
                token != NULL ;
                token = _tcstok(NULL, TEXT(",")))
    {
        if      (NULL == pAPType) { pAPType = token; }
        else if (NULL == pAPName) { pAPName = token; }
        else if (NULL == pATType) { pATType = token; }
        else if (NULL == pATName) { pATName = token;

            pConfig = new RemoteAPConfigurator_t(m_pClient->GetServerHost(),
                                                 m_pClient->GetServerPort(),
                                                 pAPType, pAPName);
            if (!pConfig.valid())
            {
                LogError(TEXT("Can't allocate APConfigurator object"));
                return E_OUTOFMEMORY;
            }
#if 0
            hr = pConfig->Reconnect();
            if (FAILED(hr))
                return hr;
#endif

            pAtten = new RemoteAttenuationDriver_t(m_pClient->GetServerHost(),
                                                   m_pClient->GetServerPort(),
                                                   pATType, pATName);
            if (!pAtten.valid())
            {
                LogError(TEXT("Can't allocate AttenuationDriver object"));
                return E_OUTOFMEMORY;
            }
#if 0
            hr = pAtten->Reconnect();
            if (FAILED(hr))
                return hr;
#endif

            hr = InsertAPController(pConfig, pAtten);
            if (FAILED(hr))
                return hr;
            pConfig.release();
            pAtten.release();

            pAPType = pAPName = pATType = pATName = NULL;
        }
    }

    if (pAPName || pATType
    || (pAPType && !(_istdigit(pAPType[ 0]) && _istdigit(pAPType[ 1])   // year
                  && _istdigit(pAPType[ 2]) && _istdigit(pAPType[ 3])
                  && _istdigit(pAPType[ 4]) && _istdigit(pAPType[ 5])   // month
                  && _istdigit(pAPType[ 6]) && _istdigit(pAPType[ 7])   // day
                  && _istdigit(pAPType[ 8]) && _istdigit(pAPType[ 9])   // hour
                  && _istdigit(pAPType[10]) && _istdigit(pAPType[11])   // min
                  && _istdigit(pAPType[12]) && _istdigit(pAPType[13]))))// sec
    {
        LogError(TEXT("Extra items after APController #%d")
                 TEXT(" in config \"%.180s\""),
                 size(), &response[0]);
        return E_INVALIDARG;
    }

    // If they asked for the server's time-stamp, parse the one supplied
    // by the server or, if that wasn't supplied, insert the current local
    // time.
    if (pServerTime)
    {
        memset(pServerTime, 0, sizeof(SYSTEMTIME));
        GetSystemTime(pServerTime);

        if (pAPType)
        {
            pServerTime->wYear   = (WORD)(((pAPType[ 0] - TEXT('0')) * 1000)
                                        + ((pAPType[ 1] - TEXT('0')) * 100)
                                        + ((pAPType[ 2] - TEXT('0')) * 10)
                                        +  (pAPType[ 3] - TEXT('0')));
            pServerTime->wMonth  = (WORD)(((pAPType[ 4] - TEXT('0')) * 10)
                                        +  (pAPType[ 5] - TEXT('0')));
            pServerTime->wDay    = (WORD)(((pAPType[ 6] - TEXT('0')) * 10)
                                        +  (pAPType[ 7] - TEXT('0')));
            pServerTime->wHour   = (WORD)(((pAPType[ 8] - TEXT('0')) * 10)
                                        +  (pAPType[ 9] - TEXT('0')));
            pServerTime->wMinute = (WORD)(((pAPType[10] - TEXT('0')) * 10)
                                        +  (pAPType[11] - TEXT('0')));
            pServerTime->wSecond = (WORD)(((pAPType[12] - TEXT('0')) * 10)
                                        +  (pAPType[13] - TEXT('0')));
        }
    }

    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Disconnects from the APController server.
//
void
APPool_t::
Disconnect(void)
{
    if (m_pClient)
    {
        ce::gate<ce::critical_section> locker(APControlLocker);
        m_pClient->Disconnect();
    }
}

// ----------------------------------------------------------------------------
//
// Clears all the APControllers from the list.
//
void
APPool_t::
Clear(void)
{
    if (m_pAPList)
    {
        for (int apx = m_APListSize ; 0 <= --apx ;)
        {
            if (m_pAPList[apx])
            {
                delete m_pAPList[apx];
                m_pAPList[apx] = NULL;
            }
        }
        free(m_pAPList);
        m_pAPList = NULL;
    }
    m_APListSize = m_APListAlloc = 0;
}

// ----------------------------------------------------------------------------
//
// Retrieves an AP controller by index or case-insensitive name.
//
APController_t *
APPool_t::
GetAP(int APIndex)
{
    return (0 > APIndex || APIndex >= m_APListSize)? NULL
                                                   : m_pAPList[APIndex];
}

const APController_t *
APPool_t::
GetAP(int APIndex) const
{
    return (0 > APIndex || APIndex >= m_APListSize)? NULL
                                                   : m_pAPList[APIndex];
}

APController_t *
APPool_t::
FindAP(const TCHAR *pAPName)
{
    for (int apx = m_APListSize ; 0 <= --apx ;)
    {
        APController_t *ap = m_pAPList[apx];
        if (ap != NULL && _tcsicmp(pAPName, ap->GetAPName()) == 0)
            return ap;
    }
    return NULL;
}

const APController_t *
APPool_t::
FindAP(const TCHAR *pAPName) const
{
    return const_cast<APPool_t *>(this)->FindAP(pAPName);
}

// ----------------------------------------------------------------------------
//
// (Re)connects to the APControl control server and gets or sets the
// test status.
//
static HRESULT
SendStatus(
    APControlClient_t *pClient,
    DWORD               CommandCode,
    const TCHAR       *pCommandName,
    DWORD               NewStatus,
    DWORD             *pResponse)
{
    DWORD result;
    assert(NULL != pClient);

    ce::gate<ce::critical_section> locker(APControlLocker);

    // Do all this twice in case the connection drops.
    for (int tries = 0 ;; ++tries)
    {
        // (Re)connect if necessary.
        if (!pClient->IsConnected())
        {
            HRESULT hr = pClient->Connect();
            if (FAILED(hr))
                return hr;
        }

        // Send the command and wait for a response.
        BYTE *pReturnData = NULL; 
        DWORD  returnDataBytes, remoteResult;
        ce::tstring errorResponse;
        result = pClient->SendPacket(CommandCode,
                                    (BYTE *)&NewStatus,
                                     sizeof(DWORD),
                                    &remoteResult,
                                    &pReturnData,
                                     &returnDataBytes);
        if (ERROR_SUCCESS == result)
        {
            if (ERROR_SUCCESS == remoteResult)
            {
               *pResponse = GetTickCount();
                if (pReturnData)
                {
                    if (returnDataBytes == sizeof(DWORD))
                        memcpy(pResponse, pReturnData, sizeof(DWORD));
                    free(pReturnData);
                }
                return ERROR_SUCCESS;
            }
            else
            {
                result = APControlData_t::DecodeMessage(remoteResult,
                                                       pReturnData, 
                                                        returnDataBytes, 
                                                       &errorResponse);
                if (pReturnData)
                    free(pReturnData);
                
                LogError(TEXT("Recieved error from %s:%s: %s"),
                         pClient->GetServerHost(),
                         pClient->GetServerPort(),
                         &errorResponse[0]);
                
                return HRESULT_FROM_WIN32(result);
            }
        }
        
        if (tries > 1)
        {
            LogError(TEXT("Error getting status from %s:%s: %s"),
                     pClient->GetServerHost(),
                     pClient->GetServerPort(),
                     Win32ErrorText(result));
            return HRESULT_FROM_WIN32(result);
        }

        // Disconnect and try again.
        pClient->Disconnect();
    }
}

// ----------------------------------------------------------------------------
//
// Gets or sets the current test status.
//
HRESULT
APPool_t::
GetTestStatus(DWORD *pStatus)
{
    if (NULL == m_pClient)
    {
       *pStatus = m_Status;
        return ERROR_SUCCESS;
    }
    else
    {
        return SendStatus(m_pClient,
                          APControlData_t::GetStatusCommandCode,
                          APControlData_t::GetStatusCommandName,
                         *pStatus,
                          pStatus);
    }
}

HRESULT
APPool_t::
SetTestStatus(DWORD Status)
{
    if (NULL == m_pClient)
    {
        m_Status = Status;
        return ERROR_SUCCESS;
    }
    else
    {
        return SendStatus(m_pClient,
                          APControlData_t::SetStatusCommandCode,
                          APControlData_t::SetStatusCommandName,
                          Status,
                         &Status);
    }
}

// ----------------------------------------------------------------------------
