//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
////////////////////////////////////////////////////////////////////////////////
//
//  WHPU Test Help Suite
//
//  Module: hqatk.h
//          Helper declarations for Tux and Kato users.
//
//  Revision history:
//      03/03/1998  lblanco     Created.
//
////////////////////////////////////////////////////////////////////////////////

#ifndef __HQATK_H__
#define __HQATK_H__

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#ifndef HQATK_NOTUX
////////////////////////////////////////////////////////////////////////////////
// Tux helpers

// Incremental TESTPROC results. To use this macros, the following must be done:
//  1. Declare a global variable as follows:
//      extern ITPR variable_name[MAX_ITPR_CODE];
//  2. Define the same global as follows:
//      ITPR variable_name[MAX_ITPR_CODE] = TPRFromITPRTable;
//  3. #define the macro TPRFromITPRTableName to be the name of that global.
//  4. In each TESTPROC, declare a variable of type ITPR.
//  5. As cases in the TESTPROC are resolved, OR in the appropriate result. For
//     example, if the case is aborted, execute code similar to the following:
//          nITPR |= ITPR_ABORT;
//  6. The final code to be returned from the TESTPROC can be obtained with code
//     similar to the folowing:
//          return TPRFromITPR(nITPR);

#define ITPR_PASS   0x00
#define ITPR_SKIP   0x01
#define ITPR_ABORT  0x02
#define ITPR_FAIL   0x04

#define MAX_ITPR_CODE   4

typedef INT ITPR;

#define TPRFromITPRTable { \
    TPR_PASS, \
    TPR_SKIP, \
    TPR_ABORT, \
    TPR_ABORT \
    }
#define TPRFromITPR(code) \
    (((code) >= MAX_ITPR_CODE) ? TPR_FAIL : TPRFromITPRTableName[(code)])

#endif // HQATK_NOTUX

#ifndef HQATK_NOKATO
////////////////////////////////////////////////////////////////////////////////
// Kato helpers

enum KATOVERBOSITY
{
    LOG_NO_VERBOSITY    = 100,
    LOG_EXCEPTION       =   0,
    LOG_FAIL            =   2,
    LOG_ABORT           =   4,
    LOG_SKIP            =   6,
    LOG_NOT_IMPLEMENTED =   8,
    LOG_PASS            =  10,
    LOG_DETAIL          =  12,
    LOG_COMMENT         =  14
};

#endif // HQATK_NOKATO

////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
};
#endif // __cplusplus

#endif // __HQATK_H__
