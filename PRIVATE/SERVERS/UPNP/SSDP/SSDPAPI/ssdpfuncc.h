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

    ssdpfuncc.h

Abstract:

    This file contains cross files function prototypes.



Created: 09/5/1999

--*/
#ifndef _SSDPFUNCC_
#define _SSDPFUNCC_

#include "ssdptypesc.h"
#include "ssdpnetwork.h"

BOOL IsInListNotify(char *szType);
void PrintSsdpMessageList(MessageList *list);
BOOL InitializeListSearch();
void CleanupListSearch();
void DerefSearchRequest(PSSDP_SEARCH_REQUEST SearchRequest);
BOOL WINAPI LookupCache(PSTR szType,
	SERVICE_CALLBACK_FUNC pfSearchCallback, LPVOID pvContext);
BOOL WINAPI UpdateCache(PSSDP_MESSAGE pSsdpMessage);

void FreeMessageList(MessageList *list);

#endif // _SSDPFUNCC_

