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
/*++


File Name:

    server.cpp

Abstract:

    This file contains code which implements rpc server start and stop.

Author: Ting Cai

Created: 07/10/1999

--*/

#include <ssdppch.h>
#pragma hdrstop

/*********************************************************************/
/*                 Global vars for debugging                         */
/*********************************************************************/

// Updated through interlocked exchange
LONG bShutdown = 0;

DWORD ProcessReceiveBuffer(ssdp_queue *pQueue);
bool NotifyAliveByebye(PSSDP_REQUEST pSsdpRequest);

// ProcessSsdpRequest
VOID ProcessSsdpRequest(PSSDP_REQUEST pSsdpRequest, RECEIVE_DATA *pData)
{
    if (pSsdpRequest->Method == SSDP_M_SEARCH)
    {
        // The Source port must be greater than 1024, and not equal to 1900
        WORD THRESHHOLD = 1024;
        WORD port = 0;
        if (pData->RemoteSocket.ss_family == AF_INET)
        {
            SOCKADDR_IN *sa = (SOCKADDR_IN*)&pData->RemoteSocket;
            port = htons( sa->sin_port );
        }
        else if (pData->RemoteSocket.ss_family == AF_INET6)
        {
            SOCKADDR_IN6 *sa6 = (SOCKADDR_IN6*)&pData->RemoteSocket;
            port = htons( sa6->sin6_port );
        }
        if (port <= THRESHHOLD || port == SSDP_PORT)
        {
            TraceTag(ttidSsdpSocket, "Error: ignoring with bad source port (%d)", port);
            return;
        }
        
        TraceTag(ttidSsdpSocket, "Searching for ST (%s)", pSsdpRequest->Headers[SSDP_ST]);
        SearchListAnnounce(pSsdpRequest, pData->socket, &pData->RemoteSocket);
    }
    else if (pSsdpRequest->Method == SSDP_NOTIFY)
    {
        TraceTag(ttidSsdpSocket, "Receive notification of type (%s)", pSsdpRequest->Headers[SSDP_NT]);

        Assert(!_stricmp(pSsdpRequest->Headers[SSDP_NTS], "ssdp:alive") ||
               !_stricmp(pSsdpRequest->Headers[SSDP_NTS], "ssdp:byebye"));
        
        BOOL IsSubscribed;

        // We only cache notification that clients has subscribed.  
        IsSubscribed = NotifyAliveByebye(pSsdpRequest);

        /*if (UpdateListCache(pSsdpRequest, IsSubscribed) == FALSE)*/
    }
    else
    {
        TraceTag(ttidSsdpSocket, "Unrecognized SSDP request.");
    }
}


// ProcessReceiveBuffer
DWORD ProcessReceiveBuffer(VOID* pvContext)
{
    // Check input parameter
    ssdp_queue   *pQueue = (ssdp_queue*)pvContext;
    if (pQueue == NULL)
    {
        Assert(pvContext);
        return 0;
    }
    
    // Fetch the next message - this may be NULL on rare occassions.
    RECEIVE_DATA *pData = pQueue->pop();
    if (pData == NULL)
    {
        return 0;
    }

    // Verify the next message's buffer
    if (pData->szBuffer == NULL)
    {
        Assert(pData->szBuffer);
        free(pData);
        return 0;
    }

    // Prepare the internal work item
    SSDP_REQUEST SsdpMessage;
    InitializeSsdpRequest(&SsdpMessage);
    
    // Process the message
    if(ParseSsdpRequest(pData->szBuffer, &SsdpMessage))
    {
        if(SsdpMessage.Headers[SSDP_LOCATION])
        {
            DWORD        dw = 0;
            if(!FixURLAddressScopeId(SsdpMessage.Headers[SSDP_LOCATION], pData->dwIndex, NULL, &dw) && dw != 0)
            {
                if(char* pszLocation = (char*)SsdpAlloc(dw))
                {
                    if(FixURLAddressScopeId(SsdpMessage.Headers[SSDP_LOCATION], pData->dwIndex, pszLocation, &dw))
                    {                    
                        SsdpFree(SsdpMessage.Headers[SSDP_LOCATION]);
                        SsdpMessage.Headers[SSDP_LOCATION] = pszLocation;
                    }
                    else
                    {                    
                        SsdpFree(pszLocation);
                    }
                }
            }
        }
        
        ProcessSsdpRequest(&SsdpMessage, pData); 
    }
    
    FreeSsdpRequest(&SsdpMessage);
    free(pData->szBuffer);
    free(pData);
    
    return 0;
}
