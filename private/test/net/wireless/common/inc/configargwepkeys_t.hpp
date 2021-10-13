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
// Definitions and declarations for the ConfigArgWEPKeys_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_ConfigArgWEPKeys_t_
#define _DEFINED_ConfigArgWEPKeys_t_
#pragma once

#include <ConfigArg_t.hpp>
#include <WEPKeys_t.hpp>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// ConfigArgAString: Specializes ConfigArg to represent a set of WEP key
//                   configuration variables.
//
class ConfigArgWEPKeys_t : public ConfigArg_t
{
private:

    // Configured value:
    WEPKeys_t m_Value;

    // Default value:
    WEPKeys_t m_DefaultValue;

public:

    // Constructor/Destructor:
    ConfigArgWEPKeys_t(
        IN const TCHAR *pKeyName,
        IN const TCHAR *pDescription,
        IN const TCHAR *pDefaultValue = NULL);
    ConfigArgWEPKeys_t(
        IN const TCHAR     *pKeyName,
        IN const TCHAR     *pDescription,
        IN const WEPKeys_t &DefaultValue);
  __override virtual
   ~ConfigArgWEPKeys_t(void);

    // Copy/Assignment:
    ConfigArgWEPKeys_t(const ConfigArgWEPKeys_t &rhs);
    ConfigArgWEPKeys_t &operator = (const ConfigArgWEPKeys_t &rhs);

  // Accessors:

    operator const WEPKeys_t &(void) const
    {
        return GetValue();
    }

    const WEPKeys_t &
    GetValue(void) const
    {
        return m_Value;
    }
    virtual DWORD
    SetValue(
        IN const WEPKeys_t &Value);

    const WEPKeys_t &
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

  // Utility methods:

    // Translates the specified configuration value to text form:
    static DWORD
    ConvertToString(
                                  IN  const WEPKeys_t &Keys,
      __out_ecount(*pBufferChars) OUT TCHAR           *pBuffer,
                                  OUT DWORD           *pBufferChars);
 
};

};
};
#endif /* _DEFINED_ConfigArgWEPKeys_t_ */
// ----------------------------------------------------------------------------
