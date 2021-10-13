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
// Implementation of the Factory_t class.
//
// ----------------------------------------------------------------------------

#include "Factory_t.hpp"

#include <assert.h>

#include "WiFiBase_t.hpp"

using namespace ce;
using namespace ce::qa;

// Singleton factory instance:
static Factory_t *s_pSingletonInstance = NULL;

// ----------------------------------------------------------------------------
//
// Constructor.
//
Factory_t::
Factory_t(void)
    : m_pFunctionTable(NULL),
      m_FunctionTableSize(0),
      m_FunctionTableAlloc(0)
{
    // nothing to do
}

// ----------------------------------------------------------------------------
//
// Destructor.
//
Factory_t::
~Factory_t(void)
{
    ClearFunctionTable();
}

// ----------------------------------------------------------------------------
//
// Creates or deletes the singleton instance.
//
Factory_t *
Factory_t::
GetInstance(void)
{
    if (NULL == s_pSingletonInstance)
    {
        s_pSingletonInstance = CreateInstance();
        assert(NULL != s_pSingletonInstance);
    }
    return s_pSingletonInstance;
}

void
Factory_t::
DeleteInstance(void)
{
    if (NULL != s_pSingletonInstance)
    {
        delete s_pSingletonInstance;
        s_pSingletonInstance = NULL;
    }
}

// ----------------------------------------------------------------------------
//
// Called by TuxMain when a new test group is being started.
//
DWORD
Factory_t::
OnBeginGroup(void)
{
    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Called by TuxMain when the test group is finished.
//
DWORD
Factory_t::
OnEndGroup(void)
{
    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Destructor.
//
void
Factory_t::
ClearFunctionTable(void)
{
    if (m_pFunctionTable)
    {
        for (int fpx = m_FunctionTableSize ; --fpx >= 0 ;)
        {
            FUNCTION_TABLE_ENTRY *pFPE = &m_pFunctionTable[fpx];
            if (pFPE->lpDescription)
            {
                free((void *)(pFPE->lpDescription));
                pFPE->lpDescription = NULL;
            }
        }
        free(m_pFunctionTable);
        m_pFunctionTable = NULL;
    }
    m_FunctionTableSize = m_FunctionTableAlloc = 0;
}

// ----------------------------------------------------------------------------
// 
// Retrieves or generates the function-table for this DLL.
//
DWORD
Factory_t::
GetFunctionTable(FUNCTION_TABLE_ENTRY **ppFunctionTable)
{
    DWORD result;

    if (NULL == m_pFunctionTable)
    {
        result = CreateFunctionTable();
        if (ERROR_SUCCESS != result)
        {
            ClearFunctionTable();
            return result;
        }
    }

   *ppFunctionTable = m_pFunctionTable;
    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// This method is used by concrete test classes to add their tests
// to the function-table.
//
DWORD
Factory_t::
AddFunctionTableEntry(
    const TCHAR *Description,  // description of test
    UINT         Depth,        // depth of item in tree hierarchy
    DWORD        UserData,     // data to be passed to TestProc at runtime
    DWORD        UniqueID)     // uniquely identifies the test
{
    if (m_FunctionTableAlloc <= m_FunctionTableSize)
    {
        int newAlloc = m_FunctionTableAlloc * 2;
        if (newAlloc == 0)
            newAlloc = 64;

        size_t allocSize = newAlloc * sizeof(FUNCTION_TABLE_ENTRY);
        void *newTable = malloc(allocSize);

        if (newTable == NULL)
        {
            LogError(TEXT("Can't allocate %u bytes for function table"),
                     allocSize);
            return ERROR_OUTOFMEMORY;
        }

        if (m_pFunctionTable)
        {
            allocSize = m_FunctionTableAlloc * sizeof(FUNCTION_TABLE_ENTRY);
            memcpy(newTable, m_pFunctionTable, allocSize);
            free(m_pFunctionTable);
        }

        m_pFunctionTable = (FUNCTION_TABLE_ENTRY *)newTable;
        m_FunctionTableAlloc = newAlloc;
    }

    FUNCTION_TABLE_ENTRY fpe;
    memset(&fpe, 0, sizeof(FUNCTION_TABLE_ENTRY));
    fpe.uDepth = Depth;
    fpe.dwUserData = UserData;
    fpe.dwUniqueID = UniqueID;
    fpe.lpTestProc = TestProc;

    if (NULL != Description)
    {
        size_t descSize = (_tcslen(Description) + 1) * sizeof(TCHAR);
        fpe.lpDescription = (LPCTSTR) malloc(descSize);
        if (NULL == fpe.lpDescription)
        {
            LogError(TEXT("Can't allocate %u bytes for test description"),
                     descSize);
            return ERROR_OUTOFMEMORY;
        }
        memcpy((void *)(fpe.lpDescription), Description, descSize);
    }

    for (int next = m_FunctionTableSize-1, insert = m_FunctionTableSize ; ;
           --next, --insert)
    {
        if (UniqueID == 0 || next < 0
         || m_pFunctionTable[next].dwUniqueID < UniqueID)
        {
            m_pFunctionTable[insert] = fpe;
            break;
        }
        if (m_pFunctionTable[next].dwUniqueID == UniqueID)
        {
            LogError(TEXT("Duplicate test ID #%u \"%s\""), UniqueID,
                     Description? Description : TEXT("no test description"));
            return ERROR_INVALID_DATA;
        }
        m_pFunctionTable[insert] = m_pFunctionTable[next];
    }

    m_FunctionTableSize++;
    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Standard test-function for the function-table; Extracts the test-id
// from the specified function-table entry, generates the appropriate
// WiFiBase object and uses it to process the test.
//
TESTPROCAPI
Factory_t::
TestProc(
    UINT uMsg,                  // TPM_QUERY_THREAD_COUNT or TPM_EXECUTE
    TPPARAM tpParam,            // TPS_QUERY_THREAD_COUNT or not used
    FUNCTION_TABLE_ENTRY *pFTE) // test to be executed
{
    DWORD result;
    int retval = TPR_PASS;

    if (uMsg == TPM_QUERY_THREAD_COUNT)
    {
        assert(NULL != (TPS_QUERY_THREAD_COUNT *)tpParam);
        ((TPS_QUERY_THREAD_COUNT *)tpParam)->dwThreadCount = pFTE->dwUserData;
        return SPR_HANDLED;
    }
    else
    if (uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    assert(NULL != pFTE);

    WiFiBase_t *pTest;
    result = GetInstance()->CreateTest(pFTE, &pTest);
    if (ERROR_SUCCESS != result)
        return TPR_ABORT;

    assert(NULL != pTest);

    _try
    {
        result = pTest->Init();
        if (ERROR_SUCCESS == result)
            result = pTest->Run();
        switch (result)
        {
            case ERROR_CALL_NOT_IMPLEMENTED: retval = TPR_SKIP; break;
            case ERROR_SUCCESS:              retval = TPR_PASS; break;
            default:                         retval = TPR_FAIL; break;
        }
    }
    __except(1)
    {
        const TCHAR *desc = pFTE->lpDescription;
        LogError(TEXT("Test #%u \"%s\" caused an exception"),
                 pFTE->dwUniqueID, desc? desc : TEXT("no test decsription"));
        retval = TPR_ABORT;
    }

    result = pTest->Cleanup();
    if (retval == TPR_PASS && result != ERROR_SUCCESS)
        retval = TPR_FAIL;
    
    delete pTest;
    return retval;
}

// ----------------------------------------------------------------------------
