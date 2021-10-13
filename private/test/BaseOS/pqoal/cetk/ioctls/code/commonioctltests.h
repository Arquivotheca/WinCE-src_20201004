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
 * commonIoctlTests.h
 */

/*
 * This file includes constants, defines and functions that are common to all 
 * IOCTL testing.
 */


////////////////////////////////////////////////////////////////////////////////
#ifndef __COMMON_IOCTLTESTS_H__
#define __COMMON_IOCTLTESTS_H__


/***************************  Constants and Defines ***************************/

enum paramChecks {INBOUND, OUTBOUND};
enum buffer {BUF_NULL, BUF_CORRECT, BUF_CORRECTSIZE_MINUS_ONE, BUF_CORRECTSIZE_PLUS_ONE};
enum bufferSize {SIZE_ZERO, SIZE_CORRECT, SIZE_BUF};


typedef struct IOCTL_PARAM_CHECK {
    DWORD dwInBuf;       // Input buffer - can be null, correctsize, 
                                      // correctsize-one, correctsize+one
    DWORD dwInBufSize;   // Input buffer size - can be zero, 
                                         // correct size, size of buffer
    DWORD dwOutBuf;      // Output buffer - can be null, correctsize,
                                       // correctsize-one, correctsize+one
    DWORD dwOutBufSize;  // Output buffer size - can be zero, 
                                          // correct size, size of buffer
    BOOL bBytesRet;      // Bytes returned pointer - can be (TRUE)a valid
                                        // DWORD pointer or (FALSE) null
    BOOL bOutput;        // Expected return value of the Ioctl function - 
                                        // can be TRUE or FALSE
    DWORD dwErrorCode;   // Error code set by the Ioctl function - 
                                        // can be ERROR_INVALID_PARAMETER, 
                                        // ERROR_INSUFFICIENT_BUFFER or nothing
    TCHAR szComment[200];// Comment - describes the parameters given to the 
                                        // Ioctl funciton
}ioctlParamCheck;

extern DWORD dwNumParamTests;
extern ioctlParamCheck ioctlInParamTests[];

/**********************************  Functions ********************************/

// This function verifies if the IOCTL is supported by the platform.
BOOL VerifyIoctlSupport(DWORD dwIoctlCode, VOID* pCorrectInBuf, 
                              DWORD dwCorrectInSize);

// This function retrives the required output size for the IOCTL.
// This works in case of IOCTLs that require inbound and outbound parameters and 
// for IOCTLs that require outbound parameters only, i.e, inbound are ignored.
// For IOCTLs that require inbound parameters only or for IOCTLs that require no 
// parameters, this function cannot be used.
BOOL GetOutputSize  (DWORD dwIoctlCode, VOID* pCorrectInBuf, 
                              DWORD dwCorrectInSize, DWORD *pdwCorrectOutSize);

// This function will test the Incorrect inbound and outbound parameters for the
// IOCTLs. All combinations of incorrect values are passed into the IOCTL 
// function and the return values are verified. Any incorrect input parameters 
// should not crash the system.
BOOL CheckIncorrectInOutParameters(DWORD dwIoctlCode, DWORD dwInOutParam, 
                                   VOID* pCorrectInBuf, DWORD dwCorrectInSize, DWORD dwCorrectOutSize );

// This function will test random input values for IOCTLs that don't require any
// input parameters. It verifies that any input values do not affect the outcome
// of the IOCTL function.
BOOL CheckIfInboundParametersIgnored(DWORD dwIoctlCode,  DWORD dwCorrectOutSize , BOOL bOutputIsConstant);

// This function verifies all the different alignments of the input buffer. 
// When the input buffer is DWORD misaligned the IOCTL should return FALSE
// without throwing an exception or crashing.
// The function also checks for input buffer overflows on each call.
BOOL CheckInboundBufferAlignment(DWORD dwIoctlCode, VOID* pCorrectInBuf, 
                                     DWORD dwCorrectInSize,  DWORD dwCorrectOutSize );

// This function verifies all the different alignments of the output buffer. 
// When the output buffer is DWORD misaligned the IOCTL should still work 
// correctly returning the correct data. It should not throw an exception/crash.
// The function also checks for output buffer overflows on each call.
BOOL CheckOutboundBufferAlignment(DWORD dwIoctlCode, VOID* pCorrectInBuf, 
                                     DWORD dwCorrectInSize,  DWORD dwCorrectOutSize , BOOL bOutputIsConstant);


#endif // __COMMON_IOCTLTESTS_H__

