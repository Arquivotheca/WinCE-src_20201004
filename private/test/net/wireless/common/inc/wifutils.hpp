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
// Use of this source code is subject to the terms of the Microsoft end-user
// license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
// If you did not accept the terms of the EULA, you are not authorized to use
// this source code. For a copy of the EULA, please see the LICENSE.RTF on your
// install media.
//
// ----------------------------------------------------------------------------
//
// Wireless LAN library utility funtions.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_WiFUtils_
#define _DEFINED_WiFUtils_
#pragma once

#include <strconv.h>
#include <tchar.h>

#ifdef UNDER_CE
#include <bldver.h>
#else
#include <inc/bldver.h>
#endif

// Determine whether the standard NetLog logging functions work properly:
//
#if (CE_MAJOR_VER >= 7)
#define NETLOG_WORKS_PROPERLY 1
#endif

namespace litexml
{
    class XmlDocument_t;
    class XmlBaseElement_t;
    class XmlElement_t;
    class XmlMetaElement_t;
};

namespace ce {
namespace qa {

namespace WiFUtils {

    // Provides a generic mechanism for translating between enumerations
    // and string values:
    //
    struct EnumMap
    {
        DWORD        code;
        const TCHAR *name;
        const TCHAR *desc;
    };

    #define EnumMapEntry(_code,_desc) { DWORD(_code), TEXT(#_code), (_desc) }

    // Uses the specified EnumMap to translate the specified enum value
    // to its text equivalent.
    // Returns the specified "unknown" string if there is no match.
    //
    const TCHAR *
  __EnumMapCode2Name(
        const EnumMap Map[],
        size_t        MapSize,
        DWORD         Code,
        const TCHAR  *pUnknown);

    template <class T> const TCHAR *
    EnumMapCode2Name(
        const EnumMap Map[],
        int          MapSize,
        T            Code,
        const TCHAR *pUnknown)
    {
        return __EnumMapCode2Name(Map, MapSize, DWORD(Code), pUnknown);
    }

    // Uses the specified EnumMap to translate the specified enum value
    // to a description string.
    // Returns the specified "unknown" string if there is no match.
    //
    const TCHAR *
  __EnumMapCode2Desc(
        const EnumMap Map[],
        size_t        MapSize,
        DWORD         Code,
        const TCHAR  *pUnknown);

    template <class T> const TCHAR *
    EnumMapCode2Desc(
        const EnumMap Map[],
        int          MapSize,
        T            Code,
        const TCHAR *pUnknown)
    {
        return __EnumMapCode2Desc(Map, MapSize, DWORD(Code), pUnknown);
    }

    // Uses the specified EnumMap to translate the specified text name to
    // the corresponding enum value.
    // Returns the specified "unknown" value if there is no match.
    //
    DWORD
  __EnumMapName2Code(
        const EnumMap Map[],
        size_t        MapSize,
        const TCHAR  *pName,
        DWORD         Unknown);

    template <class T> T
    EnumMapName2Code(
        const EnumMap Map[],
        int          MapSize,
        const TCHAR *pName,
        T            Unknown)
    {
        return T(__EnumMapName2Code(Map, MapSize, pName, DWORD(Unknown)));
    }

    // Uses the specified EnumMap to translate the specified description
    // string to its corresponding enum value.
    // Looks up the string in the "name" column first then the "desc" column.
    // Returns the specified "unknown" value if there is no match.
    //
    DWORD
  __EnumMapDesc2Code(
        const EnumMap Map[],
        size_t        MapSize,
        const TCHAR  *pDesc,
        DWORD         Unknown);

    template <class T> T
    EnumMapDesc2Code(
        const EnumMap Map[],
        int          MapSize,
        const TCHAR *pDesc,
        T            Unknown)
    {
        return T(__EnumMapDesc2Code(Map, MapSize, pDesc, DWORD(Unknown)));
    }

  // String formatting:

    // Formats the specified printf-style string and inserts it into the
    // specified message buffer:
    //
    HRESULT
    FmtMessage(
        ce::tstring *pMessage,
        const TCHAR *pFormat, ...);
    HRESULT
    FmtMessageV(
        ce::tstring *pMessage,
        const TCHAR *pFormat,
        va_list      VarArgs);

    // Formats the specified printf-style string and appends it to the end
    // of the specified message buffer:
    //
    HRESULT
    AddMessage(
        ce::tstring *pMessage,
        const TCHAR *pFormat, ...);
    HRESULT
    AddMessageV(
        ce::tstring *pMessage,
        const TCHAR *pFormat,
        va_list      VarArgs);

  // Logging:

    // Logs the specified message without concern for message size, etc:
    // Also automatically splits the message into multiple lines and logs
    // each separately.
    //
    void
    LogMessageF(
        int          Severity,   // CmnPrint severity codes
        bool         AddOffsets, // true to prepend line-offsets
        const TCHAR *pFormat,
        ...);
    void
    LogMessageV(
        int          Severity,   // CmnPrint severity codes
        bool         AddOffsets, // true to prepend line-offsets
        const TCHAR *pFormat,
        va_list      VarArgs);
    void
    LogMessage(
        int                Severity,   // CmnPrint severity codes
        bool               AddOffsets, // true to prepend line-offsets
        const ce::tstring &Message);

  // Unicode/ASCII conversion:

    // Translates from Unicode to ASCII or vice-versa:
    // Generates an error message if the conversion fails and the
    // optional string-name is specified.
    // The input may be NULL or empty and, if the optional InChars
    // argument is supplied, needs no null-termination.
    // The output will ALWAYS be null-terminated and -padded.
    //
    HRESULT
    ConvertString(
      __out_ecount(OutChars) char        *pOut,
                             const char  *pIn,
                             int          OutChars,
                             const char  *pStringName = NULL,
                             int          InChars     = -1,
                             UINT         CodePage    = CP_ACP);
    HRESULT
    ConvertString(
      __out_ecount(OutChars) WCHAR       *pOut,
                             const char  *pIn,
                             int          OutChars,
                             const char  *pStringName = NULL,
                             int          InChars     = -1,
                             UINT         CodePage    = CP_ACP);
    HRESULT
    ConvertString(
      __out_ecount(OutChars) char        *pOut,
                             const WCHAR *pIn,
                             int          OutChars,
                             const char  *pStringName = NULL,
                             int          InChars     = -1,
                             UINT         CodePage    = CP_ACP);
    HRESULT
    ConvertString(
      __out_ecount(OutChars) WCHAR       *pOut,
                             const WCHAR *pIn,
                             int          OutChars,
                             const char  *pStringName = NULL,
                             int          InChars     = -1,
                             UINT         CodePage    = CP_ACP);
    HRESULT
    ConvertString(
                             ce::string  *pOut,
                             const char  *pIn,
                             const char  *pStringName = NULL,
                             int          InChars     = -1,
                             UINT         CodePage    = CP_ACP);
    HRESULT
    ConvertString(
                             ce::wstring *pOut,
                             const char  *pIn,
                             const char  *pStringName = NULL,
                             int          InChars     = -1,
                             UINT         CodePage    = CP_ACP);
    HRESULT
    ConvertString(
                             ce::string  *pOut,
                             const WCHAR *pIn,
                             const char  *pStringName = NULL,
                             int          InChars     = -1,
                             UINT         CodePage    = CP_ACP);
    HRESULT
    ConvertString(
                             ce::wstring *pOut,
                             const WCHAR *pIn,
                             const char  *pStringName = NULL,
                             int          InChars     = -1,
                             UINT         CodePage    = CP_ACP);

  // Registry:

    // Loads an optional string value from the registry:
    //
    HRESULT
    ReadRegString(
        HKEY         HKey,           // Open registry key or HKCU, HKLM, etc.
        const TCHAR *pConfigKey,     // Name of registry key
        const TCHAR *pValueKey,      // Name of registry value
        ce::string  *pData,          // Input buffer
        const TCHAR *pDefaultValue); // Default value or NULL
    HRESULT
    ReadRegString(
        HKEY         HKey,           // Open registry key or HKCU, HKLM, etc.
        const TCHAR *pConfigKey,     // Name of registry key
        const TCHAR *pValueKey,      // Name of registry value
        ce::wstring *pData,          // Input buffer
        const TCHAR *pDefaultValue); // DEfault value or NULL

    // Loads an optional integer value from the registry:
    //
    HRESULT
    ReadRegDword(
        HKEY         HKey,           // Open registry key or HKCU, HKLM, etc.
        const TCHAR *pConfigKey,     // Name of registry key
        const TCHAR *pValueKey,      // Name of registry value
        DWORD       *pData,          // Input buffer
        DWORD        DefaultValue);  // Default value

    // Writes the specified string value into the registry:
    //
    HRESULT
    WriteRegString(
        HKEY              HKey,       // Open registry key or HKCU, HKLM, etc.
        const TCHAR      *pConfigKey, // Name of registry key
        const TCHAR      *pValueKey,  // Name of registry value
        const ce::string &data);      // Value to be written
    HRESULT
    WriteRegString(
        HKEY               HKey,      // Open registry key or HKCU, HKLM, etc.
        const TCHAR       *pConfigKey,// Name of registry key
        const TCHAR       *pValueKey, // Name of registry value
        const ce::wstring &data);     // Value to be written

    // Writes the specified integer value into the registry:
    //
    HRESULT
    WriteRegDword(
        HKEY         HKey,            // Open registry key or HKCU, HKLM, etc.
        const TCHAR *pConfigKey,      // Name of registry key
        const TCHAR *pValueKey,       // Name of registry value
        DWORD        data);           // Value to be written

    // Deletes the specified Registry Key:
    //
    HRESULT
    DeleteRegKey(
        HKEY         HKey,                 // Open registry key or HKCU, HKLM, etc.
        const TCHAR *pKey,                 // Name of registry key
        const bool   bEnableLogging=true); // Flag to LogError

    // Deletes the specified Registry Value:
    //
    HRESULT
    DeleteRegValue(
        HKEY         HKey,                 // Open registry key or HKCU, HKLM, etc.
        const TCHAR *pSubKey,              // Name of registry key
        const TCHAR *pValueName,           // Name of the registry value
        const bool   bEnableLogging=true); // Flag to LogError

  // System configuraton:

    // Sets the Display Timeout
    // Setting Timeout to 60 Decimal will correspondingly display as "1 Minute" in the UI
    // Setting Timeout to 0xFFFFFFFF will display as "Never" in the UI
    //
    HRESULT
    SetDisplayTimeout(
        DWORD Timeout);

    // Disables Customer Feedback page from being called
    // The Function returns S_OK either if the key was deleted
    // or if the key doesnt exist
    //
    HRESULT
    DisableCustomerFeedback();

    // Disables ItgProxy set in the Internet Connections Settings
    // The Function returns S_OK either if the key was deleted
    // or if the key doesnt exist
    //
    HRESULT
    DisableItgProxy();

    // Disables Confirm Install Prompt
    // The Function returns S_OK either if the key was deleted
    // or if the key doesnt exist
    //
    HRESULT
    DisableConfirmInstallPrompt();

    // Enables Confirm Install Prompt
    // The Function returns S_OK either if the key was added
    //
    HRESULT
    EnableConfirmInstallPrompt();

  // Time:

    // Calculates the difference between specified "tick" counts:
    // Debug builds start out with a negative (extremely high DWORD) tick
    // count. This calculates a  run-time based on the assumption that the
    // clock cannot run backwards.
    //
    static inline long
    SubtractTickCounts(
        DWORD CurrentTicks,
        DWORD EarlierTicks)
    {
        return (long)((CurrentTicks > EarlierTicks)
                   ?  (CurrentTicks - EarlierTicks)
                   : ~(EarlierTicks - CurrentTicks) + 1);
    }

    // Gets the current system time in milliseconds since Jan 1, 1601:
    //
    _int64
    GetWallClockTime(void);

    // Inserts a time-duration into the specified buffer:
    //
    void
    FormatRunTime(
                              DWORD   StartTicks,
                              DWORD   EndTicks,
      __out_ecount(BuffChars) TCHAR *pBuffer,
                              int     BuffChars);

  // Miscellaneous:

    // Determines whether the current image is Windows Mobile:
    // Returns true for WM, false for Embedded or NT.
    //
    bool
    IsWindowsMobile(void);
    
    // Converts the specified token to an integer:
    // Returns the default value if the token is empty or invalid.
    //
    int
    GetIntToken(
        TCHAR *pToken,  // modified by TrimToken
        int DefaultValue);


    // Trims spaces from the front and back of the specified token:
    //
    TCHAR *
    TrimToken(
        TCHAR *pToken);

    // Finds the size of the input string (in characters):
    // Generates an error message if the conversion fails and the
    // optional string-name is specified.
    //
    HRESULT
    StringLen(
        const TCHAR *pIn,
        const CHAR  *pStringName,
        size_t      *psize);

    // Runs the specified command and waits for it to finish:
    // Does not wait for command to finish if wait-time is zero.
    // If the optional exit-code pointer is supplied, fills it in with
    // whatever the program returns.
    // If the optional command-output buffer is supplied, captures all
    // output from the program into the buffer.
    //
    DWORD
    RunCommand(
        const TCHAR *pProgram,
        const TCHAR *pCommand,
        DWORD        WaitTimeMS,
        DWORD       *pExitCode  = NULL,
        ce::tstring *pCmdOutput = NULL);

  // XML utility functions:

    // Creates an XML document with, if necessary, a default Meta element:
    //
    HRESULT
    CreateXmlDocument(              // Adds default meta-tag
        litexml::XmlDocument_t **ppDoc);
    HRESULT
    CreateXmlDocument(              // Adds default meta-tag
        litexml::XmlElement_t   *pRoot,
        litexml::XmlDocument_t **ppDoc);
    HRESULT
    CreateXmlDocument(              // Adds specified meta-tag (if any)
        litexml::XmlElement_t     *pRoot,
        litexml::XmlMetaElement_t *pMeta,
        litexml::XmlDocument_t   **ppDoc);

    // Pretty-prints the specified XML document, element or text into the
    // specified output buffer:
    //
    HRESULT
    FormatXmlDocument(
        const litexml::XmlDocument_t &Doc,
        ce::wstring                  *pOut,
        const WCHAR                  *pLinePrefix = NULL);
    HRESULT
    FormatXmlElement(
        const litexml::XmlElement_t &Elem,
        ce::wstring                 *pOut,
        const WCHAR                 *pLinePrefix = NULL);
    HRESULT
    FormatXmlText(
        const ce::wstring &XmlText,
        ce::wstring       *pOut,
        const WCHAR       *pLinePrefix = NULL);

    // Creates the specified XML element:
    //
    HRESULT
    CreateXmlElement(
        const WCHAR            *pName,
        litexml::XmlElement_t **ppElem);

    // Creates the specified element, initializes its value and adds
    // it to the root:
    //
    HRESULT
    AddXmlElement(
        litexml::XmlElement_t  *pRoot,
        const WCHAR            *pName,
        const ce::wstring      &Value,
        litexml::XmlElement_t **ppElem = NULL);
    HRESULT
    AddXmlBoolElement(
        litexml::XmlElement_t  *pRoot,
        const WCHAR            *pName,
        bool                    Value,
        litexml::XmlElement_t **ppElem = NULL);
    HRESULT
    AddXmlHexElement(
        litexml::XmlElement_t  *pRoot,
        const WCHAR            *pName,
        const BYTE             *pValue,
        ULONG                   Length,
        litexml::XmlElement_t **ppElem = NULL);
    HRESULT
    AddXmlIPv4AddressElement(
        litexml::XmlElement_t  *pRoot,
        const WCHAR            *pName,
        UINT32                  Value,
        litexml::XmlElement_t **ppElem = NULL);
    HRESULT
    AddXmlLongElement(
        litexml::XmlElement_t  *pRoot,
        const WCHAR            *pName,
        long                    Value,
        litexml::XmlElement_t **ppElem = NULL);
    HRESULT
    AddXmlULongElement(
        litexml::XmlElement_t  *pRoot,
        const WCHAR            *pName,
        ULONG                   Value,
        litexml::XmlElement_t **ppElem = NULL);
    HRESULT
    AddXmlStringElement(
        litexml::XmlElement_t  *pRoot,
        const WCHAR            *pName,
        const WCHAR            *pValue,
        litexml::XmlElement_t **ppElem = NULL);

    // Adds the specified attribute to the element and initializes its value:
    //
    HRESULT
    AddXmlBoolAttribute(
        litexml::XmlElement_t *pElem,
        const WCHAR           *pName,
        bool                   Value);
    HRESULT
    AddXmlIPv4AddressAttribute(
        litexml::XmlElement_t *pElem,
        const WCHAR           *pName,
        UINT32                 Value);
    HRESULT
    AddXmlLongAttribute(
        litexml::XmlElement_t *pElem,
        const WCHAR           *pName,
        long                   Value);
    HRESULT
    AddXmlULongAttribute(
        litexml::XmlElement_t *pElem,
        const WCHAR           *pName,
        ULONG                  Value);
    HRESULT
    AddXmlStringAttribute(
        litexml::XmlElement_t *pElem,
        const WCHAR           *pName,
        const WCHAR           *pValue);

    // Finds the specified element in the root's children and decodes
    // its value:
    // Returns E_NOTIMPL if the named element cannot be found.
    //
    const litexml::XmlBaseElement_t *
    GetXmlElement(
        const litexml::XmlElement_t  &Root,
        const WCHAR                  *pName,
        const litexml::XmlElement_t **ppElem = NULL);
    HRESULT
    GetXmlBoolElement(
        const litexml::XmlElement_t  &Root,
        const WCHAR                  *pName,
        bool                         *pValue,
        const litexml::XmlElement_t **ppElem = NULL);
    HRESULT
    GetXmlHexElement(
        const litexml::XmlElement_t  &Root,
        const WCHAR                  *pName,
        ce::string                   *pValue,
        const litexml::XmlElement_t **ppElem = NULL);
    HRESULT
    GetXmlIPv4AddressElement(
        const litexml::XmlElement_t  &Root,
        const WCHAR                  *pName,
        UINT32                       *pValue,
        const litexml::XmlElement_t **ppElem = NULL);
    HRESULT
    GetXmlLongElement(
        const litexml::XmlElement_t  &Root,
        const WCHAR                  *pName,
        long                         *pValue,
        long                          Minimum = LONG_MIN,
        long                          Maximum = LONG_MAX,
        const litexml::XmlElement_t **ppElem  = NULL);
    HRESULT
    GetXmlULongElement(
        const litexml::XmlElement_t  &Root,
        const WCHAR                  *pName,
        ULONG                        *pValue,
        ULONG                         Minimum = 0,
        ULONG                         Maximum = ULONG_MAX,
        const litexml::XmlElement_t **ppElem  = NULL);
    HRESULT
    GetXmlStringElement(
        const litexml::XmlElement_t  &Root,
        const WCHAR                  *pName,
        ce::wstring                  *pValue,
        const litexml::XmlElement_t **ppElem = NULL);

    // Finds the specified attribute in the element and decodes its value:
    // Returns E_NOTIMPL if the named attribute cannot be found.
    //
    HRESULT
    GetXmlBoolAttribute(
        const litexml::XmlBaseElement_t &Elem,
        const WCHAR                     *pName,
        bool                            *pValue);
    HRESULT
    GetXmlIPv4AddressAttribute(
        const litexml::XmlBaseElement_t &Elem,
        const WCHAR                     *pName,
        UINT32                          *pValue);
    HRESULT
    GetXmlLongAttribute(
        const litexml::XmlBaseElement_t &Elem,
        const WCHAR                     *pName,
        long                            *pValue,
        long                             Minimum = LONG_MIN,
        long                             Maximum = LONG_MAX);
    HRESULT
    GetXmlULongAttribute(
        const litexml::XmlBaseElement_t &Elem,
        const WCHAR                     *pName,
        ULONG                           *pValue,
        ULONG                           Minimum = 0,
        ULONG                           Maximum = ULONG_MAX);
    HRESULT
    GetXmlStringAttribute(
        const litexml::XmlBaseElement_t &Elem,
        const WCHAR                     *pName,
        ce::wstring                     *pValue);

};

// ----------------------------------------------------------------------------
//
// Simplified message/debugging functions:
//
void LogError(const TCHAR *pFormat, ...);
void LogError(const ce::tstring &Message);
void LogWarn(const TCHAR *pFormat, ...);
void LogWarn(const ce::tstring &Message);
void LogDebug(const TCHAR *pFormat, ...);
void LogDebug(const ce::tstring &Message);
void LogAlways(const TCHAR *pFormat, ...);
void LogAlways(const ce::tstring &Message);

// ----------------------------------------------------------------------------
//
// Logs a status message.
// By default, this is the same as calling LogAlways.
// If SetStatusLogger is handed a function to call, status and error
// messages will also be passed to that function. This can be used, for
// example, to capture "important" messages to a GUI status-message area.
//
void LogStatus(const TCHAR *pFormat, ...);
void LogStatus(const ce::tstring &Message);


void
SetStatusLogger(
    void (*pLogFunc)(const TCHAR *pMessage));

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
    const TCHAR *operator() (DWORD ErrorCode);
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
    const TCHAR *operator() (HRESULT ErrorCode);
};
#endif /* HRESULTErrorText */

// ----------------------------------------------------------------------------
//
// Safely copies a string into a fixed-length buffer.
// (Limits output size to MAX_PATH*100 or fewer characters.)
//
void
SafeCopy(
  __out_ecount(OutChars) char        *pOut,
                         const char  *pIn,
                         int          OutChars);
void
SafeCopy(
  __out_ecount(OutChars) WCHAR       *pOut,
                         const WCHAR *pIn,
                         int          OutChars);

// ----------------------------------------------------------------------------
//
// Safely appends a string to the end of a fixed-length buffer.
// (Limits output size to MAX_PATH*100 or fewer characters.)
//
void
SafeAppend(
  __out_ecount(OutChars) char        *pOut,
                         const char  *pIn,
                         int          OutChars);
void
SafeAppend(
  __out_ecount(OutChars) WCHAR       *pOut,
                         const WCHAR *pIn,
                         int          OutChars);

};
};
#endif /* _DEFINED_WiFUtils_ */
// ----------------------------------------------------------------------------
