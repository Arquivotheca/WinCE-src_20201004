//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

