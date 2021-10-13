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
*threadlocale.c - sets the __globallocalestatus flag to enable per thread locale
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Sets the __globallocalestatus flag to enable per thread locale.
*       Link with this obj to enable per thread locale.
*
*Revision History:
*       04-01-04  SJ    Module Created.
*	11-07-04  MSL   Support pure
*
*******************************************************************************/

int __globallocalestatus = -1 ;
