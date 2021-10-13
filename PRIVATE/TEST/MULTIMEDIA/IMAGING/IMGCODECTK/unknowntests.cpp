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
#include "UnknownTests.h"
#include <windows.h>
#include <stdlib.h>
#include "main.h"
#include "ImagingHelpers.h"
#include <string>
#include <list>
#include <exception>

using namespace std;
using namespace ImagingHelpers;

namespace UnknownTests
{
    namespace priv_QueryInterface
    {
        HRESULT TestNullPointer(IUnknown* pUnknown, const GUID * pguid)
        {
            HRESULT hr = E_FAIL;
            __try {
                hr = pUnknown->QueryInterface(*pguid, NULL);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                info(FAIL, TEXT("QueryInterface excepted with NULL pointer"));
                info(DETAIL, _LOCATIONT);
            }
            return hr;
        }
    }
    INT TestQueryInterface(IUnknown* pUnknown, GuidPList lstExpected, GuidPList lstUnexpected)
    {
        void * pvoid;
        int iIndex;
        const int cInvalidIterations = 1000;
        GUID guid;
        HRESULT hr;
        GuidPList::iterator gplIter;

        if (g_fBadParams)
        {
            info(ECHO | INDENT, TEXT("Testing invalid parameters"));
            VerifyHRFailure(priv_QueryInterface::TestNullPointer(pUnknown, *(lstExpected.begin())));
            info(UNINDENT, NULL);
        }

        // First, try some almost guaranteed bad guids
        info(ECHO | INDENT, TEXT("Testing invalid guids"));
        for (iIndex = 0; iIndex < cInvalidIterations; iIndex++)
        {
            CreateRandomGuid(&guid);
            pvoid = PTRPRESET;
            hr = pUnknown->QueryInterface(guid, &pvoid);
            if (!FAILED(hr))
            {
                info(FAIL, TEXT("QueryInterface succeeded for random invalid guid: ") _GUIDT,
                    _GUIDEXP(guid));
                info(DETAIL, _LOCATIONT);
                static_cast<IUnknown*>(pvoid)->Release();
            }
            else if (PTRPRESET != pvoid && pvoid)
            {
                info(FAIL, TEXT("QueryInterface set pointer (%p) for random invalid guid: ") _GUIDT,
                    pvoid,
                    _GUIDEXP(guid));
                info(DETAIL, _LOCATIONT);
                static_cast<IUnknown*>(pvoid)->Release();
            }
        }
        info(UNINDENT, NULL);

        info(ECHO | INDENT, TEXT("Testing known but unsupported guids"));
        for (gplIter = lstUnexpected.begin(); gplIter != lstUnexpected.end(); gplIter++)
        {
            
            pvoid = PTRPRESET;
            hr = pUnknown->QueryInterface(**gplIter, &pvoid);
            if (!FAILED(hr))
            {
                info(FAIL, TEXT("QueryInterface succeeded for known invalid guid: ") _GUIDT,
                    _GUIDEXP(**gplIter));
                info(DETAIL, _LOCATIONT);
                static_cast<IUnknown*>(pvoid)->Release();
            }
            else if (PTRPRESET != pvoid && pvoid)
            {
                info(FAIL, TEXT("QueryInterface set pointer (%p) for known invalid guid: ") _GUIDT,
                    pvoid,
                    _GUIDEXP(**gplIter));
                info(DETAIL, _LOCATIONT);
                static_cast<IUnknown*>(pvoid)->Release();
            }
        }
        info(UNINDENT, NULL);

        info(ECHO | INDENT, TEXT("Testing known valid guids"));
        for (gplIter = lstExpected.begin(); gplIter != lstExpected.end(); gplIter++)
        {
            pvoid = PTRPRESET;
            hr = pUnknown->QueryInterface(**gplIter, &pvoid);
            if (FAILED(hr))
            {
                info(FAIL, TEXT("QueryInterface failed for known good guid: ") _GUIDT
                    TEXT(" with hr = 0x%08x"), _GUIDEXP(**gplIter), hr);
                info(DETAIL, _LOCATIONT);
            }
            else if (PTRPRESET == pvoid || !pvoid)
            {
                info(FAIL, TEXT("QueryInterface did not set pointer for known good guid: ") _GUIDT
                    TEXT(" with hr = 0x%08x"), _GUIDEXP(**gplIter), hr);
                info(DETAIL, _LOCATIONT);
            }
            else
                static_cast<IUnknown*>(pvoid)->Release();
        }
        info(UNINDENT, NULL);
        return getCode();
    }
    INT TestAddRefRelease(IUnknown* pUnknown)
    {
        ULONG   ulRef1,
                ulRef2,
                ulRef3,
                ulRef4,
                ulRef5,
                ulRef6;
        bool    bSuccess = true;

        if (NULL == pUnknown)
        {
            info(ABORT, TEXT("TestAddRefRelease called with NULL unknown pointer"));
            info(DETAIL, _LOCATIONT);
            return getCode();
        }
        
        info(ECHO | INDENT, TEXT("Testing AddRef and Release"));
        // Test AddRef
        __try
        {
            ulRef1 = pUnknown->AddRef();
        }
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
            info(FAIL, TEXT("Exception with AddRef"));
            info(DETAIL, _LOCATIONT);
            bSuccess = false;
        }
    
        // Test Release
        __try
        {
            ulRef2 = pUnknown->Release();
        }
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
            info(FAIL, TEXT("Exception with AddRef"));
            info(DETAIL, _LOCATIONT);
            bSuccess = false;
        }
    
        // If we crashed, there is no point in continuing
        if(!bSuccess)
            return getCode();
    
        // If we call AddRef again, we should get the same reference count as before
        PREFAST_ASSUME(pUnknown);
        ulRef3 = pUnknown->AddRef();
    
        if(ulRef3 != ulRef1)
        {
            info(
                FAIL,
                TEXT("Unknown::AddRef was expected to return %d")
                TEXT(" (returned %d)"),
                ulRef1,
                ulRef3);
            info(DETAIL, _LOCATIONT);
            bSuccess = false;
        }
    
        // If we call AddRef another time, we should get a larger reference count
        ulRef5 = pUnknown->AddRef();
        if(ulRef5 <= ulRef3)
        {
            info(
                FAIL,
                TEXT("Unknown::AddRef was expected to increment the")
                TEXT(" reference count (was %d, returned %d)"),
                ulRef3,
                ulRef5);
            info(DETAIL, _LOCATIONT);
            bSuccess = false;
        }
    
        // Release, too, should return something larger than before. Save this value
        // for later comparison.
        ulRef6 = pUnknown->Release();
        ulRef4 = pUnknown->Release();
    
        if(ulRef4 != ulRef2)
        {
            info(
                FAIL,
                TEXT("Unknown::Release was expected to return %d")
                TEXT(" (returned %d)"),
                ulRef2,
                ulRef4);
            info(DETAIL, _LOCATIONT);
            bSuccess = false;
        }
    
        if(ulRef4 >= ulRef6)
        {
            info(
                FAIL,
                TEXT("Unknown::Release was expected to decrement the")
                TEXT(" reference count (was %d, returned %d)"),
                ulRef6,
                ulRef4);
            info(DETAIL, _LOCATIONT);
            bSuccess = false;
        }
    
        info(UNINDENT, NULL);
        return getCode();
    }
}
