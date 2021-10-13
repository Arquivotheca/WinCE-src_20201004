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
#include "pch.h"
#pragma hdrstop

#include <ssdpioctl.h>
#include <service.h>
#include <svsutil.hxx>


BOOL
UpnpAddDeviceImpl(
    IN HANDLE                                       hOwner, 
    ce::marshal_arg<ce::copy_in, UPNPDEVICEINFO*>   pDevInfo);

BOOL
UpnpRemoveDeviceImpl(
    ce::marshal_arg<ce::copy_in, PCWSTR>            pszDeviceName);

BOOL
UpnpPublishDeviceImpl(
    ce::marshal_arg<ce::copy_in, PCWSTR>            pszDeviceName);

BOOL
UpnpUnpublishDeviceImpl(
    ce::marshal_arg<ce::copy_in, PCWSTR>            pszDeviceName);

BOOL
UpnpGetSCPDPathImpl(
    ce::marshal_arg<ce::copy_in, PCWSTR>                            pszDeviceName,
    ce::marshal_arg<ce::copy_in, PCWSTR>                            pszUDN,
    ce::marshal_arg<ce::copy_in, PCWSTR>                            pszServiceId,
    ce::marshal_arg<ce::copy_out, ce::psl_buffer_wrapper<PWSTR> >   SCPDFilePath);
    
BOOL
UpnpGetUDNImpl(
    ce::marshal_arg<ce::copy_in, PCWSTR>                            pszDeviceName, 
    ce::marshal_arg<ce::copy_in, PCWSTR>                            pszTemplateUDN,
    ce::marshal_arg<ce::copy_out, ce::psl_buffer_wrapper<PWSTR> >   UDNBuf,
    ce::marshal_arg<ce::copy_out, PDWORD>                           pcchBuf);


BOOL
UpnpSubmitPropertyEventImpl(
    ce::marshal_arg<ce::copy_in, PCWSTR>    pszDeviceName,
    ce::marshal_arg<ce::copy_in, PCWSTR>    pszUDN,
    ce::marshal_arg<ce::copy_in, PCWSTR>    pszServiceId,
    ce::marshal_arg<ce::copy_array_in, ce::psl_buffer_wrapper<UPNPPARAM*> > Args);

BOOL
UpnpUpdateEventedVariablesImpl(
    ce::marshal_arg<ce::copy_in, PCWSTR>    pszDeviceName,
    ce::marshal_arg<ce::copy_in, PCWSTR>    pszUDN,
    ce::marshal_arg<ce::copy_in, PCWSTR>    pszServiceId,
    ce::marshal_arg<ce::copy_array_in, ce::psl_buffer_wrapper<UPNPPARAM*> > Args);

BOOL
SetRawControlResponseImpl(
    DWORD                                   hRequest,
    DWORD                                   dwHttpStatus,
    ce::marshal_arg<ce::copy_in, PCWSTR>    pszResp);


BOOL 
InitializeReceiveInvokeRequestImpl();

BOOL 
ReceiveInvokeRequestImpl(
    IN DWORD retCode, 
    ce::marshal_arg<ce::copy_out, ce::psl_buffer_wrapper<PBYTE> > pReqBuf,
    ce::marshal_arg<ce::copy_out, PDWORD> pcbReqBuf);

BOOL 
CancelReceiveInvokeRequestImpl();
