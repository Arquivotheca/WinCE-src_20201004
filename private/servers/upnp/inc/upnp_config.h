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
#ifndef __UPNP_CONFIG_H__
#define __UPNP_CONFIG_H__

#include <winsock2.h>
#include <iptypes.h>
#include <string.hxx>
#include <auto_xxx.hxx>

class upnp_config
{
    struct settings
    {
        bool        bInitialized;
        int         nPort;
        int         nFamily;
        unsigned    nScope;
        unsigned    nTTL;
        int         nMaxSubscribers;
        unsigned    nDefaultSubTimeout;
        unsigned    nMinSubTimeout;
        unsigned    nMaxSubTimeout;
        unsigned    nNotificationQueueSize;
        unsigned    nMaxControlPointDelay;
        int         nMaxDocumentSize;
        int         nMaxActionResponse;
        unsigned    nMaxSearchResults;
        int         nSiteScope;
        unsigned    nIPv6TTL;
        BOOL        bUseNlsForIPv4;
    };
    
public:
    // refresh the configuration
    static void refresh_configuration()
        {get_settings()->bInitialized = false; get_settings(); }
    
    // port
    static int port()
        {return get_settings()->nPort; }
        
    // family
    static int family()
        {return get_settings()->nFamily; }
        
    // scope
    static unsigned scope()
        {return get_settings()->nScope; }
        
    // TTL
    static unsigned TTL()
        {return get_settings()->nTTL; }
    
    // IPv6TTL
    static unsigned IPv6TTL()
        {return get_settings()->nIPv6TTL; }
        
    // max_subscribers
    static int max_subscribers()
        {return get_settings()->nMaxSubscribers; }
        
    // default_sub_timeout
    static unsigned default_sub_timeout()
        {return get_settings()->nDefaultSubTimeout; }    
    
    // min_sub_timeout
    static unsigned min_sub_timeout()
        {return get_settings()->nMinSubTimeout; }    

    // max_sub_timeout
    static unsigned max_sub_timeout()
        {return get_settings()->nMaxSubTimeout; }    

    // notification_queue_size
    static unsigned notification_queue_size()
        {return get_settings()->nNotificationQueueSize; }    
    
    // max_control_point_delay
    static unsigned max_control_point_delay()
        {return get_settings()->nMaxControlPointDelay; }    
        
    // max_document_size
    static int max_document_size()
        {return get_settings()->nMaxDocumentSize; }
        
    // max_action_response
    static int max_action_response()
        {return get_settings()->nMaxActionResponse; }
        
    static unsigned max_search_results()
        {return get_settings()->nMaxSearchResults; }
    
    // site_scope
    static int site_scope()
        {return get_settings()->nSiteScope; }
    
    // use_nls_for_IPv4
    static BOOL use_nls_for_IPv4()
        {return get_settings()->bUseNlsForIPv4; }
        
    // is_adapter_enabled
    static bool is_adapter_enabled(LPCSTR pszAdapterName)
    {
        ce::auto_hkey   hkeyExclude;
        ce::wstring     strAdapterName;
                    
        if(!ce::MultiByteToWideChar(CP_OEMCP, pszAdapterName, -1, &strAdapterName))
        {
            return false;
        }
        
        if(ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"COMM\\UPnP\\Exclude", 0, 0, &hkeyExclude))
        {
            DWORD   bExcludeAllExceptListed = FALSE;
            DWORD   cbSize, cbInterfaces;
            bool    bUseTaliskerSettings = true;
                
            // get size of Interfaces entry
            if(ERROR_SUCCESS == RegQueryValueEx(hkeyExclude, L"Interfaces", NULL, NULL, NULL, &(cbInterfaces = 0)))
            {
                bUseTaliskerSettings = false;
            }
                
            // get ExcludeAllExceptListed value
            if(ERROR_SUCCESS == RegQueryValueEx(hkeyExclude, L"ExcludeAllExceptListed", NULL, NULL, reinterpret_cast<LPBYTE>(&bExcludeAllExceptListed), &(cbSize = sizeof(bExcludeAllExceptListed))))
            {
                bUseTaliskerSettings = false;
            }
            
            if(bUseTaliskerSettings)
            {
                // the new exclusion entries don't exist;
                // check if there is Talisker-style entry for exclusion of this adapter
                DWORD   bExcludeAdapter = FALSE;
                
                RegQueryValueEx(hkeyExclude, strAdapterName, NULL, NULL, reinterpret_cast<LPBYTE>(&bExcludeAdapter), &(cbSize = sizeof(bExcludeAdapter)));
                
                return bExcludeAdapter == FALSE;
            }
            
            // look for the adapter on the explicit list
            if(wchar_t* pszAdapters = (wchar_t*)operator new(cbInterfaces))
            {
                if(ERROR_SUCCESS == RegQueryValueEx(hkeyExclude, L"Interfaces", NULL, NULL, reinterpret_cast<LPBYTE>(pszAdapters), &cbInterfaces))
                {
                    // check if adapter name is on the list
                    for(wchar_t *pwsz = pszAdapters; *pwsz && (pwsz - pszAdapters < (int)(cbInterfaces / sizeof(wchar_t))) ; pwsz += wcslen(pwsz) + 1)
                        if(strAdapterName == pwsz)
                        {
                            delete pszAdapters;
                            return bExcludeAllExceptListed != FALSE;
                        }
                }
                
                delete pszAdapters;
            }
                                 
            return bExcludeAllExceptListed == FALSE;
        }
        else
        {
            ce::auto_hkey hkeyICS;
            
            if(ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"Comm\\ConnectionSharing", 0, 0, &hkeyICS))
            {
                ce::wstring strPublicInterface;
                
                ce::RegQueryValue(hkeyICS, L"PublicInterface", strPublicInterface);
                
                // check if adapter name is on the list
                for(const wchar_t *pwsz = strPublicInterface; *pwsz && (pwsz - strPublicInterface < (int)(strPublicInterface.length())) ; pwsz += wcslen(pwsz) + 1)
                {
                    if(strAdapterName == pwsz)
                    {
                        return false;
                    }
                }
            }
        }
        
        return true;
    }
        
protected:
    static settings* get_settings()
    {
        static settings settings = {0};
        
        if(!settings.bInitialized)
        {
            settings.bInitialized = true;
            
            // defaults
            settings.nPort = 80;
            settings.nFamily = AF_UNSPEC;
            settings.nScope = 1;
            settings.nTTL = 4;
            settings.nMaxSubscribers = 30;
            settings.nDefaultSubTimeout = 300;
            settings.nMinSubTimeout = 300;
            settings.nMaxSubTimeout = 300;
            settings.nNotificationQueueSize = 128;
            settings.nMaxControlPointDelay = 3000;
            settings.nMaxDocumentSize = 200 * 1024;
            settings.nMaxActionResponse = 280 * 1024;  //DLNA req: max response size is 200KB
            settings.nMaxSearchResults = 100;
            settings.nSiteScope = 0;
            settings.nIPv6TTL = 255; // set TTL to 255 because over IPv6 we use address scope to control propagation of UPnP messages
            settings.bUseNlsForIPv4 = FALSE;
            
            // read settings from registry
            ce::auto_hkey   hkey;
            DWORD           cbSize, dw;
            
            if(ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"COMM\\UPnP", 0, 0, &hkey))
            {
                // port
                RegQueryValueEx(hkey, L"Port", NULL, NULL, reinterpret_cast<LPBYTE>(&settings.nPort), &(cbSize = sizeof(settings.nPort)));
                
                // address family
                if(ERROR_SUCCESS == RegQueryValueEx(hkey, L"IPVersionSetting", NULL, NULL, reinterpret_cast<LPBYTE>(&dw), &(cbSize = sizeof(dw))))
                {
                    switch(dw)
                    {
                        case 1: settings.nFamily = AF_INET;
                                break;
                        
                        case 2: settings.nFamily = AF_INET6;
                                break;
                        
                        case 3: settings.nFamily = AF_UNSPEC;
                                break;
                    }
                }
                
                // scope
                RegQueryValueEx(hkey, L"DownloadScope", NULL, NULL, reinterpret_cast<LPBYTE>(&settings.nScope), &(cbSize = sizeof(settings.nScope)));
                
                // site scope
                RegQueryValueEx(hkey, L"IPv6SiteScope", NULL, NULL, reinterpret_cast<LPBYTE>(&settings.nSiteScope), &(cbSize = sizeof(settings.nSiteScope)));
                
                // TTL
                RegQueryValueEx(hkey, L"DiscoveryTimeToLive", NULL, NULL, reinterpret_cast<LPBYTE>(&settings.nTTL), &(cbSize = sizeof(settings.nTTL)));
                
                // IPv6 TTL
                RegQueryValueEx(hkey, L"IPv6TTL", NULL, NULL, reinterpret_cast<LPBYTE>(&settings.nIPv6TTL), &(cbSize = sizeof(settings.nIPv6TTL)));
                
                // max subscribers
                RegQueryValueEx(hkey, L"MaxSubscribers", NULL, NULL, reinterpret_cast<LPBYTE>(&settings.nMaxSubscribers), &(cbSize = sizeof(settings.nMaxSubscribers)));
                
                // default sub timeout
                RegQueryValueEx(hkey, L"SubDefaultTimeout", NULL, NULL, reinterpret_cast<LPBYTE>(&settings.nDefaultSubTimeout), &(cbSize = sizeof(settings.nDefaultSubTimeout)));

                if(settings.nDefaultSubTimeout > 0xFFFFFFFF / 1000)
                {
                    settings.nDefaultSubTimeout = 0xFFFFFFFF / 1000;
                }
                
                // min sub timeout
                RegQueryValueEx(hkey, L"SubMinTimeout", NULL, NULL, reinterpret_cast<LPBYTE>(&settings.nMinSubTimeout), &(cbSize = sizeof(settings.nMinSubTimeout)));
                
                if(settings.nMinSubTimeout > 0xFFFFFFFF / 1000)
                {
                    settings.nMinSubTimeout = 0xFFFFFFFF / 1000;
                }
                
                // max sub timeout
                RegQueryValueEx(hkey, L"SubMaxTimeout", NULL, NULL, reinterpret_cast<LPBYTE>(&settings.nMaxSubTimeout), &(cbSize = sizeof(settings.nMaxSubTimeout)));
                
                if(settings.nMaxSubTimeout > 0xFFFFFFFF / 1000)
                {
                    settings.nMaxSubTimeout = 0xFFFFFFFF / 1000;
                }
                
                // notification queue
                RegQueryValueEx(hkey, L"NotificationQueueSize", NULL, NULL, reinterpret_cast<LPBYTE>(&settings.nNotificationQueueSize), &(cbSize = sizeof(settings.nNotificationQueueSize)));
                
                // max control point delay
                RegQueryValueEx(hkey, L"MaxControlPointDelay", NULL, NULL, reinterpret_cast<LPBYTE>(&settings.nMaxControlPointDelay), &(cbSize = sizeof(settings.nMaxControlPointDelay)));
                
                // max document size
                RegQueryValueEx(hkey, L"MaxDocumentSize", NULL, NULL, reinterpret_cast<LPBYTE>(&settings.nMaxDocumentSize), &(cbSize = sizeof(settings.nMaxDocumentSize)));
                
                // max action response
                RegQueryValueEx(hkey, L"MaxActionResponse", NULL, NULL, reinterpret_cast<LPBYTE>(&settings.nMaxActionResponse), &(cbSize = sizeof(settings.nMaxActionResponse)));
                
                // max search results
                RegQueryValueEx(hkey, L"MaxSearchResults", NULL, NULL, reinterpret_cast<LPBYTE>(&settings.nMaxSearchResults), &(cbSize = sizeof(settings.nMaxSearchResults)));
                
                // use NLS header for IPv4-only devices
                RegQueryValueEx(hkey, L"UseNlsForIPv4", NULL, NULL, reinterpret_cast<LPBYTE>(&settings.bUseNlsForIPv4), &(cbSize = sizeof(settings.bUseNlsForIPv4)));
            }
        }
        
        return &settings;
    }
};

#endif // __UPNP_CONFIG_H__
