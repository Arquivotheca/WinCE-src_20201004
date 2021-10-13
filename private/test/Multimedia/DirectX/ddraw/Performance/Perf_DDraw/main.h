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
#ifndef __MAIN_H__
#define __MAIN_H__

#include <windows.h>
#include <stdlib.h>
#include <platform_config.h>
#include <tchar.h>

// This is for NT
#ifndef ASSERT
#include "assert.h" 
#define ASSERT assert
#endif

#include <ddraw.h>

#include <hqalog.h>

#include "multiperflogger.h"

#include <QATestUty/DebugUty.h>

#endif