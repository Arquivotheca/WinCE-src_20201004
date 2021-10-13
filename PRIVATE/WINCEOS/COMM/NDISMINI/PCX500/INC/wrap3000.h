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
// awcnwrap.h


#ifndef	AWCNWRAP_H
#define	AWCNWRAP_H


#ifdef __cplusplus
extern "C" {
#endif

//MAKE_HEADER(int, _stdcall,MiniPortEntry, (void *entry_point, void *pCCard))
MAKE_HEADER(int, _stdcall,MiniPortEntry, (void *entry_point))
MAKE_HEADER(int, _stdcall,VWIN32_DIOCCompletionRoutine, (void *entry_point))
//
//
#define	MiniPortEntry					PREPEND(MiniPortEntry)
#define	VWIN32_DIOCCompletionRoutine	PREPEND(VWIN32_DIOCCompletionRoutine)
#ifdef __cplusplus
}
#endif

#endif	

