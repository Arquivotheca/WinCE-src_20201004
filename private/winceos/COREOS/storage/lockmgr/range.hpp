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

/*++

Module Name:
    range.hpp

Abstract:
    Range Abstraction.  A range is a 64-bit 2-tuple.

Revision History:

--*/

#ifndef __RANGE_HPP_
#define __RANGE_HPP_

#include <windows.h>
#include "lockmgrdbg.hpp"

#define CRANGE_MAX_START  0xFFFFFFFFFFFFFFFF
#define CRANGE_MIN_FINISH 0x0

typedef enum _RANGECONFLICTTYPE {
    RCT_ERROR = 0,
    RCT_NOCONFLICT_BEFORE,
    RCT_NOCONFLICT_AFTER,
    RCT_BETWEEN,
    RCT_BOUNDARY
} RANGECONFLICTTYPE, *PRANGECONFLICTTYPE;

class CRange {
    ULONGLONG m_ullStart;
    ULONGLONG m_ullFinish;
  public:
    CRange();
    VOID SetOffset(DWORD dwOffsetLow, DWORD dwOffsetHigh);
    VOID SetLength(DWORD dwLengthLow, DWORD dwLengthHigh);
    inline ULONGLONG GetStart();
    inline VOID SetStart(ULONGLONG ullStart);
    inline ULONGLONG GetFinish();
    inline VOID SetFinish(ULONGLONG ullFinish);
    RANGECONFLICTTYPE IsConflict(CRange *pRange);
    BOOL IsEqualTo(CRange *pRange);
    BOOL IsValid();
};

inline ULONGLONG CRange::GetStart() {return m_ullStart;}
inline VOID CRange::SetStart(ULONGLONG ullStart) {m_ullStart = ullStart;}
inline ULONGLONG CRange::GetFinish() {return m_ullFinish;}
inline VOID CRange::SetFinish(ULONGLONG ullFinish) {m_ullFinish = ullFinish;}

#endif // __RANGE_HPP_

