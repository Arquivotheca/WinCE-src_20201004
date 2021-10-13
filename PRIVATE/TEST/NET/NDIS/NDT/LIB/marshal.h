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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#ifndef __MARSHAL_H
#define __MARSHAL_H

//------------------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

//------------------------------------------------------------------------------

typedef enum _NDIS_REQUEST_TYPE
{
    NdisRequestQueryInformation,
    NdisRequestSetInformation,
    NdisRequestQueryStatistics,
    NdisRequestOpen,
    NdisRequestClose,
    NdisRequestSend,
    NdisRequestTransferData,
    NdisRequestReset,
    NdisRequestGeneric1,
    NdisRequestGeneric2,
    NdisRequestGeneric3,
    NdisRequestGeneric4
} NDIS_REQUEST_TYPE, *PNDIS_REQUEST_TYPE;

//------------------------------------------------------------------------------

HRESULT MarshalParameters(
   PVOID *ppvBuffer, DWORD *pcbBuffer, LPCTSTR szFormat, ...
);

HRESULT UnmarshalParameters(
   PVOID *ppvBuffer, DWORD *pcbBuffer, LPCTSTR szFormat, ...
);

HRESULT MarshalParametersV(
   PVOID *ppvBuffer, DWORD *pcbBuffer, LPCTSTR szFormat, va_list pArgs
);

HRESULT UnmarshalParametersV(
   PVOID *ppvBuffer, DWORD *pcbBuffer, LPCTSTR szFormat, va_list pArgs
);

//------------------------------------------------------------------------------

#ifdef __cplusplus
}
#endif

//------------------------------------------------------------------------------

#endif

