/*++

Copyright (c) 2004 Microsoft Corporation

Module Name:

    wlanplap.h

Abstract:

    Definitions and data strcutures for WLAN PLAP API.

Environment:

    User mode only

Revision History:

--*/

#pragma once

#include <plapcreds.h>
#ifdef __cplusplus
extern "C" {
#endif

#define L2_REASON_CODE_PRIV_GROUP_SIZE             0x10000
#define L2_REASON_CODE_PRIV_GEN_BASE               0x30000
#define L2_REASON_CODE_PRIV_L2NA_BASE              (L2_REASON_CODE_PRIV_GEN_BASE+L2_REASON_CODE_PRIV_GROUP_SIZE)

#define L2NA_LAYER2_TIMEOUT	L2_REASON_CODE_PRIV_L2NA_BASE +1
#define L2NA_LAYER2_FAIL	L2_REASON_CODE_PRIV_L2NA_BASE +2
#define L2NA_LAYER3_IP_TIMEOUT	L2_REASON_CODE_PRIV_L2NA_BASE +3
#define L2NA_LAYER3_DC_TIMEOUT	L2_REASON_CODE_PRIV_L2NA_BASE +4


// PLAP related functions
DWORD
WlanInitPlapParams(
    __inout PPLAP_UI_PARAMS pPlapUIParams
);

DWORD
WlanDeinitPlapParams();

DWORD
WlanDoPlap(
    __in GUID* pInterfaceGuid,
    __in LPCWSTR strProfileName,
    __in DWORD dwNumOfFields,
    __in_ecount(dwNumOfFields) PLAP_INPUT_FIELD_DATA pPlapData[],
    __in DWORD dwTimeOut,
    __in BOOL fAllowLogonDialogs,
    __in DWORD dwTimeOutWithUI,
    __in BOOL  fUserBasedVlan,
    __in HWND hwndParentWindow,
    __out DWORD *pdwReturnCode
);

DWORD
WlanQueryPlapCredentials(
    __out DWORD *pdwNumOfFields,
    __deref_inout_opt PPLAP_INPUT_FIELD_DATA *ppPlapData,
    __deref_out LPWSTR *pstrProfileName,
    __out GUID* pInterfaceGuid,
    __out DWORD *pdwTimeout,
    __out BOOL *pfAllowLogonDialogs,
    __out DWORD *pdwTimeoutWithUI,
    __out BOOL  *pbUserBasedVlan
);

DWORD
WlanCancelPlap(
    __in GUID* pInterfaceGuid

);

#ifdef __cplusplus
}
#endif

