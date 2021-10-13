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
 File:      dbgpub.h

 Purpose:   provide DBG subsystem prototypes accessible to public (user).

 */

#ifndef __DBGPUB_H__
#define __DBGPUB_H__
/*
 * DBG_UNotify sends a message to the debugger in the event of an exception.
 * It will need to be able to get the name, address and size of the image in
 * which the exception occurred via a call to DBGUpdateSymbols() and set the
 * PDBGARGS field of the pDbgHead. It should be called from a scheduled
 * thread, using information saved at exception time.
 */
BOOL
DBG_UNotify(
    /* IN */ UINT ExceptionCode,
    /* IN */ UINT ExceptionInfo
    );

#endif // __DBGPUB_H__
