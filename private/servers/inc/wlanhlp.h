/*++

Copyright (c) 2004 Microsoft Corporation

Module Name:

    wlanhlp.h

Abstract:

    Definitions and data strcutures for WLAN client side helper API.

Environment:

    User mode only

Revision History:

--*/

#pragma once

#include <wlanapi.h>
#include <l2cmnpriv.h>
#include <wlanui.h>
#include <netconp.h>
#include <objbase.h>
#ifndef UNDER_CE
#include <msxml6.h>
#else
#include <msxml2.h>
#endif
#include <plapcreds.h>
#include "wlanplap.h"
#ifdef __cplusplus
extern "C" {
#endif

DWORD WINAPI
WlanGenerateProfileXmlBasicSettings(
    __in LPCWSTR strProfileName,
    __in PDOT11_SSID pDot11Ssid,
    __in DOT11_BSS_TYPE BssType,
    __in DOT11_AUTH_ALGORITHM Auth,
    __in DOT11_CIPHER_ALGORITHM Cipher,
    __in BOOL bDot1xEnabled,
    __in_opt LPCWSTR strKeyPassphrase,
    __in BOOL bAutoConnect,
    __in BOOL bNonBroadcast,
    __deref_out LPWSTR *pstrProfileXml
    );

DWORD WINAPI
WlanParseProfileXmlBasicSettings(
    __in LPCWSTR strProfileXml,
    __deref_out LPWSTR *pstrProfileName,
    __out PDOT11_SSID pDot11Ssid,
    __out PDOT11_BSS_TYPE pBssType,
    __out PBOOL pbAutoConnect,
    __out PBOOL pbNonBroadcast,
    __out PBOOL pbMsSecurityPresent,
    __out PDOT11_AUTH_ALGORITHM pAuth,
    __out PDOT11_CIPHER_ALGORITHM pCipher,
    __out PBOOL pbDot1xEnabled
    );

DWORD WINAPI
WlanIsUIRequestPending(
    __in CONST GUID* pUIRequestId,
    __in CONST PL2_UI_REQUEST pReceivedRequest,
    __out BOOL *pbRequestPending
    );

DWORD WINAPI
WlanQueryExtUIRequest(
    __in CONST GUID* pInterfaceGuid,
    __in CONST GUID* pNetworkId,
    __in WLAN_CONNECTION_PHASE connPhase,
    __out GUID* pUIRequestId,
    __out PL2_UI_REQUEST *ppUIRequest
);

DWORD WINAPI
WlanSendUIResponse(
    __in CONST PL2_UI_REQUEST pRequest,
    __in CONST PL2_UI_RESPONSE pResponse
    );

DWORD WINAPI
WlanRemoveUIForwardingNetworkList(
    __in CONST GUID *pGuidCookie
);

DWORD WINAPI
WlanSetUIForwardingNetworkList(
    __in_ecount(nNetworkSZ) CONST GUID*   pNetworkArray,
    __in CONST UINT    nNetworkSZ,
    __out GUID*        pGuidCookie
);

// check whether a network is suppressed in a user session
// when a client (CFE) sets a UI forwarding list,
// all networks in the list are suppressed, i.e.
// no UI request shall be shown via balloon
DWORD WINAPI
WlanIsNetworkSuppressed(
    __in CONST GUID *pNetworkGuid,
    __out BOOL *pbSuppressed 
);

// NOTE - The signature of this function is defined by netman can not be WINAPI.
HRESULT
QueryNetconStatus(
    __in CONST GUID *pInterfaceGuid,
    __out NETCON_STATUS *pncs
    );

DWORD WINAPI
WlanGetProfileKeyInfo(
    __in DWORD dwSessionId,                                 // Session ID of the logged on user
    __in CONST GUID *pInterfaceGuid,                        // Wireless NIC GUID
    __in LPCWSTR strProfileName,                            // Profile name
    __out BOOL *pbIsPassPhrase,                             // Network key or passphrase
    __out_bcount_opt(*pcbKeyLen) BYTE *pbKeyBuffer,         // Buffer to receive the key material
    __inout DWORD *pcbKeyLen                                // Pointer to length of the key buffer on
                                                            // input, length of the actual key on output
    );

// Implementation requested by UI team to encapsulate call to WlanSetSecuritySettings for
// the special case when the object wlan_secure_add_new_all_user_profiles needs to be 
// modified.

DWORD WINAPI
WlanSetAllUserProfileRestricted
(
    __in    BOOL    bRestricted
);


//
// Verify if "Every One" can add a new all user profile - also
// verify if the source of this decision is GP or not.
//
DWORD WINAPI
WlanQueryCreateAllUserProfileRestricted
(
    __out   BOOL*                       pbRestricted,
    __out   PWLAN_OPCODE_VALUE_TYPE     pValueType
);



#ifdef __cplusplus
}
#endif

