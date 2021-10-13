//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++


Module Name:

    scapi.h

Abstract:

    Small client - Windows CE nonport part


--*/
#if ! defined (__scapi_H__)
#define __scapi_H__	1

#include <objbase.h>
#include <objidl.h>
#include <wtypes.h>

typedef struct  tagSCPROPVAR
    {
    DWORD cProp;
    PROPID __RPC_FAR *aPropID;
    PROPVARIANT __RPC_FAR *aPropVar;
    HRESULT __RPC_FAR *aStatus;
    }	SCPROPVAR;

typedef unsigned long SCHANDLE;

#if defined (__cplusplus)
extern "C" {
#endif

void scce_Listen (void);
HRESULT scce_Shutdown (void);

int  scce_RegisterDLL (void);
int  scce_UnregisterDLL (void);
int  scce_RegisterNET (void *ptr);
void scce_UnregisterNET (void);

#if defined (__cplusplus)
};
#endif

#if ! defined (SCAPI_CE_INTERNAL_ONLY)
#include "expapis.hxx"
#endif

#endif
