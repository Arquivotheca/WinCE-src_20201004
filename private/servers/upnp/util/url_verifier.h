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
    {
        m_bAny = true; 
    }
    
    void allow_private()
    {
        m_bPrivate = true; 
    }
        
    void allow_ttl(int nTTL)
    {
        m_nTTL = nTTL; m_bTTL = true;
    }
        
    void allow_site_scope(int nSiteScope)
    {
        m_nSiteScope = nSiteScope; 
    }
    
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
