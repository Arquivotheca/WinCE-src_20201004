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
/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright  1997  Microsoft Corporation.  All Rights Reserved.

Module Name:

     errmacro.h  

Abstract:
Functions:
Notes:
--*/
/*++
 
 
Module Name:
 
	ErrMacro.h
 
Abstract:
 
	This file contians the error loggin and handling macro's.
 
Author:
 
	Uknown (unknown)
 
Notes:

    To use this file cszThisFile bug be defined before this file is included.
 
--*/
#ifndef __ERRMACRO_H__
#define __ERRMACRO_H__

#define FUNCTION_ERROR( KTO, TST, ACT ); \
    if( TST ) { \
        KTO->Log( LOG_FAIL, TEXT("FAIL in %s @ line %d:  GetLastError() = %d, %s "), \
                  cszThisFile, __LINE__, GetLastError(), TEXT( #ACT ) ); \
            ACT; \
        }

#define RETURN_ERROR( KTO, TST, RTN, ACT ); \
    if( TST ) { \
        KTO->Log( LOG_FAIL, TEXT("FAIL in %s @ line %d:  return == %d, %s "), \
                   cszThisFile, __LINE__, RTN, TEXT( #ACT ) ); \
        ACT; \
        }

#define DEFAULT_ERROR( KTO, TST, ACT );  \
    if( TST ) { \
        KTO->Log( LOG_FAIL, TEXT("FAIL in %s @ line %d: \"%s\" caused \"%s\"" ), \
                      cszThisFile, __LINE__, TEXT( #TST ), TEXT( #ACT ) ); \
        ACT; \
    }

#define LOGLINENUM( KTO ) KTO->Log( LOG_DETAIL, TEXT("In %s @ line %d:" ), \
                                    cszThisFile, __LINE__ );

#define CMDLINENUM( KTO, CMD ) \
    KTO->Log( LOG_DETAIL, TEXT("In %s @ line %d: %s" ), \
                  cszThisFile, __LINE__, TEXT( #CMD ) );  \
    CMD                  

    

#endif __ERRMACRO_H__
