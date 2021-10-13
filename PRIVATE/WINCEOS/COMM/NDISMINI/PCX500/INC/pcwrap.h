//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
// Copyright 2001, Cisco Systems, Inc.  All rights reserved.
// No part of this source, or the resulting binary files, may be reproduced,
// transmitted or redistributed in any form or by any means, electronic or
// mechanical, for any purpose, without the express written permission of Cisco.
//
// pcwrap.h

//
#ifndef	__PCCARDWRAP_H__
#define	__PCCARDWRAP_H__
//

#ifdef __cplusplus
extern "C" {
#endif//__cplusplus

MAKE_HEADER(int, _stdcall,PCCARD_Get_Version, (void))
MAKE_HEADER(int, _stdcall,PCCARD_Card_Services, (int, unsigned ,void *, unsigned long, void *))
MAKE_HEADER(int, _stdcall,PCCARD_Card_Services_Ex, (int, unsigned , void *, unsigned long, void *, void *))
	
#define	PCCARD_Get_Version      PREPEND(PCCARD_Get_Version)
#define	PCCARD_Card_Services	PREPEND(PCCARD_Card_Services)
#define	PCCARD_Card_Services_Ex	PREPEND(PCCARD_Card_Services_Ex)

#define	PccardReplaceSocketServices(handle, ptr4, size3) \
		PCCARD_Card_Services( F_REPLACE_SOCKET_SERVICES, handle, NULL, size3, (void *)ptr4)  

#define	PccardRegisterClient( ptr4, size3, ret5 ) \
		PCCARD_Card_Services_Ex( F_REGISTER_CLIENT, 0, NULL, size3, (void *)ptr4, (void *)ret5)  

#define	PccardDeregisterClient( handle, ptr4, size3) \
		PCCARD_Card_Services( F_DEREGISTER_CLIENT, handle, NULL, size3, (void *)ptr4)  

#define	PccardFirstClient( ptr4, size3, ret5 ) \
		PCCARD_Card_Services_Ex( F_GET_FIRST_CLIENT, 0, NULL, size3, (void *)ptr4, (void *)ret5)  

#define	PccardExclusive( ptr4, size3 ) \
		PCCARD_Card_Services( F_REQUEST_EXCLUSIVE,  handle, NULL, size3, (void *)ptr4 )  

#define	PccardGetCardServicesInfo( ptr4, size3 ) \
					PCCARD_Card_Services( F_GET_CARD_SERVICES_INFO, 0, NULL, size3, (void *)ptr4)  

#define	PccardGetConfiguration( ptr4, size3 )	\
					PCCARD_Card_Services( F_GET_CONFIGURATION_INFO, 0, NULL, size3, (void *)ptr4)  

#define	PccardSetConfiguration( handle, ptr4, size3 ) \
					PCCARD_Card_Services( F_MODIFY_CONFIGURATION, handle, NULL, size3, (void *)ptr4 )  

#define	PccardReleaseConfiguration( handle, ptr4, size3 ) \
					PCCARD_Card_Services( F_RELEASE_CONFIGURATION, handle, NULL, size3, (void *)ptr4 )  

#define	PccardRequestConfiguration( handle, ptr4, size3 ) \
					PCCARD_Card_Services( F_REQUEST_CONFIGURATION, handle, NULL, size3, (void *)ptr4 )  

#define	PccardGetTupleData( out2, ptr4, size3 ) \
			PCCARD_Card_Services( F_GET_TUPLE_DATA, 0, out2, size3, (void *)ptr4 )  

#define	PccardGetFirstTuple( ptr4, size3 ) \
			PCCARD_Card_Services( F_GET_FIRST_TUPLE, 0, NULL, size3, (void *)ptr4 )  

#ifdef __cplusplus
}
#endif//__cplusplus
#endif//__PCCARDWRAP_H__	

