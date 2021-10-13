#pragma once
#if !defined(__DDFUNC_API_H__) && CFG_TEST_DirectDrawAPI
#define __DDFUNC_API_H__

// External Dependencies
// ---------------------
#include <QATestUty/TestUty.h>

namespace Test_DirectDrawAPI
{
    class CTest_DirectDrawCreate : virtual public TestUty::CTest
    {
    public:
        virtual eTestResult Test();

    private:
        virtual eTestResult CreateDirectDrawGUID(GUID *lpGUID);

    };
    
#if CFG_TEST_IDirectDrawClipper    
    class CTest_DirectDrawCreateClipper : virtual public TestUty::CTest
    {
    public:
        virtual eTestResult Test();
    };
#endif

    class CTest_DirectDrawEnumerate : virtual public TestUty::CTest
    {
    protected:
        DWORD m_dwPreset;
        eTestResult m_tr;
        int m_nCallbackCount;
        bool m_fCallbackExpected;
        
    public:
        CTest_DirectDrawEnumerate() :
            m_tr(trPass),
            m_nCallbackCount(0),
            m_fCallbackExpected(false)
        {
            PresetUty::Preset(m_dwPreset);
        }
        
        virtual eTestResult Test();
        static HRESULT CALLBACK Callback(LPGUID lpGUID, LPTSTR lpDescription, LPTSTR lpDriverName, LPVOID lpContext, HMONITOR hmonitor);
    };
}

#endif // header protection
