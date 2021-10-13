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

#define IF_LOG_RETURN(cond, retVal, msg, ...) \
    do { if (cond) {                          \
        Log(msg, __VA_ARGS__);                \
        return retVal; } } while (0)

#define IF_LOG_RETURN_ABORT(cond, msg, ...) \
    IF_LOG_RETURN(cond, TPR_ABORT, msg, __VA_ARGS__)

#define IF_LOG_RETURN_FAIL(cond, msg, ...) \
    IF_LOG_RETURN(cond, TPR_FAIL, msg, __VA_ARGS__)

#define IF_FAIL_LOG_RETURN(hr, retVal, msg, ...) \
    IF_LOG_RETURN(FAILED(hr), retVal, msg, __VA_ARGS__)

#define IF_FAIL_LOG_RETURN_ABORT(hr, msg, ...) \
    IF_LOG_RETURN_ABORT(FAILED(hr), msg, __VA_ARGS__)