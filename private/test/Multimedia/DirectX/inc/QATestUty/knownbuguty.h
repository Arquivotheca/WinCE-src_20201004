// ============================================================================================
//
//  QATestUty Library 
//  Copyright (c) 2000, Microsoft® Corporation  All Rights Reserved
//
//  Module: KnownBugUty.h
//
//      
//          · Provides CDebugStream manipulators to support testing and output of known bugs.
//            These are provided for both Unicode and ANSI format.
//              - manipulator KnownBug( lBugID, pszDB )
//                A stream manipulator that inserts a known bug identifier.
//                Outputs tkreport format, i.e. "###RAIDDB:BUGNUM"
//                  - lBugID : numeric bug identifier
//                  - pszDB : optional database name (default = "HPURAID")
//
//              - manipulator DetectKnownBug( fCondition, lBugID, pszDB )
//                A stream manipulator that conditionally inserts a known bug identifier.
//                  - fCondition : a test condition
//                  - lBugID and pszDB are passed to the KnownBug manipulator
//
//          · Platform specific stream manipulators.  These manipulators resolve to the KnownBug 
//            and DetectKnownBug manipulators on the specified platform; on all other platforms, 
//            they resolve to a null operation (placeholder) manipulator
//              - DXPak: DetectKnownDXBug(...) and KnownDXBug(...)
//
//          · Logging Abstratcion macros.  These macros abstract out the question of wether
//            or not an encountered bug should be logged as a failure.  This uses the set
//            of bugs to skip (most likely postponed) from DebugUty.
//              - HandleKnownWinCEBug(BUGID, MSG, ACTION)
//                Logs that a bug was encountered, but only logs a failure if the BUGID is
//                not found in the set of bugs to ignore.  ACTION is only executed if the
//                bug is not ignored.
//              - QueryForKnownWinCEBug(COND, BUGID, MSG, ACTION)
//                Checks condition, and calls "HandleKnownWinCEBug" if true
//
//
//      LEGACY ONLY FUNCTIONS/MACROS (provided for old code that hasn't yet been updated)
//
//          · function OutputKnownBug : performs a Debug(LOG_FAIL...) with 
//            standard formating, ie ###RAID:BUGNUM
//            * NOTE: This function is for legacy purposes only.
//
//          · The following macros are self explanatory via code inspection.
//            Also, they are macros instead of inline functions because they use the FILELOC mechanism
//
//            - OutputKnownHPURAIDBug(BUGID)
//            - OutputKnownKeepBug(BUGID)
//
//            - OutputKnownDXBug(BUGID)
//            - OutputKnownDXBug_(BUGID, DB) 
//            - QueryKnownDXBug(CONDITION, BUGID) 
//            - QueryKnownDXBug_(CONDITION, BUGID, DBNAME)
//
//  Revision History:
//       1/05/2000 ericflee  Created by moving KnowBugUty namespace components from DebugUty.h
//       8/01/2000 jonask    Added WinCE macros and the "[Handle|QueryFor]KnownWinCEBug" macros
// 
// =============================================================================================

#pragma once
#ifndef __KNOWNBUGUTY_H__
#define __KNOWNBUGUTY_H__

#include <DebugStreamUty.h>

namespace KnownBugUty
{
    using namespace DebugStreamUty;

    // =================
    // Known Bug support
    // =================
    class __CNullOpPlaceholder { private: DWORD m_dummymember; };
    inline CWDebugStream& operator << (CWDebugStream& stream, const __CNullOpPlaceholder &) { return stream; }

    // Unicode
    inline CWDebugStream& __KnownBug(CWDebugStream& stream, long lBugID, LPCWSTR pszDB)
    {
        return stream << "(" << pszDB << " #" << lBugID << ") ";
    }

    inline binary_stream_manipulator<long, LPCWSTR> KnownBug(long lBugID, LPCWSTR pszDB = L"HPURAID")
    {
        return binary_stream_manipulator<long, LPCWSTR>(__KnownBug, lBugID, pszDB);
    }

    inline CWDebugStream& __DetectKnownBug(CWDebugStream& stream, bool fCondition, long lBugID, LPCWSTR pszDB)
    {
        return fCondition ? (stream << KnownBug(lBugID, pszDB)) : stream;
    }

    inline ternary_stream_manipulator<bool, long, LPCWSTR> DetectKnownBug(bool fCondition, long lBugID, LPCWSTR pszDB = L"HPURAID")
    {
        return ternary_stream_manipulator<bool, long, LPCWSTR>(__DetectKnownBug, fCondition, lBugID, pszDB);
    }

    // Ansi
    inline CWDebugStream& __KnownBug(CWDebugStream& stream, long lBugID, LPCSTR pszDB)
    {
        return stream << "(" << pszDB << " #" << lBugID << ") ";
    }

    inline binary_stream_manipulator<long, LPCSTR> KnownBug(long lBugID, LPCSTR pszDB)
    {
        return binary_stream_manipulator<long, LPCSTR>(__KnownBug, lBugID, pszDB);
    }

    inline CWDebugStream& __DetectKnownBug(CWDebugStream& stream, bool fCondition, long lBugID, LPCSTR pszDB)
    {
        return fCondition ? (stream << KnownBug(lBugID, pszDB)) : stream;
    }

    inline ternary_stream_manipulator<bool, long, LPCSTR> DetectKnownBug(bool fCondition, long lBugID, LPCSTR pszDB)
    {
        return ternary_stream_manipulator<bool, long, LPCSTR>(__DetectKnownBug, fCondition, lBugID, pszDB);
    }

    // ———————————————————————
    // function OutputKnownBug
    // ———————————————————————
    extern void OutputKnownBug(long nBugNumber, LPCTSTR pszFileRef, LPCTSTR pszRaidDB = NULL);


#   define OutputKnownWinCEBug(A) KnownBugUty::OutputKnownBug(A, FILELOC, TEXT("WinCE"))
#   define OutputKnownHPURAIDBug(A) KnownBugUty::OutputKnownBug(A, FILELOC, TEXT("HPURAID"))
#   define OutputKnownKeepBug(A) KnownBugUty::OutputKnownBug(A, FILELOC, TEXT("KEEP"))

#   if CFG_PLATFORM_DX
#       define DetectKnownWinCEBug(CONDITION, BUGID) DetectKnownBug(CONDITION, BUGID, TEXT("WinCE"))
#       define KnownWinCEBug(BUGID) KnownBug(BUGID, TEXT("WinCE"))
#       define DetectKnownHUPRAIDBug(CONDITION, BUGID) DetectKnownBug(CONDITION, BUGID, TEXT("HPURAID"))
#       define KnownHPURAIDBug(BUGID) KnownBug(BUGID, TEXT("HPURAID"))
#   else
        inline __CNullOpPlaceholder DetectKnownWinCEBug(...) { return __CNullOpPlaceholder(); }
        inline __CNullOpPlaceholder KnownWinCEBug(...) { return __CNullOpPlaceholder(); }
        inline __CNullOpPlaceholder DetectKnownHUPRAIDBug(...) { return __CNullOpPlaceholder(); }
        inline __CNullOpPlaceholder KnownHPURAIDBug(...) { return __CNullOpPlaceholder(); }
#   endif

#   if CFG_PLATFORM_DX | CFG_PLATFORM_MARINER | CFG_PLATFORM_ASTB
#       define OutputKnownDXBug(BUGID) KnownBugUty::OutputKnownBug(BUGID, FILELOC, TEXT("HPURAID"))
#       define OutputKnownDXBug_(BUGID,DBNAME) KnownBugUty::OutputKnownBug(A, FILELOC, TEXT(DBNAME))
#       define QueryKnownDXBug(CONDITION, BUGID) \
            do { if (CONDITION) OutputKnownDXBug(BUGID); } while(0);
#       define QueryKnownDXBug_(CONDITION, BUGID, DBNAME) \
            do { if (CONDITION) OutputKnownDXBug_(BUGID,DBNAME); } while(0);
#       define DetectKnownDXBug DetectKnownBug
#       define KnownDXBug KnownBug
#   else
#       define OutputKnownDXBug(A)
#       define OutputKnownDXBug_(BUGID,DBNAME)
#       define QueryKnownDXBug(CONDITION, BUGID)
#       define QueryKnownDXBug_(CONDITION, BUGID, DBNAME)
        inline __CNullOpPlaceholder DetectKnownDXBug(...) { return __CNullOpPlaceholder(); }
        inline __CNullOpPlaceholder KnownDXBug(...) { return __CNullOpPlaceholder(); }
#   endif

#   if CFG_PLATFORM_ASTB
#       define OutputKnownASTBBug(A) KnownBugUty::OutputKnownBug(A, FILELOC)
#       define DetectKnownASTBBug DetectKnownBug
#       define KnownASTBBug KnownBug
#   else
#       define OutputKnownASTBBug(A) 
        inline __CNullOpPlaceholder DetectKnownASTBBug(...) { return __CNullOpPlaceholder(); }
        inline __CNullOpPlaceholder KnownASTBBug(...) { return __CNullOpPlaceholder(); }
#   endif

#   define HandleKnownWinCEBug(__BUGID, __MSG, __ACTION) \
        if (DebugUty::setSkipBugs.end() != DebugUty::setSkipBugs.find((DWORD)__BUGID)) \
        { \
            dbgout(LOG_COMMENT) << "Found Postponed Bug " << KnownWinCEBug(__BUGID) << __MSG << FILELOC << endl; \
        } \
        else \
        { \
            dbgout(LOG_FAIL) << KnownWinCEBug(__BUGID) << __MSG << STRMFILELOC << endl; \
            __ACTION; \
        } \
        

#   define QueryForKnownWinCEBug(__COND, __BUGID, __MSG, __ACTION) \
        if (__COND) \
        { \
            HandleKnownWinCEBug(__BUGID, __MSG, __ACTION); \
        } \

}  // namespace KnownBugUty



#endif
