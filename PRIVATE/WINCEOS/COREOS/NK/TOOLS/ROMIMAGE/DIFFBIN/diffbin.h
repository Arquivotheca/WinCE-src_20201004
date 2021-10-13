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
// diffbin.h
// Master header file for diffbin

#include <windows.h>

#ifndef UNDER_CE
#include <sys/types.h>
#include <sys/stat.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>

#include "nkcompr.h"
#include "cecompr.h"
#include "ehm.h"
#include "utils.h"
#include "hashtbl.h"
#include "pehdr.h"
#include "romldr.h"
#include "mapfile.h"
#include "runlist.h"
#include "imgdata.h"
#include "reloc.h"
#include "tt.h"


#define RETAILMSG(cond,printf_exp)   \
   ((cond)?(printf printf_exp),1:0)

extern DWORD   g_dwZoneMask;

#define ZONE_VERBOSE        (g_dwZoneMask & 0x00000001)
#define ZONE_PARSE_VERBOSE  (g_dwZoneMask & 0x00000002)
#define ZONE_FIXUP          (g_dwZoneMask & 0x00000004)
#define ZONE_FIXUP_VERBOSE  (g_dwZoneMask & 0x00000008)
#define ZONE_PROGRESS       (g_dwZoneMask & 0x00000010)
#define ZONE_DECOMPRESS     (g_dwZoneMask & 0x00000020)

