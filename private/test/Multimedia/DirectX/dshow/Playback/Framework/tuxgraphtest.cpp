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
////////////////////////////////////////////////////////////////////////////////
//
//  GraphTest helper class
//
////////////////////////////////////////////////////////////////////////////////

#include <clparse.h>
#include "logging.h"
#include "TuxGraphTest.h"
#include "TestDescParser.h"
#include "TestGraph.h"

// extern values from test DLLs
extern GraphFunctionTableEntry g_lpLocalFunctionTable[];
extern int g_nLocalTestFunctions;

TestConfig* GraphTest::m_pTestConfig = NULL;

TESTPROCAPI HandleTuxMessages(UINT uMsg, TPPARAM tpParam)
{
    DWORD retval = SPR_NOT_HANDLED;
    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        // not a multi-threaded test
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 0;
        retval = SPR_HANDLED;
    }
    return retval;
}

GraphTest::GraphTest() :     
    m_pFunctionTable(NULL),
    m_nEntries(0)
{
}

GraphTest::~GraphTest()
{
    // Delete the global config object
    if ( m_pTestConfig )
        delete m_pTestConfig;
    m_pTestConfig = NULL;

    // Clear up the TUX function table
    for(int i = 0; i < m_nEntries; i++)
    {
        if (m_pFunctionTable[i].lpDescription)
        {
            delete [] m_pFunctionTable[i].lpDescription;
            m_pFunctionTable[i].lpDescription = NULL;
        }
    }
    if (m_pFunctionTable)
        delete [] m_pFunctionTable;
}

HRESULT GraphTest::ParseCommandLine(const TCHAR* szCmdLine)
{
    HRESULT hr = S_OK;
    
    CClParse cmdLine((LPCTSTR) szCmdLine);
    
    // Parse command line
    if(cmdLine.GetOpt(TEXT("GRF")))
    {
        TestGraph::SaveGraph(true);
    }
    else
        TestGraph::SaveGraph(false);
        

    // Look for the required config file name /Config
    TCHAR szConfigFile[MAX_PATH];
    if(false == cmdLine.GetOptString(TEXT("Config"), szConfigFile, countof(szConfigFile)))
    {
        LOG(TEXT("Config file name required"));
        return E_INVALIDARG;
    }


    // If config file is specified, parse the config file
    m_pTestConfig = ParseConfigFile( szConfigFile );

    // Create the function table - the TPPARAM object contains the informaton about the test
    return ( m_pTestConfig ) ? S_OK : E_FAIL;
}

int GraphTest::GetLocalTestIndex(TCHAR* szTestName)
{
    for(int i = 0; i < g_nLocalTestFunctions; i++)
    {
        if (!_tcscmp(szTestName, g_lpLocalFunctionTable[i].szTestName))
            break;
    }
    return (i < g_nLocalTestFunctions) ? i : -1;
}

HRESULT GraphTest::AssignTestId(TestDescList* pTestDescList, TestGroupList* pTestGroupList)
{
    bool bTestIdBase = false;

    // Check if test group list is present and not empty
    if (pTestGroupList && !pTestGroupList->empty())
    {
        // Check if the first test group has the testIdBase set
        // If not, then we just assume that we are not going to use any testIdBase values
        TestGroupList::iterator groupIterator = pTestGroupList->begin();
        if ((*groupIterator)->testIdBase != -1)
            bTestIdBase = true;
    }

    if (bTestIdBase)
    {
        // If set, then we will expect all test groups to have a testIdBase or use the last testId from the previous group
        INT nextTestId = -1;
        TestGroupList::iterator groupIterator = pTestGroupList->begin();
        while(groupIterator != pTestGroupList->end())
        {
            TestGroup* pTestGroup = *groupIterator;
            // testIdBase will always be set for the first test group and hence nextTestId will be set at the end of the outer loop
            DWORD testIdBase = pTestGroup->testIdBase;
            if (testIdBase == -1)
                testIdBase = nextTestId;

            // The first test id is the testIdBase from the test group
            DWORD testId = testIdBase;
            TestDescList::iterator testIterator = pTestGroup->testDescList.begin();
            // Iterate over the tests - setting test ids
            while(testIterator != pTestGroup->testDescList.end())
            {
                (*testIterator)->testId = testId;
                testId++;
                testIterator++;
            }

            // Save the last test id used in case the next test group doesn't have a testIdBase
            nextTestId = testId;
            
            groupIterator++;
        }
    }
    else {
        // If no test group or testIdBase is not set, set the starting testId to 1 and increment
        DWORD testId = 1;
        TestDescList::iterator testIterator = pTestDescList->begin();
        while(testIterator != pTestDescList->end())
        {
            (*testIterator)->testId = testId;
            testId++;
            testIterator++;
        }
    }

    return S_OK;
}

void GraphTest::AddToFunctionTable(FUNCTION_TABLE_ENTRY* fte, TCHAR* szDesc, unsigned int depth, DWORD dwUserData, DWORD dwUniqueId, TESTPROC lpTestProc)
{
    fte->lpDescription = szDesc;
    fte->uDepth = depth;
    fte->dwUserData = dwUserData;
    fte->dwUniqueID = dwUniqueId;
    fte->lpTestProc = lpTestProc;
}

HRESULT GraphTest::AddGroupToFunctionTable(FUNCTION_TABLE_ENTRY *fte, TestGroup* pTestGroup, int base, unsigned int depth)
{
    // Clone the name
    TCHAR* szDesc = new TCHAR[TEST_DESC_LENGTH];
    if (!szDesc)
        return E_OUTOFMEMORY;
    errno_t  Error = 0;
    Error = _tcscpy_s (szDesc,TEST_DESC_LENGTH, pTestGroup->szGroupName);
    if(Error != 0)
    {
        LOG(TEXT("GraphTest::AddGroupToFunctionTable - _tcscpy_s Error)"));
    }

    // Add the group
    AddToFunctionTable(&fte[base], szDesc, depth, 0, 0, NULL);
    base++;

    // Add the tests with depth incremented
    AddTestsToFunctionTable(&fte[0], pTestGroup->GetTestDescList(), base, depth + 1);

    return S_OK;
}

HRESULT GraphTest::AddTestsToFunctionTable(FUNCTION_TABLE_ENTRY *fte, TestDescList* pTestDescList, int base, unsigned int depth)
{
    HRESULT hr = E_FAIL;
    int nTests = pTestDescList->size();
    errno_t  Error = 0;
    for(int i = 0; i < nTests; i++)
    {
        TestDesc* pTestDesc = pTestDescList->at(i);
        // Get the local test index
        int test = GetLocalTestIndex(pTestDesc->szTestName);
        if (test == -1)
        {
            LOG(TEXT("ERROR: Could not map test name %s"), pTestDesc->szTestName);
            hr = E_FAIL;
            break;
        }

        // Clone the name
        TCHAR* szDesc = new TCHAR[TEST_DESC_LENGTH];
        if (!szDesc)
            return E_FAIL;

        _tcscpy_s (szDesc, TEST_DESC_LENGTH,g_lpLocalFunctionTable[test].szTestDesc);
        if(Error != 0)
        {
            LOG(TEXT("GraphTest::AddGroupToFunctionTable - _tcscpy_s Error)"));
        }

        // Cat a separator
        _tcscat_s (szDesc,TEST_DESC_LENGTH,TEXT("-"));
        if(Error != 0)
        {
            LOG(TEXT("GraphTest::AddGroupToFunctionTable - _tcscat_s Error)"));
        }

        // Cat the test desc in the test to the local test desc
        _tcscat_s (szDesc,TEST_DESC_LENGTH,pTestDesc->szTestDesc);
        if(Error != 0)
        {
            LOG(TEXT("GraphTest::AddGroupToFunctionTable - _tcscat_s Error)"));
        }

        // Add to the function table at the right index
        AddToFunctionTable(&fte[base + i], szDesc, depth, (DWORD)pTestDesc, pTestDesc->testId, g_lpLocalFunctionTable[test].lpTestProc);
    }

    return hr;
}

HRESULT GraphTest::GetTuxFunctionTable(FUNCTION_TABLE_ENTRY** ppFTE)
{
    HRESULT hr = S_OK;

    // If the config file hasn't been parsed return an error
    if ( !m_pTestConfig )
        return E_FAIL;

    TestGroupList* pTestGroupList = m_pTestConfig->GetTestGroupList();
    TestDescList* pTestDescList = m_pTestConfig->GetTestDescList();

    int nTests = pTestDescList->size();
    int nGroups = pTestGroupList->size();
    int nEntries = 0;
    
    if (nGroups)
        nEntries = nTests + nGroups + 1;
    else 
        nEntries = nTests + 2;

    // Assign test ids if requested
    if ( m_pTestConfig->GenerateId() )
        AssignTestId(pTestDescList, pTestGroupList);

    // Allocate the function table
    FUNCTION_TABLE_ENTRY* fte = new FUNCTION_TABLE_ENTRY[nEntries];
    if (!fte)
    {
        LOG(TEXT("%S: ERROR %d@%s - failed to allocate Tux function table."), __FUNCTION__, __LINE__, __FILE__);
        return E_OUTOFMEMORY;
    }

    int base = 0;
    unsigned int depth = 0;
    if (nGroups)
    {
        for(int i = 0; i < nGroups; i++)
        {
            TestGroup* pTestGroup = pTestGroupList->at(i);
            TestDescList* pGroupTestList = pTestGroup->GetTestDescList();

            // Add the group with a depth of 0?
            AddGroupToFunctionTable(&fte[0], pTestGroup, base, depth);

            // Move the base index
            base += pGroupTestList->size() + 1;
        }
    }
    else {
        // Add the starting group entry
        TCHAR* szDesc = new TCHAR[TEST_DESC_LENGTH];
        errno_t  Error;
        if (!szDesc)
            return E_OUTOFMEMORY;
        Error = _tcscpy_s (szDesc,TEST_DESC_LENGTH, TEXT("Graph Test"));
        if(Error != 0)
        {
            LOG(TEXT("GraphTest::GetTuxFunctionTable - _tcscpy_s Error)"));
        }
        AddToFunctionTable(&fte[0], szDesc, 0, 0, 0, NULL);
        AddTestsToFunctionTable(&fte[1], pTestDescList, 0, depth + 1);
    }

    // Add the culminating entry
    AddToFunctionTable(&fte[nEntries - 1], NULL, 0, 0, 0, NULL);

    if (SUCCEEDED(hr))
    {
        *ppFTE = m_pFunctionTable = fte;
        m_nEntries = nEntries;
    }
    else {
        if (fte)
            delete [] fte;
        *ppFTE = NULL;
    }

    return hr;
}

TESTPROCAPI PlaceholderTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    // This shouldn't actually be called
    return TPR_SKIP;
}
