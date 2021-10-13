//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#pragma once

#define SSDP_IOCTL_FIRST                        1


#define SSDP_IOCTL_REGISTERNOTIFICATION         1
struct RegisterNotificationParams{
    HANDLE*		phNotify;  // ptr
    DWORD		nt;
    PCWSTR		pwszUSN;// ptr
    PCWSTR		pwszQueryString;// ptr
    PCWSTR		pwszMsgQueue;//ptr
};

#define SSDP_IOCTL_DEREGISTERNOTIFICATION       2
struct DeregisterNotificationParams{
    HANDLE hNotify;
};

#define SSDP_IOCTL_GETSUBSCRIPTIONID            3
struct GetSubscriptionIdParams {
    HANDLE hNotify;     // IN
    PDWORD pcbSid;      // IN,OUT   ptr
    PSTR   pszSid;      // OUT  ptr
    PDWORD pdwTimeout;  // OUT  ptr
};

/*#define SSDP_IOCTL_LOOKUPCACHE                  4
struct LookupCacheParams
{
    PSTR szType;
    SERVICE_CALLBACK_FUNC pfSearchCallback;
    LPVOID pvContext;
};

#define SSDP_IOCTL_UPDATECACHE                  5
struct UpdateCacheParams {
    PSSDP_MESSAGE pSsdpMessage;
};

#define SSDP_IOCTL_CLEANUPCACHE                 6*/


#define SSDP_IOCTL_LAST                         10


#define UPNP_IOCTL_FIRST                        20
#define UPNP_IOCTL_ADD_DEVICE                   21
struct AddDeviceParams
{
    UPNPDEVICEINFO *pDeviceInfo;
};
#define UPNP_IOCTL_REMOVE_DEVICE                22
#define UPNP_IOCTL_PUBLISH_DEVICE               23
#define UPNP_IOCTL_UNPUBLISH_DEVICE             24
#define UPNP_IOCTL_SUBMIT_PROPERTY_EVENT        25
struct SubmitPropertyEventParams 
{
    PCWSTR pszDeviceName;
    PCWSTR pszUDN;
    PCWSTR pszServiceId;
    DWORD nArgs;
    UPNPPARAM *rgArgs;
};

#define UPNP_IOCTL_SET_RAW_CONTROL_RESPONSE     26
struct SetRawControlResponseParams
{
    DWORD hRequest;
    DWORD dwHttpStatus;
    PCWSTR pszRespXML;
};

#define UPNP_IOCTL_GET_UDN                      27
struct GetUDNParams
{
    PCWSTR pszDeviceName;
    PCWSTR pszTemplateUDN;
    PWSTR  pszUDNBuf;
    PDWORD pchBuf;
};
#define UPNP_IOCTL_GET_SCPD_PATH                28
struct GetSCPDPathParams
{
    PCWSTR pszDeviceName;
    PCWSTR pszUDN;
    PCWSTR pszServiceId;
    PWSTR  pszSCPDPath;
    DWORD  cchPath;
};

#define UPNP_IOCTL_PROCESS_CALLBACKS			 29
typedef DWORD  (*PFUPNPCALLBACK) (PVOID pvContext, PBYTE pbInBuf, DWORD cbInBuf);
struct ProcessCallbacksParams
{
	PFUPNPCALLBACK pfCallback;
	PVOID pvContext;
};

#define UPNP_IOCTL_CANCEL_CALLBACKS				 30
#define UPNP_IOCTL_LAST                         32

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

