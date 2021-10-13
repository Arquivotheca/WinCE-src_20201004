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
