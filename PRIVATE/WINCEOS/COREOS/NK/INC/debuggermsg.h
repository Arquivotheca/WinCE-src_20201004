//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
