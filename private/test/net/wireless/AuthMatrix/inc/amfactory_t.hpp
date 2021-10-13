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
// Definitions and declarations for the AMFactory_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_AMFactory_t_
#define _DEFINED_AMFactory_t_
#pragma once

#include <Factory_t.hpp>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Customized Factory object for generating AuthMatrix tests.
//
class AMFactory_t : public Factory_t
{
private:

    // Copy and assignment are deliberately disabled:
    AMFactory_t(const AMFactory_t &src);
    AMFactory_t &operator = (const AMFactory_t &src);

public:

    // Constructor and destructor:
    AMFactory_t(void);
   ~AMFactory_t(void);

    // Parses the DLL's command-line arguments:
  __override virtual void  PrintUsage(void) const;
  __override virtual DWORD ParseCommandLine(int argc, TCHAR *argv[]);

protected:

    // Generates the function-table for this DLL:
  __override virtual DWORD
    CreateFunctionTable(void);

    // Generates the WiFiBase object for processing the specified test:
  __override virtual DWORD
    CreateTest(
        const FUNCTION_TABLE_ENTRY *pFTE,
        WiFiBase_t                **ppTest);
};

};
};
#endif /* _DEFINED_AMFactory_t_ */
// ----------------------------------------------------------------------------
