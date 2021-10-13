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
// ----------------------------------------------------------------------------
//
// Use of this source code is subject to the terms of the Microsoft end-user
// license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
// If you did not accept the terms of the EULA, you are not authorized to use
// this source code. For a copy of the EULA, please see the LICENSE.RTF on your
// install media.
//
// ----------------------------------------------------------------------------
//
// Definitions and declarations for the CMWiFiIntf_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_CMWiFiIntf_t_
#define _DEFINED_CMWiFiIntf_t_
#pragma once

#include "CMWiFiTypes.hpp"
#if (CMWIFI_VERSION > 0)

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// CMWiFiIntf_t is an interface to the dynamically-loaded CMNet library.
// Each object of this class represents a reference to the library.
// When the last of these objects is deleted, the library is unloaded.
//
class CMWiFiIntf_t
{
public:

    // Construction and destruction:
    //
    CMWiFiIntf_t(void);
   ~CMWiFiIntf_t(void);

    static bool
    IsLibraryLoaded(void)
    {
        return sm_hCMDll != NULL;
    }

    static CM_RESULT
    AcquireConnection(
        __in CM_CONNECTION_HANDLE hConnection)
    {
        CM_RESULT retval = CMRE_NOT_IMPLEMENTED;
        if (sm_pfAcquireConnection)
            retval = sm_pfAcquireConnection(
                                hConnection);
        return retval;
    }

    static CM_RESULT
    AddConnectionConfig(
        __in                  const WCHAR          *pszConnection,
        __in_bcount(cbConfig) CM_CONNECTION_CONFIG *pConfig,
        __in                  DWORD                 cbConfig)
    {
        CM_RESULT retval = CMRE_NOT_IMPLEMENTED;
        if (sm_pfAddConnectionConfig)
            retval = sm_pfAddConnectionConfig(
                                pszConnection,
                                pConfig,
                                cbConfig);
        return retval;
    }

    static CM_RESULT
    CloseSession(
        CM_SESSION_HANDLE hSession)
    {
        CM_RESULT retval = CMRE_NOT_IMPLEMENTED;
        if (sm_pfCloseSession)
            retval = sm_pfCloseSession(
                                hSession);
        return retval;
    }

    static CM_SESSION_HANDLE
    CreateSession(
        void)
    {
        CM_SESSION_HANDLE retval = NULL;
        if (sm_pfCreateSession)
            retval = sm_pfCreateSession();
        return retval;
    }

    static CM_RESULT
    DeleteConnectionConfig(
        __in const WCHAR *pszConnection)
    {
        CM_RESULT retval = CMRE_NOT_IMPLEMENTED;
        if (sm_pfDeleteConnectionConfig)
            retval = sm_pfDeleteConnectionConfig(
                                pszConnection);
        return retval;
    }

    static CM_RESULT
    EnumConnectionsConfig(
        __inout_bcount(*pcbNameList) CM_CONNECTION_NAME_LIST *pNameList,
        __inout                      DWORD                   *pcbNameList)
    {
        CM_RESULT retval = CMRE_NOT_IMPLEMENTED;
        if (sm_pfEnumConnectionsConfig)
            retval = sm_pfEnumConnectionsConfig(
                                pNameList,
                                pcbNameList);
        return retval;
    }

    static CM_RESULT
    EnumConnectionsConfigByType(
        __in                         CM_CONNECTION_TYPE       Type,
        __inout_bcount(*pcbNameList) CM_CONNECTION_NAME_LIST *pNameList,
        __inout                      DWORD                   *pcbNameList)
    {
        CM_RESULT retval = CMRE_NOT_IMPLEMENTED;
        if (sm_pfEnumConnectionsConfigByType)
            retval = sm_pfEnumConnectionsConfigByType(
                                Type,
                                pNameList,
                                pcbNameList);
        return retval;
    }

    static CM_RESULT
    GetConnectionConfig(
        __in                       const WCHAR          *pszConnection,
        __inout_bcount(*pcbConfig) CM_CONNECTION_CONFIG *pConfig,
        __inout                    DWORD                *pcbConfig)
    {
        CM_RESULT retval = CMRE_NOT_IMPLEMENTED;
        if (sm_pfGetConnectionConfig)
            retval = sm_pfGetConnectionConfig(
                                pszConnection,
                                pConfig,
                                pcbConfig);
        return retval;
    }

    static CM_RESULT
    GetConnectionDetailsByHandle(
        __in                        CM_CONNECTION_HANDLE   hConnection,
        __inout_bcount(*pcbDetails) CM_CONNECTION_DETAILS *pDetails,
        __inout                     DWORD                 *pcbDetails)
    {
        CM_RESULT retval = CMRE_NOT_IMPLEMENTED;
        if (sm_pfGetConnectionDetailsByHandle)
            retval = sm_pfGetConnectionDetailsByHandle(
                                hConnection,
                                pDetails,
                                pcbDetails);
        return retval;
    }

    static CM_RESULT
    GetConnectionDetailsByName(
        __in                        const WCHAR           *pszConnection,
        __inout_bcount(*pcbDetails) CM_CONNECTION_DETAILS *pDetails,
        __inout                     DWORD                 *pcbDetails)
    {
        CM_RESULT retval = CMRE_NOT_IMPLEMENTED;
        if (sm_pfGetConnectionDetailsByName)
            retval = sm_pfGetConnectionDetailsByName(
                                pszConnection,
                                pDetails,
                                pcbDetails);
        return retval;
    }
    
    static CM_RESULT
    GetConnectionSelectionResult(
        __in CM_SESSION_HANDLE hSession)
    {
        CM_RESULT retval = CMRE_NOT_IMPLEMENTED;
        if (sm_pfGetConnectionSelectionResult)
            retval = sm_pfGetConnectionSelectionResult(
                                hSession);
        return retval;
    }

    static CM_RESULT
    GetFirstCandidateConnection(
        __in  CM_SESSION_HANDLE              hSession,
        __in  const WCHAR                   *pszHost,
        __in  CM_CONNECTION_SELECTION_OPTION Option,
        __out CM_CONNECTION_HANDLE          *phConnection)
    {
        CM_RESULT retval = CMRE_NOT_IMPLEMENTED;
        if (sm_pfGetFirstCandidateConnection)
            retval = sm_pfGetFirstCandidateConnection(
                                hSession,
                                pszHost,
                                Option,
                                phConnection);
        return retval;
    }

    static CM_RESULT
    GetFirstIpAddr(
        __in                            CM_CONNECTION_HANDLE hConnection,
        __in                            const WCHAR         *pszHost,
        __in                            ADDRESS_FAMILY       AddrFamily,
        __in                            USHORT               SrcPort,
        __in                            USHORT               DstPort,
        __inout_bcount(*pcbAddressPair) CM_ADDRESS_PAIR     *pAddressPair,
        __inout                         DWORD               *pcbAddressPair)
    {
        CM_RESULT retval = CMRE_NOT_IMPLEMENTED;
        if (sm_pfGetFirstIpAddr)
            retval = sm_pfGetFirstIpAddr(
                                hConnection,
                                pszHost,
                                AddrFamily,
                                SrcPort,
                                DstPort,
                                pAddressPair,
                                pcbAddressPair);
        return retval;
    }

    static CM_RESULT
    GetNextCandidateConnection(
        __in  CM_SESSION_HANDLE     hSession,
        __out CM_CONNECTION_HANDLE *phConnection)
    {
        CM_RESULT retval = CMRE_NOT_IMPLEMENTED;
        if (sm_pfGetNextCandidateConnection)
            retval = sm_pfGetNextCandidateConnection(
                                hSession,
                                phConnection);
        return retval;
    }

    static CM_RESULT
    GetNextIpAddr(
        __in                            CM_CONNECTION_HANDLE hConnection,
        __inout_bcount(*pcbAddressPair) CM_ADDRESS_PAIR     *pAddressPair,
        __inout                         DWORD               *pcbAddressPair)
    {
        CM_RESULT retval = CMRE_NOT_IMPLEMENTED;
        if (sm_pfGetNextIpAddr)
            retval = sm_pfGetNextIpAddr(
                                hConnection,
                                pAddressPair,
                                pcbAddressPair);
        return retval;
    }

    static CM_RESULT
    GetNotification(
        __in                                 CM_NOTIFICATIONS_LISTENER_HANDLE hListener,
        __inout_bcount_opt(*pcbNotification) CM_NOTIFICATION                 *pNotification,
        __inout                              DWORD                           *pcbNotification)
    {
        CM_RESULT retval = CMRE_NOT_IMPLEMENTED;
        if (sm_pfGetNotification)
            retval = sm_pfGetNotification(
                                hListener,
                                pNotification,
                                pcbNotification);
        return retval;
    }

    static CM_RESULT
    GetPolicyConfig(
        __in_bcount(cbKey)      CM_POLICY_CONFIG_KEY    *pKey,
        __in                    DWORD                    cbKey,
        __out_bcount(*pcbData)  CM_POLICY_CONFIG_DATA   *pData,
        __inout                 DWORD                   *pcbData)
    {
        CM_RESULT retval = CMRE_NOT_IMPLEMENTED;
        if (sm_pfGetPolicyConfig)
            retval = sm_pfGetPolicyConfig(
                                pKey,
                                cbKey,
                                pData,
                                pcbData);
        return retval;
    }

    static CM_RESULT
    GetToUpdateConnectionConfig(
        __in                       const WCHAR             *pszConnection,
        __inout_bcount(*pcbConfig) CM_CONNECTION_CONFIG    *pConfig,
        __inout                    DWORD                   *pcbConfig,
        __out                      CM_CONFIG_CHANGE_HANDLE *phConfig)
    {
        CM_RESULT retval = CMRE_NOT_IMPLEMENTED;
        if (sm_pfGetToUpdateConnectionConfig)
            retval = sm_pfGetToUpdateConnectionConfig(
                                pszConnection,
                                pConfig,
                                pcbConfig,
                                phConfig);
        return retval;
    }

    static CM_RESULT
    GetToUpdatePolicyConfig(
        __in_bcount(cbKey)      CM_POLICY_CONFIG_KEY        *pKey,
        __in                    DWORD                        cbKey,
        __out_bcount(*pcbData)  CM_POLICY_CONFIG_DATA       *pData,
        __inout                 DWORD                       *pcbData,
        __out                   CM_CONFIG_CHANGE_HANDLE     *phConfig)
    {
        CM_RESULT retval = CMRE_NOT_IMPLEMENTED;
        if (sm_pfGetToUpdatePolicyConfig)
            retval = sm_pfGetToUpdatePolicyConfig(
                                pKey,
                                cbKey,
                                pData,
                                pcbData,
                                phConfig);
        return retval;
    }

    static CM_RESULT
    RegisterNotificationsListener(
        __in_bcount(cbRegistration) CM_NOTIFICATIONS_LISTENER_REGISTRATION *pRegistration,
        __in                        DWORD                                   cbRegistration,
        __out                       CM_NOTIFICATIONS_LISTENER_HANDLE       *phListener)
    {
        CM_RESULT retval = CMRE_NOT_IMPLEMENTED;
        if (sm_pfRegisterNotificationsListener)
            retval = sm_pfRegisterNotificationsListener(
                                pRegistration,
                                cbRegistration,
                                phListener);
        return retval;
    }

    static CM_RESULT
    ReleaseConnection(
        __in CM_CONNECTION_HANDLE hConnection)
    {
        CM_RESULT retval = CMRE_NOT_IMPLEMENTED;
        if (sm_pfReleaseConnection)
            retval = sm_pfReleaseConnection(
                                hConnection);
        return retval;
    }

    static CM_RESULT
    SetPreferences(
        __in                       CM_SESSION_HANDLE     hSession,
        __in_bcount(cbPreferences) const CM_PREFERENCES *pPreferences,
        __in                       DWORD                 cbPreferences)
    {
        CM_RESULT retval = CMRE_NOT_IMPLEMENTED;
        if (sm_pfSetPreferences)
            retval = sm_pfSetPreferences(
                                hSession,
                                pPreferences,
                                cbPreferences);
        return retval;
    }

    static CM_RESULT
    SetPriority(
        __in CM_SESSION_HANDLE hSession,
        __in CM_PRIORITY       Priority)
    {
        CM_RESULT retval = CMRE_NOT_IMPLEMENTED;
        if (sm_pfSetPriority)
            retval = sm_pfSetPriority(
                                hSession,
                                Priority);
        return retval;
    }

    static CM_RESULT
    SetRequirements(
        __in                        CM_SESSION_HANDLE      hSession,
        __in_bcount(cbRequirements) const CM_REQUIREMENTS *pRequirements,
        __in                        DWORD                  cbRequirements)
    {
        CM_RESULT retval = CMRE_NOT_IMPLEMENTED;
        if (sm_pfSetRequirements)
            retval = sm_pfSetRequirements(
                                hSession,
                                pRequirements,
                                cbRequirements);
        return retval;
    }

    static CM_RESULT
    UnregisterNotificationsListener(
        __in CM_NOTIFICATIONS_LISTENER_HANDLE hListener)
    {
        CM_RESULT retval = CMRE_NOT_IMPLEMENTED;
        if (sm_pfUnregisterNotificationsListener)
            retval = sm_pfUnregisterNotificationsListener(
                                hListener);
        return retval;
    }

    static CM_RESULT
    UpdateConnectionConfig(
        __in                  CM_CONFIG_CHANGE_HANDLE hConfig,
        __in                  CM_CONFIG_OPTION        Option,
        __in_bcount(cbConfig) CM_CONNECTION_CONFIG   *pConfig,
        __in                  DWORD                   cbConfig)
    {
        CM_RESULT retval = CMRE_NOT_IMPLEMENTED;
        if (sm_pfUpdateConnectionConfig)
            retval = sm_pfUpdateConnectionConfig(
                                hConfig,
                                Option,
                                pConfig,
                                cbConfig);
        return retval;
    }

    static CM_RESULT
    UpdatePolicyConfig(
        __in                  CM_CONFIG_CHANGE_HANDLE    hConfig,
        __in_bcount(cbData)   CM_POLICY_CONFIG_DATA     *pData,
        __in                  DWORD                      cbData)
    {
        CM_RESULT retval = CMRE_NOT_IMPLEMENTED;
        if (sm_pfUpdatePolicyConfig)
            retval = sm_pfUpdatePolicyConfig(
                                hConfig,
                                pData,
                                cbData);
        return retval;
    }

private:

    // Procedure signatures:
    //
    
    typedef BOOL                  (*PFAPIInit_t)                                    (void);
    typedef CM_RESULT         (*PFAPIDeInit)                                   (void);
    typedef CM_RESULT         (*PFAcquireConnection_t)              (CM_CONNECTION_HANDLE);
    typedef CM_RESULT         (*PFAddConnectionConfig_t)            (const WCHAR *, CM_CONNECTION_CONFIG *, DWORD);
    typedef CM_RESULT         (*PFCloseSession_t)                   (CM_SESSION_HANDLE);
    typedef CM_SESSION_HANDLE (*PFCreateSession_t)                  (void);
    typedef CM_RESULT         (*PFDeleteConnectionConfig_t)         (const WCHAR *);
    typedef CM_RESULT         (*PFEnumConnectionsConfig_t)          (CM_CONNECTION_NAME_LIST *, DWORD *);
    typedef CM_RESULT         (*PFEnumConnectionsConfigByType_t)    (CM_CONNECTION_TYPE, CM_CONNECTION_NAME_LIST *, DWORD *);
    typedef CM_RESULT         (*PFGetConnectionConfig_t)            (const WCHAR *, CM_CONNECTION_CONFIG *, DWORD *);
    typedef CM_RESULT         (*PFGetConnectionDetailsByHandle_t)   (CM_CONNECTION_HANDLE, CM_CONNECTION_DETAILS *, DWORD *);
    typedef CM_RESULT         (*PFGetConnectionDetailsByName_t)     (const WCHAR *, CM_CONNECTION_DETAILS *, DWORD *);
    typedef CM_RESULT         (*PFGetConnectionSelectionResult_t)   (CM_SESSION_HANDLE);
    typedef CM_RESULT         (*PFGetFirstCandidateConnection_t)    (CM_SESSION_HANDLE, const WCHAR *, CM_CONNECTION_SELECTION_OPTION, CM_CONNECTION_HANDLE *);
    typedef CM_RESULT         (*PFGetFirstIpAddr_t)                 (CM_CONNECTION_HANDLE, const WCHAR *, ADDRESS_FAMILY, USHORT, USHORT, CM_ADDRESS_PAIR *, DWORD *);
    typedef CM_RESULT         (*PFGetNextCandidateConnection_t)     (CM_SESSION_HANDLE, CM_CONNECTION_HANDLE *);
    typedef CM_RESULT         (*PFGetNextIpAddr_t)                  (CM_CONNECTION_HANDLE, CM_ADDRESS_PAIR *, DWORD *);
    typedef CM_RESULT         (*PFGetNotification_t)                (CM_NOTIFICATIONS_LISTENER_HANDLE, CM_NOTIFICATION *, DWORD *);
    typedef CM_RESULT         (*PFGetPolicyConfig_t)                (CM_POLICY_CONFIG_KEY *, DWORD, CM_POLICY_CONFIG_DATA *, DWORD *);
    typedef CM_RESULT         (*PFGetToUpdateConnectionConfig_t)    (const WCHAR *, CM_CONNECTION_CONFIG *, DWORD *, CM_CONFIG_CHANGE_HANDLE *);
    typedef CM_RESULT         (*PFGetToUpdatePolicyConfig_t)        (CM_POLICY_CONFIG_KEY *, DWORD, CM_POLICY_CONFIG_DATA *, DWORD *, CM_CONFIG_CHANGE_HANDLE *);
    typedef CM_RESULT         (*PFRegisterNotificationsListener_t)  (CM_NOTIFICATIONS_LISTENER_REGISTRATION *, DWORD, CM_NOTIFICATIONS_LISTENER_HANDLE *);
    typedef CM_RESULT         (*PFReleaseConnection_t)              (CM_CONNECTION_HANDLE);
    typedef CM_RESULT         (*PFSetPreferences_t)                 (CM_SESSION_HANDLE, const CM_PREFERENCES *, DWORD);
    typedef CM_RESULT         (*PFSetPriority_t)                    (CM_SESSION_HANDLE, CM_PRIORITY);
    typedef CM_RESULT         (*PFSetRequirements_t)                (CM_SESSION_HANDLE, const CM_REQUIREMENTS *, DWORD);
    typedef CM_RESULT         (*PFUnregisterNotificationsListener_t)(CM_NOTIFICATIONS_LISTENER_HANDLE);
    typedef CM_RESULT         (*PFUpdateConnectionConfig_t)         (CM_CONFIG_CHANGE_HANDLE, CM_CONFIG_OPTION, CM_CONNECTION_CONFIG *, DWORD);
    typedef CM_RESULT         (*PFUpdatePolicyConfig_t)             (CM_CONFIG_CHANGE_HANDLE, CM_POLICY_CONFIG_DATA *, DWORD);

    // Static data:
    //
    static HINSTANCE sm_hCMDll;
    static LONG      sm_CMUsers;

    static PFAcquireConnection_t               sm_pfAcquireConnection;
    static PFAddConnectionConfig_t             sm_pfAddConnectionConfig;
    static PFCloseSession_t                    sm_pfCloseSession;
    static PFCreateSession_t                   sm_pfCreateSession;
    static PFDeleteConnectionConfig_t          sm_pfDeleteConnectionConfig;
    static PFEnumConnectionsConfig_t           sm_pfEnumConnectionsConfig;
    static PFEnumConnectionsConfigByType_t     sm_pfEnumConnectionsConfigByType;
    static PFGetConnectionConfig_t             sm_pfGetConnectionConfig;
    static PFGetConnectionDetailsByHandle_t    sm_pfGetConnectionDetailsByHandle;
    static PFGetConnectionDetailsByName_t      sm_pfGetConnectionDetailsByName;
    static PFGetConnectionSelectionResult_t    sm_pfGetConnectionSelectionResult;
    static PFGetFirstCandidateConnection_t     sm_pfGetFirstCandidateConnection;
    static PFGetFirstIpAddr_t                  sm_pfGetFirstIpAddr;
    static PFGetNextCandidateConnection_t      sm_pfGetNextCandidateConnection;
    static PFGetNextIpAddr_t                   sm_pfGetNextIpAddr;
    static PFGetNotification_t                 sm_pfGetNotification;
    static PFGetPolicyConfig_t                 sm_pfGetPolicyConfig;
    static PFGetToUpdateConnectionConfig_t     sm_pfGetToUpdateConnectionConfig;
    static PFGetToUpdatePolicyConfig_t         sm_pfGetToUpdatePolicyConfig;
    static PFRegisterNotificationsListener_t   sm_pfRegisterNotificationsListener;
    static PFReleaseConnection_t               sm_pfReleaseConnection;
    static PFSetPreferences_t                  sm_pfSetPreferences;
    static PFSetPriority_t                     sm_pfSetPriority;
    static PFSetRequirements_t                 sm_pfSetRequirements;
    static PFUnregisterNotificationsListener_t sm_pfUnregisterNotificationsListener;
    static PFUpdateConnectionConfig_t          sm_pfUpdateConnectionConfig;
    static PFUpdatePolicyConfig_t              sm_pfUpdatePolicyConfig;

    // (Re)initializes the static data:
    //
    static void
    Clear(void);

    // Copy and assignment are deliberately disabled:
    //
    CMWiFiIntf_t(const CMWiFiIntf_t &rhs);
    CMWiFiIntf_t &operator = (const CMWiFiIntf_t &rhs);
};

// Disable direct calls to the CMapi functions:
//
#define CmAcquireConnection               DoNotLinkCMNetDirectly_UseCMWiFiIntfInstead
#define CmAddConnectionConfig             DoNotLinkCMNetDirectly_UseCMWiFiIntfInstead
#define CmCloseSession                    DoNotLinkCMNetDirectly_UseCMWiFiIntfInstead
#define CmCreateSession                   DoNotLinkCMNetDirectly_UseCMWiFiIntfInstead
#define CmDeleteConnectionConfig          DoNotLinkCMNetDirectly_UseCMWiFiIntfInstead
#define CmEnumConnectionsConfig           DoNotLinkCMNetDirectly_UseCMWiFiIntfInstead
#define CmEnumConnectionsConfigByType     DoNotLinkCMNetDirectly_UseCMWiFiIntfInstead
#define CmGetConnectionConfig             DoNotLinkCMNetDirectly_UseCMWiFiIntfInstead
#define CmGetConnectionDetailsByHandle    DoNotLinkCMNetDirectly_UseCMWiFiIntfInstead
#define CmGetConnectionDetailsByName      DoNotLinkCMNetDirectly_UseCMWiFiIntfInstead
#define CmGetConnectionSelectionResult    DoNotLinkCMNetDirectly_UseCMWiFiIntfInstead
#define CmGetFirstCandidateConnection     DoNotLinkCMNetDirectly_UseCMWiFiIntfInstead
#define CmGetFirstIpAddr                  DoNotLinkCMNetDirectly_UseCMWiFiIntfInstead
#define CmGetNextCandidateConnection      DoNotLinkCMNetDirectly_UseCMWiFiIntfInstead
#define CmGetNextIpAddr                   DoNotLinkCMNetDirectly_UseCMWiFiIntfInstead
#define CmGetNotification                 DoNotLinkCMNetDirectly_UseCMWiFiIntfInstead
#define CmGetPolicyConfig                 DoNotLinkCMNetDirectly_UseCMWiFiIntfInstead
#define CmGetToUpdateConnectionConfig     DoNotLinkCMNetDirectly_UseCMWiFiIntfInstead
#define CmGetToUpdatePolicyConfig         DoNotLinkCMNetDirectly_UseCMWiFiIntfInstead
#define CmRegisterNotificationsListener   DoNotLinkCMNetDirectly_UseCMWiFiIntfInstead
#define CmReleaseConnection               DoNotLinkCMNetDirectly_UseCMWiFiIntfInstead
#define CmSetPreferences                  DoNotLinkCMNetDirectly_UseCMWiFiIntfInstead
#define CmSetPriority                     DoNotLinkCMNetDirectly_UseCMWiFiIntfInstead
#define CmSetRequirements                 DoNotLinkCMNetDirectly_UseCMWiFiIntfInstead
#define CmUnregisterNotificationsListener DoNotLinkCMNetDirectly_UseCMWiFiIntfInstead
#define CmUpdateConnectionConfig          DoNotLinkCMNetDirectly_UseCMWiFiIntfInstead
#define CmUpdatePolicyConfig              DoNotLinkCMNetDirectly_UseCMWiFiIntfInstead

// ----------------------------------------------------------------------------
//
// CMWiFiErrorText is a simple class and methods for translating CM_RESULT
// error codes into text form for error messages:
//      LogError(TEXT("oops, eek!: %s"), CMWiFiErrorText(result));
//
#ifndef CMWiFiErrorText
#define CMWiFiErrorText __CMWiFiErrorText()
class __CMWiFiErrorText
{
private:
    TCHAR m_ErrorText[96];
public:
  __CMWiFiErrorText(void) { }
    const TCHAR *operator() (CM_RESULT ResultCode);
};
#endif /* CMWiFiErrorText */

// ----------------------------------------------------------------------------
//
// Translates from the specified Connection Manager result-code to the
// corresponding Win32 error-code.
//
DWORD
CM_RESULT2Win32(CM_RESULT ResultCode);

};
};

#endif /* if (CMWIFI_VERSION > 0) */
#endif /* _DEFINED_CMWiFiIntf_t_ */
// ----------------------------------------------------------------------------
