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
// Definitions and declarations for the AuthMatrixSuite_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_AuthMatrixSuite_H_
#define _DEFINED_AuthMatrixSuite_H_
#pragma once

#include <WQMTestSuiteBase_t.hpp>

#include <CmdArgList_t.hpp>
#include <WiFiConfig_t.hpp>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Class definition for WQM AuthMatrix suite.
//
class AuthMatrixSuite_t : public WQMTestSuiteBase_t
{
private:

    // Copy and assignment are deliberately disabled:
    AuthMatrixSuite_t(const AuthMatrixSuite_t &src);
    AuthMatrixSuite_t &operator = (const AuthMatrixSuite_t &src);

    // Test-case configuration:
    CmdArgString_t *m_pTestDesc;
    WiFiConfig_t    m_APConfig;
    WiFiConfig_t    m_Config;
    CmdArgBool_t   *m_pIsBadModes;
    CmdArgBool_t   *m_pIsBadKey;
    CmdArgBool_t   *m_pIsStressTest ;
    CmdArgList_t    m_TestConfigList1;
    CmdArgList_t    m_TestConfigList2;

public:

    // Constructor and destructor:
    AuthMatrixSuite_t(HMODULE hModule, IN HANDLE logHandle);
  __override virtual
   ~AuthMatrixSuite_t(void);

  __override virtual BOOL  PrepareToRun(void);  
  __override virtual int   Execute(IN WQMTestCase_t *pTestCase);
  __override virtual void  Cleanup(void);

};

};
}
#endif /* _DEFINED_AuthMatrixSuite_H_ */
// ----------------------------------------------------------------------------
