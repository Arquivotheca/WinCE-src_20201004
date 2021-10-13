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

#ifndef __GLOBAL_H__
#define __GLOBAL_H__

#include <windows.h>
#include <winsock2.h>
#include "webproxy.h"
#include <string.hxx>
#include "utils.h"
#include <svsutil.hxx>
#include <strsafe.h>
#include <errorrep.h>

#define RK_WEBPROXY                     L"Comm\\WebProxy"
#define RK_FILTERS                      L"Comm\\WebProxy\\Filters"
#define RV_MAX_CONNECTIONS              L"MaxConnections"
#define RV_SESSION_TIMEOUT              L"SessionTimeoutSeconds"
#define RV_SESSION_THREADS              L"SessionThreads"
#define RV_PORT                         L"Port"
#define RV_AUTH_NTLM                    L"NTLMAuth"
#define RV_AUTH_BASIC                   L"BasicAuth"
#define RV_FILTERLIB                    L"FilterList"
#define RV_MAXHEADERSSIZE               L"MaxHeadersSize"
#define RV_PROXYSTRINGSIZE              L"ProxyErrorStringSize"
#define RV_SECOND_PROXY_HOST            L"SecondProxyHost"
#define RV_SECOND_PROXY_PORT            L"SecondProxyPort"
#define RK_ICS                          L"Comm\\ConnectionSharing"
#define RV_PUBLIC_INTF                  L"PublicInterface"
#define RV_PRIVATE_INTF                 L"PrivateInterface"

#define DEFAULT_FILTER_LIB_SZ           L"prxfltr.dll"
#define FILTER_REQUEST_PROC_SZ          L"ProxyFilterHttpRequest"
#define FILTER_INIT_PROC_SZ             L"ProxyInitializeFilter"
#define FILTER_UNINIT_PROC_SZ           L"ProxyUninitializeFilter"
#define FILTER_ADDR_CHANGE_PROC_SZ      L"ProxyNotifyAddrChange"
#define FILTER_SIGNAL_FILTER_PROC_SZ    L"ProxySignalFilter"
#define DEFAULT_HOST_NAME_SZ            "wceproxy"

#define DEFAULT_HTTPS_PORT              443
#define DEFAULT_HTTPS_PORT_SZ           "443"
#define DEFAULT_PROXY_PORT              8080
#define DEFAULT_HTTP_PORT               80
#define DEFAULT_MAX_BUFFER_SIZE         24576
#define DEFAULT_MAX_CONNECTIONS         20
#define DEFAULT_AUTH_NTLM               1
#define DEFAULT_AUTH_BASIC              1
#define DEFAULT_SESSION_TIMEOUT         30
#define MAX_CHUNKED_READ_ATTEMPTS       3
#define MAX_LEN_NUMBER_STR              10
#define DEFAULT_PROXY_STRING_SIZE       512
#define FREE_LIST_SIZE                  20
#define MAX_ADDRESS_SIZE                256
#define MAX_SOCKET_LIST                 64

class ProxySettings {
public:
    ProxySettings() {
        InitializeCriticalSection(&csSettings);
    }
    ~ProxySettings() {
        DeleteCriticalSection(&csSettings);
    }
    
    void Reset(void) {
        strHostName = "";
        strPrivateAddrV4 = "";
        strPrivateMaskV4 = "";
        strSecondProxyHost = "";
        iSecondProxyPort = DEFAULT_HTTP_PORT;
        strProxyPacURL = "http://192.168.0.1/proxy.pac";
        iPort = DEFAULT_PROXY_PORT;
        iMaxConnections = DEFAULT_MAX_CONNECTIONS;
        iMaxBufferSize = DEFAULT_MAX_BUFFER_SIZE;
        fNTLMAuth = DEFAULT_AUTH_NTLM;
        fBasicAuth = DEFAULT_AUTH_BASIC;
        iSessionTimeout = DEFAULT_SESSION_TIMEOUT;
    }

    void Lock(void) {
        EnterCriticalSection(&csSettings);
    }
    void Unlock(void) {
        LeaveCriticalSection(&csSettings);
    }

    CRITICAL_SECTION csSettings;
    
    ce::string strHostName;
    ce::string strPrivateAddrV4;
    ce::string strPrivateMaskV4;
    stringi strSecondProxyHost;
    int iSecondProxyPort;
    stringi strProxyPacURL;
    int iPort;
    int iMaxConnections;
    int iMaxBufferSize;
    BOOL fNTLMAuth;
    BOOL fBasicAuth;
    int iSessionTimeout;
};


class ProxyErrors {
public:
    char* szURLNotFound;
    int cchURLNotFound;
    char* szProxyAuthFailed;
    int cchProxyAuthFailed;
    char* szHeaderNotSupported;
    int cchHeaderNotSupported;
    char* szProtocolNotSupported;
    int cchProtocolNotSupported;
    char* szVersionNotSupported;
    int cchVersionNotSupported;
    char* szMiscError;
    int cchMiscError;
};

extern ProxySettings* g_pSettings;
extern ProxyErrors* g_pErrors;
extern SVSThreadPool* g_pThreadPool;

#endif // __GLOBAL_H__

