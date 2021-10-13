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
// Definitions and declarations for the CmdArg_t classes.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_CmdArg_t_
#define _DEFINED_CmdArg_t_
#pragma once

#include <ConfigArg_t.hpp>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// CmdArg: Base for a series of command-line application configuration
// argument classes.
//
class CmdArg_t
{
private:

    // Derived class's configuration object:
    ConfigArg_t *m_pConfig;

    // Command-line argument name:
    ce::tstring m_ArgName;

    // Command-line usage (optional):
    ce::tstring m_Usage;
    bool        m_fUsageSupplied;

public:

    // Constructor/Destructor:
    CmdArg_t(
        IN ConfigArg_t *pConfig,
        IN const TCHAR *pArgName,
        IN const TCHAR *pUsage = NULL); // optional - uses description if not set
    virtual
   ~CmdArg_t(void);

    // Copy/Assignment:
    CmdArg_t(const CmdArg_t &rhs);
    CmdArg_t &operator = (const CmdArg_t &rhs);

    // Increments or decrements the specified object's reference count:
    // Attach returns false if the object has been deleted.
    // Detach deletes when the reference count reaches zero.
    static bool
    Attach(CmdArg_t *pArg);
    static void
    Detach(CmdArg_t *pArg);
    
  // Accessors:

    const TCHAR *
    GetKeyName(void) const
    {
        return m_pConfig->GetKeyName();
    }
    const TCHAR *
    GetArgName(void) const
    {
        return m_ArgName;
    }
    const TCHAR *
    GetDescription(void) const
    {
        return m_pConfig->GetDescription();
    }
    const TCHAR *
    GetUsage(void) const
    {
        if (m_fUsageSupplied)
             return m_Usage;
        else return GetDescription();
    }

    // Formats the command-argument usage into the specified output buffer:
    virtual DWORD
    FormatUsage(
      __out_ecount(*pBufferChars) OUT TCHAR *pBuffer,
                                  OUT DWORD *pBufferChars) const = 0;

    // Translates the configuration value to or from text form:
    // The default implementations just pass through to the derived class's
    // configuration object and, if necessary, issue an error message.
    virtual DWORD
    ToString(
      __out_ecount(*pBufferChars) OUT TCHAR *pBuffer,
                                  OUT DWORD *pBufferChars) const;
    virtual DWORD
    FromString(
        IN const TCHAR *pArgValue);

    // Reads, writes or deletes the value from the specified registry:
    // The default implementations just pass through to the derived class's
    // configuration object and, if necessary, issue an error message.
    virtual DWORD
    ReadRegistry(
        IN HKEY          ConfigKey,   // Opened registry sub-key
        IN const TCHAR *pConfigName); // Registry sub-key name
    virtual DWORD
    WriteRegistry(
        IN HKEY          ConfigKey,
        IN const TCHAR *pConfigName) const;
    virtual DWORD
    DeleteRegistry(
        IN HKEY          ConfigKey,
        IN const TCHAR *pConfigName) const;

};

// ----------------------------------------------------------------------------
//
// CmdArgFlag: Specializes CmdArg to represent a command-line
// configuration-flag argument.
//
class CmdArgFlag_t : public CmdArg_t
{
private:

    // Configuration object:
    ConfigArgBool_t m_Config;

public:

    // Constructor/Destructor:
    CmdArgFlag_t(
        IN const TCHAR *pKeyName,
        IN const TCHAR *pArgName,
        IN const TCHAR *pDescription,
        IN const TCHAR *pUsage = NULL);
  __override virtual
   ~CmdArgFlag_t(void);

    // Copy/Assignment:
    CmdArgFlag_t(const CmdArgFlag_t &rhs);
    CmdArgFlag_t &operator = (const CmdArgFlag_t &rhs);

  // Accessors:

    ConfigArgBool_t &
    GetConfig(void) { return m_Config; }
    const ConfigArgBool_t &
    GetConfig(void) const { return m_Config; }
    
    operator
    void *(void) const
    {
        return m_Config.GetValue()? (void *)this : NULL;
    }

    bool
    GetValue(void) const
    {
        return m_Config.GetValue();
    }

    // Formats the command-argument usage into the specified output buffer:
  __override virtual DWORD
    FormatUsage(
      __out_ecount(*pBufferChars) OUT TCHAR *pBuffer,
                                  OUT DWORD *pBufferChars) const;
    
    // Translates the configuration value from text form:
  __override virtual DWORD
    FromString(
        IN const TCHAR *pArgValue);
    
};

// ----------------------------------------------------------------------------
//
// CmdArgBool: Specializes CmdArg to represent a boolean command-line
// configuration argument.
//
class CmdArgBool_t : public CmdArg_t
{
private:

    // Configuration object:
    ConfigArgBool_t m_Config;

public:

    // Constructor/Destructor:
    CmdArgBool_t(
        IN const TCHAR *pKeyName,
        IN const TCHAR *pArgName,
        IN const TCHAR *pDescription,
        IN bool         DefaultValue,
        IN const TCHAR *pUsage = NULL);
  __override virtual
   ~CmdArgBool_t(void);

    // Copy/Assignment:
    CmdArgBool_t(const CmdArgBool_t &rhs);
    CmdArgBool_t &operator = (const CmdArgBool_t &rhs);

  // Accessors:

    ConfigArgBool_t &
    GetConfig(void) { return m_Config; }
    const ConfigArgBool_t &
    GetConfig(void) const { return m_Config; }
    
    operator void *(void) const
    {
        return m_Config.GetValue()? (void *)this : NULL;
    }

    bool
    GetValue(void) const
    {
        return m_Config.GetValue();
    }

    bool
    GetDefaultValue(void) const
    {
        return m_Config.GetDefaultValue();
    }

    // Formats the command-argument usage into the specified output buffer:
  __override virtual DWORD
    FormatUsage(
      __out_ecount(*pBufferChars) OUT TCHAR *pBuffer,
                                  OUT DWORD *pBufferChars) const;
    
    // Translates the configuration value from text form:
  __override virtual DWORD
    FromString(
        IN const TCHAR *pArgValue);
    
};

// ----------------------------------------------------------------------------
//
// CmdArgAString: Specializes CmdArg to represent an ASCII string
// command-line configuration argument.
//
class CmdArgStringA_t : public CmdArg_t
{
private:

    // Configuration object:
    ConfigArgStringA_t m_Config;

public:

    // Constructor/Destructor:
    CmdArgStringA_t(
        IN const TCHAR *pKeyName,
        IN const TCHAR *pArgName,
        IN const TCHAR *pDescription,
        IN const char  *pDefaultValue = NULL,
        IN const TCHAR *pUsage        = NULL);// optional - uses description if not set
  __override virtual
   ~CmdArgStringA_t(void);

    // Copy/Assignment:
    CmdArgStringA_t(const CmdArgStringA_t &rhs);
    CmdArgStringA_t &operator = (const CmdArgStringA_t &rhs);

  // Accessors:

    ConfigArgStringA_t &
    GetConfig(void) { return m_Config; }
    const ConfigArgStringA_t &
    GetConfig(void) const { return m_Config; }
    
    operator const char *(void) const
    {
        return m_Config.GetValue();
    }

    const char *
    GetValue(void) const
    {
        return m_Config.GetValue();
    }

    const char *
    GetDefaultValue(void) const
    {
        return m_Config.GetDefaultValue();
    }

    // Formats the command-argument usage into the specified output buffer:
  __override virtual DWORD
    FormatUsage(
      __out_ecount(*pBufferChars) OUT TCHAR *pBuffer,
                                  OUT DWORD *pBufferChars) const;
    
    // Translates the configuration value from text form:
  __override virtual DWORD
    FromString(
        IN const TCHAR *pArgValue);
    
};

// ----------------------------------------------------------------------------
//
// CmdArgWString: Specializes CmdArg to represent Unicode, wide-character,
// string command-line configuration argument.
//
class CmdArgStringW_t : public CmdArg_t
{
private:

    // Configuration object:
    ConfigArgStringW_t m_Config;

public:

    // Constructor/Destructor:
    CmdArgStringW_t(
        IN const TCHAR *pKeyName,
        IN const TCHAR *pArgName,
        IN const TCHAR *pDescription,
        IN const WCHAR *pDefaultValue = NULL,
        IN const TCHAR *pUsage        = NULL);
  __override virtual
   ~CmdArgStringW_t(void);

    // Copy/Assignment:
    CmdArgStringW_t(const CmdArgStringW_t &rhs);
    CmdArgStringW_t &operator = (const CmdArgStringW_t &rhs);

  // Accessors:

    ConfigArgStringW_t &
    GetConfig(void) { return m_Config; }
    const ConfigArgStringW_t &
    GetConfig(void) const { return m_Config; }
    
    operator const WCHAR *(void) const
    {
        return m_Config.GetValue();
    }

    const WCHAR *
    GetValue(void) const
    {
        return m_Config.GetValue();
    }

    const WCHAR *
    GetDefaultValue(void) const
    {
        return m_Config.GetDefaultValue();
    }

    // Formats the command-argument usage into the specified output buffer:
  __override virtual DWORD
    FormatUsage(
      __out_ecount(*pBufferChars) OUT TCHAR *pBuffer,
                                  OUT DWORD *pBufferChars) const;
    
    // Translates the configuration value from text form:
  __override virtual DWORD
    FromString(
        IN const TCHAR *pArgValue);
    
};

// ----------------------------------------------------------------------------
//
// CmdArgString: Specializes CmdArg to represent a TCHAR-type string
// command-line configuration argument.
//
#ifdef UNICODE
typedef CmdArgStringW_t CmdArgString_t;
#else
typedef CmdArgStringA_t CmdArgString_t;
#endif

// ----------------------------------------------------------------------------
//
// CmdArgMultiAString: Specializes CmdArg to represent an ASCII string-list
// command-line configuration argument.
//
class CmdArgMultiStringA_t : public CmdArg_t
{
private:

    // Configuration object:
    ConfigArgMultiStringA_t m_Config;

public:

    // Constructor/Destructor:
    CmdArgMultiStringA_t(
        IN const TCHAR *pKeyName,
        IN const TCHAR *pArgName,
        IN const TCHAR *pDescription,
        IN const char  *pSeparators = ",:;",
        IN const TCHAR *pUsage      = NULL,
        IN bool         fTrimSpaces = true,
        IN int          MaximumStrings = INT_MAX / 1024);
  __override virtual
   ~CmdArgMultiStringA_t(void);

    // Copy/Assignment:
    CmdArgMultiStringA_t(const CmdArgMultiStringA_t &rhs);
    CmdArgMultiStringA_t &operator = (const CmdArgMultiStringA_t &rhs);

  // Accessors:

    ConfigArgMultiStringA_t &
    GetConfig(void) { return m_Config; }
    const ConfigArgMultiStringA_t &
    GetConfig(void) const { return m_Config; }
    
    int Size(void) const
    {
        return m_Config.Size();
    }
    const char *operator[] (IN int Index) const
    {
        return m_Config.GetValue(Index);
    }

    const char *
    GetValue(
        IN int Index) const
    {
        return m_Config.GetValue(Index);
    }

    // Formats the command-argument usage into the specified output buffer:
  __override virtual DWORD
    FormatUsage(
      __out_ecount(*pBufferChars) OUT TCHAR *pBuffer,
                                  OUT DWORD *pBufferChars) const;
    
    // Translates the configuration value from text form:
  __override virtual DWORD
    FromString(
        IN const TCHAR *pArgValue);
    
};

// ----------------------------------------------------------------------------
//
// CmdArgMultiWString: Specializes CmdArg to represent a Unicode,
// wide-character, string-list command-line configuration argument.
//
class CmdArgMultiStringW_t : public CmdArg_t
{
private:

    // Configuration object:
    ConfigArgMultiStringW_t m_Config;

public:

    // Constructor/Destructor:
    CmdArgMultiStringW_t(
        IN const TCHAR *pKeyName,
        IN const TCHAR *pArgName,
        IN const TCHAR *pDescription,
        IN const WCHAR *pSeparators = L",:;",
        IN const TCHAR *pUsage      = NULL,
        IN bool         fTrimSpaces = true,
        IN int          MaximumStrings = INT_MAX / 1024);
  __override virtual
   ~CmdArgMultiStringW_t(void);

    // Copy/Assignment:
    CmdArgMultiStringW_t(const CmdArgMultiStringW_t &rhs);
    CmdArgMultiStringW_t &operator = (const CmdArgMultiStringW_t &rhs);

  // Accessors:

    ConfigArgMultiStringW_t &
    GetConfig(void) { return m_Config; }
    const ConfigArgMultiStringW_t &
    GetConfig(void) const { return m_Config; }
    
    int Size(void) const
    {
        return m_Config.Size();
    }

    const WCHAR *operator[] (IN int Index) const
    {
        return m_Config.GetValue(Index);
    }

    const WCHAR *
    GetValue(
        IN int Index) const
    {
        return m_Config.GetValue(Index);
    }

    // Formats the command-argument usage into the specified output buffer:
  __override virtual DWORD
    FormatUsage(
      __out_ecount(*pBufferChars) OUT TCHAR *pBuffer,
                                  OUT DWORD *pBufferChars) const;
    
    // Translates the configuration value from text form:
  __override virtual DWORD
    FromString(
        IN const TCHAR *pArgValue);

};

// ----------------------------------------------------------------------------
//
// CmdArgMultiString: Specializes CmdArg to represent a TCHAR-type
// string-list command-line configuration argument.
//
#ifdef UNICODE
typedef CmdArgMultiStringW_t CmdArgMultiString_t;
#else
typedef CmdArgMultiStringA_t CmdArgMultiString_t;
#endif

// ----------------------------------------------------------------------------
//
// CmdArgLong: Specializes CmdArg to represent a long-integer command-line
// configuration argument.
//
class CmdArgLong_t : public CmdArg_t
{
private:

    // Configuration object:
    ConfigArgLong_t m_Config;

public:

    // Constructor/Destructor:
    CmdArgLong_t(
        IN const TCHAR *pKeyName,
        IN const TCHAR *pArgName,
        IN const TCHAR *pDescription,
        IN long         DefaultValue,
        IN long         MinimumValue,
        IN long         MaximumValue,
        IN const TCHAR *pUsage = NULL);
  __override virtual
   ~CmdArgLong_t(void);

    // Copy/Assignment:
    CmdArgLong_t(const CmdArgLong_t &rhs);
    CmdArgLong_t &operator = (const CmdArgLong_t &rhs);

  // Accessors:

    ConfigArgLong_t &
    GetConfig(void) { return m_Config; }
    const ConfigArgLong_t &
    GetConfig(void) const { return m_Config; }

    operator long (void) const
    {
        return m_Config.GetValue();
    }

    long
    GetValue(void) const
    {
        return m_Config.GetValue();
    }

    long
    GetDefaultValue(void) const
    {
        return m_Config.GetDefaultValue();
    }
    long
    GetMinimumValue(void) const
    {
        return m_Config.GetMinimumValue();
    }
    long
    GetMaximumValue(void) const
    {
        return m_Config.GetMaximumValue();
    }

    // Formats the command-argument usage into the specified output buffer:
  __override virtual DWORD
    FormatUsage(
      __out_ecount(*pBufferChars) OUT TCHAR *pBuffer,
                                  OUT DWORD *pBufferChars) const;
    
    // Translates the configuration value from text form:
  __override virtual DWORD
    FromString(
        IN const TCHAR *pArgValue);
    
};

// ----------------------------------------------------------------------------
//
// CmdArgInt: Specializes CmdArg to represent an integer command-line
// configuration argument.
//
class CmdArgInt_t : public CmdArg_t
{
private:

    // Configuration object:
    CmdArgLong_t m_CmdArg;

public:

    // Constructor/Destructor:
    CmdArgInt_t(
        IN const TCHAR *pKeyName,
        IN const TCHAR *pArgName,
        IN const TCHAR *pDescription,
        IN int          DefaultValue,
        IN int          MinimumValue,
        IN int          MaximumValue,
        IN const TCHAR *pUsage = NULL);
  __override virtual
   ~CmdArgInt_t(void);

    // Copy/Assignment:
    CmdArgInt_t(const CmdArgInt_t &rhs);
    CmdArgInt_t &operator = (const CmdArgInt_t &rhs);

  // Accessors:

    ConfigArgLong_t &
    GetConfig(void) { return m_CmdArg.GetConfig(); }
    const ConfigArgLong_t &
    GetConfig(void) const { return m_CmdArg.GetConfig(); }
    
    operator int (void) const
    {
        return m_CmdArg.GetValue();
    }

    int
    GetValue(void) const
    {
        return m_CmdArg.GetValue();
    }

    int
    GetDefaultValue(void) const
    {
        return m_CmdArg.GetDefaultValue();
    }
    int
    GetMinimumValue(void) const
    {
        return m_CmdArg.GetMinimumValue();
    }
    int
    GetMaximumValue(void) const
    {
        return m_CmdArg.GetMaximumValue();
    }

    // Formats the command-argument usage into the specified output buffer:
  __override virtual DWORD
    FormatUsage(
      __out_ecount(*pBufferChars) OUT TCHAR *pBuffer,
                                  OUT DWORD *pBufferChars) const;
    
    // Translates the configuration value from text form:
  __override virtual DWORD
    FromString(
        IN const TCHAR *pArgValue);
    
};

// ----------------------------------------------------------------------------
//
// CmdArgULong: Specializes CmdArg to represent an unsigned long integer
// command-line configuration argument.
//
class CmdArgULong_t : public CmdArg_t
{
private:

    // Configuration object:
    ConfigArgULong_t m_Config;

public:

    // Constructor/Destructor:
    CmdArgULong_t(
        IN const TCHAR   *pKeyName,
        IN const TCHAR   *pArgName,
        IN const TCHAR   *pDescription,
        IN unsigned long  DefaultValue,
        IN unsigned long  MinimumValue,
        IN unsigned long  MaximumValue,
        IN const TCHAR   *pUsage = NULL);
  __override virtual
   ~CmdArgULong_t(void);

    // Copy/Assignment:
    CmdArgULong_t(const CmdArgULong_t &rhs);
    CmdArgULong_t &operator = (const CmdArgULong_t &rhs);

  // Accessors:

    ConfigArgULong_t &
    GetConfig(void) { return m_Config; }
    const ConfigArgULong_t &
    GetConfig(void) const { return m_Config; }
    
    operator unsigned long (void) const
    {
        return m_Config.GetValue();
    }

    unsigned long
    GetValue(void) const
    {
        return m_Config.GetValue();
    }

    unsigned long
    GetDefaultValue(void) const
    {
        return m_Config.GetDefaultValue();
    }
    unsigned long
    GetMinimumValue(void) const
    {
        return m_Config.GetMinimumValue();
    }
    unsigned long
    GetMaximumValue(void) const
    {
        return m_Config.GetMaximumValue();
    }

    // Formats the command-argument usage into the specified output buffer:
  __override virtual DWORD
    FormatUsage(
      __out_ecount(*pBufferChars) OUT TCHAR *pBuffer,
                                  OUT DWORD *pBufferChars) const;
    
    // Translates the configuration value from text form:
  __override virtual DWORD
    FromString(
        IN const TCHAR *pArgValue);
    
};

// ----------------------------------------------------------------------------
//
// CmdArgUInt: Specializes CmdArg to represent an unsigned integer
// command-line configuration argument.
//
class CmdArgUInt_t : public CmdArg_t
{
private:

    // Configuration object:
    CmdArgULong_t m_CmdArg;

public:

    // Constructor/Destructor:
    CmdArgUInt_t(
        IN const TCHAR  *pKeyName,
        IN const TCHAR  *pArgName,
        IN const TCHAR  *pDescription,
        IN unsigned int  DefaultValue,
        IN unsigned int  MinimumValue,
        IN unsigned int  MaximumValue,
        IN const TCHAR  *pUsage = NULL);
  __override virtual
   ~CmdArgUInt_t(void);

    // Copy/Assignment:
    CmdArgUInt_t(const CmdArgUInt_t &rhs);
    CmdArgUInt_t &operator = (const CmdArgUInt_t &rhs);

  // Accessors:

    ConfigArgULong_t &
    GetConfig(void) { return m_CmdArg.GetConfig(); }
    const ConfigArgULong_t &
    GetConfig(void) const { return m_CmdArg.GetConfig(); }
    
    operator unsigned int (void) const
    {
        return m_CmdArg.GetValue();
    }

    unsigned int
    GetValue(void)  const
    {
        return m_CmdArg.GetValue();
    }

    unsigned int
    GetDefaultValue(void) const
    {
        return m_CmdArg.GetDefaultValue();
    }
    unsigned int
    GetMinimumValue(void) const
    {
        return m_CmdArg.GetMinimumValue();
    }
    unsigned int
    GetMaximumValue(void) const
    {
        return m_CmdArg.GetMaximumValue();
    }

    // Formats the command-argument usage into the specified output buffer:
  __override virtual DWORD
    FormatUsage(
      __out_ecount(*pBufferChars) OUT TCHAR *pBuffer,
                                  OUT DWORD *pBufferChars) const;
    
    // Translates the configuration value from text form:
  __override virtual DWORD
    FromString(
        IN const TCHAR *pArgValue);
    
};

// ----------------------------------------------------------------------------
//
// CmdArgDWORD: Specializes CmdArg to represent a DWORD command-lines
// configuration argument.
//
class CmdArgDWORD_t : public CmdArg_t
{
private:

    // Configuration object:
    CmdArgULong_t m_CmdArg;

public:

    // Constructor/Destructor:
    CmdArgDWORD_t(
        IN const TCHAR *pKeyName,
        IN const TCHAR *pArgName,
        IN const TCHAR *pDescription,
        IN DWORD        DefaultValue,
        IN DWORD        MinimumValue,
        IN DWORD        MaximumValue,
        IN const TCHAR *pUsage = NULL);
  __override virtual
   ~CmdArgDWORD_t(void);

    // Copy/Assignment:
    CmdArgDWORD_t(const CmdArgDWORD_t &rhs);
    CmdArgDWORD_t &operator = (const CmdArgDWORD_t &rhs);
  // Accessors:

    ConfigArgULong_t &
    GetConfig(void) { return m_CmdArg.GetConfig(); }
    const ConfigArgULong_t &
    GetConfig(void) const { return m_CmdArg.GetConfig(); }
    
    operator DWORD (void) const
    {
        return m_CmdArg.GetValue();
    }

    DWORD
    GetValue(void) const
    {
        return m_CmdArg.GetValue();
    }

    DWORD
    GetDefaultValue(void) const
    {
        return m_CmdArg.GetDefaultValue();
    }
    DWORD
    GetMinimumValue(void) const
    {
        return m_CmdArg.GetMinimumValue();
    }
    DWORD
    GetMaximumValue(void) const
    {
        return m_CmdArg.GetMaximumValue();
    }

    // Formats the command-argument usage into the specified output buffer:
  __override virtual DWORD
    FormatUsage(
      __out_ecount(*pBufferChars) OUT TCHAR *pBuffer,
                                  OUT DWORD *pBufferChars) const;
    
    // Translates the configuration value from text form:
  __override virtual DWORD
    FromString(
        IN const TCHAR *pArgValue);
    
};

// ----------------------------------------------------------------------------
//
// CmdArgDouble: Specializes CmdArg to represent a double-precision
// floating-point command-line configuration argument.
//
class CmdArgDouble_t : public CmdArg_t
{
private:

    // Configuration object:
    ConfigArgDouble_t m_Config;

public:

    // Constructor/Destructor:
    CmdArgDouble_t(
        IN const TCHAR *pKeyName,
        IN const TCHAR *pArgName,
        IN const TCHAR *pDescription,
        IN double       DefaultValue,
        IN double       MinimumValue,
        IN double       MaximumValue,
        IN const TCHAR *pUsage = NULL);
  __override virtual
   ~CmdArgDouble_t(void);

    // Copy/Assignment:
    CmdArgDouble_t(const CmdArgDouble_t &rhs);
    CmdArgDouble_t &operator = (const CmdArgDouble_t &rhs);

  // Accessors:

    ConfigArgDouble_t &
    GetConfig(void) { return m_Config; }
    const ConfigArgDouble_t &
    GetConfig(void) const { return m_Config; }
    
    operator double (void) const
    {
        return m_Config.GetValue();
    }

    double
    GetValue(void) const
    {
        return m_Config.GetValue();
    }

    double
    GetDefaultValue(void) const
    {
        return m_Config.GetDefaultValue();
    }
    double
    GetMinimumValue(void) const
    {
        return m_Config.GetMinimumValue();
    }
    double
    GetMaximumValue(void) const
    {
        return m_Config.GetMaximumValue();
    }

    // Formats the command-argument usage into the specified output buffer:
  __override virtual DWORD
    FormatUsage(
      __out_ecount(*pBufferChars) OUT TCHAR *pBuffer,
                                  OUT DWORD *pBufferChars) const;
    
    // Translates the configuration value from text form:
  __override virtual DWORD
    FromString(
        IN const TCHAR *pArgValue);
    
};

};
};
#endif /* _DEFINED_CmdArg_t_ */
// ----------------------------------------------------------------------------
