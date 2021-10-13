//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++


File Name:

    server.c

Abstract:

    This file contains code which implements rpc server start and stop.



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
    SSDP_REQUEST SsdpMessage;
    RECEIVE_DATA *pData;
    DWORD        dw;
    ssdp_queue   *pQueue = (ssdp_queue*)pvContext;
    
    pData = pQueue->pop();
    
    Assert(pData);

    AssertSz(pData->szBuffer != NULL, "SocketReceive should have allocated the buffer"); 
    
    InitializeSsdpRequest(&SsdpMessage);
    
    if(ParseSsdpRequest(pData->szBuffer, &SsdpMessage))
    {
        if(SsdpMessage.Headers[SSDP_LOCATION])
			if(!FixURLAddressScopeId(SsdpMessage.Headers[SSDP_LOCATION], pData->dwIndex, NULL, &(dw = 0)) && dw != 0)
                if(char* pszLocation = (char*)SsdpAlloc(dw))
                    if(FixURLAddressScopeId(SsdpMessage.Headers[SSDP_LOCATION], pData->dwIndex, pszLocation, &dw))
                    {                    
                        SsdpFree(SsdpMessage.Headers[SSDP_LOCATION]);
                        SsdpMessage.Headers[SSDP_LOCATION] = pszLocation;
                    }
                    else
                        SsdpFree(pszLocation);
        
        ProcessSsdpRequest(&SsdpMessage, pData); 
    }
    
    FreeSsdpRequest(&SsdpMessage);
    free(pData->szBuffer);
    free(pData);
    
    return 0;
}
