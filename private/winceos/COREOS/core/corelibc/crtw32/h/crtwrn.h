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
*crtwrn.h
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       This file contains the IDs and messages for warnings
*       in the CRT headers.
*
*Revision History:
*       05-04-04  AJS   Created
*       07-20-04  AC    Moved stringify to crtdefs
*       10-08-04  AGH   Added 4 warning messages
*       10-18-04  AC    Added _CRT_WARNING_MANAGED
*       03-23-05  MSL   New deprecation warning with function name
*       05-32-07  ATC   Added warning message for using both /clr /D_STATIC_CPPLIB together
*
****/

#pragma once

#ifndef _INC_CRTWRN
#define _INC_CRTWRN

#include <crtdefs.h>

#define __CRT_WARNING( _Number, _Description ) \
    message("" __FILE__ "(" _CRT_STRINGIZE(__LINE__) ") : " \
    "warning CRT" _CRT_STRINGIZE(_Number) ": " _CRT_STRINGIZE(_Description))

#define _CRT_WARNING( _Id ) \
    __CRT_WARNING( _CRTWRN_WNUMBER_##_Id, _CRTWRN_MESSAGE_##_Id )

/*
A warning is a 4-digit ID number (_CRTWRN_WNUMBER_*)
followed by a message (_CRTWRN_MESSAGE_*)
Emit a warning by adding the following code to the header file:
    #pragma _CRT_WARNING( id )
*/

/* NAME */
/* #pragma _CRT_WARNING( NAME ) */
/* #define _CRTWRN_WNUMBER_NAME  9999 */
/* #define _CRTWRN_MESSAGE_NAME  description */

/* ID 1001 is obsolete; do not reuse it */

/* ID 1002 is obsolete; do not reuse it */

/* ID 1003 is obsolete; do not reuse it */

/* _NO_SPECIAL_TRANSFER */
/* #pragma _CRT_WARNING( _NO_SPECIAL_TRANSFER ) */
#define _CRTWRN_WNUMBER__NO_SPECIAL_TRANSFER  1004
#define _CRTWRN_MESSAGE__NO_SPECIAL_TRANSFER \
    Special transfer of control routines not defined for this platform

/* ID 1005 is obsolete; do not reuse it */

/* ID 1006 is obsolete; do not reuse it */

/* _DEPRECATE_STATIC_CPPLIB */
/* #pragma push_macro("_STATIC_CPPLIB") */
/* #undef _STATIC_CPPLIB */
/* #pragma _CRT_WARNING( _DEPRECATE_STATIC_CPPLIB ) */
/* #pragma pop_macro("_STATIC_CPPLIB") */
#define _CRTWRN_WNUMBER__DEPRECATE_STATIC_CPPLIB  1007
#define _CRTWRN_MESSAGE__DEPRECATE_STATIC_CPPLIB _STATIC_CPPLIB is deprecated

/* ID 1008 is obsolete; do not reuse it */

#endif /* _INC_CRTWRN */
