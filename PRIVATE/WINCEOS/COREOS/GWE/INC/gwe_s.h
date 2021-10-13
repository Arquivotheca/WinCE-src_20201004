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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//

/*


*/
#ifndef __GWE_S_H__
#define __GWE_S_H__

#include <intsafe.h>

//
// If FULL_GWE_DEBUG is defined, enable some painfully slow
// runtime checks
//
#ifdef FULL_GWE_DEBUG
#define CHECK_GWE_HEAP()                                \
    do {                                                \
        if (!HeapValidate(GetProcessHeap(), 0, NULL))   \
            DebugBreak();                               \
    } while (0)
#else
#define CHECK_GWE_HEAP()
#endif

//  Useful macros
#define ARRAYSIZE(a)    (sizeof(a)/sizeof(a[0]))

#define ISKMODEADDR(Addr) ((int)Addr < 0)

// TODO:(yama) Temporary stub made just for simplification before Bor-Ming adds the actual method
BOOL GweIsValidUsrStrW(LPCTSTR lpString);

//	GWE internal memory tracking mechanism.
#define MEM_ACCOUNT	0

// Used for debugging the deletion of GDI objects still in use by the window mgr
#define CHECK_UNUSED 0
#if CHECK_UNUSED
extern fCheckUnused;
#endif


#ifdef __cplusplus
extern "C" {
#endif

// Debug zone definitions for GWE
#define DBGWNDCLS       DEBUGZONE(0)    //  0x1
#define DBGWNDMGRS      DEBUGZONE(1)    //  0x2
#define DBGWNDMGRC      DEBUGZONE(2)    //  0x4
#define DBGNC           DEBUGZONE(3)    //  0x8
#define DBGMENU         DEBUGZONE(4)    //  0x10
#define DBGDLG          DEBUGZONE(5)    //  0x20
#define DBGWARN         DEBUGZONE(6)    //  0x40
#define DBGWMSDUMP      DEBUGZONE(7)    //  0x80
#define DBGWMSDETAIL    DEBUGZONE(8)    //  0x100
#define DBGREGION       DEBUGZONE(9)    //  0x200
#define DBGTASKBAR      DEBUGZONE(10)   //  0x400
#define DBGACTIVE       DEBUGZONE(11)   //  0x800
#define DBGKEYBD        DEBUGZONE(12)   //  0x1000
#define DBGMSGAPI       DEBUGZONE(13)   //  0x2000
#define DBGMSGDETAIL1   DEBUGZONE(14)   //  0x4000
#define DBGTIMERAPI     DEBUGZONE(15)   //  0x8000
#define DBGTIMERTHREAD  DEBUGZONE(16)   //  0x10000
#define DBGCARET        DEBUGZONE(17)   //  0x20000
#define DBGACCEL        DEBUGZONE(18)   //  0x40000
#define DBGTOUCH        DEBUGZONE(19)   //  0x80000
#define DBGINPUTDUMP    DEBUGZONE(20)   //  0x100000
#define DBGCLIPBD       DEBUGZONE(21)   //  0x200000
#define DBGGDI          DEBUGZONE(22)   //  0x400000
#define DBGGDIVERBOSE   DEBUGZONE(23)   //  0x800000
#define DBGCLEANUP      DEBUGZONE(24)   //  0x1000000
#define DBGIMM          DEBUGZONE(25)   //  0x2000000
#define DBGDDI          DEBUGZONE(26)   // 0x4000000
#define DBGUSPCE        DEBUGZONE(27)   // 0x8000000
#define DBGDDRAW        DEBUGZONE(28)   //  0x10000000
// 0x20000000 used by mercury
#define DBGTOUCHCONTROL DEBUGZONE(30)   //  0x40000000  

extern HINSTANCE        v_hinstUser;
extern HPROCESS         v_hprcUser;

// should we repaint/invalidate on power on?
#define POR_DONOTHING	0	// the display driver handles everything
#define POR_SAVEBITS	1	// gwe should save and restore the bits
#define POR_INVALIDATE 	2	// gwe should invalidate and repaint
#define POR_SAVEVIDEOMEM 3  // gwe and driver need to save video memory
extern DWORD			v_dwPORepaint;
extern HDC 				v_hdcScreen, v_hdcSaveBits;
extern DWORD			v_OldAppCheck;

// Process data. Uses DWORDs instead of a ULONGLONG because GENTBL can
// cause a ULONGLONG to be misaligned causing exceptions on MIPS.
typedef struct ProcessData
{
    DWORD   dwProcessGestureFlagsLowPart;
    DWORD   dwProcessGestureFlagsHighPart;
}
ProcessData;

// Creates a ULONGLONG from two DWORDS
__inline
ULONGLONG
MAKEULONGLONG(
    DWORD dwLo,
    DWORD dwHi
    )
{
    ULONGLONG ull = ((ULONGLONG) dwHi) << 32;
    ull |= dwLo;
    return ull;
}

// Returns the process gesture flags.
__inline
ULONGLONG
GetProcessGestureFlags(
    const ProcessData *pProcessData
    )
{
    ASSERT(pProcessData);

    return MAKEULONGLONG(pProcessData->dwProcessGestureFlagsLowPart, 
        pProcessData->dwProcessGestureFlagsHighPart);
}

// Sets the process gesture flags.
__inline
VOID
SetProcessGestureFlags(
    ULONGLONG ullProcessGestureFlags,
    ProcessData *pProcessData
    )
{
    ASSERT(pProcessData);

    pProcessData->dwProcessGestureFlagsHighPart = HIDWORD(ullProcessGestureFlags);
    pProcessData->dwProcessGestureFlagsLowPart  = LODWORD(ullProcessGestureFlags);
}



// data struct for storing process data
typedef struct
{
    HANDLE      hProcess;
    ProcessData pdData;
}
ProcessDataNode;

ProcessData* InitProcessData( HANDLE hProcess, const ProcessData& pdDefault );
void DeleteProcessData( HANDLE hProcess );

// data struct for ProcInputLocale table.
typedef struct
{
	HANDLE	hProcess;
	HKL		hklProcess;
} 
ProcInputLocaleNode;

void AddToProcessInputLocaleTable( HANDLE hProcess );
void DeleteFromProcessInputLocaleTable( HANDLE hProcess );
HKL  GetProcessInputLocale( HANDLE hProcess );
bool SetProcessInputLocale( HANDLE hProcess, HKL hklProcess );

// Is this hkl an IME?
#define IsHklIme(hkl) ((((UINT) hkl) & 0xff000000) == 0xe0000000)

#pragma warning(disable:4509)
// warning C4509: nonstandard extension used: 'Function' uses SEH and 'protect'
// has destructor


//
// These numbers serve as indices into the rgatomSysClass[] array so that
// we can get the atoms for the various classes.  The order of the control
// classes is assumed to be the same as the class XXXCODE constants defined
// in dlgmgr.h.
//
#define ICLS_BUTTON         0
#define ICLS_EDIT           1
#define ICLS_STATIC         2
#define ICLS_LISTBOX        3
#define ICLS_SCROLLBAR      4
#define ICLS_COMBOBOX       5       // End of special dlgmgr indices
#define ICLS_DIALOG         6

//	New control for candidate list window
#define ICLS_CANDLIST		7
#define ICLS_IME			8
#define ICLS_CTL_MAX		9

#define ICLS_DESKTOP        9
#define ICLS_MENU           10
#define ICLS_SWITCH         11
#define ICLS_MDICLIENT      12
#define ICLS_COMBOLISTBOX   13
#define ICLS_MAX            14      // Number of system classes

// Originally used SetThreadPriority to set threads to one of the 8 available priorities.
// However, since there are 256 thread priorities we use CeSetThreadPriority to set them.
// In moving from SetThreadPriority to CeSetThreadPriority we retained the same default thread
// priority values and SetThreadPriority(THREAD_PRIORITY_*) became
// CeSetThreadPriority(248 + THREAD_PRIORITY_*)
#define	THREAD_PRIORITY_BASE	248

extern ATOM rgatomSysClass[ICLS_MAX];

extern LPCTSTR v_szSoundToPlay;

extern HKL g_hklSystemDefault;

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus


BOOL GWE_sndPlaySound( LPCWSTR lpszSoundName, UINT fuSound );
MMRESULT GWE_waveOutMessage(HWAVEOUT hwo, UINT uMsg, DWORD dw1, DWORD dw2);

#ifndef __GWE_CRITICAL_SECTION_HPP_INCLUDED__
#include <GweCriticalSection.hpp>
#endif



#endif



#endif /* !__GWE_S_H__ */
