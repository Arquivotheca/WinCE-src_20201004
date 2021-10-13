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
// --------------------------------------------------------------------
//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
//                                                                     
// --------------------------------------------------------------------


#include <WINDOWS.H>
#include <TCHAR.H>
#include <TUX.H>
#include <KATOEX.H>
#include "xwifi11b_testproc.h"

#include <windev.h>
#include <ndis.h>
#include <msgqueue.h>

extern CKato *g_pKato;
extern SPS_SHELL_INFO *g_pShellInfo;

#define HEX(c)  ((c)<='9'?(c)-'0':(c)<='F'?(c)-'A'+0xA:(c)-'a'+0xA)


/*
g_pKato->Log(LOG_FAIL,
g_pKato->Log(LOG_ABORT,
g_pKato->Log(LOG_DETAIL,
g_pKato->Log(LOG_COMMENT
g_pKato->Log(LOG_SKIP
g_pKato->Log(LOG_NOT_IMPLEMENTED
*/

#include <winuser.h>


BOOL
IsDialogWindowPoppedUp
(
WCHAR *szTitle
)
{
    return FindWindow(NULL, szTitle)!=NULL;
}

