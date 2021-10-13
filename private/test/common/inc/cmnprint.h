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
// ----------------------------------------------------------------------------
//
// Definitions and declarations for the CmnPrint library.
//
// This library is designed to provide a generic logging mechanism for use
// in libraries. A library can just include this header file and log its
// messages like this:
//     #include <cmnprint.h>
//     ...
//     CmnPrint(PT_LOG, TEXT("Hello %"), TEXT("world!"));
//     CmnPrint(PT_FAIL, TEXT("EEK! EEK!"));
// 
// The determination of how the messages will be logged is defered until
// run-time. The application which links the library invokes a series of
// CmnPrint_RegisterPrintFunction calls (usually one for each verbosity
// level) to register the function to be called by CmnPrint:
//     CmnPrint_Initialize(gszMainProgName);
//     CmnPrint_RegisterPrintFunctionEx(PT_LOG,     NetLogDebug,   FALSE);
//     CmnPrint_RegisterPrintFunctionEx(PT_WARN1,   NetLogWarning, FALSE);
//     CmnPrint_RegisterPrintFunctionEx(PT_WARN2,   NetLogWarning, FALSE);
//     CmnPrint_RegisterPrintFunctionEx(PT_FAIL,    NetLogError,   FALSE);
//     CmnPrint_RegisterPrintFunctionEx(PT_VERBOSE, NetLogMessage, FALSE);
//
// This registration process tells CmnPrint how to log messages. If this
// isn't done, CmnPrint silently ignores logged messages.
// 
// CmnPrint supports component or library logging by associating library
// names with corresponding message-verbosity levels. Using this facility
// the level of verbosity for each library's messages can be controlled
// independantly.
// 
// For example the "JV" library can log its messages like this:
//     LibPrint("JV", PT_LOG, TEXT("Hi there! This is JV #%d"), JvId);
// 
// Then applications can specify whether they want to log these messages
// like this:
//     LibPrint_SetLogLevel("JV", LOG_PASS);   <= turns the message off
//     LibPrint_SetLogLevel("JV", LOG_DETAIL); <= turns the message on
//
// The libraries can optionally be configured using registry entries. Two
// new sub-directories have been added to HKCU\Pagasus. The first directory,
// HKCU\Pegasus\Libraries, contains DWORD values named the same as the
// libraries to be controlled. The second directory, HKCU\Pegasus\Programs,
// contains a list of program names each containing a list of libraries as
// described above. It also contains a sub-directory, HKCU\Pegasus\Programs\
// {progname}\Temporary, with library values translated from command-line
// arguments (see netmain):
//
//    HKCU\Pegasus\Libraries
//        BC=0  <= turns off all BC (backchannel library) messages
//        JV=15 <= turns on all JV messages
//    HKCU\Pegasus\Programs\MyProgram
//        BC=15 <= turns on all BC messages for MyProgram executable
//        JV=0  <= turns off all JV messages for MyProgram executable
//    HKCU\Pegasus\Programs\MyProgram\Temporary
//        BC=10 <= temporarily sets BC messages to LOG_PASS level
//        JV=10 <= temporarily sets JV messages to LOG_PASS level
//
// The three directories are listed in the order of load and, consequently,
// precedence. I.e., if a definition is in multiple directories, the last
// one loaded takes precedence.
//
// For example, the definitions above will set the BC level to 0 for all
// programs except "MyProgram". For that program, the second definition,
// BC=15, is loaded and takes precedence over the default. Then the third
// definition, BC=10, is loaded and overrides the level to BC=10.
//
// The Temporary directory will be cleared out each time MyProgram is started
// then populated with the translated command-line arguments (see netmain).
// As a consequence, any libraries inserted manually will be ignored.
//
// ----------------------------------------------------------------------------

#ifndef __CMN_PRINT_H__
#define __CMN_PRINT_H__
#pragma once

#include <windows.h>
#include <katoex.h>

#ifndef UNDER_CE
#include <tchar.h>
#endif

// Log verbosity levels and types of print functions:
//
#ifndef MAX_PRINT_TYPE
#define PT_LOG          0  // Equates to Kato LOG_DETAIL (12) or NetLogDebug
#define PT_WARN1        1  // Equates to Kato LOG_WARNING (3) or NetLogWarning
#define PT_WARN2        2  // Equates to Kato LOG_SKIP    (6) or NetLogWarning
#define PT_FAIL         3  // Equates to Kato LOG_FAIL    (2) or NetLogError
#define PT_VERBOSE      4  // Equates to Kato LOG_PASS   (10) or NetLogMessage
#define MAX_PRINT_TYPE  4
#define PT_ALL (ULONG)(-1) // For registering one function for all message types
#endif

// Library log-level registry keys:
//
#ifndef LPT_REG_DIRECTORY
#define LPT_REG_DIRECTORY  TEXT("Pegasus")   /* Top level under HKCU */
#define LPT_REG_EXEDIR     TEXT("Programs")  /* Dir under Pegasus */
#define LPT_REG_LIBDIR     TEXT("Libraries") /* Manual dir under Programs or Pegasus */
#define LPT_REG_TEMPDIR    TEXT("Temporary") /* Automated dir under Programs */
#endif

// Katoex.h does not define this:
//
#ifndef LOG_WARNING
#define LOG_WARNING  3
#endif

// Default library verbosity level:
//
#ifndef PT_DEF_VERBOSITY
#ifndef DEBUG
#define PT_DEF_VERBOSITY LOG_DETAIL
#else
#define PT_DEF_VERBOSITY LOG_PASS
#endif
#endif

// Maximum characters that CmnPrint can output on a single line.
// More than this will be truncated.
//
#ifndef MAX_PRINT_CHARS
#define MAX_PRINT_CHARS  KATO_MAX_STRING_LENGTH - 64
#endif

// Message-logging function prototypes:
//
typedef void (*LPF_PRINT)(LPCTSTR pText);          // For simple print functions like OutputDebugString
typedef void (*LPF_PRINTEX)(LPCTSTR pFormat, ...); // For more complex print functions like printf

// ----------------------------------------------------------------------------
//
// Initializes the CmnPrint library.
// Must be called before any other CmnPrint function.
// The program name is optional but recommended. Without it, the library-
// verbosity registry-lookups will get the program name from the command-
// line. While technically correct, this may not be the prefered, generic,
// name for the application.
//
EXTERN_C void
CmnPrint_Initialize(
    LPCTSTR pProgramName = NULL);

// ----------------------------------------------------------------------------
//
// Cleans up resources used by the CmnPrint library.
//
EXTERN_C void
CmnPrint_Cleanup(void);

// ----------------------------------------------------------------------------
//
// Registers the specified function for logging the given type of message(s).
// Returns TRUE if successful, FALSE otherwise.
// The special PT_ALL message-type can be used to set a function to be used
// for logging all message types.
// The fAddEOL flag indicates CmnPrint should add a newline to the end of
// each message.
//
EXTERN_C BOOL
CmnPrint_RegisterPrintFunction(
    DWORD     MsgType,
    LPF_PRINT pfPrint,         // For simple print functions like OutputDebugString
    BOOL      fAddEOL);

EXTERN_C BOOL
CmnPrint_RegisterPrintFunctionEx(
    DWORD       MsgType,
    LPF_PRINTEX pfPrint,       // For more complex print functions like printf
    BOOL        fAddEOL);

// ----------------------------------------------------------------------------
//
// Logs the specified message type.
//
EXTERN_C void
CmnPrint(
    DWORD   MsgType,
    LPCTSTR pFormat, ...);

EXTERN_C void
CmnPrintV(
    DWORD   MsgType,
    LPCTSTR pFormat,
    va_list pArgs);

// ----------------------------------------------------------------------------
//
// Gets or sets the message-verbosity level for the named library or, if
// the name is NULL, the default.
// The Set function returns the previous level.
// The SetDefault version has no effect if the level has already been set.
// Due to the non-linearity of the PT log-levels, these functions use the
// Kato LOG_EXCEPTION / LOG_COMMENT levels instead (see katoex.h).
//
EXTERN_C int
LibPrint_GetLogLevel(
    LPCSTR pLibName);

EXTERN_C int
LibPrint_SetLogLevel(
    LPCSTR pLibName,
    int    NewLevel);

EXTERN_C int
LibPrint_SetDefaultLevel(
    LPCSTR pLibName,
    int    DefaultLevel);

// ----------------------------------------------------------------------------
//
// Logs the specified message type.
// Unlike the normal CmpPrint function, this one looks up the specified
// library and ignores the message if the library's current verbosity
// level is below that of the specified message-type.
//
EXTERN_C void
LibPrint(
    LPCSTR  pLibName,
    DWORD   MsgType,
    LPCTSTR pFormat, ...);

EXTERN_C void
LibPrintV(
    LPCSTR  pLibName,
    DWORD   MsgType,
    LPCTSTR pFormat,
    va_list pArgs);

#endif /* __CMN_PRINT_H__ */
// ----------------------------------------------------------------------------
