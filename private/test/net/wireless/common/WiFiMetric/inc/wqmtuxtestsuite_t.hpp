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
// Definitions and declarations for the WQMTuxTestSuite_t class.
//
// ----------------------------------------------------------------------------

#ifndef _WQMTUXTESTSUITE_
#define _WQMTUXTESTSUITE_
#pragma once

#include <tux.h>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Customized Factory object for generating SSID tests.
//
class WQMTuxTestSuite_t : public WQMTestSuiteBase_t
{
private:

    // Copy and assignment are deliberately disabled:
    WQMTuxTestSuite_t(const WQMTuxTestSuite_t &src);
    WQMTuxTestSuite_t &operator = (const WQMTuxTestSuite_t &src);

    FUNCTION_TABLE_ENTRY* m_pTuxFuncTable;
    
public:

    // Constructor and destructor:
    WQMTuxTestSuite_t(HMODULE h_Module, IN HANDLE logHandle);
   ~WQMTuxTestSuite_t(void);

    __override virtual BOOL   PrepareToRun(void);  
    __override virtual int    Execute(IN WQMTestCase_t* testCase);
    __override virtual void   Cleanup(void);  

};

}
}

#endif /* _WQMTUXTESTSUITE_ */
// ----------------------------------------------------------------------------
