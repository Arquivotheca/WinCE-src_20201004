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
#ifndef __TUXSTUFF_H__
#define __TUXSTUFF_H__

#include "ws2bvt.h"

// Test function prototypes (TestProc's)
TESTPROCAPI PresenceTest                        (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI TransferTest                        (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);

// Our function table that we pass to Tux
static FUNCTION_TABLE_ENTRY g_lpFTE[] = {
TEXT(   "Presence Tests"                             ), 2,                              0,        0, NULL,
TEXT(         "IPv4 Stack"                           ), 3,                      PRES_IPV4, PRES+  1, PresenceTest,
TEXT(         "IPv6 Stack"                           ), 3,                      PRES_IPV6, PRES+  2, PresenceTest,
TEXT(   "Transfer Tests"                             ), 2,                              0,        0, NULL,
TEXT(         "IPv4 TCP Transfer"                    ), 3,                 TRANS_TCP_IPV4, TRAN+  1, TransferTest,
TEXT(         "IPv6 TCP Transfer"                    ), 3,                 TRANS_TCP_IPV6, TRAN+  2, TransferTest,
NULL,                                                0,                                 0,        0, NULL  // marks end of list
};

#endif // __TUXSTUFF_H__
