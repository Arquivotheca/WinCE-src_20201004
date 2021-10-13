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
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright  1998-2000  Microsoft Corporation.  All Rights Reserved.

Module Name:

      keyspt.h

Abstract:

  Header of support function in keyspt.cpp
  The functions are used by the key press and key sequence tests
  to record and display keyboard events.


Notes:
	
  Adapted from the multitst suite "keys.cpp"
--*/

#ifndef _KEYSPT_H
#define _KEYSPT_H

#include <windows.h>
#include <tchar.h>
#include "global.h"

void emptyKeyAndInstrBuffers();
void recordKey (int message, UINT wParam, LONG lParam);
void recordInstruction(TCHAR *str);
void showKeyEvent(HDC hdc);
void showInstruction(HDC hdc);

#endif