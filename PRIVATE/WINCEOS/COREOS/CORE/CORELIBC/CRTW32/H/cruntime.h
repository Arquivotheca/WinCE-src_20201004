//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef _INC_CRUNTIME
#define _INC_CRUNTIME

#include <stdlib.h>

#ifndef x86
#define UNALIGNED __unaligned
#else
#define UNALIGNED
#endif

#define REG1
#define REG2
#define REG3

#endif	/* _INC_CRUNTIME */

