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
// Definitions and declarations for the Factory_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_Factory_t_
#define _DEFINED_Factory_t_
#pragma once

#ifndef DEBUG
#ifndef NDEBUG
#define NDEBUG
#endif
#endif

#include <windows.h>
#include <tux.h>

// This global variable must be defined by the concrete Factory class:
extern TCHAR *g_pDllName;

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Factory is an abstract base class defining an interface providing a
// generic Tux ShellProc with a machanism for processing any number of
// specialized tests.
// 
// Test DLLs are implemented by deriving a concrete version of this class
// which generates a specific type of test objects.
//
class WiFiBase_t;
class Factory_t
{
private:

    // Tux function table:
    FUNCTION_TABLE_ENTRY *m_pFunctionTable;
    int                   m_FunctionTableSize;
    int                   m_FunctionTableAlloc;

    // Copy and assignment are deliberately disabled:
    Factory_t(const Factory_t &src);
    Factory_t &operator = (const Factory_t &src);

public:

    // Create or delete the singleton instance:
    static Factory_t *GetInstance(void);
    static void    DeleteInstance(void);

    // Constructor and destructor:
    Factory_t(void);
    virtual
   ~Factory_t(void);

    // Called by TuxMain when a test group is starting or finished:
    // The default implementation does nothing.
    virtual DWORD OnBeginGroup(void);
    virtual DWORD OnEndGroup(void);

    // Parses the DLL's command-line arguments:
    virtual void  PrintUsage(void) const = 0;
    virtual DWORD ParseCommandLine(int argc, TCHAR *argv[]) = 0;

    // Clears the function-table:
    void
    ClearFunctionTable(void);

    // Retrieves or generates the function-table for this DLL:
    DWORD
    GetFunctionTable(FUNCTION_TABLE_ENTRY **ppFunctionTable);

    // This method is used by concrete test classes to add their tests
    // to the function-table:
    DWORD
    AddFunctionTableEntry(
        const TCHAR *Description,  // description of test
        UINT         Depth,        // depth of item in tree hierarchy
        DWORD        UserData,     // data to be passed to TestProc at runtime
        DWORD        UniqueID,     // uniquely identifies the test
        bool        fGroupNode = false);

protected:

    // Generates the function-table for this DLL:
    virtual DWORD
    CreateFunctionTable(void) = 0;

    // Generates the WiFiBase object for processing the specified test:
    virtual DWORD
    CreateTest(
        const FUNCTION_TABLE_ENTRY *pFTE,
        WiFiBase_t                **ppTest) = 0;

private:

    // Standard test-function for the function-table; Extracts the test-id
    // from the specified function-table entry, generates the appropriate
    // WiFiBase object and uses it to process the test:
    static TESTPROCAPI
    TestProc(
        UINT uMsg,                   // TPM_QUERY_THREAD_COUNT or TPM_EXECUTE
        TPPARAM tpParam,             // TPS_QUERY_THREAD_COUNT or not used
        FUNCTION_TABLE_ENTRY *pFTE); // test to be executed

    // Creates the singleton Factory instance:
    // This method is NOT implemented in Factory_t.cpp. Instead, that must
    // be done by the contrete factory class to create the appropriate type
    // of object.
    static Factory_t *
    CreateInstance(void);
};

};
};
#endif /* _DEFINED_Factory_t_ */
// ----------------------------------------------------------------------------
