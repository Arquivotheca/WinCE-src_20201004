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
// =============================================================================
//
//  QATestUty Library 
//  Copyright (c) 1998-2000, Microsoft® Corporation  All Rights Reserved
//
//  Module: DebugUty.h
//
//          · macro FILELOC : returns an expression temporary LPCTSTR formatted
//            as "[FILE: filename LINE: lineno]" where filename and lineno
//            denote the source file location of the macro use.
//
//          · function OutputKnownBug : performs a Debug(LOG_FAIL...) with 
//            standard formating, ie ###RAID:BUGNUM
//
//          · function OutputUnexpectedException : Debug(LOG_EXCEPTION...) 
//            with a standard format.
//
//          · global g_ErrorMap : maps standard HRESULTs to persistant LPCTSTRs
//
//          · class flag_string_map : provides a mechanism for declaring maps
//            from bitflags to a composited string. See class declaration for
//            more details.
//
//  Revision History:
//      11/xx/1998 ericflee  Create for QAMaple library
//       1/28/1999 ericflee  Added CTestName and ::DebugW overload
//       5/05/1999 ericflee  Various updates (see windiff for more details)
//       1/05/2000 ericflee  Moved KnowBugUty namespace components to KnownBugUty.h
// 
// =================================================================================

#pragma once

#ifndef __DEBUGUTY_H__
#define __DEBUGUTY_H__

// External Dependencies
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
#include <vector>
#include <set>
#include <const_map.h>
#include <DebugStreamUty.h>
#include "LocaleUty.h"
#include "PresetUty.h"

#pragma warning ( disable : 4786 )


namespace DebugUty
{
    // —————————————
    // macro FILELOC
    // —————————————
    class CSourceFileRef
    {
        enum { BUFFER_SIZE = 128 };
        
    public:
        static LPCTSTR SplitPath(LPCTSTR pszSource);

        TCHAR m_szBuffer[BUFFER_SIZE];

        CSourceFileRef(LPCTSTR pszFilePath, DWORD dwLineNum)
        {
            _snwprintf_s(m_szBuffer,
                       BUFFER_SIZE,
                       TEXT(" [FILE: %s LINE: %d]"),
                       SplitPath(pszFilePath),
                       dwLineNum );
            m_szBuffer[BUFFER_SIZE-1] = TEXT('\0');
        }

        operator LPCTSTR() { return m_szBuffer; }
    };

#   define SOURCE_LOCATION ((LPCTSTR) DebugUty::CSourceFileRef(TEXT(__FILE__), __LINE__))
#   define FILELOC SOURCE_LOCATION

    // A stream-only file location macro that doesn't need any stack space.
#   define STRMFILELOC TEXT(" [FILE: ") << DebugUty::CSourceFileRef::SplitPath(TEXT(__FILE__)) << TEXT(" LINE: ") << __LINE__ << TEXT("]")

    // FILEREF is now obselete; replace with FILELOC
#   define FILEREF(FILENAME,LINENUM) FILELOC

    // —————————————————————————
    // Set of bugs not to report
    // —————————————————————————
    extern std::set<DWORD> setSkipBugs;

    // ——————————————————————————————————
    // function OutputUnexpectedException
    // ——————————————————————————————————
    extern void OutputUnexpectedException(LPCTSTR pszFunctionName, LPCTSTR pszFileRef);


    inline LPCTSTR bool_to_string(bool b)
    {
        return b ? TEXT("true") : TEXT("false");
    }

    // ——————————
    // g_ErrorMap
    // ——————————
    extern const std::const_map<HRESULT, LPCTSTR> g_ErrorMap;

    // —————————————————————
    // class flag_string_map
    // —————————————————————
    class flag_string_map 
    {
    public:
        typedef std::pair<DWORD, LPCTSTR> value_type;
        typedef std::vector<value_type>::const_iterator const_iterator;
    
        // constructor : initialization is like std::vector
        template<class _Iter>
        flag_string_map(_Iter first, _Iter last) 
            : m_vMap(first, last) {}           
    
        // returns a std::tstring appropriate to the flags.
        //  formatted as (reg-expr): '[' FLAGNAME ( | FLAGNAME )* ( | 0xXX )? ']'
        //  where FLAGNAME corresponds to the set bit, and 0xXX corresponds
        //  to any flags detected that were not mapped.
        // example usage:
        //  printf("dsFlags = %s\n", g_SomeFlagMap[dwFlags].c_str());
        std::tstring operator[] (DWORD dwFlags) const;

        // prints the strings appropriate to the flags, one
        //  per line, to the given stream.  This should be 
        //  used when the number of flags to be printed (and
        //  thus the string returned from the [] operator)
        //  may be very large.
        // example usage:
        //  g_SomeFlagMap.PrintFlags(dbgout, dwFlags);
        void PrintFlags(DebugStreamUty::CDebugStream &stream, DWORD dwFlags) const;

    private:
        std::vector<value_type> m_vMap;
    };

} // namespace DebugUty


using DebugUty::flag_string_map;
    // just until we get a change to change all the dependant source files 


#include "KnownBugUty.h"


// ————————————————————————————————————————————————
// Some useful macros that can tighten up test code
// ————————————————————————————————————————————————
#define QueryCondition_LOG(__VERBOSITY, __CONDITION, __MSG, __ACTION) \
    if (__CONDITION) \
    { \
        dbgout(__VERBOSITY) << __MSG << STRMFILELOC << endl; \
        __ACTION; \
    }

#define QueryCondition(__CONDITION, __MSG, __ACTION) \
    QueryCondition_LOG(LOG_FAIL, __CONDITION, __MSG, __ACTION)

#define CheckCondition(__CONDITION, __MSG, __RESULT) \
    QueryCondition(__CONDITION, __MSG, return __RESULT)

#define QueryHRESULT_LOG_MSG(__VERBOSITY, __HR, __MSG, __ACTION) \
    if (FAILED(__HR)) \
    { \
        dbgout(__VERBOSITY) << __MSG << " returned hr=" << g_ErrorMap[__HR] << " (" << HEX((DWORD)__HR) << ")" \
        << STRMFILELOC << endl; \
        __ACTION; \
    }

#define QueryHRESULT_LOG(__VERBOSITY, __HR, __MSG, __ACTION) \
    QueryHRESULT_LOG_MSG(__VERBOSITY, __HR, __MSG, __ACTION)

#define QueryHRESULT(__HR, __MSG, __ACTION) \
    QueryHRESULT_LOG(LOG_FAIL, __HR, __MSG, __ACTION) 

#define QueryHRESULT_MSG(__HR, __MSG, __ACTION) \
    QueryHRESULT_LOG_MSG(LOG_FAIL, __HR, __MSG, __ACTION) 

#define CheckHRESULT_LOG(__VERBOSITY, __HR, __MSG, __RESULT) \
    QueryHRESULT_LOG(__VERBOSITY, __HR, __MSG, return __RESULT)

#define CheckHRESULT(__HR, __MSG, __RESULT) \
    QueryHRESULT(__HR, __MSG, return __RESULT)

#define CheckCall_LOG(__VERBOSITY, __FUNC, __RESULT) \
    do \
    { \
        HRESULT hRet = (__FUNC); \
        CheckHRESULT_LOG(__VERBOSITY, hRet, TEXT(#__FUNC), __RESULT); \
    } while(0);

#define CheckCall(__FUNC, __RESULT) \
    CheckCall_LOG(LOG_FAIL, __FUNC, __RESULT);

#define QueryCall_LOG(__VERBOSITY, __FUNC, __ACTION) \
    do \
    { \
        HRESULT hRet = (__FUNC); \
        QueryHRESULT_LOG(__VERBOSITY, hRet, TEXT(#__FUNC), __ACTION); \
    } while(0);

#define QueryCall(__FUNC, __ACTION) \
    QueryCall_LOG(LOG_FAIL, __FUNC, __ACTION);

#define QueryForHRESULT_LOG_MSG(__VERBOSITY, __HR, __HREXPECTED, __MSG, __ACTION) \
    if (__HR!=__HREXPECTED) \
    { \
        dbgout(__VERBOSITY) \
            << __MSG << " returned hr=" << g_ErrorMap[__HR] << " (" << HEX((DWORD)__HR) << ")" \
            << ", expected " << g_ErrorMap[__HREXPECTED] << STRMFILELOC << endl; \
        __ACTION; \
    }

#define QueryForHRESULT_LOG(__VERBOSITY, __HR, __HREXPECTED, __MSG, __ACTION) \
    QueryForHRESULT_LOG_MSG(__VERBOSITY, __HR, __HREXPECTED, __MSG, __ACTION)

#define QueryForHRESULT(__HR, __HREXPECTED, __MSG, __ACTION) \
    QueryForHRESULT_LOG_MSG(LOG_FAIL, __HR, __HREXPECTED, __MSG, __ACTION)

#define CheckForHRESULT(__HR, __HREXPECTED, __MSG, __RESULT) \
    QueryForHRESULT(__HR, __HREXPECTED, __MSG, return __RESULT)
    
#endif
