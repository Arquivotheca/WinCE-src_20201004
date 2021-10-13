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
// ----------------------------------------------------------------------------
//
// Use of this source code is subject to the terms of the Microsoft end-user
// license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
// If you did not accept the terms of the EULA, you are not authorized to use
// this source code. For a copy of the EULA, please see the LICENSE.RTF on your
// install media.
//
// ----------------------------------------------------------------------------
//
// Definitions and declarations for the WiFUtils class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_WiFUtils_
#define _DEFINED_WiFUtils_
#pragma once

#ifndef DEBUG
#ifndef NDEBUG
#define NDEBUG
#endif
#endif

#include <windows.h>
#include <inc/string.hxx>

// ----------------------------------------------------------------------------
//
// Macros:
// 
#undef  COUNTOF
#define COUNTOF(a) (sizeof(a) / sizeof((a)[0]))

// ----------------------------------------------------------------------------
//
// tstring: a generic Unicode or ASCII dynamic-string class:
//
namespace ce {
#ifndef CE_TSTRING_DEFINED
#define CE_TSTRING_DEFINED
#ifdef UNICODE
typedef ce::wstring tstring;
#else
typedef ce::string  tstring;
#endif
#endif
namespace qa {

// ----------------------------------------------------------------------------
//
// Wireless LAN library utility funtions.
//
struct WiFUtils
{
    // Formats the specified printf-style string and inserts it into the
    // specified message buffer:
    static HRESULT
    FmtMessage(
        OUT ce::tstring *pMessage,
        IN  const TCHAR *pFormat, ...);
    static HRESULT
    FmtMessageV(
        OUT ce::tstring *pMessage,
        IN  const TCHAR *pFormat,
        IN  va_list      VarArgs);

    // Formats the specified printf-style string and appends it to the end
    // of the specified message buffer:
    static HRESULT
    AddMessage(
        OUT ce::tstring *pMessage,
        IN  const TCHAR *pFormat, ...);
    static HRESULT
    AddMessageV(
        OUT ce::tstring *pMessage,
        IN  const TCHAR *pFormat,
        IN  va_list      VarArgs);

    // Logs the specified message without concern for message size, etc:
    // Also automatically splits the message into multiple lines and logs
    // each separately.
    static void
    LogMessageF(
        IN int          Severity,   // CmnPrint severity codes
        IN bool         AddOffsets, // true to prepend line-offsets
        IN const TCHAR *pFormat,
        ...);
    static void
    LogMessageV(
        IN int          Severity,   // CmnPrint severity codes
        IN bool         AddOffsets, // true to prepend line-offsets
        IN const TCHAR *pFormat,
        IN va_list      VarArgs);
    static void
    LogMessage(
        IN int                Severity,   // CmnPrint severity codes
        IN bool               AddOffsets, // true to prepend line-offsets
        IN const ce::tstring &Message);

    // Calculates the overflow-corrected difference between specified
    // "tick" counts:
    static long
    SubtractTickCounts(
        IN DWORD TickCount1,
        IN DWORD TickCount2)
    {
        return (long)((TickCount1 > TickCount2)
                   ?  (TickCount1 - TickCount2)
                   : ~(TickCount2 - TickCount1) + 1);
    }

    // Translates from Unicode to ASCII or vice-versa:
    // Generates an error message if the conversion fails and the
    // optional string-name is specified.
    // The input may be NULL or empty and, if the optional InChars
    // argument is supplied, needs no null-termination.
    // The output will ALWAYS be null-terminated and -padded.
    static HRESULT
    ConvertString(
      __out_ecount(OutChars) OUT char        *pOut,
                             IN  const char  *pIn,
                             IN  int          OutChars,
                             IN  const char *pStringName = NULL,
                             IN  int          InChars = -1,
                             IN  UINT         CodePage = CP_ACP);
    static HRESULT
    ConvertString(
      __out_ecount(OutChars) OUT WCHAR       *pOut,
                             IN  const char  *pIn,
                             IN  int          OutChars,
                             IN  const char  *pStringName = NULL,
                             IN  int          InChars = -1,
                             IN  UINT         CodePage = CP_ACP);
    static HRESULT
    ConvertString(
      __out_ecount(OutChars) OUT char        *pOut,
                             IN  const WCHAR *pIn,
                             IN  int          OutChars,
                             IN  const char  *pStringName = NULL,
                             IN  int          InChars = -1,
                             IN  UINT         CodePage = CP_ACP);
    static HRESULT
    ConvertString(
      __out_ecount(OutChars) OUT WCHAR       *pOut,
                             IN  const WCHAR *pIn,
                             IN  int          OutChars,
                             IN  const char  *pStringName = NULL,
                             IN  int          InChars = -1,
                             IN  UINT         CodePage = CP_ACP);
    static HRESULT
    ConvertString(
                             OUT ce::string  *pOut,
                             IN  const char  *pIn,
                             IN  const char  *pStringName = NULL,
                             IN  int          InChars = -1,
                             IN  UINT         CodePage = CP_ACP);
    static HRESULT
    ConvertString(
                             OUT ce::wstring *pOut,
                             IN  const char  *pIn,
                             IN  const char  *pStringName = NULL,
                             IN  int          InChars = -1,
                             IN  UINT         CodePage = CP_ACP);
    static HRESULT
    ConvertString(
                             OUT ce::string  *pOut,
                             IN  const WCHAR *pIn,
                             IN  const char  *pStringName = NULL,
                             IN  int          InChars = -1,
                             IN  UINT         CodePage = CP_ACP);
    static HRESULT
    ConvertString(
                             OUT ce::wstring *pOut,
                             IN  const WCHAR *pIn,
                             IN  const char  *pStringName = NULL,
                             IN  int          InChars = -1,
                             IN  UINT         CodePage = CP_ACP);

    // Loads an optional string value from the registry:
    static HRESULT
    ReadRegString(
        IN  HKEY         HKey,           // Open registry key or HKCU, HKLM, etc.
        IN  const TCHAR *pConfigKey,     // Name of registry key
        IN  const TCHAR *pValueKey,      // Name of registry value
        OUT ce::string  *pData,          // Input buffer
        IN  const TCHAR *pDefaultValue); // Default value or NULL
    static HRESULT
    ReadRegString(
        IN  HKEY         HKey,           // Open registry key or HKCU, HKLM, etc.
        IN  const TCHAR *pConfigKey,     // Name of registry key
        IN  const TCHAR *pValueKey,      // Name of registry value
        OUT ce::wstring *pData,          // Input buffer
        IN  const TCHAR *pDefaultValue); // DEfault value or NULL

    // Loads an optional integer value from the registry:
    static HRESULT
    ReadRegDword(
        IN  HKEY         HKey,           // Open registry key or HKCU, HKLM, etc.
        IN  const TCHAR *pConfigKey,     // Name of registry key
        IN  const TCHAR *pValueKey,      // Name of registry value
        OUT DWORD       *pData,          // Input buffer
        IN  DWORD        DefaultValue);  // Default value

    // Writes the specified string value into the registry:
    static HRESULT
    WriteRegString(
        IN  HKEY              HKey,       // Open registry key or HKCU, HKLM, etc.
        IN  const TCHAR      *pConfigKey, // Name of registry key
        IN  const TCHAR      *pValueKey,  // Name of registry value
        IN  const ce::string &data);      // Value to be written
    static HRESULT
    WriteRegString(
        IN  HKEY               HKey,      // Open registry key or HKCU, HKLM, etc.
        IN  const TCHAR       *pConfigKey,// Name of registry key
        IN  const TCHAR       *pValueKey, // Name of registry value
        IN  const ce::wstring &data);     // Value to be written

    // Writes the specified integer value into the registry:
    static HRESULT
    WriteRegDword(
        IN  HKEY         HKey,            // Open registry key or HKCU, HKLM, etc.
        IN  const TCHAR *pConfigKey,      // Name of registry key
        IN  const TCHAR *pValueKey,       // Name of registry value
        IN  DWORD        data);           // Value to be written
};

// ----------------------------------------------------------------------------
//
// Simplified message/debugging functions:
//
void LogError(IN const TCHAR *pFormat, ...);
void LogError(IN const ce::tstring &Message);
void LogWarn(IN const TCHAR *pFormat, ...);
void LogWarn(IN const ce::tstring &Message);
void LogDebug(IN const TCHAR *pFormat, ...);
void LogDebug(IN const ce::tstring &Message);
void LogAlways(IN const TCHAR *pFormat, ...);
void LogAlways(IN const ce::tstring &Message);

// ----------------------------------------------------------------------------
//
// Logs a status message.
// By default, this is the same as calling LogAlways.
// If SetStatusLogger is handed a function to call, status and error
// messages will also be passed to that function. This can be used, for
// example, to capture "important" messages to a GUI status-message area.
//
void LogStatus(IN const TCHAR *pFormat, ...);
void LogStatus(IN const ce::tstring &Message);


void
SetStatusLogger(
    IN void (*pLogFunc)(const TCHAR *pMessage));

void
ClearStatusLogger(
    void);

// ----------------------------------------------------------------------------
//
// Provides a simple mechanism for capturing error-message output.
// Once one of these objects is created all messages sent to LogError
// by the current thread will be stored in the specified buffer.
//
class ErrorLock
{
private:
    ce::tstring *m_pErrorBuffer;

public:
    ErrorLock(ce::tstring *pErrorBuffer)
        : m_pErrorBuffer(pErrorBuffer)
    {
        StartCapture();
    }
   ~ErrorLock(void)
    {
        StopCapture();
    }

    void
    StartCapture(void);
    void
    StopCapture(void);
};

// ----------------------------------------------------------------------------
//
// Win32ErrorText and HRESULTErrorText are simple classes and methods
// for translating Win32 or HRESULT error codes into text form for
// error messages:
//      LogError(TEXT("oops, eek!: %s"), Win32ErrorText(result));
//   -- or --
//      LogError(TEXT("oops, eek!: %s"), HRESULTErrorText(hr));
//
#ifndef Win32ErrorText
#define Win32ErrorText __Win32ErrorText()
class __Win32ErrorText
{
private:
    TCHAR m_ErrorText[96];
public:
  __Win32ErrorText(void) { }
    const TCHAR *operator() (IN DWORD ErrorCode);
};
#endif /* Win32ErrorText */

#ifndef HRESULTErrorText
#define HRESULTErrorText __HRESULTErrorText()
class __HRESULTErrorText
{
private:
    TCHAR m_ErrorText[96];
public:
  __HRESULTErrorText(void) { }
    const TCHAR *operator() (IN HRESULT ErrorCode);
};
#endif /* HRESULTErrorText */

// ----------------------------------------------------------------------------
//
// Safely copies a string into a fixed-length buffer.
// (Limits output size to MAX_PATH*100 or fewer characters.)
//
void
SafeCopy(
  __out_ecount(OutChars) OUT char        *pOut,
                         IN  const char  *pIn,
                         IN  int          OutChars);
void
SafeCopy(
  __out_ecount(OutChars) OUT WCHAR       *pOut,
                         IN  const WCHAR *pIn,
                         IN  int          OutChars);

// ----------------------------------------------------------------------------
//
// Safely appends a string to the end of a fixed-length buffer.
// (Limits output size to MAX_PATH*100 or fewer characters.)
//
void
SafeAppend(
  __out_ecount(OutChars) OUT char        *pOut,
                         IN  const char  *pIn,
                         IN  int          OutChars);
void
SafeAppend(
  __out_ecount(OutChars) OUT WCHAR       *pOut,
                         IN  const WCHAR *pIn,
                         IN  int          OutChars);

};
};
#endif /* _DEFINED_WiFUtils_ */
// ----------------------------------------------------------------------------
