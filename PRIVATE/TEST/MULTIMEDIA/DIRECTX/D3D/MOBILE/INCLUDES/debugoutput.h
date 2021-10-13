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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#pragma once

#include <kato.h>

//
// DO_ERROR: This macro is used to enable easy printing of the filename and line
// when printing out a debug message. Call DebugOut with DO_ERROR as first entry
// to print out the file and line number after printing the message.
//
#define DO_ERROR __LINE__,_T(__FILE__)

//
// DebugOut
//
//   Printf-like debug logging. Uses either OutputDebugString or a Kato logger
//   object. By default, OutputDebugString is called. If SetKato is called with
//   a pointer to a Kato logger, DebugOut will use that Kato logger object.
//
// Parameters:
//
//   szFormat        Formatting string (as in printf).
//   ...             Additional arguments.
//
// Return value:
//
//   void  
//
void DebugOut(LPCTSTR szFormat, ...);

//
// DebugOut
//
//   Printf-like debug logging. Uses either OutputDebugString or a Kato logger
//   object. By default, OutputDebugString is called. If SetKato is called with
//   a pointer to a Kato logger, DebugOut will use that Kato logger object.
//   This version will first print out the debug string given in szFormat, then
//   print out the file name and line number.
//   (Can be used with macro DO_ERROR: DebugOut(DO_ERROR, L"DebugMessage");
//
// Parameters:
//
//   iLine           Line number (such as provided with __LINE__ macro)
//   szFile          Filename (such as provided with __FILE__ macro)
//   szFormat        Formatting string (as in printf).
//   ...             Additional arguments.
//
// Return value:
//
//   void  
//
void DebugOut(int iLine, LPCTSTR szFile, LPCTSTR szFormat, ...);

//
// SetKato
//
//   Use this to cause DebugLog to output to a Kato logger instead of
//   OutputDebugString. Kato objects provide integration with Tux parameters
//   for logging (such as outputting to debugger, to a file, appending to a 
//   file, etc.). Use SetKato as soon as the Kato logger has been created.
//   SetKato can also be used to reset DebugOut to use OutputDebugString:
//   pass NULL to do this.
//
// Parameters:
//
//   pKato           Pointer to a Kato logger. NULL to reset to default logging.
//
// Return value:
//
//   void  
//
void SetKato(CKato * pKato);



