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