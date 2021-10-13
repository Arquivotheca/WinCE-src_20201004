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

Module Name:

    DebuggerMsg.h

Module Description:

    Header file that defines debugger macros for kd, hd, osaxs to send messages
    to the serial port.

    The module that includes this should define DEBUGGERPRINTF to a 
    function that has the following arguments DEBUGGERPRINTF(LPWSTR, ...).

--*/
#pragma once
#ifndef _DEBUGGERMSG_H
#define _DEBUGGERMSG_H

#ifdef DEBUG
#ifdef SHIP_BUILD
#undef SHIP_BUILD
#endif
#endif

#ifdef SHIP_BUILD
// Zero messages

#define DEBUGGERMSG(cond, printf_exp)
#define DBGRETAILMSG(cond, printf_exp)

#else//SHIP_BUILD

#ifdef DEBUG
#define DEBUGGERMSG(cond, printf_exp) ((void)((cond)?(DEBUGGERPRINTF printf_exp), 1:0))
#else//DEBUG
#define DEBUGGERMSG(cond, printf_exp)
#endif//DEBUG

#define DBGRETAILMSG(cond, printf_exp) ((void)((cond)?(DEBUGGERPRINTF printf_exp), 1:0))

#endif//SHIP_BUILD

#endif
