#pragma once
#if !defined(__IUNKNOWNTESTS_H__)
#define __IUNKNOWNTESTS_H__

// External Dependencies
// ---------------------
#include <QATestUty/TestUty.h>

namespace Test_IUnknown
{
    eTestResult TestAddRefRelease(IUnknown *punk);
    eTestResult TestQueryInterface(IUnknown *punk, const std::vector<IID> &vectValidIIDs, const std::vector<IID> &vectInvalidIIDs);
};

#endif // header protection

