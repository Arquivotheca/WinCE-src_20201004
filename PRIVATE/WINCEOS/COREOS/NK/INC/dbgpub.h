//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
