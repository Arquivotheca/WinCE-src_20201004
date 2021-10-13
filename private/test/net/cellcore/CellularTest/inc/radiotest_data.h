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

#ifndef _radiotest_data_h_
#define _radiotest_data_h_

#include "radiotest.h"

// Initialize threads etc
BOOL DAT_Init(DWORD dwUserData = 0);
BOOL DAT_DeInit(DWORD dwUserData = 0);
BOOL DAT_Summary(DWORD dwUserData = 0);

// Tests
BOOL DAT_Download(DWORD dwUserData);
BOOL DAT_Upload(DWORD dwUserData);
BOOL DAT_GPRSSequentialAPNsTest(DWORD dwUserData);
BOOL DAT_GPRSMultipleAPNsTest(DWORD dwUserData);

// Keeping track of block sizes
#define   MAX_DOWNLOAD_BLOCKS       256
#define   DEFAULT_BLOCK_SIZE        262144
#define   DEFAULT_BLOCK_PER_SESSION 40
#define   DEFAULT_DOWNLOAD_PATH     _T("http://www.sfone.net/gprs.zip")
#define   DEFAULT_UPLOAD_PATH       _T("http://www.sfone.net")
#define   DEFAULT_CONN_TIMEOUT      4
#define   DEFAULT_DATA_TIMEOUT      4
#define   DEFAULT_UPLOAD_SIZE       1048576
#define   DEFAULT_GPRS_MAPN_SIZE    1048576

#define   GPRS_BASE_NAME            _T("DataTrans_GPRS")
#define   DEFAULT_NUM_CONTEXTS      1
#define   MAX_NUM_CONTEXTS          15

#define   DATA_TEST_IPC_NAME        _T("Radiotest_data")

// CSD or GPRS again
typedef enum
{
    GPRS_DATA,
    CSD_DATA
}DATA_TYPE;

extern DWORD g_dwRandomBlockSizes[];
extern DWORD g_dwWinInetConnectTimeoutMinute;
extern DWORD g_dwWinInetDataTimeoutMinute;
extern DWORD g_dwNumContexts;
extern TCHAR g_tszConnName[];
extern BOOL  g_fLeaveOn;
extern BOOL  g_fReleaseAll;

BOOL Connect (DATA_TYPE dt, DWORD *dwContextID);
BOOL Disconnect (DATA_TYPE dt, DWORD *dwContextID);

BOOL TestPassAsync (DATA_TYPE dt, DWORD dwIterations, DWORD dwBlockSize, BOOL fSuspendResume, BOOL fShowDetails, BOOL fDHCPSleep = FALSE, BOOL fStayOpen = FALSE);
#endif
