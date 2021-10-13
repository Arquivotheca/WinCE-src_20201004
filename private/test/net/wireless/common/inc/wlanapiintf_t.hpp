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
// Definitions and declarations for the WLANAPIIntf_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_WLANAPIIntf_t_
#define _DEFINED_WLANAPIIntf_t_
#pragma once

#include "WLANTypes.hpp"
#if (WLAN_VERSION > 0)

#include <wlanapi.h>

// API protection settings Start

// These definitions are moved from the public header wlanapi.h. CE doesn't support these types, private them to avoid break.

// Definition of access masks for setting non-default security
// settings on WLAN configuration objects and connection profiles.

#define WLAN_READ_ACCESS    ( STANDARD_RIGHTS_READ | FILE_READ_DATA )
#define WLAN_EXECUTE_ACCESS ( WLAN_READ_ACCESS | STANDARD_RIGHTS_EXECUTE | FILE_EXECUTE )
#define WLAN_WRITE_ACCESS   ( WLAN_READ_ACCESS | WLAN_EXECUTE_ACCESS | STANDARD_RIGHTS_WRITE | FILE_WRITE_DATA | DELETE | WRITE_DAC )


typedef enum
_WLAN_SECURABLE_OBJECT
{
    wlan_secure_permit_list = 0,
    wlan_secure_deny_list,
    wlan_secure_ac_enabled,
    wlan_secure_bc_scan_enabled,
    wlan_secure_bss_type,
    wlan_secure_show_denied,
    wlan_secure_interface_properties,
    wlan_secure_ihv_control,
    wlan_secure_all_user_profiles_order,
    wlan_secure_add_new_all_user_profiles,
    wlan_secure_add_new_per_user_profiles,
    wlan_secure_media_streaming_mode_enabled,
    wlan_secure_current_operation_mode,

    WLAN_SECURABLE_OBJECT_COUNT
}
WLAN_SECURABLE_OBJECT, *PWLAN_SECURABLE_OBJECT;

// API protection settings End

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// WLANAPIIntf_t is an interface to the dynamically-loaded WLANAPI library.
// Each object of this class represents a reference to the library.
// When the last of these objects is deleted, the library is unloaded.
//
class WLANAPIIntf_t
{
public:

    // Construction and destruction:
    //
    WLANAPIIntf_t(void);
   ~WLANAPIIntf_t(void);

    static bool
    IsLibraryLoaded(void)
    {
        return sm_hWLANDll != NULL;
    }

    static PVOID
    AllocateMemory(
      __in DWORD dwMemorySize)
    {
        PVOID retval = NULL;
        if (sm_pfAllocateMemory)
            retval = sm_pfAllocateMemory(
                                dwMemorySize);
        return retval;
    }

    static DWORD
    CloseHandle(
      __in       HANDLE hClientHandle,
      __reserved PVOID  pReserved)
    {
        DWORD retval = ERROR_INVALID_FUNCTION;
        if (sm_pfCloseHandle)
            retval = sm_pfCloseHandle(
                                hClientHandle,
                                pReserved);
        return retval;
    }

    static DWORD
    Connect(
      __in       HANDLE                            hClientHandle,
      __in       const GUID                       *pInterfaceGuid,
      __in       const PWLAN_CONNECTION_PARAMETERS pConnectionParameters,
      __reserved PVOID                             pReserved)
    {
        DWORD retval = ERROR_INVALID_FUNCTION;
        if (sm_pfConnect)
            retval = sm_pfConnect(
                                hClientHandle,
                                pInterfaceGuid,
                                pConnectionParameters,
                                pReserved);
        return retval;
    }

    static DWORD
    DeleteProfile(
      __in       HANDLE      hClientHandle,
      __in       const GUID *pInterfaceGuid,
      __in       LPCWSTR     strProfileName,
      __reserved PVOID       pReserved)
    {
        DWORD retval = ERROR_INVALID_FUNCTION;
        if (sm_pfDeleteProfile)
            retval = sm_pfDeleteProfile(
                                hClientHandle,
                                pInterfaceGuid,
                                strProfileName,
                                pReserved);
        return retval;
    }

    static DWORD
    Disconnect(
      __in       HANDLE      hClientHandle,
      __in       const GUID *pInterfaceGuid,
      __reserved PVOID       pReserved)
    {
        DWORD retval = ERROR_INVALID_FUNCTION;
        if (sm_pfDisconnect)
            retval = sm_pfDisconnect(
                                hClientHandle,
                                pInterfaceGuid,
                                pReserved);
        return retval;
    }

    static DWORD
    EnumInterfaces(
      __in        HANDLE                     hClientHandle,
      __reserved  PVOID                      pReserved,
      __deref_out PWLAN_INTERFACE_INFO_LIST *ppInterfaceList)
    {
        DWORD retval = ERROR_INVALID_FUNCTION;
        if (sm_pfEnumInterfaces)
             retval = sm_pfEnumInterfaces(
                                hClientHandle,
                                pReserved,
                                ppInterfaceList);
        return retval;
    }

    static DWORD
    ExtractPsdIEDataList(
      __in                      HANDLE               hClientHandle,
      __in                      DWORD                dwIeDataSize,
      __in_bcount(dwIeDataSize) const PBYTE          pRawIeData,
      __in                      LPCWSTR              strFormat,
      __reserved                PVOID                pReserved,
      __deref_out               PWLAN_RAW_DATA_LIST *ppPsdIEDataList)
    {
        DWORD retval = ERROR_INVALID_FUNCTION;
        if (sm_pfExtractPsdIEDataList)
             retval = sm_pfExtractPsdIEDataList(
                                hClientHandle,
                                dwIeDataSize,
                                pRawIeData,
                                strFormat,
                                pReserved,
                                ppPsdIEDataList);
        return retval;
    }

    static void
    FreeMemory(
      __in PVOID pMemory)
    {
        if (sm_pfFreeMemory)
            sm_pfFreeMemory(pMemory);
    }

    static DWORD
    GetAvailableNetworkList(
      __in        HANDLE                        hClientHandle,
      __in        const GUID                   *pInterfaceGuid,
      __in        DWORD                         dwFlags,
      __reserved  PVOID                         pReserved,
      __deref_out PWLAN_AVAILABLE_NETWORK_LIST *ppAvailableNetworkList)
    {
        DWORD retval = ERROR_INVALID_FUNCTION;
        if (sm_pfGetAvailableNetworkList)
            retval = sm_pfGetAvailableNetworkList(
                                hClientHandle,
                                pInterfaceGuid,
                                dwFlags,
                                pReserved,
                                ppAvailableNetworkList);
        return retval;
    }

    static DWORD
    GetFilterList(
      __in        HANDLE                hClientHandle,
      __in        WLAN_FILTER_LIST_TYPE wlanFilterListType,
      __reserved  PVOID                 pReserved,
      __deref_out PDOT11_NETWORK_LIST  *ppNetworkList)
    {
        DWORD retval = ERROR_INVALID_FUNCTION;
        if (sm_pfGetFilterList)
            retval = sm_pfGetFilterList(
                                hClientHandle,
                                wlanFilterListType,
                                pReserved,
                                ppNetworkList);
        return retval;
    }

    static DWORD
    GetInterfaceCapability(
      __in        HANDLE                      hClientHandle,
      __in        const GUID                 *pInterfaceGuid,
      __reserved  PVOID                       pReserved,
      __deref_out PWLAN_INTERFACE_CAPABILITY *ppCapability)
    {
        DWORD retval = ERROR_INVALID_FUNCTION;
        if (sm_pfGetInterfaceCapability)
            retval = sm_pfGetInterfaceCapability(
                                hClientHandle,
                                pInterfaceGuid,
                                pReserved,
                                ppCapability);
        return retval;
    }

    static DWORD
    GetNetworkBssList(
      __in        HANDLE            hClientHandle,
      __in        const GUID       *pInterfaceGuid,
      __in_opt    const PDOT11_SSID pDot11Ssid,
      __in        DOT11_BSS_TYPE    dot11BssType,
      __in        BOOL              bSecurityEnabled,
      __reserved  PVOID             pReserved,
      __deref_out PWLAN_BSS_LIST   *ppWlanBssList)
    {
        DWORD retval = ERROR_INVALID_FUNCTION;
        if (sm_pfGetNetworkBssList)
            retval = sm_pfGetNetworkBssList(
                                hClientHandle,
                                pInterfaceGuid,
                                pDot11Ssid,
                                dot11BssType,
                                bSecurityEnabled,
                                pReserved,
                                ppWlanBssList);
        return retval;
    }

    static DWORD
    GetProfile(
      __in        HANDLE      hClientHandle,
      __in        const GUID *pInterfaceGuid,
      __in        LPCWSTR     strProfileName,
      __reserved  PVOID       pReserved,
      __deref_out LPWSTR     *pstrProfileXml,
      __out_opt   DWORD      *pdwFlags,
      __out_opt   DWORD      *pdwGrantedAccess)
    {
        DWORD retval = ERROR_INVALID_FUNCTION;
        if (sm_pfGetProfile)
            retval = sm_pfGetProfile(
                                hClientHandle,
                                pInterfaceGuid,
                                strProfileName,
                                pReserved,
                                pstrProfileXml,
                                pdwFlags,
                                pdwGrantedAccess);
        return retval;
    }

    static DWORD
    GetProfileCustomUserData(
      __in                             HANDLE      hClientHandle,
      __in                             const GUID *pInterfaceGuid,
      __in                             LPCWSTR     strProfileName,
      __reserved                       PVOID       pReserved,
      __out                            DWORD      *pdwDataSize,
      __deref_out_bcount(*pdwDataSize) PBYTE      *ppData)
    {
        DWORD retval = ERROR_INVALID_FUNCTION;
        if (sm_pfGetProfileCustomUserData)
            retval = sm_pfGetProfileCustomUserData(
                                hClientHandle,
                                pInterfaceGuid,
                                strProfileName,
                                pReserved,
                                pdwDataSize,
                                ppData);
        return retval;
    }

    static DWORD
    GetProfileList(
      __in        HANDLE                   hClientHandle,
      __in        const GUID              *pInterfaceGuid,
      __reserved  PVOID                    pReserved,
      __deref_out PWLAN_PROFILE_INFO_LIST *ppProfileList)
    {
        DWORD retval = ERROR_INVALID_FUNCTION;
        if (sm_pfGetProfileList)
            retval = sm_pfGetProfileList(
                                hClientHandle,
                                pInterfaceGuid,
                                pReserved,
                                ppProfileList);
        return retval;
    }

    static DWORD
    GetSecuritySettings(
      __in        HANDLE                  hClientHandle,
      __in        WLAN_SECURABLE_OBJECT   SecurableObject,
      __out_opt   PWLAN_OPCODE_VALUE_TYPE pValueType,
      __deref_out LPWSTR                 *pstrCurrentSDDL,
      __out       PDWORD                  pdwGrantedAccess)
    {
        DWORD retval = ERROR_INVALID_FUNCTION;
        if (sm_pfGetSecuritySettings)
            retval = sm_pfGetSecuritySettings(
                                hClientHandle,
                                SecurableObject,
                                pValueType,
                                pstrCurrentSDDL,
                                pdwGrantedAccess);
        return retval;
    }

    static DWORD
    IhvControl(
      __in                                HANDLE                hClientHandle,
      __in                                const GUID           *pInterfaceGuid,
      __in                                WLAN_IHV_CONTROL_TYPE Type,
      __in                                DWORD                 dwInBufferSize,
      __in_bcount(dwInBufferSize)         PVOID                 pInBuffer,
      __in                                DWORD                 dwOutBufferSize,
      __inout_bcount_opt(dwOutBufferSize) PVOID                 pOutBuffer,
      __out                               PDWORD                pdwBytesReturned)
    {
        DWORD retval = ERROR_INVALID_FUNCTION;
        if (sm_pfIhvControl)
            retval = sm_pfIhvControl(
                                hClientHandle,
                                pInterfaceGuid,
                                Type,
                                dwInBufferSize,
                                pInBuffer,
                                dwOutBufferSize,
                                pOutBuffer,
                                pdwBytesReturned);
        return retval;
    }

    static DWORD
    OpenHandle(
      __in       DWORD   dwClientVersion,
      __reserved PVOID   pReserved,
      __out      PDWORD  pdwNegotiatedVersion,
      __out      PHANDLE phClientHandle)
    {
        DWORD retval = ERROR_INVALID_FUNCTION;
        if (sm_pfOpenHandle)
            retval = sm_pfOpenHandle(
                                dwClientVersion,
                                pReserved,
                                pdwNegotiatedVersion,
                                phClientHandle);
        return retval;
    }

    static DWORD
    QueryAutoConfigParameter(
      __in                             HANDLE                  hClientHandle,
      __in                             WLAN_AUTOCONF_OPCODE    OpCode,
      __reserved                       PVOID                   pReserved,
      __out                            PDWORD                  pdwDataSize,
      __deref_out_bcount(*pdwDataSize) PVOID                  *ppData,
      __out_opt                        PWLAN_OPCODE_VALUE_TYPE pWlanOpcodeValueType)
    {
        DWORD retval = ERROR_INVALID_FUNCTION;
        if (sm_pfQueryAutoConfigParameter)
            retval = sm_pfQueryAutoConfigParameter(
                                hClientHandle,
                                OpCode,
                                pReserved,
                                pdwDataSize,
                                ppData,
                                pWlanOpcodeValueType);
        return retval;
    }

    static DWORD
    QueryInterface(
      __in                             HANDLE                  hClientHandle,
      __in                             const GUID             *pInterfaceGuid,
      __in                             WLAN_INTF_OPCODE        OpCode,
      __reserved                       PVOID                   pReserved,
      __out                            PDWORD                  pdwDataSize,
      __deref_out_bcount(*pdwDataSize) PVOID                  *ppData,
      __out_opt                        PWLAN_OPCODE_VALUE_TYPE pWlanOpcodeValueType)
    {
        DWORD retval = ERROR_INVALID_FUNCTION;
        if (sm_pfQueryInterface)
            retval = sm_pfQueryInterface(
                                hClientHandle,
                                pInterfaceGuid,
                                OpCode,
                                pReserved,
                                pdwDataSize,
                                ppData,
                                pWlanOpcodeValueType);
        return retval;
    }

    static DWORD
    ReasonCodeToString(
      __in                      DWORD  dwReasonCode,
      __in                      DWORD  dwBufferSize,
      __in_ecount(dwBufferSize) PWCHAR pStringBuffer,
      __reserved                PVOID  pReserved)
    {
        DWORD retval = ERROR_INVALID_FUNCTION;
        if (sm_pfReasonCodeToString)
            retval = sm_pfReasonCodeToString(
                                dwReasonCode,
                                dwBufferSize,
                                pStringBuffer,
                                pReserved);
        return retval;
    }

    static DWORD
    RegisterNotification(
      __in       HANDLE                     hClientHandle,
      __in       DWORD                      dwNotifSource,
      __in       BOOL                       bIgnoreDuplicate,
      __in_opt   WLAN_NOTIFICATION_CALLBACK funcCallback,
      __in_opt   PVOID                      pCallbackContext,
      __reserved PVOID                      pReserved,
      __out_opt  PDWORD                     pdwPrevNotifSource)
    {
        DWORD retval = ERROR_INVALID_FUNCTION;
        if (sm_pfRegisterNotification)
            retval = sm_pfRegisterNotification(
                                hClientHandle,
                                dwNotifSource,
                                bIgnoreDuplicate,
                                funcCallback,
                                pCallbackContext,
                                pReserved,
                                pdwPrevNotifSource);
        return retval;
    }

    static DWORD
    RenameProfile(
      __in       HANDLE      hClientHandle,
      __in       const GUID *pInterfaceGuid,
      __in       LPCWSTR     strOldProfileName,
      __in       LPCWSTR     strNewProfileName,
      __reserved PVOID       pReserved)
    {
        DWORD retval = ERROR_INVALID_FUNCTION;
        if (sm_pfRenameProfile)
            retval = sm_pfRenameProfile(
                                hClientHandle,
                                pInterfaceGuid,
                                strOldProfileName,
                                strNewProfileName,
                                pReserved);
        return retval;
    }

    static DWORD
    SaveTemporaryProfile(
      __in       HANDLE      hClientHandle,
      __in       const GUID *pInterfaceGuid,
      __in       LPCWSTR     strProfileName,
      __in_opt   LPCWSTR     strAllUserProfileSecurity,
      __in       DWORD       dwFlags,
      __in       BOOL        bOverWrite,
      __reserved PVOID       pReserved)
    {
        DWORD retval = ERROR_INVALID_FUNCTION;
        if (sm_pfSaveTemporaryProfile)
            retval = sm_pfSaveTemporaryProfile(
                                hClientHandle,
                                pInterfaceGuid,
                                strProfileName,
                                strAllUserProfileSecurity,
                                dwFlags,
                                bOverWrite,
                                pReserved);
        return retval;
    }

    static DWORD
    Scan(
      __in       HANDLE               hClientHandle,
      __in       const GUID          *pInterfaceGuid,
      __in_opt   const PDOT11_SSID    pDot11Ssid,
      __in_opt   const PWLAN_RAW_DATA pIeData,
      __reserved PVOID                pReserved)
    {
        DWORD retval = ERROR_INVALID_FUNCTION;
        if (sm_pfScan)
            retval = sm_pfScan(
                                hClientHandle,
                                pInterfaceGuid,
                                pDot11Ssid,
                                pIeData,
                                pReserved);
        return retval;
    }

    static DWORD
    SetAutoConfigParameter(
      __in                    HANDLE               hClientHandle,
      __in                    WLAN_AUTOCONF_OPCODE OpCode,
      __in                    DWORD                dwDataSize,
      __in_bcount(dwDataSize) const PVOID          pData,
      __reserved              PVOID                pReserved)
    {
        DWORD retval = ERROR_INVALID_FUNCTION;
        if (sm_pfSetAutoConfigParameter)
            retval = sm_pfSetAutoConfigParameter(
                                hClientHandle,
                                OpCode,
                                dwDataSize,
                                pData,
                                pReserved);
        return retval;
    }

    static DWORD
    SetFilterList(
      __in       HANDLE                    hClientHandle,
      __in       WLAN_FILTER_LIST_TYPE     wlanFilterListType,
      __in_opt   const PDOT11_NETWORK_LIST pNetworkList,
      __reserved PVOID                     pReserved)
    {
        DWORD retval = ERROR_INVALID_FUNCTION;
        if (sm_pfSetFilterList)
            retval = sm_pfSetFilterList(
                                hClientHandle,
                                wlanFilterListType,
                                pNetworkList,
                                pReserved);
        return retval;
    }

    static DWORD
    SetInterface(
      __in                    HANDLE           hClientHandle,
      __in                    const GUID      *pInterfaceGuid,
      __in                    WLAN_INTF_OPCODE OpCode,
      __in                    DWORD            dwDataSize,
      __in_bcount(dwDataSize) const PVOID      pData,
      __reserved              PVOID            pReserved)
    {
        DWORD retval = ERROR_INVALID_FUNCTION;
        if (sm_pfSetInterface)
            retval = sm_pfSetInterface(
                                hClientHandle,
                                pInterfaceGuid,
                                OpCode,
                                dwDataSize,
                                pData,
                                pReserved);
        return retval;
    }

    static DWORD
    SetProfile(
      __in       HANDLE      hClientHandle,
      __in       const GUID *pInterfaceGuid,
      __in       DWORD       dwFlags,
      __in       LPCWSTR     strProfileXml,
      __in_opt   LPCWSTR     strAllUserProfileSecurity,
      __in       BOOL        bOverwrite,
      __reserved PVOID       pReserved,
      __out      DWORD      *pdwReasonCode)
    {
        DWORD retval = ERROR_INVALID_FUNCTION;
        if (sm_pfSetProfile)
            retval = sm_pfSetProfile(
                                hClientHandle,
                                pInterfaceGuid,
                                dwFlags,
                                strProfileXml,
                                strAllUserProfileSecurity,
                                bOverwrite,
                                pReserved,
                                pdwReasonCode);
        return retval;
    }

    static DWORD
    SetProfileCustomUserData(
      __in                    HANDLE      hClientHandle,
      __in                    const GUID *pInterfaceGuid,
      __in                    LPCWSTR     strProfileName,
      __in                    DWORD       dwDataSize,
      __in_bcount(dwDataSize) const PBYTE pData,
      __reserved              PVOID       pReserved)
    {
        DWORD retval = ERROR_INVALID_FUNCTION;
        if (sm_pfSetProfileCustomUserData)
            retval = sm_pfSetProfileCustomUserData(
                                hClientHandle,
                                pInterfaceGuid,
                                strProfileName,
                                dwDataSize,
                                pData,
                                pReserved);
        return retval;
    }

    static DWORD
    SetProfileEapUserData(
      __in                           HANDLE          hClientHandle,
      __in                           const GUID     *pInterfaceGuid,
      __in                           LPCWSTR         strProfileName,
      __in                           EAP_METHOD_TYPE eapType,
      __in                           DWORD           dwFlags,
      __in                           DWORD           dwEapUserDataSize,
      __in_bcount(dwEapUserDataSize) const LPBYTE    pbEapUserData,
      __reserved                     PVOID           pReserved)
    {
        DWORD retval = ERROR_INVALID_FUNCTION;
        if (sm_pfSetProfileEapUserData)
            retval = sm_pfSetProfileEapUserData(
                                hClientHandle,
                                pInterfaceGuid,
                                strProfileName,
                                eapType,
                                dwFlags,
                                dwEapUserDataSize,
                                pbEapUserData,
                                pReserved);
        return retval;
    }

    static DWORD
    SetProfileEapXmlUserData(
      __in       HANDLE      hClientHandle,
      __in       const GUID *pInterfaceGuid,
      __in       LPCWSTR     strProfileName,
      __in       DWORD       dwFlags,
      __in       LPCWSTR     strEapXmlUserData,
      __reserved PVOID       pReserved)
    {
        DWORD retval = ERROR_INVALID_FUNCTION;
        if (sm_pfSetProfileEapXmlUserData)
            retval = sm_pfSetProfileEapXmlUserData(
                                hClientHandle,
                                pInterfaceGuid,
                                strProfileName,
                                dwFlags,
                                strEapXmlUserData,
                                pReserved);
        return retval;
    }

    static DWORD
    SetProfileList(
      __in                 HANDLE      hClientHandle,
      __in                 const GUID *pInterfaceGuid,
      __in                 DWORD       dwItems,
      __in_ecount(dwItems) LPCWSTR    *strProfileNames,
      __reserved           PVOID       pReserved)
    {
        DWORD retval = ERROR_INVALID_FUNCTION;
        if (sm_pfSetProfileList)
            retval = sm_pfSetProfileList(
                                hClientHandle,
                                pInterfaceGuid,
                                dwItems,
                                strProfileNames,
                                pReserved);
        return retval;
    }

    static DWORD
    SetProfilePosition(
      __in       HANDLE      hClientHandle,
      __in       const GUID *pInterfaceGuid,
      __in       LPCWSTR     strProfileName,
      __in       DWORD       dwPosition,
      __reserved PVOID       pReserved)
    {
        DWORD retval = ERROR_INVALID_FUNCTION;
        if (sm_pfSetProfilePosition)
            retval = sm_pfSetProfilePosition(
                                hClientHandle,
                                pInterfaceGuid,
                                strProfileName,
                                dwPosition,
                                pReserved);
        return retval;
    }

    static DWORD
    SetPsdIEDataList(
      __in       HANDLE                    hClientHandle,
      __in_opt   LPCWSTR                   strFormat,
      __in_opt   const PWLAN_RAW_DATA_LIST pPsdIEDataList,
      __reserved PVOID                     pReserved)
    {
        DWORD retval = ERROR_INVALID_FUNCTION;
        if (sm_pfSetPsdIEDataList)
            retval = sm_pfSetPsdIEDataList(
                                hClientHandle,
                                strFormat,
                                pPsdIEDataList,
                                pReserved);
        return retval;
    }

    static DWORD
    SetSecuritySettings(
      __in HANDLE                hClientHandle,
      __in WLAN_SECURABLE_OBJECT SecurableObject,
      __in LPCWSTR               strModifiedSDDL)
    {
        DWORD retval = ERROR_INVALID_FUNCTION;
        if (sm_pfSetSecuritySettings)
            retval = sm_pfSetSecuritySettings(
                                hClientHandle,
                                SecurableObject,
                                strModifiedSDDL);
        return retval;
    }

    static DWORD
    UIEditProfile(
      __in       DWORD             dwClientVersion,
      __in       LPCWSTR           wstrProfileName,
      __in       GUID             *pInterfaceGuid,
      __in       HWND              hWnd,
      __in       WL_DISPLAY_PAGES  wlStartPage,
      __reserved PVOID             pReserved,
      __out_opt  PWLAN_REASON_CODE pWlanReasonCode)
    {
        DWORD retval = ERROR_INVALID_FUNCTION;
        if (sm_pfUIEditProfile)
            retval = sm_pfUIEditProfile(
                                dwClientVersion,
                                wstrProfileName,
                                pInterfaceGuid,
                                hWnd,
                                wlStartPage,
                                pReserved,
                                pWlanReasonCode);
        return retval;
    }

    static BOOL 
    IsWMBuild(void)
    {
        // Just use SetProfile func to decide
        if(sm_hCmDll)
            return TRUE;
        return FALSE;
    }

private:

    // Procedure signatures:
    //
    typedef PVOID (*PFAllocateMemory_t)           (DWORD);
    typedef DWORD (*PFCloseHandle_t)              (HANDLE, PVOID);
    typedef DWORD (*PFConnect_t)                  (HANDLE, const GUID *, const PWLAN_CONNECTION_PARAMETERS, PVOID);
    typedef DWORD (*PFDeleteProfile_t)            (HANDLE, const GUID *, LPCWSTR, PVOID);
    typedef DWORD (*PFDisconnect_t)               (HANDLE, const GUID *, PVOID);
    typedef DWORD (*PFEnumInterfaces_t)           (HANDLE, PVOID, PWLAN_INTERFACE_INFO_LIST *);
    typedef DWORD (*PFExtractPsdIEDataList_t)     (HANDLE, DWORD, const PBYTE, LPCWSTR, PVOID, PWLAN_RAW_DATA_LIST *);
    typedef VOID  (*PFFreeMemory_t)               (PVOID);
    typedef DWORD (*PFGetAvailableNetworkList_t)  (HANDLE, const GUID *, DWORD, PVOID, PWLAN_AVAILABLE_NETWORK_LIST *);
    typedef DWORD (*PFGetFilterList_t)            (HANDLE, WLAN_FILTER_LIST_TYPE, PVOID, PDOT11_NETWORK_LIST *);
    typedef DWORD (*PFGetInterfaceCapability_t)   (HANDLE, const GUID *, PVOID, PWLAN_INTERFACE_CAPABILITY *);
    typedef DWORD (*PFGetNetworkBssList_t)        (HANDLE, const GUID *, const PDOT11_SSID, DOT11_BSS_TYPE, BOOL, PVOID, PWLAN_BSS_LIST *);
    typedef DWORD (*PFGetProfile_t)               (HANDLE, const GUID *, LPCWSTR, PVOID, LPWSTR *, DWORD *, DWORD *);
    typedef DWORD (*PFGetProfileCustomUserData_t) (HANDLE, const GUID *, LPCWSTR, PVOID, DWORD *, PBYTE *);
    typedef DWORD (*PFGetProfileList_t)           (HANDLE, const GUID *, PVOID, PWLAN_PROFILE_INFO_LIST *);
    typedef DWORD (*PFGetSecuritySettings_t)      (HANDLE, WLAN_SECURABLE_OBJECT, PWLAN_OPCODE_VALUE_TYPE, LPWSTR *, PDWORD);
    typedef DWORD (*PFIhvControl_t)               (HANDLE, const GUID *, WLAN_IHV_CONTROL_TYPE, DWORD, PVOID, DWORD, PVOID, PDWORD);
    typedef DWORD (*PFOpenHandle_t)               (DWORD, PVOID, PDWORD, PHANDLE);
    typedef DWORD (*PFQueryAutoConfigParameter_t) (HANDLE, WLAN_AUTOCONF_OPCODE, PVOID, PDWORD, PVOID *, PWLAN_OPCODE_VALUE_TYPE);
    typedef DWORD (*PFQueryInterface_t)           (HANDLE, const GUID *, WLAN_INTF_OPCODE, PVOID, PDWORD, PVOID *, PWLAN_OPCODE_VALUE_TYPE);
    typedef DWORD (*PFReasonCodeToString_t)       (DWORD, DWORD, PWCHAR, PVOID);
    typedef DWORD (*PFRegisterNotification_t)     (HANDLE, DWORD, BOOL, WLAN_NOTIFICATION_CALLBACK, PVOID, PVOID, PDWORD);
    typedef DWORD (*PFRenameProfile_t)            (HANDLE, const GUID *, LPCWSTR, LPCWSTR, PVOID);
    typedef DWORD (*PFSaveTemporaryProfile_t)     (HANDLE, const GUID *, LPCWSTR, LPCWSTR, DWORD, BOOL, PVOID);
    typedef DWORD (*PFScan_t)                     (HANDLE, const GUID *, const PDOT11_SSID, const PWLAN_RAW_DATA, PVOID);
    typedef DWORD (*PFSetAutoConfigParameter_t)   (HANDLE, WLAN_AUTOCONF_OPCODE, DWORD, const PVOID, PVOID);
    typedef DWORD (*PFSetFilterList_t)            (HANDLE, WLAN_FILTER_LIST_TYPE, const PDOT11_NETWORK_LIST, PVOID);
    typedef DWORD (*PFSetInterface_t)             (HANDLE, const GUID *, WLAN_INTF_OPCODE, DWORD, const PVOID, PVOID);
    typedef DWORD (*PFSetProfile_t)               (HANDLE, const GUID *, DWORD, LPCWSTR, LPCWSTR, BOOL, PVOID, DWORD *);
    typedef DWORD (*PFSetProfileCustomUserData_t) (HANDLE, const GUID *, LPCWSTR, DWORD, const PBYTE, PVOID);
    typedef DWORD (*PFSetProfileEapUserData_t)    (HANDLE, const GUID *, LPCWSTR, EAP_METHOD_TYPE, DWORD, DWORD, const LPBYTE, PVOID);
    typedef DWORD (*PFSetProfileEapXmlUserData_t) (HANDLE, const GUID *, LPCWSTR, DWORD, LPCWSTR, PVOID);
    typedef DWORD (*PFSetProfileList_t)           (HANDLE, const GUID *, DWORD, LPCWSTR *, PVOID);
    typedef DWORD (*PFSetProfilePosition_t)       (HANDLE, const GUID *, LPCWSTR, DWORD, PVOID);
    typedef DWORD (*PFSetPsdIEDataList_t)         (HANDLE, LPCWSTR, const PWLAN_RAW_DATA_LIST, PVOID);
    typedef DWORD (*PFSetSecuritySettings_t)      (HANDLE, WLAN_SECURABLE_OBJECT, LPCWSTR);
    typedef DWORD (*PFUIEditProfile_t)            (DWORD, LPCWSTR, GUID *, HWND, WL_DISPLAY_PAGES, PVOID, PWLAN_REASON_CODE);

    // Static data:
    //
    static HINSTANCE sm_hWLANDll;
    static HINSTANCE sm_hCmDll;
    static LONG      sm_WLANUsers;

    static PFAllocateMemory_t           sm_pfAllocateMemory;
    static PFCloseHandle_t              sm_pfCloseHandle;
    static PFConnect_t                  sm_pfConnect;
    static PFDeleteProfile_t            sm_pfDeleteProfile;
    static PFDisconnect_t               sm_pfDisconnect;
    static PFEnumInterfaces_t           sm_pfEnumInterfaces;
    static PFExtractPsdIEDataList_t     sm_pfExtractPsdIEDataList;
    static PFFreeMemory_t               sm_pfFreeMemory;
    static PFGetAvailableNetworkList_t  sm_pfGetAvailableNetworkList;
    static PFGetFilterList_t            sm_pfGetFilterList;
    static PFGetInterfaceCapability_t   sm_pfGetInterfaceCapability;
    static PFGetNetworkBssList_t        sm_pfGetNetworkBssList;
    static PFGetProfile_t               sm_pfGetProfile;
    static PFGetProfileCustomUserData_t sm_pfGetProfileCustomUserData;
    static PFGetProfileList_t           sm_pfGetProfileList;
    static PFGetSecuritySettings_t      sm_pfGetSecuritySettings;
    static PFIhvControl_t               sm_pfIhvControl;
    static PFOpenHandle_t               sm_pfOpenHandle;
    static PFQueryAutoConfigParameter_t sm_pfQueryAutoConfigParameter;
    static PFQueryInterface_t           sm_pfQueryInterface;
    static PFReasonCodeToString_t       sm_pfReasonCodeToString;
    static PFRegisterNotification_t     sm_pfRegisterNotification;
    static PFRenameProfile_t            sm_pfRenameProfile;
    static PFSaveTemporaryProfile_t     sm_pfSaveTemporaryProfile;
    static PFScan_t                     sm_pfScan;
    static PFSetAutoConfigParameter_t   sm_pfSetAutoConfigParameter;
    static PFSetFilterList_t            sm_pfSetFilterList;
    static PFSetInterface_t             sm_pfSetInterface;
    static PFSetProfile_t               sm_pfSetProfile;
    static PFSetProfileCustomUserData_t sm_pfSetProfileCustomUserData;
    static PFSetProfileEapUserData_t    sm_pfSetProfileEapUserData;
    static PFSetProfileEapXmlUserData_t sm_pfSetProfileEapXmlUserData;
    static PFSetProfileList_t           sm_pfSetProfileList;
    static PFSetProfilePosition_t       sm_pfSetProfilePosition;
    static PFSetPsdIEDataList_t         sm_pfSetPsdIEDataList;
    static PFSetSecuritySettings_t      sm_pfSetSecuritySettings;
    static PFUIEditProfile_t            sm_pfUIEditProfile;

    // (Re)initializes the static data:
    //
    static void
    Clear(void);

    // Copy and assignment are deliberately disabled:
    //
    WLANAPIIntf_t(IN const WLANAPIIntf_t &rhs);
    WLANAPIIntf_t &operator = (IN const WLANAPIIntf_t &rhs);
};

// Disable direct calls to the wlanapi functions:
//
#define WlanAllocateMemory           DoNotLinkWlanDirectly_UseWLANAPIIntfInstead
#define WlanCloseHandle              DoNotLinkWlanDirectly_UseWLANAPIIntfInstead
#define WlanConnect                  DoNotLinkWlanDirectly_UseWLANAPIIntfInstead
#define WlanDeleteProfile            DoNotLinkWlanDirectly_UseWLANAPIIntfInstead
#define WlanDisconnect               DoNotLinkWlanDirectly_UseWLANAPIIntfInstead
#define WlanEnumInterfaces           DoNotLinkWlanDirectly_UseWLANAPIIntfInstead
#define WlanExtractPsdIEDataList     DoNotLinkWlanDirectly_UseWLANAPIIntfInstead
#define WlanFreeMemory               DoNotLinkWlanDirectly_UseWLANAPIIntfInstead
#define WlanGetAvailableNetworkList  DoNotLinkWlanDirectly_UseWLANAPIIntfInstead
#define WlanGetFilterList            DoNotLinkWlanDirectly_UseWLANAPIIntfInstead
#define WlanGetInterfaceCapability   DoNotLinkWlanDirectly_UseWLANAPIIntfInstead
#define WlanGetNetworkBssList        DoNotLinkWlanDirectly_UseWLANAPIIntfInstead
#define WlanGetProfile               DoNotLinkWlanDirectly_UseWLANAPIIntfInstead
#define WlanGetProfileCustomUserData DoNotLinkWlanDirectly_UseWLANAPIIntfInstead
#define WlanGetProfileList           DoNotLinkWlanDirectly_UseWLANAPIIntfInstead
#define WlanGetSecuritySettings      DoNotLinkWlanDirectly_UseWLANAPIIntfInstead
#define WlanIhvControl               DoNotLinkWlanDirectly_UseWLANAPIIntfInstead
#define WlanOpenHandle               DoNotLinkWlanDirectly_UseWLANAPIIntfInstead
#define WlanQueryAutoConfigParameter DoNotLinkWlanDirectly_UseWLANAPIIntfInstead
#define WlanQueryInterface           DoNotLinkWlanDirectly_UseWLANAPIIntfInstead
#define WlanReasonCodeToString       DoNotLinkWlanDirectly_UseWLANAPIIntfInstead
#define WlanRegisterNotification     DoNotLinkWlanDirectly_UseWLANAPIIntfInstead
#define WlanRenameProfile            DoNotLinkWlanDirectly_UseWLANAPIIntfInstead
#define WlanSaveTemporaryProfile     DoNotLinkWlanDirectly_UseWLANAPIIntfInstead
#define WlanScan                     DoNotLinkWlanDirectly_UseWLANAPIIntfInstead
#define WlanSetAutoConfigParameter   DoNotLinkWlanDirectly_UseWLANAPIIntfInstead
#define WlanSetFilterList            DoNotLinkWlanDirectly_UseWLANAPIIntfInstead
#define WlanSetInterface             DoNotLinkWlanDirectly_UseWLANAPIIntfInstead
#define WlanSetProfile               DoNotLinkWlanDirectly_UseWLANAPIIntfInstead
#define WlanSetProfileCustomUserData DoNotLinkWlanDirectly_UseWLANAPIIntfInstead
#define WlanSetProfileEapUserData    DoNotLinkWlanDirectly_UseWLANAPIIntfInstead
#define WlanSetProfileEapXmlUserData DoNotLinkWlanDirectly_UseWLANAPIIntfInstead
#define WlanSetProfileList           DoNotLinkWlanDirectly_UseWLANAPIIntfInstead
#define WlanSetProfilePosition       DoNotLinkWlanDirectly_UseWLANAPIIntfInstead
#define WlanSetPsdIEDataList         DoNotLinkWlanDirectly_UseWLANAPIIntfInstead
#define WlanSetSecuritySettings      DoNotLinkWlanDirectly_UseWLANAPIIntfInstead
#define WlanUIEditProfile            DoNotLinkWlanDirectly_UseWLANAPIIntfInstead

// ----------------------------------------------------------------------------
//
// WlanErrorText is a simple class and methods for translating Win32
// error codes and WLAN reason codes into text form for error messages:
//      LogError(TEXT("oops, eek!: %s"), WlanErrorText(result,reason));
//
#ifndef WlanErrorText
#define WlanErrorText __WlanErrorText()
class __WlanErrorText
{
private:
    TCHAR m_ErrorText[192];
public:
  __WlanErrorText(void) { }
    const TCHAR *operator() (DWORD ErrorCode, DWORD ReasonCode);
};
#endif /* Win32ErrorText */

};
};

#endif /* if (WLAN_VERSION > 0) */
#endif /* _DEFINED_WLANAPIIntf_t_ */
// ----------------------------------------------------------------------------
