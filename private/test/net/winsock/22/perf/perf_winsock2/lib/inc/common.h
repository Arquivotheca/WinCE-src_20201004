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
// common.h

#ifndef _COMMON_H_
#define _COMMON_H_

#ifndef _PREFAST_
#pragma warning(disable:4068)
#endif

// Server port that client will connect on to submit test requests
const WORD  SERVER_SERVICE_REQUEST_PORT = 4998;

// Length of time to wait before closing connection on no activity
const DWORD TEST_TIMEOUT_MS = 20000;

// Length of time to wait on test connection establishment
const DWORD TEST_CONNECTION_TIMEOUT_MS = 50000;

#endif // _COMMON_H_
