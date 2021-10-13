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
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.
Copyright (c) 1995-2000 Microsoft Corporation

Module Name:   KitlTran_priv.h

Abstract:  
    This contains declarations the KITL transport routines.
Functions:


Notes: 
    This is a copy of the kitltran.h file found in the Tools tree at:
        %_WINCEROOT%\Tools\platman\source\kitl\inc

--*/
#ifndef _KITL_TRANSPORT_H_
#define _KITL_TRANSPORT_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _TRANPORTINFOSTRUCT {
    WORD cbData;        // sizeof the struct
    WORD wVersion;      // version of the struct
    WORD wTranGuid;     // the guid of the transport (see below)
    WORD wFlags;        // transport flags (see below)
    DWORD dwBandWidth;  // bandwidth
} TRANPORTINFOSTRUCT, *PTRANPORTINFOSTRUCT;

#define CURRENT_TRANINFO_VERSION    1   // current version of the TRANSPORTINFOSTRUCT

// transport GUIDs
#define TRAN_GUID_DEFAULT       0
#define TRAN_GUID_ETHERNET      1
#define TRAN_GUID_SERIAL        2
#define TRAN_GUID_PARALLEL      3
#define TRAN_GUID_USB           4

#define TRAN_GUID_OEMSTART      128     // OEM specific transport starts here

// transport flags
#define TRAN_DIRECTCONNTECT   0x0001      // direction connected to host (e.g. serial, parallel)


// typedefs
typedef UINT    TRANSPORTID;
typedef BOOL (* PFN_EnumProc) (LPCWSTR pszDevName, LPVOID pData);
// the types of all the transport export functions
typedef BOOL (* PFN_TRANINITLIB) (LPCWSTR pszRegKeyRoot);
typedef BOOL (* PFN_TRANDEINITLIB) (void);
typedef TRANSPORTID (* PFN_TRANCREATE) (LPCWSTR pszDeviceName);
typedef BOOL (* PFN_TRANDELETE) (TRANSPORTID id);
typedef BOOL (* PFN_TRANSEND) (TRANSPORTID id, LPCVOID pData, USHORT cbData, DWORD dwTimeout);
typedef BOOL (* PFN_TRANRECV) (TRANSPORTID id, LPVOID pBuffer, USHORT *pcbBuffer, DWORD dwTimeout);
typedef BOOL (* PFN_TRANRECVEX) (TRANSPORTID id, LPVOID pBuffer, USHORT *pcbBuffer, DWORD dwTimeout, BOOL* pfConfigPacket);
typedef DWORD (* PFN_TRANGETCPUID) (TRANSPORTID id);
typedef BOOL (* PFN_TRANENUMDEV) (PFN_EnumProc pfnCB, LPVOID pUserData);
typedef BOOL (* PFN_TRANGETHOSTCFG) (TRANSPORTID id, LPVOID pBuffer, PUSHORT pcbBuffer);
typedef BOOL (* PFN_TRANSETDEVCFG) (TRANSPORTID id, LPCVOID pData, USHORT cbData);
typedef BOOL (* PFN_TRANGETINFO) (PTRANPORTINFOSTRUCT pInfo);
typedef BOOL (* PFN_TRANGETXMLPARAMS) (LPCWSTR *ppstrXML);
typedef BOOL (* PFN_TRANSETPARAM) (LPCWSTR strName, LPCWSTR strValue, LPWSTR pstrError, long dwErrorStringSize);

#define INVALID_TRANSPORTID     ((TRANSPORTID) -1)

// library initialization/deinitialization
BOOL TranInitLibrary (LPCWSTR pszRegKeyRoot);
BOOL TranDeInitLibrary (void);

// transport creation/deletion
TRANSPORTID TranCreate (LPCWSTR pszDeviceName);
BOOL TranDelete (TRANSPORTID id);

// send and receive via a given transport
BOOL TranSend (TRANSPORTID id, LPCVOID pData, USHORT cbData, DWORD dwTimeout);
BOOL TranRecv (TRANSPORTID id, LPVOID pBuffer, USHORT *pcbBuffer, DWORD dwTimeout);

// transport configuration
BOOL TranGetHostCfg (TRANSPORTID id, LPVOID pBuffer, PUSHORT pcbBuffer);
BOOL TranSetDevCfg (TRANSPORTID id, LPCVOID pData, USHORT cbData);

// device name service
BOOL TranEnumKnownDevice (PFN_EnumProc pfnCB, LPVOID pUserData);

// trasnport-wide settings
BOOL TranGetInfo (PTRANPORTINFOSTRUCT pInfo);
BOOL TranGetXMLParams (LPCWSTR *ppstrXML);
BOOL TranSetParam (LPCWSTR strName, LPCWSTR strValue, LPWSTR pstrError, long dwErrorStringSize);


#ifdef __cplusplus
}
#endif


#endif  // _KITL_TRANSPORT_H_
