//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#pragma once

#include <psl_marshaler.hxx>

#define UPNP_IOCTL_INVOKE                       2000
#define SSDP_IOCTL_INVOKE                       2001

#define SSDP_IOCTL_FIRST                        1


#define SSDP_IOCTL_REGISTERNOTIFICATION         1
#define SSDP_IOCTL_DEREGISTERNOTIFICATION       2
#define SSDP_IOCTL_GETSUBSCRIPTIONID            3
#define SSDP_IOCTL_REGISTER_SERVICE             7
#define SSDP_IOCTL_DEREGISTER_SERVICE           8
#define SSDP_IOCTL_LAST                         10


#define UPNP_IOCTL_FIRST                        20
#define UPNP_IOCTL_ADD_DEVICE                   21
#define UPNP_IOCTL_REMOVE_DEVICE                22
#define UPNP_IOCTL_PUBLISH_DEVICE               23
#define UPNP_IOCTL_UNPUBLISH_DEVICE             24
#define UPNP_IOCTL_SUBMIT_PROPERTY_EVENT        25
#define UPNP_IOCTL_SET_RAW_CONTROL_RESPONSE     26
#define UPNP_IOCTL_GET_UDN                      27
#define UPNP_IOCTL_GET_SCPD_PATH                28
#define UPNP_IOCTL_PROCESS_CALLBACKS			29
#define UPNP_IOCTL_CANCEL_CALLBACKS				30
#define UPNP_IOCTL_UPDATE_EVENTED_VARIABLES     31
#define UPNP_IOCTL_LAST                         32


typedef DWORD (*PFUPNPCALLBACK)(PVOID pvContext, PBYTE pbInBuf, DWORD cbInBuf);

//
// ce::psl_data_wrapper specialization for PFUPNPCALLBACK
//
template<>
class ce::psl_data_wrapper<PFUPNPCALLBACK>
{
public:
    void marshal(PFUPNPCALLBACK x)
        {data = x; }
    
    PFUPNPCALLBACK unmarshal(HANDLE)
        {return data; }

private:
    PFUPNPCALLBACK data;
};


//
// ce::psl_data_wrapper specialization for SSDP_MESSAGE*
//
template<>
class ce::psl_data_wrapper<SSDP_MESSAGE*>
{
public:
    void marshal(SSDP_MESSAGE* x)
        {data = *x; }
    
    SSDP_MESSAGE* unmarshal(HANDLE hClientProcess)
    {
        data.szAltHeaders = (LPSTR)MapPtrToProcWithSize((LPVOID)data.szAltHeaders, sizeof(*data.szAltHeaders), hClientProcess);
        data.szContent    = (LPSTR)MapPtrToProcWithSize((LPVOID)data.szContent, sizeof(*data.szContent), hClientProcess);
        data.szLocHeader  = (LPSTR)MapPtrToProcWithSize((LPVOID)data.szLocHeader, sizeof(*data.szLocHeader), hClientProcess);
        data.szNls        = (LPSTR)MapPtrToProcWithSize((LPVOID)data.szNls, sizeof(*data.szNls), hClientProcess);
        data.szSid        = (LPSTR)MapPtrToProcWithSize((LPVOID)data.szSid, sizeof(*data.szSid), hClientProcess);
        data.szType       = (LPSTR)MapPtrToProcWithSize((LPVOID)data.szType, sizeof(*data.szType), hClientProcess);
        data.szUSN        = (LPSTR)MapPtrToProcWithSize((LPVOID)data.szUSN, sizeof(*data.szUSN), hClientProcess);
         
        return &data;
    }

private:
    SSDP_MESSAGE data;
};


//
// ce::psl_data_wrapper specialization for UPNPDEVICEINFO*
//
template<>
class ce::psl_data_wrapper<UPNPDEVICEINFO*>
{
public:
    void marshal(UPNPDEVICEINFO* x)
        {data = *x; }
    
    UPNPDEVICEINFO* unmarshal(HANDLE hClientProcess)
    {
        data.pszDeviceDescription = (PWSTR)MapPtrToProcWithSize((LPVOID)data.pszDeviceDescription, sizeof(*data.pszDeviceDescription), hClientProcess);
        data.pszDeviceName        = (PWSTR)MapPtrToProcWithSize((LPVOID)data.pszDeviceName, sizeof(*data.pszDeviceName), hClientProcess);
        data.pszUDN               = (PWSTR)MapPtrToProcWithSize((LPVOID)data.pszUDN, sizeof(*data.pszUDN), hClientProcess);
         
        return &data;
    }

private:
    UPNPDEVICEINFO data;
};


//
// ce::psl_data_wrapper specialization for UPNPPARAM*
// undefined - use UPNPPARAMS for marshaling
//
template<>
class ce::psl_data_wrapper<UPNPPARAM*>;


// UPNPPARAMS
struct UPNPPARAMS
{
    UPNPPARAMS()
        : nParams(0),
          pParams(NULL)
    {}
    
    UPNPPARAMS(DWORD n, UPNPPARAM* p)
        : nParams(n),
          pParams(p)
    {}
    
    DWORD       nParams;
    UPNPPARAM*  pParams;
};


//
// ce::psl_data_wrapper specialization for UPNPPARAMS
//
template<>
class ce::psl_data_wrapper<UPNPPARAMS>
{
public:
    psl_data_wrapper()
    {
        ASSERT(NULL == data.pParams);
    }
    
    ~psl_data_wrapper()
    {
        delete [] data.pParams;
    }

    void marshal(UPNPPARAMS x)
    {
        if(data.pParams = new UPNPPARAM[x.nParams])
        {
            data.nParams = x.nParams;

            for(unsigned i = 0; i < data.nParams; ++i)
            {
                data.pParams[i].pszName = x.pParams[i].pszName;
                data.pParams[i].pszValue = x.pParams[i].pszValue;
            }
        }
    }
    
    UPNPPARAMS unmarshal(HANDLE hClientProcess)
    {
        if(data.pParams = (UPNPPARAM*)MapPtrToProcWithSize((LPVOID)data.pParams, data.nParams * sizeof(*data.pParams), hClientProcess))
        {
            for(unsigned i = 0; i < data.nParams; ++i)
            {
                data.pParams[i].pszName = (LPWSTR)MapPtrToProcWithSize((LPVOID)data.pParams[i].pszName, sizeof(*data.pParams[i].pszName), hClientProcess);
                data.pParams[i].pszValue = (LPWSTR)MapPtrToProcWithSize((LPVOID)data.pParams[i].pszValue, sizeof(*data.pParams[i].pszValue), hClientProcess);
            }
        }

        return data;
    }

private:
    UPNPPARAMS data;    
};


extern "C" {
BOOL WINAPI
SDP_IOControl(
    DWORD  dwOpenData, 
    DWORD  dwCode, 
    PBYTE  pBufIn,
    DWORD  dwLenIn, 
    PBYTE  pBufOut, 
    DWORD  dwLenOut,
    PDWORD pdwActualOut
    );
}

#define MAX_UPNP_MESSAGE_LENGTH 0x6000      // 24K 

#define TAG_REQHEADER		1
#define TAG_SERVICEID		2
#define TAG_REQSOAP			3
#define TAG_UDN             4


typedef struct _TAG_DESC
{
	WORD wTag;
	WORD cbLength;
	PBYTE pbData;
} TAG_DESC;

typedef struct _UPNP_REQ
{
    DWORD cbData;   // length of data tagged onto the end of this structure
    DWORD hSource;
    DWORD hTarget;
    UPNPCB_ID cbId;
}UPNP_REQ;

typedef struct _UPNP_REP
{
    DWORD cbData;   // length of data tagged onto the end of this structure
    DWORD dwRetCode;
    DWORD dwHttpStatus;
} UPNP_REP;

