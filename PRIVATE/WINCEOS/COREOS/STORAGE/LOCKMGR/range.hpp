//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

