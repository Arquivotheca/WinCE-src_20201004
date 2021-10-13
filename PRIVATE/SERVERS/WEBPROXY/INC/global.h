//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#ifndef __GLOBAL_H__
#define __GLOBAL_H__

#include <windows.h>
#include <winsock2.h>
#include "webproxy.h"

#define RK_WEBPROXY					L"Comm\\WebProxy"
#define RV_MAX_CONNECTIONS			L"MaxConnections"
#define RV_SESSION_TIMEOUT			L"SessionTimeoutSeconds"
#define RV_SESSION_THREADS			L"SessionThreads"
#define RV_PORT						L"Port"
#define RV_AUTH						L"NTLMAuth"
#define RV_FILTERLIB				L"FilterLib"
#define RV_MAXHEADERSSIZE			L"MaxHeadersSize"
#define RV_PROXYSTRINGSIZE			L"ProxyErrorStringSize"

#define RK_ICS						L"Comm\\ConnectionSharing"
#define RV_PUBLIC_INTF				L"PublicInterface"
#define RV_PRIVATE_INTF				L"PrivateInterface"

#define DEFAULT_FILTER_LIB_SZ		L"prxfltr.dll"
#define FILTER_REQUEST_PROC_SZ		L"ProxyFilterHttpRequest"
#define FILTER_INIT_PROC_SZ			L"ProxyInitializeFilter"
#define FILTER_UNINIT_PROC_SZ		L"ProxyUninitializeFilter"
#define FILTER_ADDR_CHANGE_PROC_SZ	L"ProxyNotifyAddrChange"
#define DEFAULT_HOST_NAME_SZ		"wceproxy"

#define DEFAULT_HTTPS_PORT			443
#define DEFAULT_HTTPS_PORT_SZ		"443"

#define DEFAULT_PROXY_PORT			8080
#define DEFAULT_HTTP_PORT			80

#define DEFAULT_MAX_HEADERS_SIZE	24576
#define DEFAULT_MAX_CONNECTIONS		30
#define DEFAULT_AUTH				1

#define DEFAULT_SESSION_TIMEOUT		60
#define DEFAULT_SESSION_THREADS		10

// Use this buffer size since most servers return size of 1460 (2922 = 1460*2 + 1 byte
// since proxy appends '\0' to packets)
#define DEFAULT_BUFFER_SIZE			2921	

#define DEFAULT_PROXY_STRING_SIZE	512
#define FREE_LIST_SIZE				10


typedef DWORD (*PFN_ProxyFilterHttpRequest) (PPROXY_HTTP_REQUEST pRequest);
typedef DWORD (*PFN_ProxyInitializeFilter) (PPROXY_HTTP_INFORMATION pInfo);
typedef DWORD (*PFN_ProxyUninitializeFilter) (void);
typedef DWORD (*PFN_ProxyNotifyAddrChange) (PPROXY_ADDRCHANGE_PROPS pProps);

extern PFN_ProxyFilterHttpRequest ProxyFilterHttpRequest;
extern PFN_ProxyInitializeFilter ProxyInitializeFilter;
extern PFN_ProxyUninitializeFilter ProxyUninitializeFilter;
extern PFN_ProxyNotifyAddrChange ProxyNotifyAddrChange;

#endif // __GLOBAL_H__

