#pragma once
#if !defined(__DDFUNC_SETS_H__)
#define __DDFUNC_SETS_H__

// External Dependencies
// ---------------------
#include <QATestUty/TestUty.h>
#include <DDrawUty_Config.h>
#include <DDTest_Base.h>
#include <DDTest_Iterators.h>

namespace Test_Surface_Sets
{
    // Single Surface Sets
    class CTest_All : virtual public Test_Surface_Iterators::CIterateSurfaces
    {
    public:
        virtual eTestResult PreSurfaceTest();
    };
};
namespace Test_Mode_Sets
{
    class CTest_All : virtual public Test_DDraw_Iterators::CIterateDDraw
    {
    public:
        virtual eTestResult PreDDrawTest();
    };
};
    
#endif // header protection


