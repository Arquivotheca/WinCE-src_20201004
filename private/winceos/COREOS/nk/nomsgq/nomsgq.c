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
 *              NK Kernel loader code
 *
 *
 * Module Name:
 *
 *              nomsgq.c
 *
 * Abstract:
 *
 *              This file implements the NK kernel message queue stubs
 *
 */

#include "kernel.h"

// MSGQRead - read from a message queue
BOOL MSGQRead (PEVENT lpe, LPVOID lpBuffer, DWORD cbSize, LPDWORD lpNumberOfBytesRead, DWORD dwTimeout, LPDWORD pdwFlags, PHANDLE phdTok)
{
    SetLastError (ERROR_NOT_SUPPORTED);
    return FALSE;
}

// MSGQWrite - write to a message queue
BOOL MSGQWrite (PEVENT lpe, LPCVOID lpBuffer, DWORD cbDataSize, DWORD dwTimeout, DWORD dwFlags)
{
    SetLastError (ERROR_NOT_SUPPORTED);
    return FALSE;
}

// MSGQGetInfo - get information from a message queue
BOOL MSGQGetInfo (PEVENT lpe, PMSGQUEUEINFO lpInfo)
{
    SetLastError (ERROR_NOT_SUPPORTED);
    return FALSE;
}

// MSGQClose - called when a message queue is closed
BOOL MSGQClose (PEVENT lpe)
{
    SetLastError (ERROR_NOT_SUPPORTED);
    return FALSE;
}

// MSGQPreClose - called when all handle to the message queue are closed (can still be locked)
BOOL MSGQPreClose (PEVENT lpe)
{
    SetLastError (ERROR_NOT_SUPPORTED);
    return FALSE;
}

// open a message queue
HANDLE NKOpenMsgQueueEx (PPROCESS pprc, HANDLE hMsgQ, HANDLE hDstPrc, PMSGQUEUEOPTIONS lpOptions)
{
    SetLastError (ERROR_NOT_SUPPORTED);
    return NULL;
}

// create a message queue
HANDLE NKCreateMsgQueue (LPCWSTR pQName, PMSGQUEUEOPTIONS lpOptions)
{
    SetLastError (ERROR_NOT_SUPPORTED);
    return NULL;
}

// InitMsgQueue - initialize kernel message queue support
BOOL InitMsgQueue(void)
{
    return FALSE;
}
