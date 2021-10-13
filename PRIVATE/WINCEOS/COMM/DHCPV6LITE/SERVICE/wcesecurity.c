//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

