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

#include "RasServerTest.h"

// Test function prototypes (TestProc's)
TESTPROCAPI RasServerGetStatus                (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI RasServerEnable                    (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI RasServerDisable                (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI RasServerGetParams                (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI RasServerSetParams                (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI RasServerLineAdd                (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI RasServerLineRemove                (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI RasServerLineEnable                (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI RasServerLineDisable            (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI RasServerLineGetParams            (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI RasServerLineSetParams             (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI RasServerUserSetCredentials     (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI RasServerUserDeleteCredentials     (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI RasServerSetNetPrefix           (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI RasServerGetNetPrefix           (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI RasServerLineGetConnInfo        (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);
TESTPROCAPI RasServerCommon                 (UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);

// BASE is a unique value assigned to a given tester or component.  This value,
// when combined with each of the following test's unique IDs, allows every 
// test case within the entire team to be uniquely identified.

#define BASE 0x00010000
#define RAS_SERVER_BASE    0

// Our function table that we pass to Tux
static FUNCTION_TABLE_ENTRY g_lpFTE[] = 
{
    TEXT(   "RAS Server IOCTL Tests"                     ), 1,                                    0,                    0, NULL,
    TEXT(   "RASCNTL_COMMON"                               ), 2,                                    0,                    0, NULL,
    TEXT(         "RC_INVALID_HANDLE"                    ), 3,                 RASSERVER_INVALID_HANDLE,     RAS_SERVER_BASE+   1, RasServerCommon,
    TEXT(         "RC_INVALID_IN_BUF"                    ), 3,                 RASSERVER_INVALID_IN_BUF,     RAS_SERVER_BASE+   2, RasServerCommon,
    TEXT(         "RC_INVALID_IN_LEN"                    ), 3,                 RASSERVER_INVALID_IN_LEN,     RAS_SERVER_BASE+   3, RasServerCommon,
    TEXT(         "RC_INVALID_OUT_BUF"                   ), 3,                 RASSERVER_INVALID_OUT_BUF,     RAS_SERVER_BASE+   4, RasServerCommon,
    TEXT(         "RC_INVALID_OUT_LEN"                   ), 3,                 RASSERVER_INVALID_OUT_LEN,     RAS_SERVER_BASE+   5, RasServerCommon,
    TEXT(   "RASCNTL_SERVER_GET_STATUS"                  ), 2,                                    0,                    0, NULL,
    TEXT(         "RSGS_VALID_ENABLED"                   ), 3,                 RASSERVER_ENABLED_SERVER,     RAS_SERVER_BASE+   6, RasServerGetStatus,
    TEXT(         "RSGS_VALID_DISABLED"                     ), 3,                   RASSERVER_DISABLED_SERVER,     RAS_SERVER_BASE+   7, RasServerGetStatus,
    TEXT(   "RASCNTL_SERVER_ENABLE"                      ), 2,                                    0,                    0, NULL,
    TEXT(         "RSE_VALID_ENABLED"                    ), 3,                 RASSERVER_ALREADY_ENABLED,     RAS_SERVER_BASE+  16, RasServerEnable,
    TEXT(         "RSE_VALID_DISABLED"                   ), 3,                 RASSERVER_ENABLE,             RAS_SERVER_BASE+  17, RasServerEnable,
    TEXT(   "RASCNTL_SERVER_DISABLE"                     ), 2,                                    0,                    0, NULL,
    TEXT(         "RSD_INVALID_DISABLED"                 ), 3,                 RASSERVER_ALREADY_DISABLED,     RAS_SERVER_BASE+  26, RasServerDisable,
    TEXT(         "RSD_VALID_ENABLED"                     ), 3,                   RASSERVER_ENABLED_SERVER,     RAS_SERVER_BASE+  27, RasServerDisable,
    TEXT(   "RASCNTL_SERVER_GET_PARAMETERS"              ), 2,                                    0,                    0, NULL,
    TEXT(         "RSGP_VALID_DISP"                         ), 3,                   RASSERVER_DISPLAY,            RAS_SERVER_BASE+  36, RasServerGetParams,
    TEXT(         "RSGP_DISABLED_PARAMS"                 ), 3,                   RASSERVER_DISABLED_PARAMS,            RAS_SERVER_BASE+  37, RasServerGetParams,
    TEXT(         "RSGP_ENABLED_PARAMS"                     ), 3,                   RASSERVER_ENABLED_PARAMS,            RAS_SERVER_BASE+  38, RasServerGetParams,
    TEXT(   "RASCNTL_SERVER_SET_PARAMETERS"              ), 2,                                    0,                    0, NULL,
    TEXT(         "RSSP_VALID_PARAMS"                    ), 3,                 RASSERVER_SET_VALID_PARAMS,     RAS_SERVER_BASE+  46, RasServerSetParams,
    TEXT(         "RSSP_INVALID_PARAMS"                     ), 3,                   RASSERVER_SET_INVALID_PARAMS,RAS_SERVER_BASE+  47, RasServerSetParams,
    TEXT(   "RASCNTL_SERVER_LINE_ADD"                    ), 2,                                    0,                    0, NULL,
    TEXT(         "RSLA_INVALID_ALREADY_ADDED"           ), 3,                 RASSERVER_ALREADY_ADDED,     RAS_SERVER_BASE+  57, RasServerLineAdd,
    TEXT(         "RSLA_INVALID_LINE"                    ), 3,                 RASSERVER_INVALID_LINE,         RAS_SERVER_BASE+  58, RasServerLineAdd,
    TEXT(         "RSLA_VALID_LINE"                         ), 3,                   RASSERVER_VALID_LINE,         RAS_SERVER_BASE+  59, RasServerLineAdd,
    TEXT(   "RASCNTL_SERVER_LINE_REMOVE"                 ), 2,                                    0,                    0, NULL,
    TEXT(         "RSLR_INVALID_REMOVED"                 ), 3,                 RASSERVER_ALREADY_REMOVED,     RAS_SERVER_BASE+  66, RasServerLineRemove,
    TEXT(         "RSLR_INVALID_NONEXIST"                ), 3,                 RASSERVER_NONEXISTENT_REMOVED, RAS_SERVER_BASE+  68, RasServerLineRemove,
    TEXT(         "RSLR_VALID_REMOVE"                     ), 3,                   RASSERVER_VALID_REMOVED,     RAS_SERVER_BASE+  69, RasServerLineRemove,
    TEXT(   "RASCNTL_SERVER_LINE_ENABLE"                 ), 2,                                    0,                    0, NULL,
    TEXT(         "RSLE_INVALID_ENABLED_ALREADY"         ), 3,                 RASSERVER_ALREADY_ENABLED,     RAS_SERVER_BASE+  76, RasServerLineEnable,
    TEXT(         "RSLE_INVALID_ENABLED"                 ), 3,                 RASSERVER_INVALID_ENABLED,     RAS_SERVER_BASE+  77, RasServerLineEnable,
    TEXT(         "RSLE_VALID_ENABLE"                    ), 3,                 RASSERVER_VALID_ENABLED,     RAS_SERVER_BASE+  78, RasServerLineEnable,
    TEXT(         "RSLE_INVALID_NONEXIST"                 ), 3,                   RASSERVER_NONEXISTENT_ENABLED, RAS_SERVER_BASE+  79, RasServerLineEnable,
    TEXT(   "RASCNTL_SERVER_LINE_DISABLE"                ), 2,                                    0,                    0, NULL,
    TEXT(         "RSLD_INVALID_ALREAY_DISABLED"         ), 3,                 RASSERVER_ALREADY_DISABLED,     RAS_SERVER_BASE+  86, RasServerLineDisable,
    TEXT(         "RSLD_INVALID_NONEXIST"                ), 3,                 RASSERVER_NONEXISTENT_DISABLED, RAS_SERVER_BASE+  87, RasServerLineDisable,
    TEXT(         "RSLD_VALID_DISABLED"                  ), 3,                 RASSERVER_VALID_DISABLED,    RAS_SERVER_BASE+  88, RasServerLineDisable,
    TEXT(   "RASCNTL_SERVER_LINE_GET_PARAMETERS"         ), 2,                                    0,                    0, NULL,
    TEXT(         "RSLGP_INVALID_LINE"                   ), 3,                 RASSERVER_INVALID_LINE,         RAS_SERVER_BASE+  96, RasServerLineGetParams,
    TEXT(         "RSLGP_VALID_LINE"                     ), 3,                   RASSERVER_VALID_LINE,         RAS_SERVER_BASE+  97, RasServerLineGetParams,
    TEXT(   "RASCNTL_SERVER_LINE_SET_PARAMETERS"         ), 2,                                    0,                    0, NULL,
    TEXT(         "RSLSP_INVALID_SETLINE"                ), 3,                 RASSERVER_INVALID_SET_LINE,     RAS_SERVER_BASE+  106, RasServerLineSetParams,
    TEXT(         "RSLSP_VALID_SETLINE"                  ), 3,                 RASSERVER_VALID_SET_LINE,     RAS_SERVER_BASE+  107, RasServerLineSetParams,
    TEXT(         "RSLSP_INVALID_NONEXIST"                 ), 3,                   RASSERVER_NONEXIST_LINE,     RAS_SERVER_BASE+  108, RasServerLineSetParams,
    TEXT(   "RASCNTL_SERVER_USER_SET_CREDENTIALS"        ), 2,                                    0,                    0, NULL,
    TEXT(         "RSUSC_INVALID_USERNAME"               ), 3,                 RASSERVER_INVALID_USER_NAME, RAS_SERVER_BASE+  116, RasServerUserSetCredentials,
    TEXT(         "RSUSC_NULL_USERNAME"                  ), 3,                 RASSERVER_NULL_USER_NAME, RAS_SERVER_BASE+  115, RasServerUserSetCredentials,
    TEXT(         "RSUSC_INVALID_LONG_USERNAME"          ), 3,                 RASSERVER_LONG_USER_NAME,     RAS_SERVER_BASE+  117, RasServerUserSetCredentials,
    TEXT(         "RSUSC_INVALID_LONG_PW"                ), 3,                 RASSERVER_LONG_PASSWORD,     RAS_SERVER_BASE+  118, RasServerUserSetCredentials,
    TEXT(         "RSUSC_INVALID_PASSWORD"               ), 3,                 RASSERVER_INVALID_PASSWORD,     RAS_SERVER_BASE+  119, RasServerUserSetCredentials,
    TEXT(         "RSUSC_VALID_INTL_UN"                     ), 3,                   RASSERVER_INTL_USERNAMES,     RAS_SERVER_BASE+  120, RasServerUserSetCredentials,
    TEXT(   "RASCNTL_SERVER_USER_DELETE_CREDENTIALS"     ), 2,                                    0,                    0, NULL,
    TEXT(         "RSUDC_INVALID_USERNAME"               ), 3,                 RASSERVER_INVALID_USER_NAME, RAS_SERVER_BASE+  126, RasServerUserDeleteCredentials,
    TEXT(         "RSUDC_INVALID_USER_EXISTS"            ), 3,                 RASSERVER_USER_EXISTS,         RAS_SERVER_BASE+  127, RasServerUserDeleteCredentials,
    TEXT(         "RSUDC_INVALID_USER_NOT_EXISTS"        ), 3,                 RASSERVER_USER_NOT_EXISTS,     RAS_SERVER_BASE+  128, RasServerUserDeleteCredentials,
    TEXT(         "RSUDC_NULL_USER_INTL_NAME"             ), 3,                   RASSERVER_USER_INTL_NAME,     RAS_SERVER_BASE+  129, RasServerUserDeleteCredentials,
    TEXT(   "RASCNTL_SERVER_SET_IPV6_NET_PREFIX"         ), 2,                                    0,                    0, NULL,
    TEXT(         "RSSINP_VALID_RANGE"                    ), 3,                 RASSERVER_VALID_RANGE,         RAS_SERVER_BASE+  136, RasServerGetNetPrefix,
    TEXT(         "RSSINP_INVALID_RANGE"                  ), 3,                 RASSERVER_INVALID_RANGE,     RAS_SERVER_BASE+  137, RasServerGetNetPrefix,
    TEXT(   "RASCNTL_SERVER_GET_IPV6_NET_PREFIX"          ), 2,                                    0,                    0, NULL,
    TEXT(         "RSGINP_VALID_RANGE"                    ), 3,                 RASSERVER_VALID_RANGE,         RAS_SERVER_BASE+  146, RasServerGetNetPrefix,
    TEXT(         "RSGINP_INVALID_RANGE"                  ), 3,                 RASSERVER_INVALID_RANGE,     RAS_SERVER_BASE+  147, RasServerGetNetPrefix,
    TEXT(   "RASCNTL_SERVER_LINE_GET_CONNECTION_INFO"     ), 2,                                    0,                    0, NULL,
    TEXT(         "RSLGCI_VALID_DISABLED_LINE"            ), 3,                 RASSERVER_LINE_DISABLED,     RAS_SERVER_BASE+  156, RasServerLineGetConnInfo,
    TEXT(         "RSLGCI_VALID_ADDED_LINE"               ), 3,                 RASSERVER_LINE_ADDED,         RAS_SERVER_BASE+  157, RasServerLineGetConnInfo,
    TEXT(         "RSLGCI_VALID_REMOVED_LINE"             ), 3,                 RASSERVER_LINE_REMOVED,     RAS_SERVER_BASE+  158, RasServerLineGetConnInfo,
    TEXT(         "RSLGCI_VALID_ENABLED_LINE"             ), 3,                 RASSERVER_LINE_ENABLED,     RAS_SERVER_BASE+  159, RasServerLineGetConnInfo,
    NULL,                                                 0,                                        0,                    0, NULL  // marks end of list
};

#endif // __TUXSTUFF_H__
