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
#include <windows.h>
#include <kato.h>
#include <tux.h>
#include "bencheng.h"
#include "utilities.h"
#include "Otak.h"
// required for StringCchCopy on NT
#include <strsafe.h>

#ifndef TUXMAIN_H
#define TUXMAIN_H


// kato Log Definitions
enum infoType {
   FAIL,
   ECHO,
   DETAIL,
   ABORT,
   SKIP,
};

void info(infoType iType, LPCTSTR szFormat, ...);
TESTPROCAPI getCode(void);

// kato log definitions
#define MY_EXCEPTION          0
#define MY_FAIL               2
#define MY_ABORT              4
#define MY_SKIP               6
#define MY_NOT_IMPLEMENTED    8
#define MY_PASS              10

#endif
