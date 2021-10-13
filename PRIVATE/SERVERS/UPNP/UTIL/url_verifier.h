//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __URL_VERIFIER__
#define __URL_VERIFIER__

#include <winsock2.h>
#include <Ws2tcpip.h>
#include <iphlpapi.h>

// url_verifier
class url_verifier
{
public:
    url_verifier();
    
    void allow_any()
        {m_bAny = true; }
    
    void allow_private()
        {m_bPrivate = true; }
        
    void allow_ttl(int nTTL)
        {m_nTTL = nTTL; m_bTTL = true;}
        
    void allow_site_scope(int nSiteScope)
        {m_nSiteScope = nSiteScope; }
    
    bool is_url_ok(LPCSTR pszURL);
    
private:
    bool is_ipv4_addr_local(sockaddr_in* paddrTarget, sockaddr_in addrLocal, sockaddr_in maskLocal);
    bool is_ipv6_addr_local(sockaddr_in6* paddrTarget);

private:
    enum {TARGET_PRIVATE_ADDRS = 5};
    
    static char*        szaddrPrivate[TARGET_PRIVATE_ADDRS];
    static char*        szmaskPrivate[TARGET_PRIVATE_ADDRS];
    static sockaddr_in  addrPrivate[TARGET_PRIVATE_ADDRS];
    static sockaddr_in  maskPrivate[TARGET_PRIVATE_ADDRS];
    bool                m_bAny;
    bool                m_bPrivate;
    bool                m_bTTL;
    int                 m_nSiteScope;
    int                 m_nTTL;
};

#endif // __URL_VERIFIER__
