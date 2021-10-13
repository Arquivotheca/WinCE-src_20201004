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
// Definitions and declarations for the ConfigArg_t classes.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_ConfigArg_t_
#define _DEFINED_ConfigArg_t_
#pragma once

#include <MemBuffer_t.hpp>

#ifdef _PREFAST_
#pragma warning(push)
#pragma warning(disable:26020)
#else
#pragma warning(disable:4068)
#endif

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
// ConfigArg: Base for a series of application configuration
//            variable classes.
//
class CmdArg_t;
class ConfigArg_t
{
private:

    // Number references to this object:
    friend class CmdArg_t; // Let CmdArg_t access reference count
    LONG m_RefCount;
    
    // Key in lists and/or registry:
    ce::tstring m_KeyName;

    // Description text:
    ce::tstring m_Description;

public:

    // Constructor/Destructor:
    ConfigArg_t(
        IN const TCHAR *pKeyName,
        IN const TCHAR *pDescription);
    virtual
   ~ConfigArg_t(void);

    // Copy/Assignment:
    ConfigArg_t(const ConfigArg_t &rhs);
    ConfigArg_t &operator = (const ConfigArg_t &rhs);

    // Increments or decrements the specified object's reference count:
    // Attach returns false if the object has been deleted.
    // Detach deletes object when reference count reaches zero.
    static bool
    Attach(ConfigArg_t *pConfig);
    static void
    Detach(ConfigArg_t *pConfig);

  // Accessors:

    const TCHAR *
    GetKeyName(void) const
    {
        return m_KeyName;
    }

    virtual void
    SetKeyName(const TCHAR* keyName);
    
    const TCHAR *
    GetDescription(void) const
    {
        return m_Description;
    }

    // Translates the configuration value to or from text form:
    virtual DWORD
    ToString(
      __out_ecount(*pBufferChars) OUT TCHAR *pBuffer,
                                  OUT DWORD *pBufferChars) const = 0;
    virtual DWORD
    FromString(
        IN const TCHAR *pArgValue) = 0;

    // Reads or writes the value from/to the specified registry:
    virtual DWORD
    ReadRegistry(
        IN HKEY ConfigKey) = 0;  // Open registry key or HKLM, HKCU, etc.
    virtual DWORD
    WriteRegistry(
        IN HKEY ConfigKey) const = 0;

    // Deletes the value from the specified registry:
    // The default implementation just deletes the named value.
    virtual DWORD
    DeleteRegistry(
        IN HKEY ConfigKey) const;

};

// ----------------------------------------------------------------------------
//
// ConfigArgBool: Specializes ConfigArg to represent a boolean
//                configuration variable.
//
class ConfigArgBool_t : public ConfigArg_t
{
private:

    // Configured value:
    bool m_Value;

    // Default value:
    bool m_DefaultValue;

    // Indicates how the registry value is stored - one of REG_SZ or
    // REG_DWORD (default):
    DWORD m_RegistryType;

public:

    // Constructor/Destructor:
    ConfigArgBool_t(
        IN const TCHAR *pKeyName,
        IN const TCHAR *pDescription,
        IN bool         DefaultValue);
  __override virtual
   ~ConfigArgBool_t(void);

    // Copy/Assignment:
    ConfigArgBool_t(const ConfigArgBool_t &rhs);
    ConfigArgBool_t &operator = (const ConfigArgBool_t &rhs);

  // Accessors:

    operator void *(void) const
    {
        return GetValue()? (void *)this : NULL;
    }

    bool
    GetValue(void) const
    {
        return m_Value;
    }
    virtual DWORD
    SetValue(
        IN bool Value);

    bool
    GetDefaultValue(void) const
    {
        return m_DefaultValue;
    }

    // Translates the configuration value to or from text form:
  __override virtual DWORD
    ToString(
      __out_ecount(*pBufferChars) OUT TCHAR *pBuffer,
                                  OUT DWORD *pBufferChars) const;
  __override virtual DWORD
    FromString(
        IN const TCHAR *pArgValue);

    // Reads or writes the value from/to the specified registry:
  __override virtual DWORD
    ReadRegistry(
        IN HKEY ConfigKey);  // Open registry key or HKLM, HKCU, etc.
  __override virtual DWORD
    WriteRegistry(
        IN HKEY ConfigKey) const;

};

// ----------------------------------------------------------------------------
//
// ConfigArgStringA: Specializes ConfigArg to represent an ASCII string
//                   configuration variable.
//
class ConfigArgStringA_t : public ConfigArg_t
{
private:

    // Configured value:
    ce::string m_Value;

    // Default value:
    ce::string m_DefaultValue;

public:

    // Constructor/Destructor:
    ConfigArgStringA_t(
        IN const TCHAR  *pKeyName,
        IN const TCHAR  *pDescription,
        IN const char   *pDefaultValue = NULL);
  __override virtual
   ~ConfigArgStringA_t(void);

    // Copy/Assignment:
    ConfigArgStringA_t(const ConfigArgStringA_t &rhs);
    ConfigArgStringA_t &operator = (const ConfigArgStringA_t &rhs);

  // Accessors:

    operator const char *(void) const
    {
        return GetValue();
    }

    const char *
    GetValue(void) const
    {
        return m_Value;
    }
    virtual DWORD
    SetValue(
        IN const char *pValue);

    const char *
    GetDefaultValue(void) const
    {
        return m_DefaultValue;
    }

    // Translates the configuration value to or from text form:
  __override virtual DWORD
    ToString(
      __out_ecount(*pBufferChars) OUT TCHAR *pBuffer,
                                  OUT DWORD *pBufferChars) const;
  __override virtual DWORD
    FromString(
        IN const TCHAR *pArgValue);

    // Reads or writes the value from/to the specified registry:
  __override virtual DWORD
    ReadRegistry(
        IN HKEY ConfigKey);  // Open registry key or HKLM, HKCU, etc.
  __override virtual DWORD
    WriteRegistry(
        IN HKEY ConfigKey) const;

};

// ----------------------------------------------------------------------------
//
// ConfigArgStringW: Specializes ConfigArg to represent Unicode, wide-character,
//                   string configuration variable.
//
class ConfigArgStringW_t : public ConfigArg_t
{
private:

    // Configured value:
    ce::wstring m_Value;

    // Default value:
    ce::wstring m_DefaultValue;

public:

    // Constructor/Destructor:
    ConfigArgStringW_t(
        IN const TCHAR  *pKeyName,
        IN const TCHAR  *pDescription,
        IN const WCHAR  *pDefaultValue = NULL);
  __override virtual
   ~ConfigArgStringW_t(void);

    // Copy/Assignment:
    ConfigArgStringW_t(const ConfigArgStringW_t &rhs);
    ConfigArgStringW_t &operator = (const ConfigArgStringW_t &rhs);

  // Accessors:

    operator const WCHAR *(void) const
    {
        return GetValue();
    }

    const WCHAR *
    GetValue(void) const
    {
        return m_Value;
    }
    virtual DWORD
    SetValue(
        IN const WCHAR *pValue);

    const WCHAR *
    GetDefaultValue(void) const
    {
        return m_DefaultValue;
    }

    // Translates the configuration value to or from text form:
  __override virtual DWORD
    ToString(
      __out_ecount(*pBufferChars) OUT TCHAR *pBuffer,
                                  OUT DWORD *pBufferChars) const;
  __override virtual DWORD
    FromString(
        IN const TCHAR *pArgValue);

    // Reads or writes the value from/to the specified registry:
  __override virtual DWORD
    ReadRegistry(
        IN HKEY ConfigKey);  // Open registry key or HKLM, HKCU, etc.
  __override virtual DWORD
    WriteRegistry(
        IN HKEY ConfigKey) const;

};

// ----------------------------------------------------------------------------
//
// ConfigArgString: Specializes ConfigArg to represent a TCHAR-type string
//                  configuration variable.
//
#ifdef UNICODE
typedef ConfigArgStringW_t ConfigArgString_t;
#else
typedef ConfigArgStringA_t ConfigArgString_t;
#endif

// ----------------------------------------------------------------------------
//
// ConfigArgMultiStringA: Specializes ConfigArg to represent an ASCII
//                        string-list configuration variable.
//
class ConfigArgMultiStringA_t : public ConfigArg_t
{
private:

    // String list:
    MemBuffer_t m_StringBuffer;
    MemBuffer_t m_StringOffsets;
    int         m_StringListSize;

    // String containing characters for separating the strings
    // (ala strtok) in the variable:
    ce::string m_Separators;

    // Flag indicating whether space characters are to be removed
    // from the beginning and end of each of the strings:
    bool m_fTrimSpaces;

    // Maximum list size (optional):
    int m_MaximumStrings;

public:

    // Constructor/Destructor:
    ConfigArgMultiStringA_t(
        IN const TCHAR *pKeyName,
        IN const TCHAR *pDescription,
        IN const char  *pSeparators = ",:;",
        IN bool         fTrimSpaces = true,
        IN int          MaximumStrings = INT_MAX / 1024);
  __override virtual
   ~ConfigArgMultiStringA_t(void);

    // Copy/Assignment:
    ConfigArgMultiStringA_t(const ConfigArgMultiStringA_t &rhs);
    ConfigArgMultiStringA_t &operator = (const ConfigArgMultiStringA_t &rhs);

  // Accessors:

    int Size(void) const
    {
        return m_StringListSize;
    }

    const char *operator[] (IN int Index) const
    {
        return GetValue(Index);
    }

    const char *
    GetValue(
        IN int Index) const;
    virtual DWORD
    SetValue(
        IN int Index,
        IN const char *pString);

    virtual DWORD
    Insert(
        IN int Index,
        IN const char *pString);
    virtual DWORD
    Remove(
        IN int Index);
    virtual DWORD
    Append(
        IN const char *pString);

    void
    Free(void);
    void
    Clear(void);

    // Translates the configuration value to or from text form:
  __override virtual DWORD
    ToString(
      __out_ecount(*pBufferChars) OUT TCHAR *pBuffer,
                                  OUT DWORD *pBufferChars) const;
  __override virtual DWORD
    FromString(
        IN const TCHAR *pArgValue);

    // Reads or writes the value from/to the specified registry:
  __override virtual DWORD
    ReadRegistry(
        IN HKEY ConfigKey);  // Open registry key or HKLM, HKCU, etc.
  __override virtual DWORD
    WriteRegistry(
        IN HKEY ConfigKey) const;

};

// ----------------------------------------------------------------------------
//
// ConfigArgMultiStringW: Specializes ConfigArg to represent a Unicode,
//                        wide-character, string-list configuration variable.
//
class ConfigArgMultiStringW_t : public ConfigArg_t
{
private:

    // String list:
    MemBuffer_t m_StringBuffer;
    MemBuffer_t m_StringOffsets;
    int         m_StringListSize;
    int         m_StringListCapacity;

    // String containing characters for separating the strings
    // (ala strtok) in the variable:
    ce::wstring m_Separators;

    // Flag indicating whether space characters are to be removed
    // from the beginning and end of each of the strings:
    bool m_fTrimSpaces;

    // Maximum list size (optional):
    int m_MaximumStrings;

public:

    // Constructor/Destructor:
    ConfigArgMultiStringW_t(
        IN const TCHAR *pKeyName,
        IN const TCHAR *pDescription,
        IN const WCHAR *pSeparators = L",:;",
        IN bool         fTrimSpaces = true,
        IN int          MaximumStrings = INT_MAX / 1024);
  __override virtual
   ~ConfigArgMultiStringW_t(void);

    // Copy/Assignment:
    ConfigArgMultiStringW_t(const ConfigArgMultiStringW_t &rhs);
    ConfigArgMultiStringW_t &operator = (const ConfigArgMultiStringW_t &rhs);

  // Accessors:

    int Size(void) const
    {
        return m_StringListSize;
    }

    const WCHAR *operator[] (IN int Index) const
    {
        return GetValue(Index);
    }

    const WCHAR *
    GetValue(
        IN int Index) const;
    virtual DWORD
    SetValue(
        IN int Index,
        IN const WCHAR *pString);

    virtual DWORD
    Insert(
        IN int Index,
        IN const WCHAR *pString);
    virtual DWORD
    Remove(
        IN int Index);
    virtual DWORD
    Append(
        IN const WCHAR *pString);

    void
    Free(void);
    void
    Clear(void);

    // Translates the configuration value to or from text form:
  __override virtual DWORD
    ToString(
      __out_ecount(*pBufferChars) OUT TCHAR *pBuffer,
                                  OUT DWORD *pBufferChars) const;
  __override virtual DWORD
    FromString(
        IN const TCHAR *pArgValue);

    // Reads or writes the value from/to the specified registry:
  __override virtual DWORD
    ReadRegistry(
        IN HKEY ConfigKey);  // Open registry key or HKLM, HKCU, etc.
  __override virtual DWORD
    WriteRegistry(
        IN HKEY ConfigKey) const;

};

// ----------------------------------------------------------------------------
//
// ConfigArgMultiString: Specializes ConfigArg to represent a TCHAR-type
//                       string-list configuration variable.
//
#ifdef UNICODE
typedef ConfigArgMultiStringW_t ConfigArgMultiString_t;
#else
typedef ConfigArgMultiStringA_t ConfigArgMultiString_t;
#endif

// ----------------------------------------------------------------------------
//
// ConfigArgLong: Specializes ConfigArg to represent a long-integer
//                configuration variable.
//
class ConfigArgLong_t : public ConfigArg_t
{
private:

    // Configured value:
    long m_Value;

    // Default, minimum and maximum values:
    long m_DefaultValue;
    long m_MinimumValue;
    long m_MaximumValue;

    // Indicates how the registry value is stored - one of REG_SZ (default),
    // REG_BINARY or REG_DWORD:
    DWORD m_RegistryType;

public:

    // Constructor/Destructor:
    ConfigArgLong_t(
        IN const TCHAR *pKeyName,
        IN const TCHAR *pDescription,
        IN long         DefaultValue,
        IN long         MinimumValue,
        IN long         MaximumValue);
  __override virtual
   ~ConfigArgLong_t(void);

    // Copy/Assignment:
    ConfigArgLong_t(const ConfigArgLong_t &rhs);
    ConfigArgLong_t &operator = (const ConfigArgLong_t &rhs);

  // Accessors:

    operator long (void) const
    {
        return GetValue();
    }

    long
    GetValue(void) const
    {
        return m_Value;
    }
    virtual DWORD
    SetValue(
        IN long Value);

    long
    GetDefaultValue(void) const
    {
        return m_DefaultValue;
    }
    long
    GetMinimumValue(void) const
    {
        return m_MinimumValue;
    }
    long
    GetMaximumValue(void) const
    {
        return m_MaximumValue;
    }

    // Translates the configuration value to or from text form:
  __override virtual DWORD
    ToString(
      __out_ecount(*pBufferChars) OUT TCHAR *pBuffer,
                                  OUT DWORD *pBufferChars) const;
  __override virtual DWORD
    FromString(
        IN const TCHAR *pArgValue);

    // Reads or writes the value from/to the specified registry:
  __override virtual DWORD
    ReadRegistry(
        IN HKEY ConfigKey);  // Open registry key or HKLM, HKCU, etc.
  __override virtual DWORD
    WriteRegistry(
        IN HKEY ConfigKey) const;

};

// ----------------------------------------------------------------------------
//
// ConfigArgInt: Specializes ConfigArg to represent an integer
//               configuration variable.
//
class ConfigArgInt_t : public ConfigArg_t
{
private:

    // Configured value:
    ConfigArgLong_t m_Config;

public:

    // Constructor/Destructor:
    ConfigArgInt_t(
        IN const TCHAR *pKeyName,
        IN const TCHAR *pDescription,
        IN int          DefaultValue,
        IN int          MinimumValue,
        IN int          MaximumValue);
  __override virtual
   ~ConfigArgInt_t(void);

    // Copy/Assignment:
    ConfigArgInt_t(const ConfigArgInt_t &rhs);
    ConfigArgInt_t &operator = (const ConfigArgInt_t &rhs);

  // Accessors:

  __override virtual void
    SetKeyName(const TCHAR *pNewName);
  
    operator int (void) const
    {
        return m_Config.GetValue();
    }

    int
    GetValue(void) const
    {
        return m_Config.GetValue();
    }
    virtual DWORD
    SetValue(
        IN int Value);

    int
    GetDefaultValue(void) const
    {
        return m_Config.GetDefaultValue();
    }
    int
    GetMinimumValue(void) const
    {
        return m_Config.GetMinimumValue();
    }
    int
    GetMaximumValue(void) const
    {
        return m_Config.GetMaximumValue();
    }

    // Translates the configuration value to or from text form:
  __override virtual DWORD
    ToString(
      __out_ecount(*pBufferChars) OUT TCHAR *pBuffer,
                                  OUT DWORD *pBufferChars) const;
  __override virtual DWORD
    FromString(
        IN const TCHAR *pArgValue);

    // Reads or writes the value from/to the specified registry:
  __override virtual DWORD
    ReadRegistry(
        IN HKEY ConfigKey);  // Open registry key or HKLM, HKCU, etc.
  __override virtual DWORD
    WriteRegistry(
        IN HKEY ConfigKey) const;

};

// ----------------------------------------------------------------------------
//
// ConfigArgULong: Specializes ConfigArg to represent an unsigned long integer
//                 configuration variable.
//
class ConfigArgULong_t : public ConfigArg_t
{
private:

    // Configured value:
    unsigned long m_Value;

    // Default, minimum and maximum values:
    unsigned long m_DefaultValue;
    unsigned long m_MinimumValue;
    unsigned long m_MaximumValue;

    // Indicates how the registry value is stored - one of REG_SZ,
    // REG_BINARY or REG_DWORD (default):
    DWORD m_RegistryType;

public:

    // Constructor/Destructor:
    ConfigArgULong_t(
        IN const TCHAR   *pKeyName,
        IN const TCHAR   *pDescription,
        IN unsigned long  DefaultValue,
        IN unsigned long  MinimumValue,
        IN unsigned long  MaximumValue);
  __override virtual
   ~ConfigArgULong_t(void);

    // Copy/Assignment:
    ConfigArgULong_t(const ConfigArgULong_t &rhs);
    ConfigArgULong_t &operator = (const ConfigArgULong_t &rhs);

  // Accessors:

    operator unsigned long (void) const
    {
        return GetValue();
    }

    unsigned long
    GetValue(void)  const
    {
        return m_Value;
    }
    virtual DWORD
    SetValue(
        IN unsigned long Value);

    unsigned long
    GetDefaultValue(void) const
    {
        return m_DefaultValue;
    }
    unsigned long
    GetMinimumValue(void) const
    {
        return m_MinimumValue;
    }
    unsigned long
    GetMaximumValue(void) const
    {
        return m_MaximumValue;
    }

    // Translates the configuration value to or from text form:
  __override virtual DWORD
    ToString(
      __out_ecount(*pBufferChars) OUT TCHAR *pBuffer,
                                  OUT DWORD *pBufferChars) const;
  __override virtual DWORD
    FromString(
        IN const TCHAR *pArgValue);

    // Reads or writes the value from/to the specified registry:
  __override virtual DWORD
    ReadRegistry(
        IN HKEY ConfigKey);  // Open registry key or HKLM, HKCU, etc.
  __override virtual DWORD
    WriteRegistry(
        IN HKEY ConfigKey) const;

};

// ----------------------------------------------------------------------------
//
// ConfigArgUInt: Specializes ConfigArg to represent an unsigned integer
//                configuration variable.
//
class ConfigArgUInt_t : public ConfigArg_t
{
private:

    // Configured value:
    ConfigArgULong_t m_Config;

public:

    // Constructor/Destructor:
    ConfigArgUInt_t(
        IN const TCHAR  *pKeyName,
        IN const TCHAR  *pDescription,
        IN unsigned int  DefaultValue,
        IN unsigned int  MinimumValue,
        IN unsigned int  MaximumValue);
  __override virtual
   ~ConfigArgUInt_t(void);

    // Copy/Assignment:
    ConfigArgUInt_t(const ConfigArgUInt_t &rhs);
    ConfigArgUInt_t &operator = (const ConfigArgUInt_t &rhs);

  // Accessors:

  __override virtual void
    SetKeyName(const TCHAR *pNewName);

    operator unsigned int (void) const
    {
        return m_Config.GetValue();
    }

    unsigned int
    GetValue(void) const
    {
        return m_Config.GetValue();
    }
    virtual DWORD
    SetValue(
        IN unsigned int Value);

    unsigned int
    GetDefaultValue(void) const
    {
        return m_Config.GetDefaultValue();
    }
    unsigned int
    GetMinimumValue(void) const
    {
        return m_Config.GetMinimumValue();
    }
    unsigned int
    GetMaximumValue(void) const
    {
        return m_Config.GetMaximumValue();
    }

    // Translates the configuration value to or from text form:
  __override virtual DWORD
    ToString(
      __out_ecount(*pBufferChars) OUT TCHAR *pBuffer,
                                  OUT DWORD *pBufferChars) const;
  __override virtual DWORD
    FromString(
        IN const TCHAR *pArgValue);

    // Reads or writes the value from/to the specified registry:
  __override virtual DWORD
    ReadRegistry(
        IN HKEY ConfigKey);  // Open registry key or HKLM, HKCU, etc.
  __override virtual DWORD
    WriteRegistry(
        IN HKEY ConfigKey) const;

};

// ----------------------------------------------------------------------------
//
// ConfigArgDWORD: Specializes ConfigArg to represent a DWORD
//                 configuration variable.
//
class ConfigArgDWORD_t : public ConfigArg_t
{
private:

    // Configured value:
    ConfigArgULong_t m_Config;

public:

    // Constructor/Destructor:
    ConfigArgDWORD_t(
        IN const TCHAR *pKeyName,
        IN const TCHAR *pDescription,
        IN DWORD        DefaultValue,
        IN DWORD        MinimumValue,
        IN DWORD        MaximumValue);
  __override virtual
   ~ConfigArgDWORD_t(void);

    // Copy/Assignment:
    ConfigArgDWORD_t(const ConfigArgDWORD_t &rhs);
    ConfigArgDWORD_t &operator = (const ConfigArgDWORD_t &rhs);

  // Accessors:

  __override virtual void
    SetKeyName(const TCHAR *pNewName);
  
    operator DWORD (void) const
    {
        return m_Config.GetValue();
    }

    DWORD
    GetValue(void)  const
    {
        return m_Config.GetValue();
    }
    virtual DWORD
    SetValue(
        IN DWORD Value);

    DWORD
    GetDefaultValue(void) const
    {
        return m_Config.GetDefaultValue();
    }
    DWORD
    GetMinimumValue(void) const
    {
        return m_Config.GetMinimumValue();
    }
    DWORD
    GetMaximumValue(void) const
    {
        return m_Config.GetMaximumValue();
    }

    // Translates the configuration value to or from text form:
  __override virtual DWORD
    ToString(
      __out_ecount(*pBufferChars) OUT TCHAR *pBuffer,
                                  OUT DWORD *pBufferChars) const;
  __override virtual DWORD
    FromString(
        IN const TCHAR *pArgValue);

    // Reads or writes the value from/to the specified registry:
  __override virtual DWORD
    ReadRegistry(
        IN HKEY ConfigKey);  // Open registry key or HKLM, HKCU, etc.
  __override virtual DWORD
    WriteRegistry(
        IN HKEY ConfigKey) const;

};

// ----------------------------------------------------------------------------
//
// ConfigArgDouble: Specializes ConfigArg to represent a double-precision
//                  floating-point configuration variable.
//
class ConfigArgDouble_t : public ConfigArg_t
{
private:

    // Configured value:
    double m_Value;

    // Default, minimum and maximum values:
    double m_DefaultValue;
    double m_MinimumValue;
    double m_MaximumValue;

    // Indicates how the registry value is stored - one of REG_SZ (default)
    // or REG_BINARY:
    DWORD m_RegistryType;

public:

    // Constructor/Destructor:
    ConfigArgDouble_t(
        IN const TCHAR *pKeyName,
        IN const TCHAR *pDescription,
        IN double       DefaultValue,
        IN double       MinimumValue,
        IN double       MaximumValue);
  __override virtual
   ~ConfigArgDouble_t(void);

    // Copy/Assignment:
    ConfigArgDouble_t(const ConfigArgDouble_t &rhs);
    ConfigArgDouble_t &operator = (const ConfigArgDouble_t &rhs);

  // Accessors:

    operator double (void) const
    {
        return GetValue();
    }

    double
    GetValue(void) const
    {
        return m_Value;
    }
    virtual DWORD
    SetValue(
        IN double Value);

    double
    GetDefaultValue(void) const
    {
        return m_DefaultValue;
    }
    double
    GetMinimumValue(void) const
    {
        return m_MinimumValue;
    }
    double
    GetMaximumValue(void) const
    {
        return m_MaximumValue;
    }

    // Translates the configuration value to or from text form:
  __override virtual DWORD
    ToString(
      __out_ecount(*pBufferChars) OUT TCHAR *pBuffer,
                                  OUT DWORD *pBufferChars) const;
  __override virtual DWORD
    FromString(
        IN const TCHAR *pArgValue);

    // Reads or writes the value from/to the specified registry:
  __override virtual DWORD
    ReadRegistry(
        IN HKEY ConfigKey);  // Open registry key or HKLM, HKCU, etc.
  __override virtual DWORD
    WriteRegistry(
        IN HKEY ConfigKey) const;

};

};
};

#ifdef _PREFAST_
#pragma warning(pop)
#endif

#endif /* _DEFINED_ConfigArg_t_ */
// ----------------------------------------------------------------------------
