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
/***
*atodbl.c - convert a string to a floating point value
*
*   Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*   Converts a string to a floating point value.
*
*Revision History:
*       03-24-03  GB    written
*       04-07-04  MSL   Changes to support locale-specific strgtold12
*                       VSW 247190
*       07-29-04  AC    Moved the implementaton to atodbl.inl
*
*******************************************************************************/

#include <fltintrn.h>
#include <float.h>
#include <math.h>
#include <mtdll.h>
#include <internal.h>
#include <setlocal.h>

#include <atodbl.inl>
