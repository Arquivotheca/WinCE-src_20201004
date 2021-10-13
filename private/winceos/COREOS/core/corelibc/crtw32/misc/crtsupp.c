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
*crtsupp.c - CRT Support functions
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Provide COREDLL-specific implementations of CRT support routines
*
*Revision History:
*       24-09-12  Refactored the code from coredll
*******************************************************************************/

#include <corecrt.h>
#include <mtdll.h>
#include <errorrep.h>
#include <stdlib.h>

crtEHData *
__crt_get_storage_EH()
/*++

Description:

    Return a pointer to a wchar_t* for use by wcstok.

--*/
{
    return &GetCRTStorage()->EHData;
}


/*
 * Include implementations from buildbtcrt
 */
#include <exitproc.c>
#include <raiseexc.c>

