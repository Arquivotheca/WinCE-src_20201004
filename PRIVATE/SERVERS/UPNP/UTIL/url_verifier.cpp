//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>

#ifdef UTIL_HTTPLITE
#	include "dubinet.h"
#else
#	include "wininet.h"
#endif

#include "url_verifier.h"

char* url_verifier::szaddrPrivate[TARGET_PRIVATE_ADDRS] = {
    {"10.0.0.0"},
    {"169.254.0.0"},
    {"192.168.0.0"},
    {"172.16.0.0"},
    {"127.0.0.1"},
};

char* url_verifier::szmaskPrivate[TARGET_PRIVATE_ADDRS] = {
    {"255.0.0.0"},
    {"255.255.0.0"},
    {"255.255.0.0"},
    {"255.240.0.0"},
    {"255.255.255.255"},
};

sockaddr_in url_verifier::addrPrivate[TARGET_PRIVATE_ADDRS];
sockaddr_in url_verifier::maskPrivate[TARGET_PRIVATE_ADDRS];

// url_verifier
url_verifier::url_verifier()
    : m_bAny(false),
      m_bPrivate(false),
      m_nTTL(0),
      m_bTTL(false),
      m_nSiteScope(0)
{
    for(int i=0; i < TARGET_PRIVATE_ADDRS; i++)
    {
        addrPrivate[i].sin_addr.s_addr = inet_addr(szaddrPrivate[i]);
        maskPrivate[i].sin_addr.s_addr = inet_addr(szmaskPrivate[i]);

    }
}


// is_url_ok
bool url_verifier::is_url_ok(LPCSTR pszURL)
{
    if(m_bAny)
        return true;

    URL_COMPONENTSA     urlComp = {0};
    char                pszHost[INTERNET_MAX_URL_LENGTH + 1];
    struct addrinfo     hints = {0}, *ai;
    bool                bLocal = false;

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM; 
    hints.ai_flags = AI_PASSIVE;

    ///////////////////////////////
    // crack URL
    urlComp.dwStructSize = sizeof(URL_COMPONENTS);
    urlComp.lpszHostName = pszHost;
    urlComp.dwHostNameLength = INTERNET_MAX_URL_LENGTH;

    if(FALSE == InternetCrackUrlA(pszURL, 0, 0, &urlComp))
        return false;
    
    if(ERROR_SUCCESS != getaddrinfo(pszHost, "", &hints, &ai))
        return false;
    
    if(ai->ai_family == AF_INET)
    {
        ///////////////////////////////////
        // check if IPv4 host is local
        PIP_ADAPTER_INFO    pip = NULL;
        ULONG               ulSize = 0;
        
        GetAdaptersInfo(NULL, &ulSize);
        if(ulSize)
        {
            pip = reinterpret_cast<PIP_ADAPTER_INFO>(operator new(ulSize));

            DWORD dwRet = GetAdaptersInfo(pip, &ulSize);

            if(ERROR_SUCCESS == dwRet)
            {
                for(PIP_ADAPTER_INFO pipIter = pip; pipIter && !bLocal; pipIter = pipIter->Next)
                {
                    for(PIP_ADDR_STRING pIpAddr = &pipIter->IpAddressList; pIpAddr && !bLocal; pIpAddr = pIpAddr->Next)
                    {
                        sockaddr_in addrLocal;
                        sockaddr_in maskLocal;

                        addrLocal.sin_addr.s_addr = inet_addr(pIpAddr->IpAddress.String);
                        maskLocal.sin_addr.s_addr = inet_addr(pIpAddr->IpMask.String);
                        
                        if (addrLocal.sin_addr.s_addr != 0)
                            if(is_ipv4_addr_local((sockaddr_in*)ai->ai_addr, addrLocal, maskLocal))
                                bLocal = true;
                    }
                }
            }

            delete pip;
        }
    }

    if(ai->ai_family == AF_INET6)
    {
        bLocal = is_ipv6_addr_local((sockaddr_in6*)ai->ai_addr);
    }
    
    freeaddrinfo(ai);

    return bLocal;
}


// is_addr_local
bool url_verifier::is_ipv4_addr_local(sockaddr_in* paddrTarget, sockaddr_in addrLocal, sockaddr_in maskLocal)
{
    // is this on my subnet?
    if ((addrLocal.sin_addr.s_addr & maskLocal.sin_addr.s_addr) ==
        (paddrTarget->sin_addr.s_addr & maskLocal.sin_addr.s_addr))
    {
        return true;
    }
    else if(m_bPrivate)
    {
        // is this a known private address from table?
        for (int i = 0; i < TARGET_PRIVATE_ADDRS; i++)
        {
            if ((addrPrivate[i].sin_addr.s_addr & maskPrivate[i].sin_addr.s_addr) ==
                (paddrTarget->sin_addr.s_addr & maskPrivate[i].sin_addr.s_addr))
            {
                return true;
            }
        }
    }

    if(m_bTTL)
    {
        // check if actually within specified number of hops
        // only if previous tests failed, and allowing value is TTL
        DWORD dwHops;
        DWORD dwRtt;
        if (GetRTTAndHopCount(paddrTarget->sin_addr.s_addr, &dwHops, m_nTTL, &dwRtt))
        {
            return true;
        }
    }

    return false;
}


// is_ipv6_addr_local
bool url_verifier::is_ipv6_addr_local(sockaddr_in6* paddrTarget)
{
    if(IN6_IS_ADDR_LOOPBACK(&paddrTarget->sin6_addr))
        return true;
        
    if(IN6_IS_ADDR_LINKLOCAL(&paddrTarget->sin6_addr))
        return true;
        
    if(m_nSiteScope >= 3 && IN6_IS_ADDR_SITELOCAL(&paddrTarget->sin6_addr))
        return true;
        
    return false;
}
