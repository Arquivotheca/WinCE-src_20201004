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
*mlsearch.cpp - do a binary search
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines _lsearch() - do a binary search an an array
*
*Revision History:
*       08-20-03  GB    initial version
*       10-31-04  MSL   Enable __clrcall safe version
*
*******************************************************************************/

#ifdef MRTDLL
#undef MRTDLL
#endif

#if defined(_M_CEE)
#include "lsearch.c"

#ifdef __USE_CONTEXT
#error __USE_CONTEXT should be undefined
#endif

#define __USE_CONTEXT
#include "lsearch.c"
#endif
