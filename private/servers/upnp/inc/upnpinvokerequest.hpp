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
#ifndef __UPNP_INVOKE_REQUEST__HPP__INCLUDED__
#define __UPNP_INVOKE_REQUEST__HPP__INCLUDED__

#include <windows.h>
#include <upnpdevapi.h>


// maximum message length = 24K x sizeof(wchar_t)
#define MAXIMUM_UPNP_MESSAGE_LENGTH 0xA000
#define MAXIMUM_NUMBER_OF_TAG_DESCRIPTIONS 4

// maximum of 4 different upnp request tags
#define TAG_REQHEADER        1
#define TAG_SERVICEID        2
#define TAG_REQSOAP          3
#define TAG_UDN              4

typedef struct _TagDescription
{
    DWORD wTagType;
    DWORD cbDataLength;
    DWORD dataOffset;
} TagDescription, *PTagDescription;


typedef struct _UpnpRequest
{
    DWORD cbData;   // length of data tagged onto the end of this structure
    DWORD hSource; 
    DWORD hTarget;
    UPNPCB_ID cbId;
} UpnpRequest, *PUpnpRequest;


typedef struct _UpnpResponse
{
    DWORD cbData;   // length of data tagged onto the end of this structure
    DWORD dwRetCode;
    DWORD dwHttpStatus;
} UpnpResponse, *PUpnpResponse;



// data after the UpnpInvokeRequest contains information
typedef struct _UpnpInvokeRequest {
    DWORD numberOfTagDescriptions;
    TagDescription tagDescription[1];
} UpnpInvokeRequest, *PUpnpInvokeRequest;




/**
 *  UpnpInvokeRequestContainer_t:
 *
 *      - maintains an allocated buffer representing a upnp invoke request
 *
 */
class UpnpInvokeRequestContainer_t{
    public:
        // initialize container
        UpnpInvokeRequestContainer_t();


        // cleanup allocated resources
        ~UpnpInvokeRequestContainer_t();


        // create a upnp invoke request buffer from data                                        
        BOOL CreateUpnpInvokeRequest(
                                        const WCHAR* requestXml,
                                        const WCHAR* serviceId,
                                        const WCHAR* udn,
                                        DWORD hSource,
                                        DWORD hTarget,
                                        UPNPCB_ID requestId);


        // parse a buffer for upnp invoke request data
        BOOL CreateUpnpInvokeRequest(
                                        PBYTE pBuffer, 
                                        DWORD cbBufferSize);


        // accessors
        DWORD GetUpnpInvokeRequestSize();
        UpnpInvokeRequest* GetUpnpInvokeRequest();
        UpnpRequest* GetUpnpRequest();
        const WCHAR* GetServiceID();
        const WCHAR* GetRequestXML();
        const WCHAR* GetUDN();

    private:
        BOOL m_AllocatedInvokeRequest;
        UpnpInvokeRequest* m_UpnpInvokeRequest;
        DWORD m_UpnpInvokeRequestBufferSize;

        UpnpRequest* m_UpnpRequest;
        WCHAR* m_ServiceID;
        WCHAR* m_RequestXML;
        WCHAR* m_UDN;
};

#endif
