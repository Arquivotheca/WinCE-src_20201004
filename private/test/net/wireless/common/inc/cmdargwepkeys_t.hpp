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
// Definitions and declarations for the CmdArgWEPKeys_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_CmdArgWEPKeys_t_
#define _DEFINED_CmdArgWEPKeys_t_
#pragma once

#include <CmdArg_t.hpp>
#include <ConfigArgWEPKeys_t.hpp>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// CmdArgAString: Specializes CmdArg to represent a WEP key-value
//                command-line configuration argument.
//
class CmdArgWEPKeys_t : public CmdArg_t
{
private:

    // Configuration object:
    ConfigArgWEPKeys_t m_Config;

public:

    // Constructor/Destructor:
    CmdArgWEPKeys_t(
        IN const TCHAR *pKeyName,
        IN const TCHAR *pArgName,
        IN const TCHAR *pDescription,
        IN const TCHAR *pDefaultValue = NULL,
        IN const TCHAR *pUsage        = NULL);// optional - uses description if not set  
    CmdArgWEPKeys_t(
        IN const TCHAR     *pKeyName,
        IN const TCHAR     *pArgName,
        IN const TCHAR     *pDescription,
        IN const WEPKeys_t &DefaultValue,
        IN const TCHAR     *pUsage  = NULL); // optional - uses description if not set
  __override virtual
   ~CmdArgWEPKeys_t(void);

    // Copy/Assignment:
    CmdArgWEPKeys_t(const CmdArgWEPKeys_t &rhs);
    CmdArgWEPKeys_t &operator = (const CmdArgWEPKeys_t &rhs);

  // Accessors:

    ConfigArgWEPKeys_t &
    GetConfig(void) { return m_Config; }
    const ConfigArgWEPKeys_t &
    GetConfig(void) const { return m_Config; }
    
    operator const WEPKeys_t &(void) const
    {
        return m_Config.GetValue();
    }

    const WEPKeys_t &
    GetValue(void) const
    {
        return m_Config.GetValue();
    }

    const WEPKeys_t &
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

};
};
#endif /* _DEFINED_CmdArgWEPKeys_t_ */
// ----------------------------------------------------------------------------
