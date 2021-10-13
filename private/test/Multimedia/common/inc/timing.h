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

#pragma once
#include <limits>
#include <assert.h>

namespace Timing
{
    #pragma push_macro("max")
    #pragma push_macro("min")
    #undef max
    #undef min

    template<typename T>
    inline T GetElapsed(T& start, T& end)
    {
        assert(start >= std::numeric_limits<T>::min());

        if (end < start) // overflow
            return (std::numeric_limits<T>::max() - start) +
                (end - std::numeric_limits<T>::min());
        else // no overflow
            return end - start;
    }

    #pragma pop_macro("max")
    #pragma pop_macro("min")

    inline DWORD GetLowPrecisionElapsed(DWORD start, DWORD* end = NULL)
    {
        DWORD time = timeGetTime();
        if (end) *end = time;
        return GetElapsed(start, time);
    }

    inline LONGLONG GetHighPrecisionElapsed(LARGE_INTEGER& start)
    {
        LARGE_INTEGER end;
        BOOL b = ::QueryPerformanceCounter(&end);
        assert(b);
        return GetElapsed(start.QuadPart, end.QuadPart);
    }
};