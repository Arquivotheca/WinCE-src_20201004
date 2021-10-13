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


#define MAX_ACE 6

#define DHCPV6_OBJECT_SERVER 0

#define DHCPV6_OBJECT_COUNT 1

#define SERVER_ACCESS_ADMINISTER 0x00000001

#define SERVER_ACCESS_ENUMERATE 0x00000002

#define SERVER_READ (STANDARD_RIGHTS_READ |\
                     SERVER_ACCESS_ENUMERATE)

#define SERVER_WRITE (STANDARD_RIGHTS_WRITE |\
                      SERVER_ACCESS_ADMINISTER |\
                      SERVER_ACCESS_ENUMERATE)

#define SERVER_EXECUTE (STANDARD_RIGHTS_EXECUTE |\
                        SERVER_ACCESS_ENUMERATE)

#define SERVER_ALL_ACCESS (STANDARD_RIGHTS_REQUIRED |\
                           SERVER_ACCESS_ADMINISTER |\
                           SERVER_ACCESS_ENUMERATE)


DWORD
InitializeDHCPV6Security(
    PSECURITY_DESCRIPTOR * ppDHCPV6SD
    );

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
    );

DWORD
ValidateSecurity(
    DWORD dwObjectType,
    ACCESS_MASK DesiredAccess,
    LPVOID pObjectHandle,
    PACCESS_MASK pGrantedAccess
    );

VOID
MapGenericToSpecificAccess(
    DWORD dwObjectType,
    ACCESS_MASK GenericAccess,
    PACCESS_MASK pSpecificAccess
    );

BOOL
GetTokenHandle(
    PHANDLE phToken
    );

DWORD
InitializeSecurityOnFly(
    PSECURITY_DESCRIPTOR * ppDHCPV6SD
    );

