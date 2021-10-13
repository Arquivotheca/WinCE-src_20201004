//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

// Register an interface with tcpip6
typedef IP_STATUS (*IPV6AddInterfaceRtn)(
						IN  PWSTR	wszInterfaceName,
						IN  const LLIPBindInfo *BindInfo,
						OUT PVOID *pIpContext,
						OUT PDWORD pIfIndex);

// Deregister an interface with tcpip6, Context is the value returned from AddInterface
typedef void (*IPV6DelInterfaceRtn)(IN PVOID IpContext);


typedef void (*IPV6SendCompleteRtn)(IN PVOID IpContext,
									IN PNDIS_PACKET pPacket,
									IN IP_STATUS Status);
 
typedef void (*IPV6ReceiveRtn)(IN PVOID IpContext,
							   IN	PBYTE pData,
							   IN	DWORD cbData);
