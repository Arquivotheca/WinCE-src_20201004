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
/*
 *      NK Kernel stubs code
 *
 *
 * Module Name:
 *
 *      stubs.c
 *
 * Abstract:
 *
 *      This file implements stubs/duplicates for functions in CoreDll
 *
 */

/* Copies of functions in CoreDll which are needed here because they're needed
   to load CoreDll, but also need to be user-callable without trapping (thus they
   are also in CoreDll). */

#include "kernel.h"

/* -------------------- functions duplicated from DLL ------------------- */

#ifdef DEBUG
#define DOCHECKCS
#endif

#include "..\..\core\dll\cscode.c"
#include "..\..\core\dll\dbgprint.c"
#include "..\..\core\dll\dlist.c"

