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

#pragma once

// This should really be coming from logging.h, unfortunately the incorrect logging.h gets
// included as a result of bad include paths, so this will have to do.
#define LOG Debug
void Debug(LPCTSTR szFormat, ...) ;
void DebugDetailed(DWORD verbosity, LPCTSTR szFormat, ...) ;

#include "stdafx.h"
#include "convertbase64.h"

#include <urlmon.h>

const long DEFAULT_MAXPATH            = 3072;                                    // 3K - This big to protect against URL strings that are really long

static DWORD WINAPI _SendHTTPCommand(void* _pThreadObject);

static DWORD HTTPSENDWAIT = 120000;        // 2 Minutes

#pragma pack(push, 1)    // byte align this struct
    struct THREADED_SOCKET
    {
        THREADED_SOCKET()
        {
            ZeroMemory(this, sizeof(THREADED_SOCKET));
        }

        SOCKET          *pSocket;

        char *          lpTriggerURL;
        bool            bSendURL;
        bool            bRemoveHeader;
        char *          lpData;
        char *          lpHeader;
        long            nDataLength;
        long            nHeaderLength;
        DWORD           dwTCPFrameSize;
        HANDLE          hFile;

        bool            bReturn;
    } ;
#pragma pack(pop)

inline void GetURLPathFromURL(char * lpURLPatch, char * lpURL)
{
    *lpURLPatch = '\0';

    char *lpTemp = (char *)_alloca(DEFAULT_MAXPATH);
    if (!lpTemp)
        return;
    memset(lpTemp, 0, DEFAULT_MAXPATH);
    sprintf_s(lpTemp, DEFAULT_MAXPATH, "%s", lpURL);
    char *lpLocation = strstr(lpTemp, "//");
    if (lpLocation)
    {
        lpLocation += 2;

        // Look for start of URL Patch
        while (*lpLocation != '\\' && *lpLocation != '/' && *lpLocation != '\0')
            lpLocation++;

        if (*lpLocation != '\0')
            sprintf_s(lpURLPatch, MAX_PATH, "%s", lpLocation);
    }
}

inline void RemoveAuthenticationFromURL(char * lpNoAuthURL, char * lpURL)
{
    *lpNoAuthURL = '\0';

    // Check for Authentication token
    if (!strstr(lpURL, "@"))
    {
        sprintf_s(lpNoAuthURL, MAX_PATH, "%s", lpURL);
        return;
    }

    char strURLType[MAX_PATH];
    sprintf_s(strURLType, MAX_PATH, "%s", "http://");
    if (strstr(lpURL, "https:") || strstr(lpURL, "HTTPS:"))
        sprintf_s(strURLType, MAX_PATH, "%s", "https://");
    if (strstr(lpURL, "mms:") || strstr(lpURL, "MMS:"))
        sprintf_s(strURLType, MAX_PATH, "%s", "mms://");

    char *lpLocation = strstr(lpURL, "@");
    if (lpLocation)
    {
        char * lpSearch = strstr(lpLocation + 1, "@");
        while (lpSearch)
        {
            lpLocation = lpSearch;
            lpSearch = strstr(lpSearch + 1, "@");
        }
    }

    if (!lpLocation)
        return;

    sprintf_s(lpNoAuthURL, MAX_PATH, "%s%s", strURLType, lpLocation + 1);
}

inline void GetURLOnlyFromURL(char * lpURLOnly, char * lpURL)
{
    sprintf_s(lpURLOnly, MAX_PATH, "%s", lpURL);

    char *lpTemp = (char *)_alloca(MAX_PATH + 1);
    if (!lpTemp)
        return;
    memset(lpTemp, 0, strlen(lpURL) + 1);
    sprintf_s(lpTemp, MAX_PATH, "%s", lpURL);
    char *lpLocation = lpTemp + 8;        // Advance past http:// or https:// or mms://
    while (*lpLocation != '\\' && *lpLocation != '/' && *lpLocation != '\0')
        lpLocation++;

    if (*lpLocation != '\0')
    {
        *(lpLocation + 1) = '\0';
        sprintf_s(lpURLOnly, MAX_PATH, "%s", lpTemp);
    }
}

inline void GetAddressFromURL(char * lpAddress, char * lpURL)
{
    *lpAddress = '\0';

    char *lpTemp = (char *)_alloca(MAX_PATH + 1);
    if (!lpTemp)
        return;
    memset(lpTemp, 0, strlen(lpURL) + 1);
    sprintf_s(lpTemp, MAX_PATH, "%s", lpURL);
    char *lpLocation = strstr(lpTemp, "//");
    if (lpLocation)
    {
        // Look for last @
        char *lpAtLoc = NULL;
        char *lpSearch = strstr(lpLocation, "@");
        while (lpSearch)
        {
            lpAtLoc = lpSearch;
            lpSearch = strstr(lpSearch + 1, "@");
        }

        if (lpAtLoc)
            lpLocation = lpAtLoc + 1;
        else
            lpLocation += 2;

        // Look for end of address
        char *lpTempLocation = lpLocation;
        while (*lpTempLocation != '\\' && *lpTempLocation != '/' && *lpTempLocation != ':' && *lpTempLocation != '\0')
            lpTempLocation++;
        *lpTempLocation = '\0';

        sprintf_s(lpAddress, MAX_PATH, "%s", lpLocation);
    }
}

inline long GetPortFromURL(char * lpURL)
{
    char *lpTemp = (char *)_alloca(MAX_PATH + 1);
    if (!lpTemp)
        return 80;
    memset(lpTemp, 0, strlen(lpURL) + 1);
    sprintf_s(lpTemp, MAX_PATH, "%s", lpURL);

    long nPort = 80;        // Default HTTP port
    if (strstr(lpTemp, "https:") != NULL || strstr(lpTemp, "HTTPS:") != NULL)
        nPort = 443;        // Default HTTPS port
//    if (strstr(lpTemp, "mms:") != NULL || strstr(lpTemp, "MMS:") != NULL)
//        nPort = 1755;        // Default mms port

    char *lpLocation = strstr(lpTemp, "//");
    if (lpLocation)
    {
        // Look for last @
        char *lpAtLoc = NULL;
        char *lpSearch = strstr(lpLocation, "@");
        while (lpSearch)
        {
            lpAtLoc = lpSearch;
            lpSearch = strstr(lpSearch + 1, "@");
        }

        if (lpAtLoc)
            lpLocation = lpAtLoc;

        char *lpTempLocation = strstr(lpLocation, ":");
        if (lpTempLocation)
        {
            lpLocation = lpTempLocation + 1;

            lpTempLocation = strstr(lpLocation, "/");
            if (lpTempLocation)
                *lpTempLocation = '\0';

            nPort = atoi(lpLocation);
        }
    }

    return nPort;
}

inline void GetUsernameFromURL(char * lpUsername, char * lpURL)
{
    *lpUsername = '\0';

    char *lpTemp = (char *)_alloca(MAX_PATH + 1);
    if (!lpTemp)
        return;
    memset(lpTemp, 0, strlen(lpURL) + 1);
    sprintf_s(lpTemp, MAX_PATH, "%s", lpURL);

    long nPort = 80;        // Default HTTP port
    if (strstr(lpTemp, "https") != NULL || strstr(lpTemp, "HTTPS") != NULL)
        nPort = 443;        // Default HTTPS port
    if (strstr(lpTemp, "mms") != NULL || strstr(lpTemp, "MMS") != NULL)
        nPort = 1755;        // Default HTTPS port

    char *lpLocation = strstr(lpTemp, "//");
    if (lpLocation)
    {
        lpLocation += 2;

        // Look for last @
        char *lpAtLoc = NULL;
        char *lpSearch = strstr(lpLocation, "@");
        while (lpSearch)
        {
            lpAtLoc = lpSearch;
            lpSearch = strstr(lpSearch + 1, "@");
        }

        // Look for last :
        char *lpColonLoc = NULL;        // format http://username:password@...
        lpSearch = strstr(lpLocation, ":");
        while (lpSearch && lpAtLoc)
        {
            if (lpSearch < lpAtLoc)
                lpColonLoc = lpSearch;
            lpSearch = strstr(lpSearch + 1, ":");
        }

        if (!lpColonLoc || !lpAtLoc)
            return;

        if (lpColonLoc > lpAtLoc)
            return;

        *lpColonLoc = '\0';

        sprintf_s(lpUsername, MAX_PATH, "%s", lpLocation);
    }

    return;
}

inline void GetPasswordFromURL(char * lpPassword, char * lpURL)
{
    *lpPassword = '\0';

    char *lpTemp = (char *)_alloca(MAX_PATH + 1);
    if (!lpTemp)
        return;
    memset(lpTemp, 0, strlen(lpURL) + 1);
    sprintf_s(lpTemp, MAX_PATH, "%s", lpURL);

    long nPort = 80;        // Default HTTP port
    if (strstr(lpTemp, "https") != NULL || strstr(lpTemp, "HTTPS") != NULL)
        nPort = 443;        // Default HTTPS port
    if (strstr(lpTemp, "mms") != NULL || strstr(lpTemp, "MMS") != NULL)
        nPort = 1755;        // Default MMS port

    char *lpLocation = strstr(lpTemp, "//");
    if (lpLocation)
    {
        lpLocation++;

        // Look for last @
        char *lpAtLoc = NULL;
        char *lpSearch = strstr(lpLocation, "@");
        while (lpSearch)
        {
            lpAtLoc = lpSearch;
            lpSearch = strstr(lpSearch + 1, "@");
        }

        // Look for last :
        char *lpColonLoc = NULL;        // format http://username:password@...
        lpSearch = strstr(lpLocation, ":");
        while (lpSearch && lpAtLoc)
        {
            if (lpSearch < lpAtLoc)
                lpColonLoc = lpSearch;
            lpSearch = strstr(lpSearch + 1, ":");
        }

        if (!lpColonLoc || !lpAtLoc)
            return;

        if (lpColonLoc > lpAtLoc)
            return;

        *lpAtLoc = '\0';

        sprintf_s(lpPassword, MAX_PATH, "%s", lpColonLoc + 1);
    }

    return;
}

////////////////////////////////////////////
//
//    Function:    URLEncode
//
//    Description:    
//
//    Parameters:    
//
inline void URLEncode(char * lpURL, long nURLLength, char * lpSafeURL, long nSafeURLLength)
{
    HRESULT hr = S_OK;

    if (nSafeURLLength < (nURLLength * 3))
        return;            // Safe must be at least 3 times which is the worst case if the entire string needs to be encoded.  When encoding you replace a char will 3 chars

    memset(lpSafeURL, 0, nSafeURLLength);
    char * lpURLPosition = lpURL;
    char * lpSafeURLPosition = lpSafeURL;

    // Skip Past username and password if in the URL string because username and password is passed into HTTPConnect function and need to be non-encoded
    char * lpAt = strstr(lpURLPosition, "@");
    if (lpAt)
    {
        char * lpSearch = strstr(lpAt + 1, "@");
        while (lpSearch)
        {
            lpAt = lpSearch;
            lpSearch = strstr(lpSearch + 1, "@");
        }
    }

    char * lpColon = strstr(lpURLPosition + 8, ":");
    if (lpColon && lpAt)
    {
        char * lpSearch = strstr(lpColon + 1, ":");
        while (lpSearch)
        {
            if (lpSearch < lpAt)
                lpColon = lpSearch;
            lpSearch = strstr(lpSearch + 1, ":");
        }
    }

    char * lpAuthentication = NULL;
    if (lpColon && lpAt)
    {
        if (lpColon < lpAt)
            lpAuthentication = lpAt + 1;
    }

    bool bCheckcharacter = true;

    while (*lpURLPosition != '\0')
    {
        // character Unsafe Reason character Encode 
        //    "<" Delimiters around URLs in free text %3C 
        //    > Delimiters around URLs in free text %3E 
        //    . Delimits URLs in some systems %22 
        //    # It is used in the World Wide Web and in other systems to delimit a URL from a fragment/anchor identifier that might follow it.  %23 
        //    { Gateways and other transport agents are known to sometimes modify such characters  %7B 
        //    } Gateways and other transport agents are known to sometimes modify such characters %7D 
        //    | Gateways and other transport agents are known to sometimes modify such characters %7C 
        //    \ Gateways and other transport agents are known to sometimes modify such characters %5C 
        //    ^ Gateways and other transport agents are known to sometimes modify such characters %5E 
        //    ~    Gateways and other transport agents are known to sometimes modify such characters %7E 
        //    [ Gateways and other transport agents are known to sometimes modify such characters %5B 
        //    ] Gateways and other transport agents are known to sometimes modify such characters %5D 
        //    ` Gateways and other transport agents are known to sometimes modify such characters %60 
        //    + Indicates a space (spaces cannot be used in a URL) %20 
        //    / Separates directories and subdirectories %2F 
        //    ? Separates the actual URL and the parameters %3F 
        //    & Separator between parameters specified in the URL %26 

        // NOTE: Don't convert / or . because when I did those the URL open wasn't working.  It might just be a problem w/ one or the other but needs investigation.
        //        For now just encoding the # because that is what WiFiEye has for a password that started failing in open url after IE7 patch
        //
        bCheckcharacter = true;
        if (lpAuthentication)
        {
            if (lpURLPosition < lpAuthentication)
                bCheckcharacter = false;
        }

        if (bCheckcharacter)
        {
            switch(*lpURLPosition)
            {
                case '<':
                    strcat_s(lpSafeURLPosition, MAX_PATH, "%3C");
                    lpSafeURLPosition += 3;
                    lpURLPosition++;
                    continue;
                    break;

                case '>':
                    strcat_s(lpSafeURLPosition, MAX_PATH, "%3E");
                    lpSafeURLPosition += 3;
                    lpURLPosition++;
                    continue;
                    break;

/*                case '.':
                    strcat_s(lpSafeURLPosition, MAX_PATH, "%22");
                    lpSafeURLPosition += 3;
                    lpURLPosition++;
                    continue;
                    break;
*/
                case '#':
                    strcat_s(lpSafeURLPosition, MAX_PATH, "%23");
                    lpSafeURLPosition += 3;
                    lpURLPosition++;
                    continue;
                    break;

                case '{':
                    strcat_s(lpSafeURLPosition, MAX_PATH, "%7B");
                    lpSafeURLPosition += 3;
                    lpURLPosition++;
                    continue;
                    break;

                case '}':
                    strcat_s(lpSafeURLPosition, MAX_PATH, "%7D");
                    lpSafeURLPosition += 3;
                    lpURLPosition++;
                    continue;
                    break;

                case '|':
                    strcat_s(lpSafeURLPosition, MAX_PATH, "%7C");
                    lpSafeURLPosition += 3;
                    lpURLPosition++;
                    continue;
                    break;

                case '\\':
                    strcat_s(lpSafeURLPosition, MAX_PATH, "%5C");
                    lpSafeURLPosition += 3;
                    lpURLPosition++;
                    continue;
                    break;

                case '^':
                    strcat_s(lpSafeURLPosition, MAX_PATH, "%5E");
                    lpSafeURLPosition += 3;
                    lpURLPosition++;
                    continue;
                    break;

                case '~':
                    strcat_s(lpSafeURLPosition, MAX_PATH, "%7E");
                    lpSafeURLPosition += 3;
                    lpURLPosition++;
                    continue;
                    break;

                case '[':
                    strcat_s(lpSafeURLPosition, MAX_PATH, "%5B");
                    lpSafeURLPosition += 3;
                    lpURLPosition++;
                    continue;
                    break;

                case ']':
                    strcat_s(lpSafeURLPosition, MAX_PATH, "%5D");
                    lpSafeURLPosition += 3;
                    lpURLPosition++;
                    continue;
                    break;

                case '`':
                    strcat_s(lpSafeURLPosition, MAX_PATH, "%60");
                    lpSafeURLPosition += 3;
                    lpURLPosition++;
                    continue;
                    break;

                case '+':
                case ' ':
                    strcat_s(lpSafeURLPosition, MAX_PATH, "%20");
                    lpSafeURLPosition += 3;
                    lpURLPosition++;
                    continue;
                    break;

/*                case '/':
                    strcat_s(lpSafeURLPosition, MAX_PATH, "%2F");
                    lpSafeURLPosition += 3;
                    lpURLPosition++;
                    continue;
                    break;

                case '?':
                    strcat_s(lpSafeURLPosition, MAX_PATH, "%3F");
                    lpSafeURLPosition += 3;
                    lpURLPosition++;
                    continue;
                    break;

                case '&':
                    strcat_s(lpSafeURLPosition, MAX_PATH, "%26");
                    lpSafeURLPosition += 3;
                    lpURLPosition++;
                    continue;
                    break;
*/            }
        }

        // character safe so move into safe URL
        *lpSafeURLPosition = *lpURLPosition;
        lpSafeURLPosition++;
        lpURLPosition++;
    }

    return;
}

inline bool WSAIsError(int nWSACode)
{
    bool bReturn = true;

//char strError[1024];

    switch(nWSACode)
    {
        case WSANOTINITIALISED:
//sprintf_s(strError, 1024, "A successful WSAStartup call must occur before using this function.");
            break;
 
        case WSAENETDOWN:
//sprintf_s(strError, 1024, "The network subsystem has failed.");
            break;
 
        case WSAEFAULT:
//sprintf_s(strError, 1024, "The buf parameter is not completely contained in a valid part of the user address space.");
            break;
 
        case WSAENOTCONN:
//sprintf_s(strError, 1024, "The socket is not connected.");
            break;
 
        case WSAEINTR:
//sprintf_s(strError, 1024, "The (blocking) call was canceled through WSACancelBlockingCall.");
            break;
 
        case WSAEINPROGRESS:
//sprintf_s(strError, 1024, "A blocking Windows Sockets 1.1 call is in progress, or the service provider is still processing a callback function.");
            break;
 
        case WSAENETRESET:
//sprintf_s(strError, 1024, "The connection has been broken due to the keep-alive activity detecting a failure while the operation was in progress.");
            break;
 
        case WSAENOTSOCK:
//sprintf_s(strError, 1024, "The descriptor is not a socket.");
            break;
 
        case WSAEOPNOTSUPP:
//sprintf_s(strError, 1024, "MSG_OOB was specified, but the socket is not stream-style such as type SOCK_STREAM, OOB data is not supported in the communication domain associated with this socket, or the socket is unidirectional and supports only send operations.");
            break;
 
        case WSAESHUTDOWN:
//sprintf_s(strError, 1024, "The socket has been shut down; it is not possible to receive on a socket after shutdown has been invoked with how set to SD_RECEIVE or SD_BOTH.");
            break;
 
        case WSAEWOULDBLOCK:
//sprintf_s(strError, 1024, "The socket is marked as nonblocking and the receive operation would block.");
            break;
 
        case WSAEMSGSIZE:
//sprintf_s(strError, 1024, "The message was too large to fit into the specified buffer and was truncated.");
            break;
 
        case WSAEINVAL:
//sprintf_s(strError, 1024, "The socket has not been bound with bind, or an unknown flag was specified, or MSG_OOB was specified for a socket with SO_OOBINLINE enabled or (for byte stream sockets only) len was zero or negative.");
            break;
 
        case WSAECONNABORTED:
//sprintf_s(strError, 1024, "The virtual circuit was terminated due to a time-out or other failure. The application should close the socket as it is no longer usable.");
            break;
 
        case WSAETIMEDOUT:
//sprintf_s(strError, 1024, "The connection has been dropped because of a network failure or because the peer system failed to respond.");
            break;
 
        case WSAECONNRESET:
//sprintf_s(strError, 1024, "The virtual circuit was reset by the remote side executing a hard or abortive close. The application should close the socket as it is no longer usable. On a UPD-datagram socket this error would indicate that a previous send operation resulted in an ICMP \"Port Unreachable\" message.");
            break;

        default:
            bReturn = false;
            break;
    }
    
    return bReturn;
}

inline bool SendHTTPCommand_Actual(SOCKET *pSocket, char * strTriggerURL, bool bSendURL, bool bRemoveHeader, char *lpData, long nDataLength, char *lpHeader, long nHeaderLength, HANDLE hFile, DWORD dwTCPFrameSize)
{
    long nSafeURLLength = MAX_PATH * 3;
    char * lpSafeURL = (char *)_alloca(nSafeURLLength + 1);
    memset(lpSafeURL, 0, nSafeURLLength + 1);
    URLEncode(strTriggerURL, (long)strlen(strTriggerURL), lpSafeURL, nSafeURLLength);

    char *lpAddress = (char *)_alloca(nSafeURLLength + 1);
    memset(lpAddress, 0, nSafeURLLength + 1);
    GetAddressFromURL(lpAddress, lpSafeURL);
    if (*lpAddress == '\0')
    {
        LOG(L"*SendHTTPCommand: Failed and returned NULL");
        return false;
    }
    int nPort = GetPortFromURL(lpSafeURL);

    char *lpUsername = (char *)_alloca(nSafeURLLength + 1);
    memset(lpUsername, 0, nSafeURLLength + 1);
    GetUsernameFromURL(lpUsername, lpSafeURL);
    char *lpPassword = (char *)_alloca(nSafeURLLength + 1);
    memset(lpPassword, 0, nSafeURLLength + 1);
    GetPasswordFromURL(lpPassword, lpSafeURL);

    char *lpURL = (char *)_alloca(nSafeURLLength + 1);
    memset(lpURL, 0, nSafeURLLength + 1);
    GetURLPathFromURL(lpURL, (char *)lpSafeURL);
    if (*lpURL == '\0')
    {
        LOG(L"*SendHTTPCommand: Failed and returned NULL");
        return false;
    }

    struct hostent *pHostent = NULL;
    struct in_addr addr;

    bool bSkipOpenAttempt = false;

    bool bUseHostent = false;

    // If the user input is an alpha name for the host, use gethostbyname()
    DWORD dwError = 0;
    bUseHostent = false;
    // Check if any alpha values in address
    bool bIsAlpha = false;
    char *lpCurrent = lpAddress;
    while (*lpCurrent != '\0' && !bIsAlpha)
    {
        if (isalpha(*lpCurrent))
        {
            bIsAlpha = true;
            break;
        }
        lpCurrent++;
    }
    if (bIsAlpha)        /* host address is a name */
    {
        bUseHostent = true;
// NOTE: Convert char before using
//LOG(L"SendHTTPCommand: Calling gethostbyname with %s", lpAddress);
//UPDATELOGFILE(strMessage);
        pHostent = gethostbyname((char *)lpAddress);

        if (pHostent == NULL)
        {
            dwError = WSAGetLastError();
            if (dwError != 0)
            {
                switch(dwError)
                {
                    case WSAHOST_NOT_FOUND:
// NOTE: Convert char before using
//                        LOG(L"SendHTTPCommand: (%s) Host not found: %ld", strTriggerURL, dwError);
//UPDATEERRORLOGFILE(strMessage);
                        return false;
                    case WSANO_DATA:
// NOTE: Convert char before using
//                        LOG(L"SendHTTPCommand: (%s) No data record found: %ld", strTriggerURL, dwError);
//UPDATEERRORLOGFILE(strMessage);
                        return false;

                    case WSANOTINITIALISED:
// NOTE: Convert char before using
//LOG(L"SendHTTPCommand: (%s) WSA Not Initialized - make sure to run WSAStartup: %ld", strTriggerURL, dwError);
//UPDATEERRORLOGFILE(strMessage);
                        return false;

                    default:
// NOTE: Convert char before using
//LOG(L"SendHTTPCommand: (%s) Function failed with error: %ld", strTriggerURL, dwError);
//UPDATEERRORLOGFILE(strMessage);
                        return false;
                }
            }
        }
        else
        {
// NOTE: Convert char before using
//LOG(L"SendHTTPCommand: \tOfficial name: %s", pHostent->h_name);
//UPDATELOGFILE(strMessage);
// NOTE: Convert char before using
//LOG(L"SendHTTPCommand: \tAlternate names: %s", pHostent->h_aliases);
//UPDATELOGFILE(strMessage);
LOG(L"SendHTTPCommand: \tAddress type: ");
//UPDATELOGFILE(strMessage);
            switch (pHostent->h_addrtype)
            {
                case AF_INET:
                    LOG(L"AF_INET");
                    break;
                case AF_INET6:
                    LOG(L"AF_INET");
                    break;
                case AF_NETBIOS:
                    LOG(L"AF_NETBIOS");
                    break;
                default:
                    LOG(L" %d", pHostent->h_addrtype);
                    break;
            }
//UPDATELOGFILE(strMessage);
//sprintf_s(strMessage, "SendHTTPCommand: \tAddress length: %d", pHostent->h_length);
//UPDATELOGFILE(strMessage);
            addr.s_addr = *(u_long *) pHostent->h_addr_list[0];
//sprintf_s(strMessage, "SendHTTPCommand: \tFirst IP Address: %s", inet_ntoa(addr));
//UPDATELOGFILE(strMessage);
        }
    }

    if (!bUseHostent)
        pHostent = NULL;

    struct sockaddr_in sockaddrServer;
    if (pHostent)
        sockaddrServer.sin_addr.s_addr=*((unsigned long*)pHostent->h_addr);
    else
        sockaddrServer.sin_addr.s_addr = inet_addr((char *)lpAddress);

    sockaddrServer.sin_family = AF_INET;
    sockaddrServer.sin_port = htons((USHORT)nPort);

    *pSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (*pSocket == INVALID_SOCKET)
    {
        closesocket(*pSocket);

        LOG(L"_OpenUrlThread: Error - socket() to server failed");
        return false;
    }

    bool bFxReturnCode = true;

    if (connect(*pSocket, (struct sockaddr*)&sockaddrServer, sizeof(sockaddrServer)))
    {
        closesocket(*pSocket);

// NOTE: Convert char before using
//        LOG(L"SendHTTPCommand: Error - connect() to server on Socket Failed for strTriggerURL: %s", strTriggerURL);
        return false;
    }
    else
    {
        if (!bSendURL)
        {
            int iResult = shutdown(*pSocket, SD_SEND);
            if (iResult == SOCKET_ERROR)
            {
                LOG(L"SendHTTPCommand: Shutdown failed with error: %d", WSAGetLastError());
            }
            closesocket(*pSocket);

            return true;
        }

        CComBSTR bstrEncoded;

        if (strlen(lpUsername) > 0 && strlen(lpPassword) > 0)
        {
            char strCredentials[1024];
            sprintf_s(strCredentials, 1024, "%s:%s", lpUsername, lpPassword);
            CComBSTR bstrEncodeSource(strCredentials);

            HRESULT hr = Base64Encode(bstrEncodeSource, &bstrEncoded);
            if (FAILED(hr))
            {
                LOG(L"SendHTTPCommand: Warning - Encoding Base 64 issue");
            }
        }

        char httpRequest[4096];

        USES_CONVERSION;
        if (bstrEncoded.Length() > 0)
        {
            // MMS Note: Probably need to rebuild the URL with username & password for mms to work
            //
            if (strstr(strTriggerURL, "mms:") || strstr(strTriggerURL, "MMS"))
                sprintf_s(httpRequest, 4096, "GET %s HTTP/1.0\r\nAccept: */*\r\nAuthorization: Basic %s\r\n\r\n", lpURL, OLE2A(bstrEncoded));
            else
                sprintf_s(httpRequest, 4096, "GET %s HTTP/1.0\r\nAccept: */*\r\nAuthorization: Basic %s\r\n\r\n", lpURL, OLE2A(bstrEncoded));
        }
        else
        {
            // HTTP or MMS - Same get format as http except the get has the full mms:// after get instead of http://
//            sprintf_s(httpRequest, 4096, "GET %s HTTP/1.0\r\nAccept: */*\r\n\r\n", lpURL);    // Don't use the lpURL because doesn't include whether http or mms so use the full url in lpSafeURL
            sprintf_s(httpRequest, 4096, "GET %s HTTP/1.0\r\nAccept: */*\r\n\r\n", lpSafeURL);

//            http://wcemedia03/MainMedia/RAW/test6MB.htm
//            “Get /wcemedia03/meir/test%206MB.htm HTTP/1.1\r\nAccept: */*\r\nAccept-Language: en-us\r\nHost: wcemedia03\r\nUser-Agent: Mozilla/4.0\r\n\r\n”
//            GET http://wcemedia03/meir/test%206MB.htm HTTP/1.1\r\nAccept: */*
//            GET wcemedia03/meir/test%206MB.htm HTTP/1.1\r\nAccept: */*
//            GET http://wcemedia03/meir/test%206MB.htm HTTP/1.0\r\nAccept: */*
//            GET wcemedia03/meir/test%206MB.htm HTTP/1.0\r\nAccept: */*
        }

        int nBytes = (int)strlen(httpRequest);
        int nReturn = send(*pSocket, httpRequest, nBytes, 0);
        if (nReturn != nBytes)
        {
            LOG(L"SendHTTPCommand: Error: InternetOpenUrl_ThreadedCall send Return: %d GetLastError: %ld", nReturn, GetLastError());

            int iResult = shutdown(*pSocket, SD_SEND);
            if (iResult == SOCKET_ERROR)
            {
                LOG(L"SendHTTPCommand: Shutdown failed with error: %d", WSAGetLastError());
            }
            closesocket(*pSocket);

            return false;
        }
        else
        {
            long nRequestSize = 1024;               // 1K request size to compare w/ DShow because Buffering filter is only able to pull 1K at a time from the network card (per Mier)
            if (dwTCPFrameSize > 0 && dwTCPFrameSize <= (10 * 1024 * 1024))         // Check for valid value which is less than 10MBs
                nRequestSize = dwTCPFrameSize;

            char *lpReturnTempBuffer = (char *) new BYTE[nRequestSize];
            if (!lpReturnTempBuffer)
            {
                LOG(L"SendHTTPCommand: Error: InternetOpenUrl_ThreadedCall send Memory allocation error GetLastError: %ld", GetLastError());

                int iResult = shutdown(*pSocket, SD_SEND);
                if (iResult == SOCKET_ERROR)
                {
                    LOG(L"SendHTTPCommand: Shutdown failed with error: %d", WSAGetLastError());
                }

                closesocket(*pSocket);

                return false;
            }

            memset(lpReturnTempBuffer, 0, nRequestSize);

            long nActualReturn = 0, iReturnBytes = 0, iReturnHeaderBytes = 0;
            bool bHeaderRemoved = false, bContinueRecv = true;

            // Receive until the peer closes the connection
            do {
                iReturnBytes = recv(*pSocket, lpReturnTempBuffer, nRequestSize, 0);                // IF recv returning invalid call probably because not encoding non-URL characters (e.g., space as %20, etc)
                if (iReturnBytes <= 0)
                    bContinueRecv = false;

                char *lpReturnTemp = lpReturnTempBuffer;

                if ( iReturnBytes > 0 )
                {
                    if (bRemoveHeader && !bHeaderRemoved && iReturnBytes > 8)
                    {
                        // Remove the HTTP Header and just get the content
                        if (memcmp(lpReturnTemp, "HTTP/\0", 5) == 0)        // Identical match
                        {
                            while (*lpReturnTemp != '\0' && (iReturnBytes - 4) > 0)
                            {
                                // Check for 0x0d 0x0a 0x0d 0x0a
                                if (*lpReturnTemp == 0x0d && *(lpReturnTemp + 1) == 0x0a && *(lpReturnTemp + 2) == 0x0d && *(lpReturnTemp + 3) == 0x0a && nHeaderLength > (iReturnHeaderBytes + 4))
                                {
                                    if (lpHeader)
                                    {
                                        memcpy(lpHeader + iReturnHeaderBytes, lpReturnTemp, 4);          // Save off to the header
                                        iReturnHeaderBytes += 4;
                                    }

                                    lpReturnTemp += 4;                                                   // Advance past the new lines
                                    iReturnBytes -= 4;
                                    bHeaderRemoved = true;

                                    break;
                                }

                                if (lpHeader)
                                {
                                    memcpy(lpHeader + iReturnHeaderBytes, lpReturnTemp, 1);              // Save off the header a byte at a time
                                    iReturnHeaderBytes++;
                                }

                                iReturnBytes--;
                                lpReturnTemp++;
                            }
                        }
                    }

                    if (iReturnBytes > 0 && lpReturnTemp && lpData && ((nActualReturn + iReturnBytes) <= nDataLength))
                    {
                        memcpy(lpData, lpReturnTemp + nActualReturn, iReturnBytes);
                    }

                    if (iReturnBytes > 0 && lpReturnTemp && hFile != INVALID_HANDLE_VALUE && hFile != NULL)
                    {
                        DWORD dwWritten = 0;
                        BOOL bReturn = ::WriteFile(hFile, lpReturnTemp, (DWORD)iReturnBytes, &dwWritten, NULL);
                        if (!bReturn || dwWritten != (DWORD)iReturnBytes)
                        {
                            LOG(L"SendHTTPCommand: recv failed to WriteFile: Request Write Bytes: %d  Written Bytes: %ld", iReturnBytes, dwWritten);

                            bFxReturnCode = false;
                            break;
                        }
                    }

                    nActualReturn += iReturnBytes;

#ifdef DEBUG
                    LOG(L"SendHTTPCommand: Write Bytes: %d Total Written: %ld bytes %ld KBs %ld MBs", iReturnBytes, nActualReturn, nActualReturn / 1024, nActualReturn / (1024 * 1024));
#endif

                }
                else if ( iReturnBytes == 0 )
                {
                    LOG(L"SendHTTPCommand: Connection closed");
                }
                else
                {
                    LOG(L"SendHTTPCommand: recv failed with error: %d", WSAGetLastError());
                }

            } while( bContinueRecv );

            if (lpReturnTempBuffer)
            {
                delete [] lpReturnTempBuffer;
                lpReturnTempBuffer = NULL;
            }
        }

        // shutdown the connection since no more data will be sent
        int iResult = shutdown(*pSocket, SD_SEND);
        if (iResult == SOCKET_ERROR)
        {
            LOG(L"SendHTTPCommand: Shutdown failed with error: %d", WSAGetLastError());

            closesocket(*pSocket);

            return false;
        }

        closesocket(*pSocket);
    }

    Sleep(10);

    return bFxReturnCode;
}

inline bool SendHTTPCommand(char * strTriggerURL, bool bSendURL, bool bRemoveHeader, char *lpData, long nDataLength, char *lpHeader, long nHeaderLength, DWORD dwTCPFrameSize)
{
    SOCKET socketConnection;

    THREADED_SOCKET threadObject;
    threadObject.pSocket = &socketConnection;
    threadObject.lpTriggerURL = strTriggerURL;
    threadObject.bSendURL = bSendURL;
    threadObject.bRemoveHeader = bRemoveHeader;
    threadObject.lpData = lpData;
    threadObject.nDataLength = nDataLength;
    threadObject.lpHeader = lpHeader;
    threadObject.nHeaderLength = nHeaderLength;
    threadObject.dwTCPFrameSize = dwTCPFrameSize;

    DWORD nThreadID = 0;
    HANDLE hThread = NULL;
    hThread = CreateThread( NULL,
        0, 
        _SendHTTPCommand,
        &threadObject,
        0,
        &nThreadID);

    if (hThread)
    {
        if (WAIT_TIMEOUT == WaitForSingleObject(hThread, HTTPSENDWAIT))
        {
            // shutdown the connection since no more data will be sent and allow thread to shutdown gracefully
            int iResult = shutdown(socketConnection, SD_SEND);
            closesocket(socketConnection);

            if (WAIT_TIMEOUT == WaitForSingleObject(hThread, 2000))
                TerminateThread(hThread, 0);            // Thread locked to force terminate

            threadObject.bReturn = false;

            CloseHandle(hThread);
            hThread = NULL;

            return false;
        }

        CloseHandle(hThread);
        hThread = NULL;
    }
    // If unable to start thread then don't send http because without threading around http call if http call hangs it locks the calling thread
//    else
//        return SendHTTPCommand_Actual(&socketConnection, strTriggerURL, bSendURL, bRemoveHeader, lpReturn, nDataLength, hFile);

    return threadObject.bReturn;
}

inline bool SendHTTPCommandAndWait(char * strTriggerURL, bool bSendURL, bool bRemoveHeader, char *lpData, long nDataLength, char *lpHeader, long nHeaderLength, HANDLE hFile, DWORD dwTCPFrameSize, long nWait)
{
    SOCKET socketConnection;

    THREADED_SOCKET threadObject;
    threadObject.pSocket = &socketConnection;
    threadObject.lpTriggerURL = strTriggerURL;
    threadObject.bSendURL = bSendURL;
    threadObject.bRemoveHeader = bRemoveHeader;
    threadObject.lpData = lpData;
    threadObject.nDataLength = nDataLength;
    threadObject.lpHeader = lpHeader;
    threadObject.nHeaderLength = nHeaderLength;
    threadObject.dwTCPFrameSize = dwTCPFrameSize;
    threadObject.hFile = hFile;

    DWORD nThreadID = 0;
    HANDLE hThread = NULL;
    hThread = CreateThread( NULL,
        0, 
        _SendHTTPCommand,
        &threadObject,
        0,
        &nThreadID);

    if (hThread)
    {
        if (WAIT_TIMEOUT == WaitForSingleObject(hThread, nWait))
        {
            // shutdown the connection since no more data will be sent and allow thread to shutdown gracefully
            int iResult = shutdown(socketConnection, SD_SEND);
            closesocket(socketConnection);

            if (WAIT_TIMEOUT == WaitForSingleObject(hThread, 2000))
                TerminateThread(hThread, 0);            // Thread locked to force terminate

            threadObject.bReturn = false;

            CloseHandle(hThread);
            hThread = NULL;

            return false;
        }

        CloseHandle(hThread);
        hThread = NULL;
    }
    // If unable to start thread then don't send http because without threading around http call if http call hangs it locks the calling thread
//    else
//        return SendHTTPCommand_Actual(&socketConnection, strTriggerURL, bSendURL, bRemoveHeader, lpReturn, nDataLength, hFile);

    return threadObject.bReturn;
}

DWORD WINAPI _SendHTTPCommand(void* _pThreadObject)
{
    if (!_pThreadObject)
    {
        ExitThread(1);
        return 1;
    }

    THREADED_SOCKET *pThreadObject = (THREADED_SOCKET *)_pThreadObject;

    if (!pThreadObject->pSocket)
    {
        ExitThread(1);
        return 1;
    }

    pThreadObject->bReturn = SendHTTPCommand_Actual(pThreadObject->pSocket, pThreadObject->lpTriggerURL, pThreadObject->bSendURL, pThreadObject->bRemoveHeader, pThreadObject->lpData, pThreadObject->nDataLength, pThreadObject->lpHeader, pThreadObject->nHeaderLength, pThreadObject->hFile, pThreadObject->dwTCPFrameSize);

    ExitThread(pThreadObject->bReturn);
    return pThreadObject->bReturn;
}

