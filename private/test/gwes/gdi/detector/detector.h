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
//******************************************************************************


#pragma once

#include <windows.h>
#include <katoex.h>
#include <tux.h>

// 
#define BVT_BASE  1000

// Define a wide char version of __FILE__ called __WFILE__
#define WIDEN2(x) L ## x
#define WIDEN(x) WIDEN2(x)
#define __WFILE__ WIDEN(__FILE__)


//
// Handy macros
//

#define NOT_EXECUTE(x)	do{ \
                            if((x) != TPM_EXECUTE) \
                                return TPR_NOT_HANDLED; \
                        }while(false)

//
// Easy logging macros
//

#define FAIL(x)     g_pKato->Log( LOG_FAIL, \
                        TEXT("FAIL in %s at line %d: %s"), \
                        __WFILE__, __LINE__, TEXT(x) )
                        
#define SUCCESS(x)  g_pKato->Log( LOG_PASS, \
                        TEXT("SUCCESS: %s"), TEXT(x) )

#define WARN(x)     g_pKato->Log( LOG_DETAIL, \
                        TEXT("WARNING in %s at line %d: %s"), \
                        __WFILE__, __LINE__, TEXT(x) )

#define DETAIL(x)   g_pKato->Log( LOG_DETAIL, TEXT(x) )
