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
#ifndef _TESTDEFS_H_
#define _TESTDEFS_H_

/***********************************************************************************
***
***   Headers
***
************************************************************************************/
#include <windows.h>
#include <stdlib.h>
#include <tchar.h>
#include <tux.h>

/***********************************************************************************
***
***   defs and enums
***
************************************************************************************/
#define  testCycles     50          // number of random tests to run
#define  SANITY_CHECKS  0
#define  NADA           0x10000309  // customer bit(29)=1 e_code=777
#define  NO_MESSAGES if (uMsg != TPM_EXECUTE) return TPR_NOT_HANDLED;
#define  abort(s) info(SKIP,s);

#define crWHITE  (COLORREF) 0xFFFFFF
#define crBLACK  (COLORREF) 0x000000
#define crLTGRAY (COLORREF) 0xC0C0C0
#define crDKGRAY (COLORREF) 0x626262
#define crRED    (COLORREF) 0x0000FF
#define crBLUE   (COLORREF) 0x7F0000
#define crGREEN  (COLORREF) 0x00FF00

/***********************************************************************************
***
***  Reserve WM_USER+1 to WM_USER+9 for common user messages
***
************************************************************************************/
#define USER_FLUSH_QUEUE        WM_USER + 1
#define USER_EXIT               WM_USER + 2
#define USER_EXIT_THREAD        WM_USER + 3
#define USER_CLEANUP            WM_USER + 4
// ...
#define USER_MSG                WM_USER + 10

/***********************************************************************************
***
***   Universal Log Definitions
***
************************************************************************************/
enum infoType
{
    FAIL,
    ECHO,
    DETAIL,
    ABORT,
    SKIP,
};

#define MY_EXCEPTION          0
#define MY_FAIL               2
#define MY_ABORT              4
#define MY_SKIP               6
#define MY_NOT_IMPLEMENTED    8
#define MY_PASS              10

#define TL_BVT     1
#define TL_REGRESS 2
#define TL_STRESS  3
#define TL_FUNC    4
#define TL_ALL     5

#define OK(x)   info(DETAIL,L"OK %i", x);
#define BUG(x)  {   \
                    info(DETAIL,L"");   \
                    info(DETAIL,L"******* LAB NOTE: WinCE Bug #%i ********", x);    \
                    info(DETAIL,L"");   \
                }

#define BEGINBUG(x)     {   \
                            if (g_TestLevel == TL_REGRESS) {    \
                                info(DETAIL,L"");   \
                                info(DETAIL,L"******* WinCE Bug #%i ********", x);  \
                                info(DETAIL,L"");   \
                            } else goto BUG_##x;    \
                        }

#define ENDBUG(x) BUG_##x:

/***********************************************************************************
***
***   From main.cxx
***
************************************************************************************/
extern void info(infoType, LPCTSTR,...);
extern TESTPROCAPI getCode(void);

// begin of OLD_GESTURE definition
//
// Put these old gesture definition here until these old test cases cleaned up.

#define WM_GESTURECOMMAND               WM_GESTURE

// GestureCommand commands
// Note: Command 0x0000 is reserved
#define GCI_COMMAND_PAN                 0x0002
#define GCI_COMMAND_SCROLL              0x0005
#define GCI_COMMAND_HOLD                0x0006
#define GCI_COMMAND_SELECT              0x0007
#define GCI_COMMAND_DOUBLESELECT        0x0008

#define GCI_COMMAND_MAX                 0x003F

// Note: Commands in range 0x00F0 to 0x00FE
// are reserved
#define GCI_COMMAND_END                 0x00FF

// GestureCommand parameters
#define GCF_SYNC                        0x00000001


#define GCI_TO_TGF(x)                   (1ULL << (x))

// Flags for use with enabling and disabling gestures
#define TGF_PAN                         GCI_TO_TGF(GCI_COMMAND_PAN)
#define TGF_SCROLL                      GCI_TO_TGF(GCI_COMMAND_SCROLL)
#define TGF_HOLD                        GCI_TO_TGF(GCI_COMMAND_HOLD)
#define TGF_SELECT                      GCI_TO_TGF(GCI_COMMAND_SELECT)
#define TGF_DOUBLESELECT                GCI_TO_TGF(GCI_COMMAND_DOUBLESELECT)


#define GCI_SCROLL_ANGLE(x)              (HIWORD((x)))
#define GCI_SCROLL_DIRECTION(x)          (LOWORD(((x) & 0x0000000F)))
#define GCI_SCROLL_VELOCITY(x)           (LOWORD((((x) & 0x0000FFF0) >> 4)))

// macros to go to and from angle (radians) to argument
#define GCI_GESTURE_ANGLE_TO_ARGUMENT(_arg_)   ((USHORT)((((_arg_) + 2.0 * 3.14159265) / (4.0 * 3.14159265)) * 65536.0))
#define GCI_GESTURE_ANGLE_FROM_ARGUMENT(_arg_) ((((double)(_arg_) / 65536.0) * 4.0 * 3.14159265) - 2.0 * 3.14159265)

// Macros to extract the pan delta
#define GCI_PAN_DELTAX(x)               (SHORT)(HIWORD((x)))
#define GCI_PAN_DELTAY(x)               (SHORT)(LOWORD((x)))


typedef struct tagGESTURECOMMANDINFO
{
    UINT cbSize;                /* Initialised to structure size */
    DWORD dwFlags;              /* Gesture flags */
    DWORD dwCommand;            /* Gesture command */
    DWORD dwArguments;          /* Arguments specific to gesture */
    POINT ptsLocation;          /* Coordinates of start of gesture */
} GESTURECOMMANDINFO, * PGESTURECOMMANDINFO;

#define GetGestureCommandInfo(a, b, c, d, e)    FALSE
#define GestureCommand(a, b, c, d, e)           FALSE

#ifdef tagGESTUREMETRICS
#undef tagGESTUREMETRICS
typedef struct tagGESTUREMETRICS {
    UINT   cbSize; 
    DWORD  dwID;
    DWORD  dwTimeout; 
    DWORD  dwDistanceTolerance; 
    DWORD  dwAngularTolerance;
    DWORD  dwExtraInfo;
    // OLD_GESTURE code: will be removed when WM7-M4 integration is done.
    DWORD dwGestureCommand;

} GESTUREMETRICS, *LPGESTUREMETRICS;
#endif

// end of OLD_GESTURE

#endif // _TESTDEFS_H_
