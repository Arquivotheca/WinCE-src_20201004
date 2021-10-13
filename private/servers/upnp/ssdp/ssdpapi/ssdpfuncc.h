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

    ssdpfuncc.h

Abstract:

    This file contains cross files function prototypes.

Author: Ting Cai

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

