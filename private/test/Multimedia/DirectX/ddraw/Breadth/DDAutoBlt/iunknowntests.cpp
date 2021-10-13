#include "main.h"

#include <DDrawUty.h>
#include <QATestUty/WinApp.h>

#include "IUnknownTests.h"

using namespace com_utilities;
using namespace DebugStreamUty;
using namespace DDrawUty;

namespace Test_IUnknown
{
    eTestResult TestAddRefRelease(IUnknown *punk)
    {
        // NOTE: This is stolen from the DSound BVT.
        const int iDepth = 4,
                  iNumPasses = 2;
        ULONG ulRefCountFromAddRef[iNumPasses][iDepth],
              ulRefCountFromRelease[iNumPasses][iDepth];
        eTestResult tr = trPass;
        int i, j;

        // Test AddRef
        dbgout << "Calling AddRef" << endl;
        __try
        {
            ulRefCountFromAddRef[0][0] = punk->AddRef();
        }
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
            dbgout << "AddRef threw an exception" << endl;
            return trException;
        }

        // Test Release
        dbgout << "Calling Release" << endl;;
        __try
        {
            ulRefCountFromRelease[0][0] = punk->Release();
        }
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
            dbgout << "Release threw an exception" << endl;
            return trException;
        }

        for (j=0; j<iNumPasses; j++)
        {
            dbgout << "Executing pass " << j + 1 << "..." << endl;

            for (i=0; i<iDepth; i++)
            {
                dbgout << "AddRef, iteration #" << i;
                ulRefCountFromAddRef[j][i] = punk->AddRef();
                dbgout << " returned " << ulRefCountFromAddRef[j][i] << endl;
            }

            for (i=0; i<iDepth; i++)
            {
                dbgout << "Release, iteration #" << i;
                ulRefCountFromRelease[j][iDepth - i - 1] = punk->Release();
                dbgout << " returned " << ulRefCountFromRelease[j][iDepth - i - 1] << endl;
            }
        }

        dbgout << "Analyzing reference count information within single pass..." << endl;

        for (j=0; j<iNumPasses; j++)
        {
            for (i=1; i<iDepth; i++)
            {
                if (ulRefCountFromRelease[j][i] >= ulRefCountFromAddRef[j][i])
                {
                    dbgout  << "Release RefCount[" << j << "," << i << "]="
                            << ulRefCountFromRelease[j][i] << " expected < "
                            << ulRefCountFromAddRef[j][i] << FILELOC << endl;
                    tr |= trFail;
                }
            }
        }

        dbgout << "Comparing reference count information between passes..." << endl;

        for (i=0; i<iDepth; i++)
        {
            if (ulRefCountFromAddRef[0][i] != ulRefCountFromAddRef[1][i])
            {
                dbgout  << "Reference count from AddRef[" << i << "] between pass"
                        << "1 and 2 do not match (" << ulRefCountFromRelease[0][i]
                        << ", " << ulRefCountFromAddRef[1][i] << ")" << FILELOC << endl;
                tr|=trFail;
            }
        }

        return tr;
    }

    eTestResult TestQueryInterface(IUnknown *punk, const std::vector<IID> &vectValidIIDs, const std::vector<IID> &vectInvalidIIDs)
    {
        std::vector<IID>::const_iterator itr;
        eTestResult tr = trPass;
        IUnknown *punkOut,
                 *punkOut2,
                 *punkOutUnk,
                 *punkOutUnk2;
        HRESULT hr;
        IID iid;

        // Test for Valid interfaces
        // ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
        dbgout << "Testing Valid IIDs" << endl;
        for (itr = vectValidIIDs.begin(); itr != vectValidIIDs.end(); itr++)
        {
            iid = *itr;

            dbgout << "Testing QueryInterface(" << g_IIDMap[iid] << ") " << iid << endl;

            // Perform one QI
            PresetUty::Preset(punkOut);
            hr = punk->QueryInterface(iid, (void**)&punkOut);
            CheckHRESULT(hr, "QueryInterface", trFail);
            CheckCondition(PresetUty::IsPreset(punkOut), "QI succeeded but didn't fill out interface", trFail);
            CheckCondition(NULL == punkOut, "QI succeeded but NULLed out interface", trFail);

            // Make sure the resulting interface is QI-able
            PresetUty::Preset(punkOut2);
            hr = punkOut->QueryInterface(iid, (void**)&punkOut2);
            CheckHRESULT(hr, "QueryInterface", trFail);
            CheckCondition(PresetUty::IsPreset(punkOut2), "QI succeeded but didn't fill out interface", trFail);
            CheckCondition(NULL == punkOut2, "QI succeeded but NULLed out interface", trFail);

            // Do some RefCount Checking
            ULONG lCountOut  = punkOut->AddRef(),
                  lCountOut2 = punkOut2->AddRef();

            // Undo the AddRefs in a different order than we got them
            punkOut->Release();
            punkOut2->Release();

            if(!(lCountOut < lCountOut2))
            {
                dbgout  << "Refcounts don't make sense" << endl
                        << "lCountOut =  " << lCountOut  << endl
                        << "lCountOut2 = " << lCountOut2 << endl;

                return trFail;
            }

            // Get the IUnknown interfaces back
            PresetUty::Preset(punkOutUnk);
            hr = punkOut->QueryInterface(IID_IUnknown, (void**)&punkOutUnk);
            CheckHRESULT(hr, "QueryInterface", trFail);
            CheckCondition(PresetUty::IsPreset(punkOutUnk), "QI succeeded but didn't fill out interface", trFail);
            CheckCondition(NULL == punkOutUnk, "QI succeeded but NULLed out interface", trFail);

            // Make sure the resulting interface is QI-able
            PresetUty::Preset(punkOutUnk2);
            hr = punkOut2->QueryInterface(IID_IUnknown, (void**)&punkOutUnk2);
            CheckHRESULT(hr, "QueryInterface", trFail);
            CheckCondition(PresetUty::IsPreset(punkOutUnk2), "QI succeeded but didn't fill out interface", trFail);
            CheckCondition(NULL == punkOutUnk2, "QI succeeded but NULLed out interface", trFail);

            // Verify that the IUnknowns that we got back are the same
            CheckCondition(punkOutUnk != punkOutUnk2, "Returned IUnknowns Differ", trFail);

            // Release the IUnknowns
            punkOutUnk->Release();
            punkOutUnk2->Release();
            
            // Release the interfaces different order than we got them
            punkOut->Release();
            punkOut2->Release();
            
        }
             
        // Test for Invalid interfaces
        // ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
        dbgout << "Testing Invalid IIDs" << endl;
        for (itr = vectInvalidIIDs.begin(); itr != vectInvalidIIDs.end(); itr++)
        {
            iid = *itr;

            dbgout << "Testing QueryInterface(" << g_IIDMap[iid] << ") " << iid << endl;
            
            PresetUty::Preset(punkOut);
            hr = punk->QueryInterface(iid, (void**)&punkOut);
            dbgout << "Returned " << g_ErrorMap[hr] << endl;
            CheckForHRESULT(hr, E_NOINTERFACE, "QueryInterface", trFail);
            CheckCondition(NULL != punkOut, "QI failed, but didn't NULL out interface", trFail);
        }
        
        return tr;
    }
}


