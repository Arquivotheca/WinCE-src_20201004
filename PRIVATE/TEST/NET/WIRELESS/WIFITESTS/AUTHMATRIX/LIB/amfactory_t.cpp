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
// Implementation of the AMFactory_t class.
//
// ----------------------------------------------------------------------------

#include "AMFactory_t.hpp"

#include <assert.h>
#include <cmdline.h>

#include <WiFiBase_t.hpp>

#include "AuthMatrix_t.hpp"
#include "WZCTest_t.hpp"

// DLL name:
TCHAR *g_pDllName = TEXT("AuthMatrix");

using namespace ce;
using namespace ce::qa;

// ----------------------------------------------------------------------------
//
// Creates the singleton Factory instance.
//
Factory_t *
Factory_t::
CreateInstance(void)
{
    return new AMFactory_t;
}

// ----------------------------------------------------------------------------
//
// Constructor.
//
AMFactory_t::
AMFactory_t(void)
    : Factory_t()
{
    WiFiBase_t  ::StartupInitialize();
    WZCTest_t   ::StartupInitialize();
    AuthMatrix_t::StartupInitialize();
}

// ----------------------------------------------------------------------------
//
// Destructor.
//
AMFactory_t::
~AMFactory_t(void)
{
    AuthMatrix_t::ShutdownCleanup();
    WZCTest_t   ::ShutdownCleanup();
    WiFiBase_t  ::ShutdownCleanup();
}

// ----------------------------------------------------------------------------
//
// Displays the command-argument options.
//
void
AMFactory_t::
PrintUsage(void) const
{
    WiFiBase_t  ::PrintUsage();
    WZCTest_t   ::PrintUsage();
    AuthMatrix_t::PrintUsage();
}

// ----------------------------------------------------------------------------
//
// Parses the DLL's command-line arguments.
//
DWORD
AMFactory_t::
ParseCommandLine(int argc, TCHAR *argv[])
{
    DWORD result = WiFiBase_t::ParseCommandLine(argc, argv);

    if (ERROR_SUCCESS == result)
        result = WZCTest_t::ParseCommandLine(argc, argv);

    if (ERROR_SUCCESS == result)
        result = AuthMatrix_t::ParseCommandLine(argc, argv);
    
    return result;
}

// ----------------------------------------------------------------------------
//
// Generates the function-table for this DLL.
//
DWORD
AMFactory_t::
CreateFunctionTable(void)
{
    DWORD result = WZCTest_t::AddFunctionTable(this);

    if (ERROR_SUCCESS == result)
        result = AuthMatrix_t::AddFunctionTable(this);

    if (ERROR_SUCCESS == result)
        result = AddFunctionTableEntry(NULL, 0, 0, 0);

    return result;
}

// ----------------------------------------------------------------------------
//
// Generates the WiFiBase object for processing the specified test.
//
DWORD
AMFactory_t::
CreateTest(
    const FUNCTION_TABLE_ENTRY *pFTE,
    WiFiBase_t                **ppTest)
{
    if (pFTE->dwUniqueID >= WZCTest_t::TestIdStart
     && pFTE->dwUniqueID <= WZCTest_t::TestIdEnd)
    {
        return WZCTest_t::CreateTest(pFTE, ppTest);
    }
    else
    if (pFTE->dwUniqueID >= AuthMatrix_t::TestIdStart
     && pFTE->dwUniqueID <= AuthMatrix_t::TestIdEnd)
    {
        return AuthMatrix_t::CreateTest(pFTE, ppTest);
    }
    else
    {
        LogError(TEXT("Test ID #%u is unknown"), pFTE->dwUniqueID);
        return ERROR_INVALID_FUNCTION;
    }
}

// ----------------------------------------------------------------------------
