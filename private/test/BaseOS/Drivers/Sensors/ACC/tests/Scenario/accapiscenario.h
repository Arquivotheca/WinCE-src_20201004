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

#include "CClientManager.h"
#include "AccApiFuncWrap.h"

void tst_StartScenario01( CClientManager::ClientPayload* pPayload );
void tst_StartScenario02( CClientManager::ClientPayload* pPayload );
void tst_QueueScenario01( CClientManager::ClientPayload* pPayload );
void tst_QueueScenario02( CClientManager::ClientPayload* pPayload );
void tst_EndScenario01( CClientManager::ClientPayload* pPayload );
void tst_EndScenario02( CClientManager::ClientPayload* pPayload );
void tst_EndScenario03( CClientManager::ClientPayload* pPayload );
void tst_EndScenario04( CClientManager::ClientPayload* pPayload );
void tst_CallbackScenario01( CClientManager::ClientPayload* pPayload );
void tst_CallbackScenario02( CClientManager::ClientPayload* pPayload );
void tst_CallbackScenario03( CClientManager::ClientPayload* pPayload );
void tst_CallbackScenario04( CClientManager::ClientPayload* pPayload );
void tst_PowerScenario01( CClientManager::ClientPayload* pPayload );
void tst_QueueScenario03( CClientManager::ClientPayload* pPayload );



