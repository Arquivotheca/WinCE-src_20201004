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
//	asyncCod.h

#ifndef __asyncCod_h__
#define __asyncCod_h__ 
//
#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

typedef enum	_ASYNC_CODE {
	ASYNC_CODE_GETFLASHSTATE,
}ASYNC_CODE;

typedef enum _FLASH_STATE_CODE {
	FLASH_STATE_CODE_NULL=0,
	FLASH_STATE_CODE_START,
	FLASH_STATE_CODE_ERASEFLASH,
	FLASH_STATE_CODE_PROGFLASH,
	FLASH_STATE_CODE_ENDPROG,
	FLASH_STATE_CODE_ENDSEQ,
	FLASH_STATE_CODE_CONFIG,
	FLASH_STATE_CODE_DISABLE,
	FLASH_STATE_CODE_DONE,
	FLASH_STATE_CODE_ABORT
}FLASH_STATE_CODE;

#pragma pack( push, struct_pack1 )
#pragma pack( 1 )
typedef struct	_ASYNC_BUF{
	ULONG	queryCode;
	ULONG	retCode;
	char	info[256];
}ASYNC_BUF, *PASYNC_BUF;
#pragma pack( pop, struct_pack1 )


#ifdef __cplusplus
}
#endif // __cplusplus

#endif //__asyncCod_h__ 
