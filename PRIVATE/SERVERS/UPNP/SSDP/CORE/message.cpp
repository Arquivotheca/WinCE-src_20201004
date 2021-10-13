//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <ssdppch.h>
#pragma hdrstop
#include "ssdpparser.h"

VOID PrintSsdpMessageList(MessageList *list)
{
    int i;

    TraceTag(ttidSsdpNotify, "Printing notification list.");

    for (i = 0; i < list->size; i++)
    {
        PrintSsdpRequest(list->list+i);
        TraceTag(ttidSsdpNotify, "--------");
    }
}

VOID WINAPI FreeSsdpMessage(PSSDP_MESSAGE pSsdpMessage)
{
    if(pSsdpMessage->szAltHeaders)
        free(pSsdpMessage->szAltHeaders);
        
    if(pSsdpMessage->szNls)
        free(pSsdpMessage->szNls);
        
    if(pSsdpMessage->szContent)
        free(pSsdpMessage->szContent);
        
    if(pSsdpMessage->szLocHeader)
        free(pSsdpMessage->szLocHeader);
        
    if(pSsdpMessage->szSid)
        free(pSsdpMessage->szSid);
    
    if(pSsdpMessage->szType)    
        free(pSsdpMessage->szType);
        
    if(pSsdpMessage->szUSN)
        free(pSsdpMessage->szUSN);
        
    if(pSsdpMessage)
        free(pSsdpMessage);
}

PSSDP_MESSAGE InitializeSsdpMessageFromRequest(const PSSDP_REQUEST pSsdpRequest)
{
	PSSDP_MESSAGE pSsdpMessage;
	pSsdpMessage = (PSSDP_MESSAGE)malloc(sizeof(SSDP_MESSAGE));
	if (!pSsdpMessage)
		return NULL;
		
	memset(pSsdpMessage,0,sizeof(SSDP_MESSAGE));
    pSsdpMessage->szLocHeader = NULL;
    pSsdpMessage->szAltHeaders = NULL;
    pSsdpMessage->szNls = NULL;
    pSsdpMessage->szType = NULL;
    pSsdpMessage->szUSN = NULL;
    pSsdpMessage->szContent = NULL;
    pSsdpMessage->szSid = NULL;
    pSsdpMessage->iLifeTime = 0;
    pSsdpMessage->iSeq = 0;

    if (pSsdpRequest->Headers[SSDP_NT] != NULL)
    {
        pSsdpMessage->szType = (char *) malloc(sizeof(char) * (strlen(pSsdpRequest->Headers[SSDP_NT]) + 1));
        if (pSsdpMessage->szType == NULL)
        {
            goto cleanup;
        }
        else
        {
            strcpy(pSsdpMessage->szType, pSsdpRequest->Headers[SSDP_NT]);
        }
    }
    else if (pSsdpRequest->Headers[SSDP_ST] != NULL)
    {
        pSsdpMessage->szType = (char *) malloc(sizeof(char) * (strlen(pSsdpRequest->Headers[SSDP_ST]) + 1));
        if (pSsdpMessage->szType == NULL)
        {
            goto cleanup;
        }
        else
        {
            strcpy(pSsdpMessage->szType, pSsdpRequest->Headers[SSDP_ST]);
        }
    }

    if (pSsdpRequest->Headers[SSDP_LOCATION] != NULL)
    {
        pSsdpMessage->szLocHeader = (char *) malloc(sizeof(char) * (strlen(pSsdpRequest->Headers[SSDP_LOCATION]) + 1));
        if (pSsdpMessage->szLocHeader == NULL)
        {
            goto cleanup;
        }
        else
        {
            strcpy(pSsdpMessage->szLocHeader, pSsdpRequest->Headers[SSDP_LOCATION]);
        }
    }

    if (pSsdpRequest->Headers[SSDP_AL] != NULL)
    {
        pSsdpMessage->szAltHeaders = (char *) malloc(sizeof(char) * (strlen(pSsdpRequest->Headers[SSDP_AL]) + 1));
        if (pSsdpMessage->szAltHeaders == NULL)
        {
            goto cleanup;
        }
        else
        {
            strcpy(pSsdpMessage->szAltHeaders, pSsdpRequest->Headers[SSDP_AL]);
        }
    }

    if (pSsdpRequest->Headers[SSDP_NLS] != NULL)
    {
        pSsdpMessage->szNls = (char *) malloc(sizeof(char) * (strlen(pSsdpRequest->Headers[SSDP_NLS]) + 1));
        if (pSsdpMessage->szNls == NULL)
        {
            goto cleanup;
        }
        else
        {
            strcpy(pSsdpMessage->szNls, pSsdpRequest->Headers[SSDP_NLS]);
        }
    }

    if (pSsdpRequest->Headers[SSDP_USN] != NULL)
    {
        pSsdpMessage->szUSN = (char *) malloc(sizeof(char) * (strlen(pSsdpRequest->Headers[SSDP_USN]) + 1));
        if (pSsdpMessage->szUSN == NULL)
        {
            goto cleanup;
        }
        else
        {
            strcpy(pSsdpMessage->szUSN, pSsdpRequest->Headers[SSDP_USN]);
        }
    }

    if (pSsdpRequest->Headers[GENA_SID] != NULL)
    {
        pSsdpMessage->szSid = (char *) malloc(sizeof(char) * (strlen(pSsdpRequest->Headers[GENA_SID]) + 1));
        if (pSsdpMessage->szSid == NULL)
        {
            goto cleanup;
        }
        else
        {
            strcpy(pSsdpMessage->szSid, pSsdpRequest->Headers[GENA_SID]);
        }
    }

    if (pSsdpRequest->Headers[GENA_SEQ] != NULL)
    {
        pSsdpMessage->iSeq = strtoul(pSsdpRequest->Headers[GENA_SEQ], NULL, 10);
    }

    if (pSsdpRequest->Content)
    {
        pSsdpMessage->szContent = _strdup(pSsdpRequest->Content);
        if (pSsdpMessage->szContent == NULL)
        {
            goto cleanup;
        }
    }

    pSsdpMessage->iLifeTime = GetMaxAgeFromCacheControl(pSsdpRequest->Headers[SSDP_CACHECONTROL]);

    return pSsdpMessage;

cleanup:
    FreeSsdpMessage(pSsdpMessage);

    return NULL;
}

PSSDP_MESSAGE CopySsdpMessage( const SSDP_MESSAGE *pSource)
{
	PSSDP_MESSAGE pDestination;
	pDestination = (PSSDP_MESSAGE)malloc(sizeof(SSDP_MESSAGE));
	if (!pDestination)
		return NULL;
    pDestination->szLocHeader = NULL;
    pDestination->szAltHeaders = NULL;
    pDestination->szNls = NULL;
    pDestination->szType = NULL;
    pDestination->szUSN = NULL;
    pDestination->szSid = NULL;
    pDestination->szContent = NULL;
    pDestination->iLifeTime = 0;

    if (pSource->szType != NULL)
    {
        pDestination->szType = (char *) malloc(sizeof(char) * (strlen(pSource->szType) + 1));
        if (pDestination->szType == NULL)
        {
            goto cleanup;
        }
        else
        {
            strcpy(pDestination->szType, pSource->szType);
        }
    }

    if (pSource->szLocHeader != NULL)
    {
        pDestination->szLocHeader = (char *) malloc(sizeof(char) * (strlen(pSource->szLocHeader) + 1));
        if (pDestination->szLocHeader == NULL)
        {
            goto cleanup;
        }
        else
        {
            strcpy(pDestination->szLocHeader, pSource->szLocHeader);
        }
    }

    if (pSource->szAltHeaders != NULL)
    {
        pDestination->szAltHeaders = (char *) malloc(sizeof(char) * (strlen(pSource->szAltHeaders) + 1));
        if (pDestination->szAltHeaders == NULL)
        {
            goto cleanup;
        }
        else
        {
            strcpy(pDestination->szAltHeaders, pSource->szAltHeaders);
        }
    }
    
    if (pSource->szNls != NULL)
    {
        pDestination->szNls = (char *) malloc(sizeof(char) * (strlen(pSource->szNls) + 1));
        if (pDestination->szNls == NULL)
        {
            goto cleanup;
        }
        else
        {
            strcpy(pDestination->szNls, pSource->szNls);
        }
    }

    if (pSource->szUSN != NULL)
    {
        pDestination->szUSN = (char *) malloc(sizeof(char) * (strlen(pSource->szUSN) + 1));
        if (pDestination->szUSN == NULL)
        {
            goto cleanup;
        }
        else
        {
            strcpy(pDestination->szUSN, pSource->szUSN);
        }
    }

    if (pSource->szSid != NULL)
    {
        pDestination->szSid = (char *) malloc(sizeof(char) * (strlen(pSource->szSid) + 1));
        if (pDestination->szSid == NULL)
        {
            goto cleanup;
        }
        else
        {
            strcpy(pDestination->szSid, pSource->szSid);
        }
    }

    pDestination->iLifeTime = pSource->iLifeTime;
    pDestination->iSeq = pSource->iSeq;

    return pDestination;

cleanup:
    FreeSsdpMessage(pDestination);

    return NULL;
}



