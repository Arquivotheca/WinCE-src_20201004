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
// Implementation of the WZCTest_t class.
//
// ----------------------------------------------------------------------------

#include "WZCTest_t.hpp"

#include <assert.h>

#include "Factory_t.hpp"
#include "Utils.hpp"

using namespace ce;
using namespace ce::qa;

// ----------------------------------------------------------------------------
//
// Test names:
//
struct WZCTestDesc_t
{
    enum WZCTestType_e {
         WiFiAdapter = WZCTest_t::TestIdStart,
         WiFiContext,
         WiFiDisable,
         WiFiEnable,
         WiFiClear
    };
    DWORD  m_TestId;
    TCHAR *m_Desc;
};
static const WZCTestDesc_t s_WZCTests[] =
{
     { WZCTestDesc_t::WiFiAdapter
      ,TEXT("Display WZC wireless-adapter information")
     },
     { WZCTestDesc_t::WiFiContext
      ,TEXT("Display WZC context information")
     },
     { WZCTestDesc_t::WiFiDisable
      ,TEXT("Disables the WZC service")
     },
     { WZCTestDesc_t::WiFiEnable
      ,TEXT("Enables the WZC service")
     },
     { WZCTestDesc_t::WiFiClear
      ,TEXT("Clears the current WZC Preferred Networks List")
     },
};
static int s_NumberWZCTests = COUNTOF(s_WZCTests);

// ----------------------------------------------------------------------------
//
// Initializes or cleans up static resources.
//
void
WZCTest_t::
StartupInitialize(void)
{
    // nothing to do.
}

void
WZCTest_t::
ShutdownCleanup(void)
{
    // nothing to do.
}

// ----------------------------------------------------------------------------
//
// Displays the command-argument options.
//
void
WZCTest_t::
PrintUsage(void)
{
    // nothing to do.
}

// ----------------------------------------------------------------------------
//
// Parses the DLL's command-line arguments.
//
DWORD
WZCTest_t::
ParseCommandLine(int argc, TCHAR *argv[])
{
    // nothing to do.
    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Adds all the tests to the specified factory's function-table.
//
DWORD
WZCTest_t::
AddFunctionTable(Factory_t *pFactory)
{
    DWORD result;
    for (int tx = 0 ; tx < s_NumberWZCTests ; ++tx)
    {
        result = pFactory->AddFunctionTableEntry(s_WZCTests[tx].m_Desc, 0, 0,
                                                 s_WZCTests[tx].m_TestId);
        if (ERROR_SUCCESS != result)
            return result;
    }
    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Generates an object for processing the specified test
//
DWORD
WZCTest_t::
CreateTest(
    const FUNCTION_TABLE_ENTRY *pFTE,
    WiFiBase_t                **ppTest)
{
    assert(pFTE->dwUniqueID >= WZCTest_t::TestIdStart);
    assert(pFTE->dwUniqueID <= WZCTest_t::TestIdEnd);

    WZCTest_t *pTest = new WZCTest_t((int)pFTE->dwUniqueID);
    if (pTest == NULL)
    {
        LogError(TEXT("Can't allocate %u byte WZCTest object"),
                 sizeof(WZCTest_t));
        return ERROR_OUTOFMEMORY;
    }

   *ppTest = pTest;
    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Constructor.
//
WZCTest_t::
WZCTest_t(int TestId)
    : m_TestId(TestId)
{
    // nothing to do
}

// ----------------------------------------------------------------------------
//
// Destructor.
//
WZCTest_t::
~WZCTest_t(void)
{
    // nothing to do
}

// ----------------------------------------------------------------------------
//
// Initializes this concrete test class.
//
DWORD
WZCTest_t::
Init(void)
{
    return InitWZCService();
}

// ----------------------------------------------------------------------------
//
// Runs the test.
//
DWORD
WZCTest_t::
Run(void)
{
    DWORD result;
    switch (m_TestId)
    {
        default:
        {
            WZCIntfEntry_t intf;
            result = GetWZCService()->QueryAdapterInfo(&intf);
            if (ERROR_SUCCESS == result)
            {
                intf.Log(LogAlways, true);
            }
            break;
        }
        case WZCTestDesc_t::WiFiContext:
            result = GetWZCService()->LogWZCContext(LogAlways);
            break;

        case WZCTestDesc_t::WiFiDisable:
            result = GetWZCService()->Disable();
            break;

        case WZCTestDesc_t::WiFiEnable:
            result = GetWZCService()->Enable();
            break;

        case WZCTestDesc_t::WiFiClear:
            result = GetWZCService()->Clear();
            break;
    }
    return result;
}

// ----------------------------------------------------------------------------
//
// Cleans up after the test finishes.
//
DWORD
WZCTest_t::
Cleanup(void)
{
    // nothing to do
    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
