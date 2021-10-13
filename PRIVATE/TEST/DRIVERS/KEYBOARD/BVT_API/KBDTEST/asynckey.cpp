/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright  1995-2000  Microsoft Corporation.  All Rights Reserved.

Module Name:

     asyncKey.cpp  

Abstract:

  User driven testing of the GetAsyncKeyState API

--*/

#include "global.h"
#include "keyspt.h"


//*****************************************************************************
//    Globals
//*****************************************************************************
struct AsyncKeyItem {
		BYTE      virtualKey; // the virtual key that results from key press
		BYTE      asyncKey;   // this is the actual key being pressed (i.e. left shift)
		TCHAR*    name;		  // name of the key
};

struct AsyncKeyItem  g_pbyAsyncKeyCases[] = {
						VK_MENU,		VK_RMENU,	TEXT("Right ALT"),
						VK_SHIFT,		VK_RSHIFT,	TEXT("Right Shift"),
						65,             65,			TEXT("a"),
						57,				57,			TEXT("9"),
						39,				39,			TEXT("right arrow"),
						VK_CONTROL,		VK_CONTROL,	TEXT("CTRL"),
						VK_SHIFT,		VK_LSHIFT,	TEXT("Left Shift"),
						VK_MENU,		VK_LMENU,	TEXT("Left ALT"),
};

BOOL g_bLastTestSuccess;


//**********************************************************************************
TESTPROCAPI AsyncKeyState_T( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
	NO_MESSAGES;
	SendMessage(globalHwnd,WM_SWITCHPROC,AYSNCKEY,0);

	TCHAR szBuffer[512];
	int x = 0;
	while (x < countof(g_pbyAsyncKeyCases)) {
		SendMessage(globalHwnd,WM_INIT,0,(LPARAM)&g_pbyAsyncKeyCases[x]);

		_stprintf(szBuffer, 
			TEXT("Press the <%s> key"),g_pbyAsyncKeyCases[x].name);
		recordInstruction(szBuffer);
		g_pKato->Log( LOG_DETAIL, szBuffer );
        doUpdate(NULL);

		int iPumpRtn = pump();
		if (iPumpRtn == 2) {
			g_pKato->Log( LOG_FAIL, 
                          TEXT("***FAILURE: In async test: %s"), 
                          g_pbyAsyncKeyCases[x].name);
			x++;
			continue;
		}

        if (!g_bLastTestSuccess) {
			int iMessageRtn = MessageBox(globalHwnd, 
                              TEXT("Case failed.  Do you want to try again?"),
                              TEXT("Async Key Error"),
                              MB_YESNO | MB_ICONQUESTION );
			if (iMessageRtn == IDYES)
				continue;
			else {
				g_pKato->Log( LOG_FAIL, 
                          TEXT("***FAILURE: In async test: %s"), 
                          g_pbyAsyncKeyCases[x].name);
				x++;
			}
		}
		else 
			x++;
	}

	return getCode();
}

//*********************************************************************************8
LRESULT CALLBACK AsyncWndProc( HWND hwnd, 
                               UINT message,
                               WPARAM wParam, 
                               LPARAM lParam) 
{
	static struct AsyncKeyItem * pNextAsyncKey;
	static BOOL bDone;
	PAINTSTRUCT ps;

	switch(message) 
	{
	case WM_INIT:
		pNextAsyncKey = (AsyncKeyItem *)lParam;
		bDone = FALSE;
		return 0;

	case WM_PAINT:
		BeginPaint(hwnd, &ps);

        initWindow( ps.hdc,
                    TEXT("Driver Tests: Keyboard: Async Testing"),
                    TEXT("Follow direction in the bottom box."));
        showInstruction(ps.hdc);
        EndPaint(hwnd, &ps);
        return 0;

	case WM_KEYDOWN:
		// fall through
	case WM_SYSKEYDOWN:
		if (wParam == pNextAsyncKey->virtualKey) {
			// check the async key
			SHORT sAsyncKeyState = GetAsyncKeyState(pNextAsyncKey->asyncKey);
			if (0x8000 & sAsyncKeyState) {  // is it pressed?
				g_bLastTestSuccess = TRUE;
			}
			else
				g_bLastTestSuccess = FALSE;
			bDone = TRUE;
		}
		return 0;

	case WM_GOAL:
		return bDone;

	}

	return (DefWindowProc(hwnd, message, wParam, lParam));
}
				

	

		