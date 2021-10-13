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
 
Copyright (c) 1996  Microsoft Corporation
 
Module Name:
 
	ErrMacro.h
 
Abstract:
 
	This file contians the error loggin and handling macro's.
 

 
	Uknown (unknown)
 
Notes:

    To use this file cszThisFile .
 
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
