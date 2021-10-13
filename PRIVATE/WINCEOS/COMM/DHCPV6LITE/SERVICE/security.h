//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

