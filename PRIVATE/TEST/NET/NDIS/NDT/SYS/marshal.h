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

NDIS_STATUS MarshalParameters(
   PVOID *ppvBuffer, DWORD *pcbBuffer, LPCTSTR szFormat, ...
);

NDIS_STATUS UnmarshalParameters(
   PVOID *ppvBuffer, DWORD *pcbBuffer, LPCTSTR szFormat, ...
);

//------------------------------------------------------------------------------

#ifdef __cplusplus
}
#endif

//------------------------------------------------------------------------------

#endif

