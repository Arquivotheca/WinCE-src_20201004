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


#include "precomp.h"




DWORD
InitializeDHCPV6Security(
    PSECURITY_DESCRIPTOR * ppDHCPV6SD
    )
{
    return 0;
}


DWORD
BuildDHCPV6ObjectProtection(
    DWORD dwAceCount,
    PUCHAR pAceType,
    PSID * ppAceSid,
    PACCESS_MASK pAceMask,
    PBYTE pInheritFlags,
    PSID pOwnerSid,
    PSID pGroupSid,
    PGENERIC_MAPPING pGenericMap,
    PSECURITY_DESCRIPTOR * ppSecurityDescriptor
    )
{

    return 0;
}


DWORD
ValidateSecurity(
    DWORD dwObjectType,
    ACCESS_MASK DesiredAccess,
    LPVOID pObjectHandle,
    PACCESS_MASK pGrantedAccess
    )
{
    return 0;
}


VOID
MapGenericToSpecificAccess(
    DWORD dwObjectType,
    ACCESS_MASK GenericAccess,
    PACCESS_MASK pSpecificAccess
    )
{
    return;
}


BOOL
GetTokenHandle(
    PHANDLE phToken
    )
{
    return (TRUE);
}


DWORD
InitializeSecurityOnFly(
    PSECURITY_DESCRIPTOR * ppDHCPV6SD
    )
{
    return 0;
}

