//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#pragma once
#pragma warning(disable: 4512)

#ifndef __TESTUTY_H__
#define __TESTUTY_H__

// External Dependencies
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
#include "ITest.h"
#include "DebugUty.h"
#include <Tux.h>
#include <DebugStreamUty.h>

namespace TestUty_ver1
{
    // ———————————
    // class CTest
    // ———————————
    class CTest : virtual public ITest
    {
    protected:
        LPCTSTR m_pszTestName;
        LPCTSTR m_pszTestDescription;
        DWORD m_dwThreadCount;
        DWORD m_dwRandomSeed;
        DWORD m_dwThreadNumber;

        virtual void SetRandSeed(DWORD dwSeed) {}; // For compatability with legacy code; 
                                                // the default behavior is to do nothing
                                        
    public:
        CTest();
        virtual ~CTest();

        virtual eTestResult Execute();   // Implements Execute from ITest
                                         // This function calls (1) Startup (2) Test and then (3) Shutdown
                                         // NOTE: There is usually no need to override Execute

        virtual bool Startup();          // Override to implement custom statup code.
                                         //     return false to abort the test, true to continue


        virtual eTestResult Test() = 0;  // Provide the implementation of your test

        virtual bool Shutdown();         // Override to implement custom shutdown code.
                                         //     This method will always be called after Test() is called
                                         //     return false to abort the test

        virtual LPCTSTR GetTestName();   // Implements GetTestName from ITest.  Returns m_pszTestName
                                         // NOTE: There is usually no need to override GetTestName

        virtual void HandleStructuredException(DWORD dwExceptionCode);  // This is the trap point for 
                                                                        // unhandled structured exceptions
                                                                        // that may occur in the test.  
                                                                        // Override to implement custom SEH

        virtual void Initialize(DWORD dwThreadCount, DWORD dwRandomSeed, DWORD dwThreadNumber);

        static int sm_nDefaultThreadCount;  // Default thread count (defaults to 1)
    };

    // ————————————————————
    // class CIterativeTest
    // ————————————————————
    class CIterativeTest : virtual public CTest
    {
    protected:
        long m_lIterationCount;                     // indicates the number of iterations to execute; set to zero to 
                                                    // terminate the test without failing
        
        const long m_lCurrentIteration;             // Indicates the current iteration count.  This is a read-only value.
                                                    // Changing it (like with a const_cast) will have no effect.

    public:
        CIterativeTest();

        virtual eTestResult Test();

        virtual bool StartupIteration();            // override to implement custom iteration startup
                                                    //     return false to abort the test, true to continue

        virtual eTestResult TestIteration() = 0;    // provide the implementation of your iterative test code 
                                                    //     return trPass to continue testing, all other
                                                    //     values will discontinue the test.

        virtual bool ShutdownIteration();           // override to implement custom iteration shutdown
                                                    //     This method is guaranteed to be called after every 
                                                    //     call to TestIteration.
                                                    //     return false to abort the test, true to continue

        static long sm_lDefaultIterationCount;       // initialization value for all instances (default = 1)
    };

    // —————————————————————
    // class CMemoryLeakTest
    // —————————————————————
    class CMemoryLeakTest : virtual public CIterativeTest
    {
    protected:
        std::vector<unsigned __int64> m_vullFreeMem, m_vullFragment;

    public:
        virtual bool Startup();                                 // Initializes the FreeMem and Fragment vectors
                                                                // Disables GWES OOM on (DXPak/CEPC configuration only)

        virtual eTestResult TestIteration() = 0;
        
        virtual bool ShutdownIteration();                       // profiles available memory

        virtual bool Shutdown();                                // Calls OutputExcelTable using the default dbgout object

        virtual void OutputExcelTable(DebugStreamUty::CDebugStream& dbgout);    // Outputs a table suitable for importing into MSExcel
    };

    // ——————————————————————————————————————————————
    // function eTestResult_to_TuxTestResult
    //    maps eTestResult to a Tux Test Result value
    // ——————————————————————————————————————————————
    extern UINT eTestResult_to_TuxTestResult(eTestResult tr);

    // ————————————————
    // g_eTestResultMap
    // ————————————————
    extern const DebugUty::flag_string_map g_eTestResultMap;

    // —————————————————
    // CTestClassCreator
    // —————————————————
    template <class TTestClass>
    class CTestClassAllocator
    {
    public:
        static HRESULT CreateInstance(OUT ::ITest** ppiTest)
        {
            // so you might be wondering, "Why don't you just ....
            if (ppiTest==NULL) return E_POINTER;
            *ppiTest = NULL;
            *ppiTest = new TTestClass; 
            if (*ppiTest==NULL) return E_OUTOFMEMORY;
            return S_OK;
        }
    };

    // ————————————————————————————
    // DECLARE_TEST_CLASS_ALLOCATOR
    // ————————————————————————————
#   define DECLARE_TEST_CLASS_ALLOCATOR(CF) typedef CTestClassAllocator<CF> _CTestClassAllocator;

    // ——————————————————
    // Tux Adaptor Macros
    // ——————————————————
    extern TESTPROCAPI TuxTestAdapter(UINT, TPPARAM, LPFUNCTION_TABLE_ENTRY);

#   define TUX_FTE(__DEPTH, __DESC, __ID, __TESTCLASS) \
    TUX_FTE2(__DEPTH, __DESC, __ID, TestUty_ver1::TuxTestAdapter, (DWORD) TestUty::CTestClassAllocator<__TESTCLASS>::CreateInstance )

#   define TUX_FTE2(__DEPTH, __DESC, __ID, __TESTFUNC, __TESTPARAM) \
    { TEXT(__DESC), __DEPTH, __TESTPARAM, __ID, __TESTFUNC },
        
} // namespace TestUty_ver1

namespace TestUty = TestUty_ver1;


// obselete, use g_eTestResultMap
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
inline LPCTSTR TestResult_to_string(eTestResult tr)
{
    return TestUty::g_eTestResultMap[tr].c_str();
}

#endif