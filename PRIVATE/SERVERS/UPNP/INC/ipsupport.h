//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#include <winsock2.h>
#include <ws2tcpip.h>
#include "trace.h"

#ifndef _IPSUPPORT_H_
#define _IPSUPPORT_H_


#define MAX_ADDRESS_SIZE 256
#define IPv4LOOPBACK  0x100007f


/**************************    Ipv6 Port    *********************************
Function :   AddressIsLoopback
   
    New function
    Takes a PSOCKADDR parameter and dermines if the address pointed to is either
    the V4 loopback (127.0.0.1) or the v6 loopback (::1)
    Returns TRUE if loopback, else returns FALSE.

**************************    Ipv6 Port    *********************************/
inline BOOL AddressIsLoopback(const struct sockaddr * a)
{
    BOOL bResult = FALSE;
    
    if(a->sa_family == AF_INET6)
    {
        bResult = IN6_IS_ADDR_LOOPBACK(&((const struct sockaddr_in6*)a)->sin6_addr);
    }
    else if (a->sa_family == AF_INET)
    {
        bResult = ( ((const struct sockaddr_in*)a)->sin_addr.s_addr == (u_long)IPv4LOOPBACK ? TRUE:FALSE );
    }
    return bResult;
}



/**************************    Ipv6 Port    *********************************
Function :   IPAddressIsEqual
   
    New function
    Takes two PSOCKADDR parameters and compares their address fields
    Returns TRUE if both are of the same address family AND their addresses match, 
    else returns FALSE.

**************************    Ipv6 Port    *********************************/
inline BOOL IPAddressIsEqual (const struct sockaddr * a, const struct sockaddr * b)
{
    BOOL bResult = FALSE;
    
    if(a->sa_family == AF_INET6 && b->sa_family == AF_INET6)
    {
        bResult = (memcmp(&((const struct sockaddr_in6*)a)->sin6_addr, &((const struct sockaddr_in6*)b)->sin6_addr, sizeof(struct in6_addr))==0 ? TRUE:FALSE );
    }
    else if (a->sa_family == AF_INET && b->sa_family == AF_INET)
    {
        bResult = ( ((const struct sockaddr_in*)a)->sin_addr.s_addr == ((const struct sockaddr_in*)b)->sin_addr.s_addr ? TRUE:FALSE );
    }
    return bResult;
}




// Site local is our prefered address type, then link local, don't use global
typedef enum
{
    ADDRESS_IS_UNDEF = 0,
    ADDRESS_IS_IPV6GLOBAL = 0,
    ADDRESS_IS_IPV6LOOPBACK= 1,
    ADDRESS_IS_IPV6LINKLOCAL = 2,
    ADDRESS_IS_IPV6SITELOCAL = 3,
    ADDRESS_IS_IPV4LOOPBACK= 4,
    ADDRESS_IS_IPV4= 5
 } AddressPreference;



/**************************    Ipv6 Port    *********************************
Function :   AddressTypeStr
   
    New function
    Takes an AddressPreference enum as input parameters and returns matching
    string for debug tracing
    
**************************    Ipv6 Port    *********************************/
const char ADDRESSUNDEF[] ="undefined";
const char ADDRESSIPV6GLOBAL[] ="IPv6 Global";
const char ADDRESSIPV6LINKLOCAL[] ="IPv6 Link Local";
const char ADDRESSIPV6SITELOCAL[] ="IPv6 Site Local";
const char ADDRESSIPV6LOOPBACK[] ="IPv6 Loopback";
const char ADDRESSIPV4[] ="IPv4";
const char ADDRESSIPV4LOOPBACK[] ="IPv4 Loopback";

inline const CHAR * AddressTypeStr (AddressPreference addrPref )
{
    const char * pReturnStr = (const char *)&ADDRESSUNDEF;

    switch (addrPref)
    {
        case ADDRESS_IS_IPV6GLOBAL:         
            pReturnStr = (const char *)&ADDRESSIPV6GLOBAL;  
            break;
        case ADDRESS_IS_IPV6LINKLOCAL:      
            pReturnStr = (const char *)&ADDRESSIPV6LINKLOCAL;  
            break;
        case ADDRESS_IS_IPV6SITELOCAL:      
            pReturnStr = (const char *)&ADDRESSIPV6SITELOCAL;
            break;
        case ADDRESS_IS_IPV6LOOPBACK:      
            pReturnStr = (const char *)&ADDRESSIPV6LOOPBACK;
            break;
        case ADDRESS_IS_IPV4:      
            pReturnStr = (const char *)&ADDRESSIPV4;
            break;
        case ADDRESS_IS_IPV4LOOPBACK:      
            pReturnStr = (const char *)&ADDRESSIPV4LOOPBACK;
            break;
        default:    
            pReturnStr = (const char *)&ADDRESSUNDEF;
            break;
          }        

return pReturnStr;
}



/**************************    Ipv6 Port    *********************************
Function :   IPAddressType
   
    New function
    Takes a PSOCKADDR parameter and returns an  AddressPreference enum 
    giving the address type  of the input.
    
**************************    Ipv6 Port    *********************************/
inline AddressPreference IPAddressType(const struct sockaddr * pSockaddr )
{
    AddressPreference   addressPref = ADDRESS_IS_UNDEF;
    
    // What kind of address is this IPv4, IPv6, link local, etc?
    if (pSockaddr->sa_family == AF_INET)
    {
        if( ((PSOCKADDR_IN)pSockaddr)->sin_addr.s_addr == (u_long)IPv4LOOPBACK )
        {
            addressPref = ADDRESS_IS_IPV4LOOPBACK;
        }
        else
        {
            addressPref = ADDRESS_IS_IPV4;
        }    
    }
    else //Its a v6 Address, so find the type
    {
        if (IN6_IS_ADDR_LOOPBACK( &((PSOCKADDR_IN6)pSockaddr)->sin6_addr ) )
        {
            addressPref = ADDRESS_IS_IPV6LOOPBACK;
        }
        else if (IN6_IS_ADDR_SITELOCAL( &((PSOCKADDR_IN6)pSockaddr)->sin6_addr ) )
        {
            addressPref = ADDRESS_IS_IPV6SITELOCAL;
        }
        else if (IN6_IS_ADDR_LINKLOCAL( &((PSOCKADDR_IN6)pSockaddr)->sin6_addr ) )
        {
            addressPref = ADDRESS_IS_IPV6LINKLOCAL;
        }
        else // it's a global address
        {
            addressPref = ADDRESS_IS_IPV6GLOBAL;
        }
    }

    return addressPref;
}


#endif //_IPSUPPORT_H_

