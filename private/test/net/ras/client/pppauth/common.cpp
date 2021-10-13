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
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include "common.h"
#include "pppauth.h"


#define IPV6PREFIX TEXT("200")


/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Function:

    GetRemoteServerIpAddress

Description:

    Return IP address as a string given the hostname as input

[Arguments:]

    ServerName - Hostname
    ipFamily       - IP Address family (AF_INET, AF_INET6)
    ServerIp      - IP address as a string

[Return Value:]

    Returns TRUE if the if the IP address corresponding to the hostname is found
    
-----------------------------------------------------------------------------------------*/

BOOL
GetRemoteServerIpAddress(
    TCHAR   *ServerName, 
    int     ipFamily,
    TCHAR   *ServerIp
    )
{
    CHAR        AdapterName[NI_MAXHOST]={0};
    TCHAR       szAddress[NI_MAXHOST]={0};
    DWORD       iLen = NI_MAXHOST;
    ADDRINFO    AddrHints = {0};
    ADDRINFO    *pAddrInfo = NULL;
    BOOL        bRetval = FALSE;
    HRESULT bStrCpyVal = S_OK;

    if((NULL == ServerIp) || (NULL == ServerName))
    {
        bRetval = FALSE;
        RASPPPAuthLog(RAS_LOG_FAIL, TEXT("ServerIp(%s) or ServerName(%s) is invalid!!!!"),
            (NULL == ServerIp)   ? RAS_STR_NULL : ServerIp,
            (NULL == ServerName) ? RAS_STR_NULL : ServerName);
    }
    else
    {
        //get the ipv6 address using getaddrinfo
        memset(&AddrHints, 0, sizeof(AddrHints));
        AddrHints.ai_family = ipFamily;  
        AddrHints.ai_socktype = SOCK_DGRAM;

        if (0 == WideCharToMultiByte(CP_ACP, 0, ServerName, -1, AdapterName, NI_MAXHOST, 0, 0))
        {
            RASPPPAuthLog(RAS_LOG_FAIL, TEXT("ipv6 - WideCharToMultiByte() failed with errorcode 0x%08x\n"), GetLastError());
        }
        else
        {
            if(0==getaddrinfo((LPCSTR)AdapterName, NULL, &AddrHints, &pAddrInfo))
            {
                for(ADDRINFO *pAI=pAddrInfo; pAI!=NULL; pAI=pAI->ai_next)
                {
                    if(AF_INET6 == pAI->ai_addr->sa_family) //Get IPV6 Address
                    {
                        SOCKADDR_IN6* address = (SOCKADDR_IN6*)(pAI->ai_addr);
                        if (0 == WSAAddressToString((SOCKADDR*)address, sizeof(SOCKADDR_IN6), NULL, szAddress, &iLen))
                        {
                            RASPPPAuthLog(RAS_LOG_COMMENT, TEXT("ipv6 address = %s"),szAddress);
                            if(0==_tcsnicmp(szAddress, IPV6PREFIX, _tcslen(IPV6PREFIX)))
                            {
                                bStrCpyVal = StringCchCopy(ServerIp, NI_MAXHOST * sizeof(TCHAR), szAddress);
                                ASSERT(bStrCpyVal == S_OK);
                                if(bStrCpyVal != S_OK)
                                {
                                    RASPPPAuthLog(RAS_LOG_FAIL, TEXT("Copying to the buffer ServerIp failed.Returning FALSE"));
                                    break;
                                }
                                bRetval = TRUE;
                                break;//Got the ipv6 Global address of Server, so break
                            }
                        }
                        else
                        {
                            RASPPPAuthLog(RAS_LOG_FAIL, TEXT("ipv6 - WSAAddressToString() failed with errorcode 0x%08x\n"), WSAGetLastError());
                        }
                    }
                    else if(AF_INET == pAI->ai_addr->sa_family) //Get IPV4 Adddress
                    {
                        SOCKADDR_IN* address = (SOCKADDR_IN*)(pAI->ai_addr);
                        if (0 == WSAAddressToString((SOCKADDR*)address, sizeof(SOCKADDR_IN), NULL, szAddress, &iLen))
                        {
                            RASPPPAuthLog(RAS_LOG_COMMENT, TEXT("ipv4 address = %s"),szAddress);
                            bStrCpyVal = StringCchCopy(ServerIp, NI_MAXHOST * sizeof(TCHAR), szAddress);
                            ASSERT(bStrCpyVal == S_OK);
                            if(bStrCpyVal != S_OK)
                            {
                                RASPPPAuthLog(RAS_LOG_FAIL, TEXT("Copying to the buffer ServerIp failed.Returning FALSE"));
                                break;
                            }
                            bRetval = TRUE;
                            break; //Got the Ipv4 Address of Server, so break;
                        }
                        else
                        {
                            RASPPPAuthLog(RAS_LOG_FAIL, TEXT("ipv4 - WSAAddressToString() failed with errorcode 0x%08x\n"), WSAGetLastError());
                        }
                   }
                }
                freeaddrinfo(pAddrInfo);
                
            }
            else
            {
                RASPPPAuthLog(RAS_LOG_FAIL, TEXT("getaddrinfo() failed with errorcode 0x%08x\n"), WSAGetLastError());
            }
        }
    }
    return bRetval;
}
// END OF FILE
