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
#include <UpnpInvokeRequest.hpp>
#include <pch.h>
#include <intsafe.h>
#pragma hdrstop


// disable warning about structured exception handling
#pragma warning (disable : 4530)


/**
 *  UpnpInvokeRequestContainer_t:
 *
 *      - initializes key container variables
 *
 */
UpnpInvokeRequestContainer_t::UpnpInvokeRequestContainer_t()
{
    m_AllocatedInvokeRequest = FALSE;
    m_UpnpInvokeRequest = NULL;
    m_UpnpInvokeRequestBufferSize = 0;

    m_UpnpRequest = NULL;
    m_ServiceID = NULL;
    m_RequestXML = NULL;
    m_UDN = NULL;
}



/**
 *  ~UpnpInvokeRequestContainer:
 *
 *      - cleans up all allocated resources by the container
 *
 */
UpnpInvokeRequestContainer_t::~UpnpInvokeRequestContainer_t()
{
    if(m_AllocatedInvokeRequest == TRUE)
    {
        if(m_UpnpInvokeRequest)
        {
            LocalFree(m_UpnpInvokeRequest);
        }
    }
}


/**
 *  CreateUpnpInvokeRequest:
 *
 *      - creates a upnp invoke request message
 *      - allocates the required space for the incoming invoke request
 *      - packages up the parameters
 *      - sets last error on error and returns false
 *
 */
BOOL UpnpInvokeRequestContainer_t::CreateUpnpInvokeRequest(
    const WCHAR* requestXml,
    const WCHAR* serviceId,
    const WCHAR* udn,
    DWORD hSource,
    DWORD hTarget,
    UPNPCB_ID requestId)
{
    DWORD retCode = TRUE;
    DWORD dataOffset = 0;
    DWORD tagIndex = 0;
    PBYTE dataBuffer = NULL;
    PUpnpRequest pUpnpRequest = NULL;
    DWORD requestXmlLength = 0;
    DWORD serviceIdLength = 0;
    DWORD udnLength = 0;
    DWORD numberOfTagDescriptions = 0;
    HRESULT hr = S_OK;


    TraceTag(ttidControl, "%s: [%08x] Creating UpnpInvokeRequestContainer from parameters\n", __FUNCTION__, this);
    TraceTag(ttidControl, "%s: [%08x] UDN[%S]\n", __FUNCTION__, this, udn);
    TraceTag(ttidControl, "%s: [%08x] ServiceID[%S]\n", __FUNCTION__, this, serviceId);


    requestXmlLength = requestXml ? ((wcslen(requestXml) +1)*sizeof(WCHAR)) : 0;
    serviceIdLength = serviceId ? ((wcslen(serviceId)+1)*sizeof(WCHAR)) : 0;
    udnLength = udn ? ((wcslen(udn)+1)*sizeof(WCHAR)) : 0;


    // calculate the size of buffer required first
    numberOfTagDescriptions = 1;
    if(requestXml) 
    {
        numberOfTagDescriptions++;
    }
    if(serviceId) 
    {
        numberOfTagDescriptions++;
    }
    if(udn) 
    {
        numberOfTagDescriptions++;
    }

    TraceTag(ttidControl, "%s: [%08x] Number of Tags [%d]\n", __FUNCTION__, this, numberOfTagDescriptions);

    DEBUGCHK(numberOfTagDescriptions >= 1);
    DEBUGCHK(numberOfTagDescriptions <= MAXIMUM_NUMBER_OF_TAG_DESCRIPTIONS);

    //
    //  Memory Layout of UPNP Invoke Request
    //
    //  
    //  DWORD number of tag descriptions
    //  Tag Description 0
    //      WORD tag type
    //      WORD tag data length
    //      DWORD tag data offset from start of buffer
    //  Tag Description 2
    //      WORD tag type
    //      WORD tag data length
    //      DWORD tag data offset from start of buffer
    //  Tag Description 3
    //      WORD tag type
    //      WORD tag data length
    //      DWORD tag data offset from start of buffer
    //  Tag Description 4
    //      WORD tag type
    //      WORD tag data length
    //      DWORD tag data offset from start of buffer
    //  PBYTE -- start of tag data
    //

    m_UpnpInvokeRequestBufferSize = (numberOfTagDescriptions*sizeof(TagDescription)) + sizeof(UpnpInvokeRequest);
    hr = DWordAdd(m_UpnpInvokeRequestBufferSize, sizeof(UpnpRequest), &m_UpnpInvokeRequestBufferSize);
    if(FAILED(hr))
    {
        SetLastError(ERROR_ARITHMETIC_OVERFLOW);
        return FALSE;
    }
    
    hr = DWordAdd(m_UpnpInvokeRequestBufferSize, requestXmlLength, &m_UpnpInvokeRequestBufferSize);
    if(FAILED(hr))
    {
        SetLastError(ERROR_ARITHMETIC_OVERFLOW);
        return FALSE;
    }

    hr = DWordAdd(m_UpnpInvokeRequestBufferSize, serviceIdLength, &m_UpnpInvokeRequestBufferSize);
    if(FAILED(hr))
    {
        SetLastError(ERROR_ARITHMETIC_OVERFLOW);
        return FALSE;
    }
    
    hr = DWordAdd(m_UpnpInvokeRequestBufferSize, udnLength, &m_UpnpInvokeRequestBufferSize);
    if(FAILED(hr))
    {
        SetLastError(ERROR_ARITHMETIC_OVERFLOW);
        return FALSE;
    }
    DEBUGCHK(!m_AllocatedInvokeRequest);

    m_UpnpInvokeRequest = static_cast<PUpnpInvokeRequest>(LocalAlloc(LPTR, m_UpnpInvokeRequestBufferSize));
    if(!m_UpnpInvokeRequest)
    {
        SetLastError(ERROR_OUTOFMEMORY);
        return FALSE;
    }
    DEBUGCHK(m_UpnpInvokeRequest);
    m_AllocatedInvokeRequest = TRUE;

    __try
    {
        dataOffset = sizeof(DWORD) + (numberOfTagDescriptions*sizeof(TagDescription));
        dataBuffer = reinterpret_cast<BYTE*>(m_UpnpInvokeRequest) + dataOffset;
        
        m_UpnpInvokeRequest->numberOfTagDescriptions = numberOfTagDescriptions;
        PTagDescription tagDescription = static_cast<PTagDescription>(m_UpnpInvokeRequest->tagDescription);

        // setup UpnpRequest tag
        tagDescription[tagIndex].wTagType = TAG_REQHEADER;
        tagDescription[tagIndex].cbDataLength = sizeof(UpnpRequest);
        tagDescription[tagIndex].dataOffset = dataOffset;
        dataOffset += sizeof(UpnpRequest);
        tagIndex++;

        pUpnpRequest = reinterpret_cast<UpnpRequest*>(dataBuffer);
        pUpnpRequest->cbData = requestXmlLength + serviceIdLength + udnLength;
        pUpnpRequest->hTarget = (DWORD) hTarget;
        pUpnpRequest->hSource = (DWORD) hSource;
        pUpnpRequest->cbId = requestId;
        dataBuffer += sizeof(UpnpRequest);

        if (serviceIdLength)
        {
            tagDescription[tagIndex].wTagType = TAG_SERVICEID;
            tagDescription[tagIndex].cbDataLength = serviceIdLength;
            tagDescription[tagIndex].dataOffset = dataOffset;
            dataOffset += serviceIdLength;
            memcpy(dataBuffer, reinterpret_cast<const BYTE*>(serviceId), serviceIdLength);
            m_ServiceID= reinterpret_cast<WCHAR*>(dataBuffer);
            dataBuffer += serviceIdLength;
            tagIndex++;
        }


        if (udnLength)
        {
            tagDescription[tagIndex].wTagType = TAG_UDN;
            tagDescription[tagIndex].cbDataLength = udnLength;
            tagDescription[tagIndex].dataOffset = dataOffset;
            dataOffset += udnLength;
            memcpy(dataBuffer, reinterpret_cast<const BYTE*>(udn), udnLength);
            m_UDN = reinterpret_cast<WCHAR*>(dataBuffer);
            dataBuffer += udnLength;
            tagIndex++;
        }


        if (requestXmlLength)
        {
            tagDescription[tagIndex].wTagType = TAG_REQSOAP;
            tagDescription[tagIndex].cbDataLength = requestXmlLength;
            tagDescription[tagIndex].dataOffset = dataOffset;
            dataOffset += requestXmlLength;
            memcpy(dataBuffer, reinterpret_cast<const BYTE*>(requestXml), requestXmlLength);
            m_RequestXML = reinterpret_cast<WCHAR*>(dataBuffer);
            dataBuffer += requestXmlLength;
            tagIndex++;
        } 
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        TraceTag(ttidError, "Exception in UpnpInvokeRequest::CreateUpnpInvokeRequest [Error=%d]", GetLastError());
        return FALSE;
    }

    TraceTag(ttidControl, "%s: [%08x] created UpnpInvokeRequestContainer\n", __FUNCTION__, this);
    return TRUE;
}


/**
 *  CreateUpnpInvokeRequest:
 *
 *      - creates a upnp invoke request message
 *      - allocates the required space for the incoming invoke request
 *      - packages up the parameters
 *      - sets last error on error and returns false
 *
 */
BOOL UpnpInvokeRequestContainer_t::CreateUpnpInvokeRequest(
    PBYTE pBuffer, 
    DWORD cbBufferSize)
{
    PBYTE pDataBuffer = NULL;
    if(cbBufferSize < sizeof(UpnpInvokeRequest) + sizeof(UpnpRequest))
    {
        TraceTag(ttidError, "Exception in UpnpInvokeRequest::CreateUpnpInvokeRequest [Error=ERROR_INVALID_PARAMETER]");
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    m_UpnpInvokeRequest = reinterpret_cast<UpnpInvokeRequest*>(pBuffer);
    m_UpnpInvokeRequestBufferSize = cbBufferSize;

    if((m_UpnpInvokeRequest->numberOfTagDescriptions < 1) ||
        (m_UpnpInvokeRequest->numberOfTagDescriptions > MAXIMUM_NUMBER_OF_TAG_DESCRIPTIONS)) 
    {
        TraceTag(ttidError, "Exception in UpnpInvokeRequest::CreateUpnpInvokeRequest [Error=ERROR_INVALID_PARAMETER]");
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    TraceTag(ttidControl, "%s: [%08x] Creating UpnpInvokeRequestContainer from buffer\n", __FUNCTION__, this);

    for(DWORD i=0;i<(m_UpnpInvokeRequest->numberOfTagDescriptions);i++)
    {
        switch(m_UpnpInvokeRequest->tagDescription[i].wTagType)
        {
            case TAG_REQHEADER:
                pDataBuffer = reinterpret_cast<BYTE*>(m_UpnpInvokeRequest) + m_UpnpInvokeRequest->tagDescription[i].dataOffset;
                m_UpnpRequest = reinterpret_cast<UpnpRequest*>(pDataBuffer);
                break;
            case TAG_SERVICEID:
                pDataBuffer = reinterpret_cast<BYTE*>(m_UpnpInvokeRequest) + m_UpnpInvokeRequest->tagDescription[i].dataOffset;
                m_ServiceID = reinterpret_cast<WCHAR*>(pDataBuffer);
                TraceTag(ttidControl, "%s: [%08x] ServiceID[%S]\n", __FUNCTION__, this, m_ServiceID);
                break;
            case TAG_UDN:
                pDataBuffer = reinterpret_cast<BYTE*>(m_UpnpInvokeRequest) + m_UpnpInvokeRequest->tagDescription[i].dataOffset;
                m_UDN = reinterpret_cast<WCHAR*>(pDataBuffer);
                TraceTag(ttidControl, "%s: [%08x] UDN[%S]\n", __FUNCTION__, this, m_UDN);
                break;
            case TAG_REQSOAP:
                pDataBuffer = reinterpret_cast<BYTE*>(m_UpnpInvokeRequest) + m_UpnpInvokeRequest->tagDescription[i].dataOffset;
                m_RequestXML= reinterpret_cast<WCHAR*>(pDataBuffer);
                break;
        }
    }

    TraceTag(ttidControl, "%s: [%08x] created UpnpInvokeRequestHandler\n", __FUNCTION__, this);
    return TRUE;
}


DWORD UpnpInvokeRequestContainer_t::GetUpnpInvokeRequestSize() { return m_UpnpInvokeRequestBufferSize; }
UpnpInvokeRequest* UpnpInvokeRequestContainer_t::GetUpnpInvokeRequest() { return m_UpnpInvokeRequest; }
UpnpRequest* UpnpInvokeRequestContainer_t::GetUpnpRequest() { return m_UpnpRequest; }
const WCHAR* UpnpInvokeRequestContainer_t::GetServiceID() { return m_ServiceID; }
const WCHAR* UpnpInvokeRequestContainer_t::GetRequestXML() { return m_RequestXML; }
const WCHAR* UpnpInvokeRequestContainer_t::GetUDN() { return m_UDN; }
