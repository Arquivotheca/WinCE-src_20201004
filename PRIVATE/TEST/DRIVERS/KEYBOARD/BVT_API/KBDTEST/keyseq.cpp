/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright  1998-2000  Microsoft Corporation.  All Rights Reserved.

Module Name:

     keyseq.cpp  

Abstract:

  The Key Sequence Test.


Notes:
	
  Adapted from the multitst suite "keys.cpp"
  Included by a-rajtha
  for :
        Japanese key board.
--*/

#include "keyspt.h"

//******************************************************************************
//****   Globals
//******************************************************************************
static BOOL g_bSequencePassed;  // Success of last sequence

struct keySeqIndex {
	TCHAR* szKeySeq;       // Name of key to press
        int iLast;         // index into keyEvents for last event of event seqeunce
};

struct JapKeySeqIndex {
	 TCHAR* szKeySeq;
	 int   iLast;
};

struct keyEvent {
        UINT message;       // key event
        int ikeyId;         // virtual key
		BOOL fAltPressed;
		BOOL fPrevUp;       // previously up
		BOOL fTransUp;		// moving up (being released)
};
struct JapKeyEvent {
	    UINT message;       // key event
        int ikeyId;         // virtual key
		BOOL fAltPressed;
		BOOL fPrevUp;       // previously up
		BOOL fTransUp;		// moving up (being released)
};




//    Key seqence for Japanese Key board  Key sequence is same as US keyboard but the states of shift, Alt
//                and Ctrl key may be different.

struct JapKeySeqIndex JapKeyNames[] = {
	 TEXT("a"),						   2,
     TEXT("Right Arrow"),              4,
     TEXT("Down Arrow"),               6,
     TEXT("TAB"),                      9,
	 TEXT("SHIFT"),                    11,  
     TEXT("CTRL"),                     13,   
     TEXT("ALT"),                      15,	
     TEXT("CTRL and z") ,              20,   
     TEXT("ESC"),                      23,   
     TEXT("ALT and z"),                28,  
//     TEXT("="),						             32,   
     TEXT("v"),                        31,  // 34,  
     TEXT("ENTER"),                    34,  //37,   
     TEXT("4"),                        37,	// 40,   
     TEXT("SHIFT and v"),              42,	// 45,      
     TEXT("SHIFT and 4"),              47,	//  50,   
     TEXT("BACKSPACE (DEL)"),          50,    
     TEXT("CTRL and 4"),               54,   
     TEXT("COMMA"),                    57,   
     TEXT("SPACE BAR"),                60,
	 TEXT("SHIFT and CAPS LOCK"),	   64,
	 TEXT("Right Arrow"),              66,	
     TEXT("TAB"),                      69,
	 TEXT("SHIFT"),                    71,   
     TEXT("CTRL"),                     73,   
     TEXT("ALT"),                      75,	
     TEXT("CTRL and z"),               80, 
     TEXT("ESC"),					   83,   
     TEXT("ALT and z"),				   88,  
  //   TEXT("="),			 	 	 	   93,  
     TEXT("v"),							91,  
     TEXT("ENTER"),						94,   
     TEXT("4"),							97,   
     TEXT("SHIFT and v"),				102,     
     TEXT("SHIFT and 4"),				106,  
     TEXT("BACKSPACE (DEL)"),			109,   
     TEXT("CTRL and 4"),				113,  
     TEXT("COMMA"),						116,  
     TEXT("SPACE BAR"),					119,
	 TEXT("CAPS LOCK"),					122,

};

     
struct keySeqIndex keyNames[] = {
	 TEXT("a"),						   2,
     TEXT("Right Arrow"),              4,
     TEXT("Down Arrow"),               6,
     TEXT("TAB"),                      9,
	 TEXT("SHIFT"),                    11,  
     TEXT("CTRL"),                     13,   
     TEXT("ALT"),                      15,	
     TEXT("CTRL and z"),               20,   
     TEXT("ESC"),                      23,   
     TEXT("ALT and z"),                27,     
     TEXT("v"),                        31,  
     TEXT("ENTER"),                    34,   
     TEXT("4"),                        37,   
     TEXT("SHIFT and v"),              41,      
     TEXT("SHIFT and 4"),              46,   
     TEXT("BACKSPACE (DEL)"),          50,    
     TEXT("CTRL and 4"),               54,   
     TEXT("COMMA"),                    57,   
     TEXT("SPACE BAR"),                60,
	 TEXT("SHIFT and CAPS LOCK"),	   64,
	 TEXT("Right Arrow"),              67,
     TEXT("TAB"),                      69,
	 TEXT("SHIFT"),                    72,   
     TEXT("CTRL"),                     74,   
     TEXT("ALT"),                      76,	
     TEXT("CTRL and z"),               79, 
     TEXT("ESC"),					   83,   
     TEXT("ALT and z"),				   87,  
  //			 TEXT("="),			 	 	 	   92,  
     TEXT("v"),							91,  
     TEXT("ENTER"),						94,   
     TEXT("4"),							97,   
     TEXT("SHIFT and v"),				101,     
     TEXT("SHIFT and 4"),				106,  
//     TEXT("BACKSPACE (DEL)"),			110,   
//    TEXT("CTRL and 4"),				114,  
//     TEXT("COMMA"),						117,  
//    TEXT("SPACE BAR"),					120,
//	 TEXT("CAPS LOCK"),					123,
};

struct keyEvent keyEvents[] = {
	 WM_KEYDOWN,		65,   FALSE, TRUE,  FALSE, // a
	 WM_CHAR,			97,   FALSE, TRUE,  FALSE,
	 WM_KEYUP,			65,   FALSE, FALSE, TRUE,
     WM_KEYDOWN,		39,   FALSE, TRUE,  FALSE, // right arrow
     WM_KEYUP,			39,   FALSE, FALSE, TRUE,
     WM_KEYDOWN,		40,   FALSE, TRUE,  FALSE, // down arrow
     WM_KEYUP,			40,   FALSE, FALSE, TRUE,
     WM_KEYDOWN,		 9,   FALSE, TRUE,  FALSE, // TAB
	 WM_CHAR,			 9,   FALSE, TRUE,  FALSE,
     WM_KEYUP,			 9,   FALSE, FALSE, TRUE,
	 WM_KEYDOWN,		16,	  FALSE, TRUE,  FALSE,// SHIFT
	 WM_KEYUP,			16,   FALSE, FALSE, TRUE,
	 WM_KEYDOWN,		17,   FALSE, TRUE,  FALSE, // CTRL
	 WM_KEYUP,			17,   FALSE, FALSE, TRUE,
	 WM_SYSKEYDOWN,		18,   TRUE,  TRUE,  FALSE, // ALT    
	 WM_SYSKEYUP,		18,	  FALSE, FALSE, TRUE,
	 WM_KEYDOWN,		17,   FALSE, TRUE,  FALSE,// CTRL z
	 WM_KEYDOWN,		90,   FALSE, TRUE,  FALSE,
	 WM_CHAR,			26,   FALSE, TRUE,  FALSE,
	 WM_KEYUP,			90,   FALSE, FALSE, TRUE,
	 WM_KEYUP,			17,   FALSE, FALSE, TRUE,
	 WM_KEYDOWN,		27,   FALSE, TRUE,  FALSE, // ESC
	 WM_CHAR,			27,   FALSE, TRUE,  FALSE,
	 WM_KEYUP,			27,   FALSE, FALSE, TRUE,
	 WM_SYSKEYDOWN,		18,   TRUE,  TRUE,  FALSE, // ALT z 
	 WM_SYSKEYDOWN,		90,   TRUE,  TRUE,  FALSE,
	 WM_SYSCHAR,		122,  TRUE,  TRUE,  FALSE,
	 WM_SYSKEYUP,		90,   TRUE,  FALSE, TRUE,
	 WM_KEYUP,		    18,   FALSE, FALSE, TRUE,
	 WM_KEYDOWN,		187,  FALSE, TRUE,  FALSE,  //=
	 WM_CHAR,			61,   FALSE, TRUE,  FALSE,
	 WM_KEYUP,			187,  FALSE, FALSE, TRUE,
	 WM_KEYDOWN,		86,   FALSE, TRUE,  FALSE, // v
	 WM_CHAR,			118,  FALSE, TRUE,  FALSE,
	 WM_KEYUP,			86,   FALSE, FALSE, TRUE,
	 WM_KEYDOWN,		13,   FALSE, TRUE,  FALSE, // ENTER
	 WM_CHAR,			13,   FALSE, TRUE,  FALSE,
	 WM_KEYUP,			13,   FALSE, FALSE, TRUE,
	 WM_KEYDOWN,		52,   FALSE, TRUE,  FALSE, // 4
	 WM_CHAR,			52,   FALSE, TRUE,  FALSE,
	 WM_KEYUP,			52,   FALSE, FALSE, TRUE,
	 WM_KEYDOWN,		16,   FALSE, TRUE,  FALSE, // Shift and v
	 WM_KEYDOWN,		86,   FALSE, TRUE,  FALSE,
	 WM_CHAR,			86,   FALSE, TRUE,  FALSE,
	 WM_KEYUP,			86,   FALSE, FALSE, TRUE,
	 WM_KEYUP,			16,   FALSE, FALSE, TRUE,
	 WM_KEYDOWN,		16,   FALSE, TRUE,  FALSE, // Shift and 4
	 WM_KEYDOWN,		52,   FALSE, TRUE,  FALSE,
	 WM_CHAR,			36,   FALSE, TRUE,  FALSE,
	 WM_KEYUP,			52,   FALSE, FALSE, TRUE,
	 WM_KEYUP,			16,   FALSE, FALSE, TRUE,
	 WM_KEYDOWN,		8,    FALSE, TRUE,  FALSE,// Backspace
	 WM_CHAR,			8,    FALSE, TRUE,  FALSE,
	 WM_KEYUP,			8,    FALSE, FALSE, TRUE,
	 WM_KEYDOWN,		17,   FALSE, TRUE,  FALSE,  // Ctrl and 4
	 WM_KEYDOWN,		52,   FALSE, TRUE,  FALSE,
	 WM_KEYUP,			52,   FALSE, FALSE, TRUE,
	 WM_KEYUP,			17,   FALSE, FALSE, TRUE,
	 WM_KEYDOWN,		188,  FALSE, TRUE,  FALSE,   // Comma
	 WM_CHAR,			44,   FALSE, TRUE,  FALSE,
	 WM_KEYUP,			188,  FALSE, FALSE, TRUE,
	 WM_KEYDOWN,		32,   FALSE, TRUE,  FALSE,  // Space
	 WM_CHAR,			32,   FALSE, TRUE,  FALSE,
	 WM_KEYUP,			32,   FALSE, FALSE, TRUE,
	 WM_KEYDOWN,		20,   FALSE, TRUE,  FALSE,  // CAPS LOCKS
	 WM_KEYUP,			20,   FALSE, FALSE, TRUE,
	 WM_KEYDOWN,		39,   FALSE, TRUE,  FALSE,  // right arrow
     WM_KEYUP,			39,   FALSE, FALSE, TRUE,
     WM_KEYDOWN,		 9,   FALSE, TRUE,  FALSE,  // TAB
	 WM_CHAR,			 9,   FALSE, TRUE,  FALSE,
     WM_KEYUP,			 9,   FALSE, FALSE, TRUE,
	 WM_KEYDOWN,		16,	  FALSE, TRUE,  FALSE,	// SHIFT
	 WM_KEYUP,			16,   FALSE, FALSE, TRUE,
	 WM_KEYDOWN,		17,   FALSE, TRUE,  FALSE,  // CTRL
	 WM_KEYUP,			17,   FALSE, FALSE, TRUE,
	 WM_SYSKEYDOWN,		18,   TRUE,  TRUE,  FALSE,  // ALT   
	 WM_SYSKEYUP,		18,	  FALSE, FALSE, TRUE,
	 WM_KEYDOWN,		17,   FALSE, TRUE,  FALSE,  // CTRL z
	 WM_KEYDOWN,		90,   FALSE, TRUE,  FALSE,
	 WM_CHAR,			26,   FALSE, TRUE,  FALSE,   
	 WM_KEYUP,			90,   FALSE, FALSE, TRUE,
	 WM_KEYUP,			17,   FALSE, FALSE, TRUE,
	 WM_KEYDOWN,		27,   FALSE, TRUE,  FALSE,  // ESC
	 WM_CHAR,			27,   FALSE, TRUE,  FALSE,
	 WM_KEYUP,			27,   FALSE, FALSE, TRUE,
	 WM_SYSKEYDOWN,		18,   TRUE,  TRUE,  FALSE,  // ALT z   
	 WM_SYSKEYDOWN,		90,   TRUE,  TRUE,  FALSE,
	 WM_SYSCHAR,		90,   TRUE,  TRUE,  FALSE,
	 WM_SYSKEYUP,		90,   TRUE,  FALSE, TRUE,
	 WM_KEYUP,		    18,   FALSE, FALSE, TRUE,
	 WM_KEYDOWN,		187,  FALSE, TRUE,  FALSE,  // =
	 WM_CHAR,			61,   FALSE, TRUE,  FALSE,
	 WM_KEYUP,			187,  FALSE, FALSE, TRUE,
	 WM_KEYDOWN,		86,   FALSE, TRUE,  FALSE, // v
	 WM_CHAR,			86,   FALSE, TRUE,  FALSE,
	 WM_KEYUP,			86,   FALSE, FALSE, TRUE,
	 WM_KEYDOWN,		13,   FALSE, TRUE,  FALSE, // ENTER
	 WM_CHAR,			13,   FALSE, TRUE,  FALSE,
	 WM_KEYUP,			13,   FALSE, FALSE, TRUE,
	 WM_KEYDOWN,		52,   FALSE, TRUE,  FALSE,  // 4
	 WM_CHAR,			52,   FALSE, TRUE,  FALSE,
	 WM_KEYUP,			52,   FALSE, FALSE, TRUE,
	 WM_KEYDOWN,		16,   FALSE, TRUE,  FALSE, // Shift and v
	 WM_KEYDOWN,		86,   FALSE, TRUE,  FALSE,
	 WM_CHAR,			118,  FALSE, TRUE,  FALSE,
	 WM_KEYUP,			86,   FALSE, FALSE, TRUE,
	 WM_KEYUP,			16,   FALSE, FALSE, TRUE,
	 WM_KEYDOWN,		16,   FALSE, TRUE,  FALSE,  // Shift and 4
	 WM_KEYDOWN,		52,   FALSE, TRUE,  FALSE,
	 WM_CHAR,			36,   FALSE, TRUE,  FALSE,
	 WM_KEYUP,			52,   FALSE, FALSE, TRUE,
	 WM_KEYUP,			16,   FALSE, FALSE, TRUE,
	 WM_KEYDOWN,		8,    FALSE, TRUE,  FALSE,  // Backspace
	 WM_CHAR,			8,    FALSE, TRUE,  FALSE,
	 WM_KEYUP,			8,    FALSE, FALSE, TRUE,
	 WM_KEYDOWN,		17,   FALSE, TRUE,  FALSE,  // Ctrl and 4
	 WM_KEYDOWN,		52,   FALSE, TRUE,  FALSE,
	 WM_KEYUP,			52,   FALSE, FALSE, TRUE,
	 WM_KEYUP,			17,   FALSE, FALSE, TRUE,
	 WM_KEYDOWN,		188,  FALSE, TRUE,  FALSE,  // Comma
	 WM_CHAR,			44,   FALSE, TRUE,  FALSE,
	 WM_KEYUP,			188,  FALSE, FALSE, TRUE,
	 WM_KEYDOWN,		32,   FALSE, TRUE,  FALSE,  // Space
	 WM_CHAR,			32,   FALSE, TRUE,  FALSE,
	 WM_KEYUP,			32,   FALSE, FALSE, TRUE,
	 WM_KEYDOWN,		20,   FALSE, TRUE,  FALSE,  // CAPS LOCKS
	 WM_KEYUP,			20,   FALSE, FALSE, TRUE,
};

//Key sequence for Japanese keyboard


struct JapKeyEvent JapKeyEvents[] = {
	 WM_KEYDOWN,		65,   FALSE, TRUE,  FALSE, // a      1
	 WM_CHAR,			97,   FALSE, TRUE,  FALSE,			// 2
	 WM_KEYUP,			65,   FALSE, FALSE, TRUE,		//		3

     WM_KEYDOWN,		39,   FALSE, TRUE,  FALSE, // right arrow 4
     WM_KEYUP,			39,   FALSE, FALSE, TRUE,			// 5
     
	 WM_KEYDOWN,		40,   FALSE, TRUE,  FALSE, // down arrow	6
     WM_KEYUP,			40,   FALSE, FALSE, TRUE,		//			7

     WM_KEYDOWN,		 9,   FALSE, TRUE,  FALSE, // TAB			8
	 WM_CHAR,			 9,   FALSE, TRUE,  FALSE,		//			9
     WM_KEYUP,			 9,   FALSE, FALSE, TRUE,		//			10
	 
	 WM_KEYDOWN,		16,	  FALSE, TRUE,  FALSE,// SHIFT			11
	 WM_KEYUP,			16,   FALSE, FALSE, TRUE,		//			12
	 
	 WM_KEYDOWN,		17,   FALSE, TRUE,  FALSE, // CTRL			13
	 WM_KEYUP,			17,   FALSE, FALSE, TRUE,				//	14
	 
	 WM_SYSKEYDOWN,		18,   TRUE,  TRUE,  FALSE, // ALT			15
	 WM_SYSKEYUP,		18,	  FALSE, FALSE, TRUE,		//			16
	 
	 WM_KEYDOWN,		17,   FALSE, TRUE,  FALSE,// CTRL z			17
	 WM_KEYDOWN,		90,   FALSE, TRUE,  FALSE,	//				18
	 WM_CHAR,			26,   FALSE, TRUE,  FALSE,	//				19
	 WM_KEYUP,			90,   FALSE, FALSE, TRUE,	//				20
	 WM_KEYUP,			17,   FALSE, FALSE, TRUE,	//				21
	 
	 WM_KEYDOWN,		27,   FALSE, TRUE,  FALSE, // ESC			22
	 WM_CHAR,			27,   FALSE, TRUE,  FALSE,		//			23
	 WM_KEYUP,			27,   FALSE, FALSE, TRUE,			//		24

	 WM_SYSKEYDOWN,		18,   TRUE,  TRUE,  FALSE, // ALT z			25
	 WM_SYSKEYDOWN,		90,   TRUE,  TRUE,  FALSE,		//			26
	 WM_SYSCHAR,		122,  TRUE,  TRUE,  FALSE,		//			27
	 WM_SYSKEYUP,		90,   TRUE,  FALSE, TRUE,		//			28
	 WM_KEYUP,		    18,   FALSE, FALSE, TRUE,		//			29
	
	 WM_KEYDOWN,		86,   FALSE, TRUE,  FALSE, // v	30
	 WM_CHAR,			118,  FALSE, TRUE,  FALSE,	//	31
	 WM_KEYUP,			86,   FALSE, FALSE, TRUE,	//	32
	 
	 WM_KEYDOWN,		13,   FALSE, TRUE,  FALSE, // ENTER   33
	 WM_CHAR,			13,   FALSE, TRUE,  FALSE,	//		 34
	 WM_KEYUP,			13,   FALSE, FALSE, TRUE,	//		  35
	 
	 WM_KEYDOWN,		52,   FALSE, TRUE,  FALSE, // 4			36
	 WM_CHAR,			52,   FALSE, TRUE,  FALSE,	//			37
	 WM_KEYUP,			52,   FALSE, FALSE, TRUE,		//		38
	
	 WM_KEYDOWN,		16,   FALSE, TRUE,  FALSE, // Shift and v   39
	 WM_KEYDOWN,		86,   FALSE, TRUE,  FALSE,		//  40
	 WM_CHAR,			86,   FALSE, TRUE,  FALSE,		//  41
	 WM_KEYUP,			86,   FALSE, FALSE, TRUE,		//  42
	 WM_KEYUP,			16,   FALSE, FALSE, TRUE,		//  43
	 
	 WM_KEYDOWN,		16,   FALSE, TRUE,  FALSE, // Shift and 4	44
	 WM_KEYDOWN,		52,   FALSE, TRUE,  FALSE,		//			45
	 WM_CHAR,			36,   FALSE, TRUE,  FALSE,		//			46
	 WM_KEYUP,			52,   FALSE, FALSE, TRUE,		//			47
	 WM_KEYUP,			16,   FALSE, FALSE, TRUE,		//			48
	 
	 WM_KEYDOWN,		8,    FALSE, TRUE,  FALSE,// Backspace		49
	 WM_CHAR,			8,    FALSE, TRUE,  FALSE,		//			50
	 WM_KEYUP,			8,    FALSE, FALSE, TRUE,		//			51

	 WM_KEYDOWN,		17,   FALSE, TRUE,  FALSE,  // Ctrl and 4	52
	 WM_KEYDOWN,		52,   FALSE, TRUE,  FALSE,		//			53
	 WM_KEYUP,			52,   FALSE, FALSE, TRUE,		//			54
	 WM_KEYUP,			17,   FALSE, FALSE, TRUE,		//			55

	 WM_KEYDOWN,		188,  FALSE, TRUE,  FALSE,   // Comma		56
	 WM_CHAR,			44,   FALSE, TRUE,  FALSE,			//		57
	 WM_KEYUP,			188,  FALSE, FALSE, TRUE,           //		58
	 
	 WM_KEYDOWN,		32,   FALSE, TRUE,  FALSE,  // Space		59
	 WM_CHAR,			32,   FALSE, TRUE,  FALSE,			//		60
	 WM_KEYUP,			32,   FALSE, FALSE, TRUE,			//		61

	 WM_KEYDOWN,		16,   FALSE, TRUE,  FALSE,  // Shift  jap key = needs shift also
	 WM_KEYDOWN,		20,   FALSE, TRUE,  FALSE,  // CAPS LOCK			63
	 WM_KEYUP,			20,   FALSE, FALSE, TRUE,			//				64
	 WM_KEYUP,			16,	  FALSE, FALSE, TRUE,					//		65

	 WM_KEYDOWN,		39,   FALSE, TRUE,  FALSE,  // right arrow			66
     WM_KEYUP,			39,   FALSE, FALSE, TRUE,					//		67
     
	 WM_KEYDOWN,		 9,   FALSE, TRUE,  FALSE,  // TAB			//		68
	 WM_CHAR,			 9,   FALSE, TRUE,  FALSE,					//		69
     WM_KEYUP,			 9,   FALSE, FALSE, TRUE,					//		70
	 
	 WM_KEYDOWN,		16,	  FALSE, TRUE,  FALSE,	// SHIFT		//		71
	 WM_KEYUP,			16,   FALSE, FALSE, TRUE,				//			72
	 
	 WM_KEYDOWN,		17,   FALSE, TRUE,  FALSE,  // CTRL			//		73
	 WM_KEYUP,			17,   FALSE, FALSE, TRUE,				//			74
	 
	 WM_SYSKEYDOWN,		18,   TRUE,  TRUE,  FALSE,  // ALT					75
	 WM_SYSKEYUP,		18,	  FALSE, FALSE, TRUE,			//				76
	 
	 WM_KEYDOWN,		17,   FALSE, TRUE,  FALSE,  // CTRL z				77
	 WM_KEYDOWN,		90,   FALSE, TRUE,  FALSE,			//				78
	 WM_CHAR,			26,   FALSE, TRUE,  FALSE,			//				79
	 WM_KEYUP,			90,   FALSE, FALSE, TRUE,			//				80
	 WM_KEYUP,			17,   FALSE, FALSE, TRUE,			//				81
	 
	 WM_KEYDOWN,		27,   FALSE, TRUE,  FALSE,  // ESC					82
	 WM_CHAR,			27,   FALSE, TRUE,  FALSE,			//				83
	 WM_KEYUP,			27,   FALSE, FALSE, TRUE,			//				84
	 
	 WM_SYSKEYDOWN,		18,   TRUE,  TRUE,  FALSE,  // ALT z  //			85
	 WM_SYSKEYDOWN,		90,   TRUE,  TRUE,  FALSE,				//			86
	 WM_SYSCHAR,		90,   TRUE,  TRUE,  FALSE,				//			87
	 WM_SYSKEYUP,		90,   TRUE,  FALSE, TRUE,				//		88
	 WM_KEYUP,		    18,   FALSE, FALSE, TRUE,				//		89
	 	 
	 WM_KEYDOWN,		86,   FALSE, TRUE,  FALSE, // v		//			90
	 WM_CHAR,			86,   FALSE, TRUE,  FALSE,			//			91
	 WM_KEYUP,			86,   FALSE, FALSE, TRUE,				//		92
	 
	 WM_KEYDOWN,		13,   FALSE, TRUE,  FALSE, // ENTER		//		93
	 WM_CHAR,			13,   FALSE, TRUE,  FALSE,				//		94
	 WM_KEYUP,			13,   FALSE, FALSE, TRUE,				//		95
	 
	 WM_KEYDOWN,		52,   FALSE, TRUE,  FALSE,  // 4		//		96
	 WM_CHAR,			52,   FALSE, TRUE,  FALSE,				//		97
	 WM_KEYUP,			52,   FALSE, FALSE, TRUE,				//		98
	 
	 WM_KEYDOWN,		16,   FALSE, TRUE,  FALSE, // Shift and v		99
	 WM_KEYDOWN,		86,   FALSE, TRUE,  FALSE,				//		100
	 WM_CHAR,			118,  FALSE, TRUE,  FALSE,				//		101
	 WM_KEYUP,			86,   FALSE, FALSE, TRUE,				//		102
	 WM_KEYUP,			16,   FALSE, FALSE, TRUE,				//		103
	 
	 WM_KEYDOWN,		16,   FALSE, TRUE,  FALSE,  // Shift and 4	//	104
	 WM_KEYDOWN,		52,   FALSE, TRUE,  FALSE,					//	105
	 WM_CHAR,			36,   FALSE, TRUE,  FALSE,				//		106
	 WM_KEYUP,			52,   FALSE, FALSE, TRUE,				//		107
	 WM_KEYUP,			16,   FALSE, FALSE, TRUE,				//		108
	 
	 WM_KEYDOWN,		8,    FALSE, TRUE,  FALSE,  // Backspace		109
	 WM_CHAR,			8,    FALSE, TRUE,  FALSE,				//		110		
	 WM_KEYUP,			8,    FALSE, FALSE, TRUE,				//		111
	 
	 WM_KEYDOWN,		17,   FALSE, TRUE,  FALSE,  // Ctrl and 4	//	112
	 WM_KEYDOWN,		52,   FALSE, TRUE,  FALSE,					//	113
	 WM_KEYUP,			52,   FALSE, FALSE, TRUE,					//	114
	 WM_KEYUP,			17,   FALSE, FALSE, TRUE,					//	115
	 
	 WM_KEYDOWN,		188,  FALSE, TRUE,  FALSE,  // Comma		//	116
	 WM_CHAR,			44,   FALSE, TRUE,  FALSE,					//	117
	 WM_KEYUP,			188,  FALSE, FALSE, TRUE,					//	118
	 
	 WM_KEYDOWN,		32,   FALSE, TRUE,  FALSE,  // Space		//	119
	 WM_CHAR,			32,   FALSE, TRUE,  FALSE,					//	120
	 WM_KEYUP,			32,   FALSE, FALSE, TRUE,					//	121
	 
	 WM_KEYDOWN,		20,   FALSE, TRUE,  FALSE,  // CAPS LOCKS	//	122
	 WM_KEYUP,			20,   FALSE, FALSE, TRUE,	//				//	123
};

/*    For Japanese Keyboard ************************************** */
// compare the message type and the virutal key (wParam)

BOOL isEventEqual(UINT message, WPARAM wParam, LPARAM lParam, JapKeyEvent keEvent) {
	if (message != keEvent.message) {
		 g_pKato->Log( LOG_WARN, 
			 TEXT("Unexpected message type.  Expected %u, Recieved %u"), 
             keEvent.message, message);
		 return FALSE;
	}
	if (((int)wParam) != keEvent.ikeyId) {
		g_pKato->Log( LOG_WARN, 
			 TEXT("Unexpected virtual key.  Expected %d, Recieved %d"), 
             keEvent.ikeyId, wParam);
		return FALSE;
	}
    
	BOOL fAltPressed = (0x20000000&lParam?TRUE:FALSE);
	if (fAltPressed != keEvent.fAltPressed) {
		g_pKato->Log( LOG_WARN, 
			 TEXT("Unexpected alt state."));
		return FALSE;
	}

	BOOL fPrevUp = (0x40000000&lParam?FALSE:TRUE);
	if (fPrevUp != keEvent.fPrevUp) {
		g_pKato->Log( LOG_WARN, 
			 TEXT("Unexpected prev state."));
		return FALSE;
	}

	BOOL fTransUp = (0x80000000&lParam?TRUE:FALSE);
	if (fTransUp != keEvent.fTransUp) {
		g_pKato->Log( LOG_WARN, 
			 TEXT("Unexpected trans state."));
		 return FALSE;
	}
		
	return TRUE;

}


/********************************************************************************
Check the key sequence in japanese keyboard */

LRESULT CALLBACK KeySequenceWindProc(HWND hwnd, UINT message,WPARAM wParam, LPARAM lParam) {
   PAINTSTRUCT ps;
   HDC hdc;

   static int iKey, iKeySequence;
   static BOOL bDone;   // finished with test sequence
   static TCHAR szDirections[] = TEXT("Follow Directions in Bottom Box. \r\n \
Press <x y> means press y after pressing and holding x, then release y, then release x.");

   TCHAR szBuffer[512];


   switch(message) {
   case WM_PAINT:
      BeginPaint(hwnd, &ps);
	  initWindow(ps.hdc, TEXT("Driver Tests: Keyboard: Manual Tests"), szDirections);
	  emptyKeyAndInstrBuffers();
      _stprintf(szBuffer, TEXT("Press the <%s> key(s).                                    "),
	        JapKeyNames[iKey].szKeySeq);
	  recordInstruction(szBuffer);
      showInstruction(ps.hdc);
	  g_pKato->Log( LOG_DETAIL, szBuffer);
      EndPaint(hwnd, &ps);
      return 0;
   case WM_INIT:
	  bDone = FALSE;
	  g_bSequencePassed = FALSE;
      iKey = (int)wParam;
	  if (!iKey)
		  iKeySequence = 0;
	  else
		  iKeySequence = JapKeyNames[iKey - 1].iLast + 1;
      emptyKeyAndInstrBuffers();
      return 0;
   case WM_SYSKEYDOWN:  
   case WM_KEYDOWN:
   case WM_CHAR:
	     g_pKato->Log(LOG_DETAIL,TEXT("Char : 0x%x   0x%x"),wParam,lParam); //a-rajtha
   case WM_DEADCHAR:
   case WM_SYSCHAR:
   case WM_SYSDEADCHAR:
	   if(0x40000000 & lParam)  // skip repeats 
         return 0;
   case WM_KEYUP:
   case WM_SYSKEYUP:
	  hdc = GetDC(hwnd);
      recordKey(message, wParam, lParam);
	  showKeyEvent(hdc);
	  if (isEventEqual(message, wParam, lParam, JapKeyEvents[iKeySequence])) {
		  if (iKeySequence == JapKeyNames[iKey].iLast) {   // if time for next key;
			   g_bSequencePassed = TRUE;
		       bDone = TRUE;
		  }
		  else
			  iKeySequence++;
      }  // if the event was valid
	  else {
		  bDone = TRUE;
		  // g_bSequencePassed is false as default
	  }

	  ReleaseDC(hwnd, hdc);
	  return 0;

   case WM_GOAL:
      return bDone;
   
   }
   return (DefWindowProc(hwnd, message, wParam, lParam));
}



/****************************************************************************************
// compare the message type and the virutal key (wParam)
BOOL isEventEqual(UINT message, WPARAM wParam, LPARAM lParam, keyEvent keEvent) {
	if (message != keEvent.message) {
		 g_pKato->Log( LOG_WARN, 
			 TEXT("Unexpected message type.  Expected %u, Recieved %u"), 
             keEvent.message, message);
		 return FALSE;
	}
	if (((int)wParam) != keEvent.ikeyId) {
		g_pKato->Log( LOG_WARN, 
			 TEXT("Unexpected virtual key.  Expected %d, Recieved %d"), 
             keEvent.ikeyId, wParam);
		return FALSE;
	}
    
	BOOL fAltPressed = (0x20000000&lParam?TRUE:FALSE);
	if (fAltPressed != keEvent.fAltPressed) {
		g_pKato->Log( LOG_WARN, 
			 TEXT("Unexpected alt state."));
		return FALSE;
	}

	BOOL fPrevUp = (0x40000000&lParam?FALSE:TRUE);
	if (fPrevUp != keEvent.fPrevUp) {
		g_pKato->Log( LOG_WARN, 
			 TEXT("Unexpected prev state."));
		return FALSE;
	}

	BOOL fTransUp = (0x80000000&lParam?TRUE:FALSE);
	if (fTransUp != keEvent.fTransUp) {
		g_pKato->Log( LOG_WARN, 
			 TEXT("Unexpected trans state."));
		 return FALSE;
	}
		
	return TRUE;

}


//****************************************************************************************   
LRESULT CALLBACK KeySequenceWindProc(HWND hwnd, UINT message,WPARAM wParam, LPARAM lParam) {
   PAINTSTRUCT ps;
   HDC hdc;

   static int iKey, iKeySequence;
   static BOOL bDone;   // finished with test sequence
   static TCHAR szDirections[] = TEXT("Follow Directions in Bottom Box. \r\n \
Press <x y> means press y after pressing and holding x, then release y, then release x.");

   TCHAR szBuffer[512];


   switch(message) {
   case WM_PAINT:
      BeginPaint(hwnd, &ps);
	  initWindow(ps.hdc, TEXT("Driver Tests: Keyboard: Manual Tests"), szDirections);
	  emptyKeyAndInstrBuffers();
      _stprintf(szBuffer, TEXT("Press the <%s> key(s).                                    "),
	        keyNames[iKey].szKeySeq);
	  recordInstruction(szBuffer);
      showInstruction(ps.hdc);
	  g_pKato->Log( LOG_DETAIL, szBuffer);
      EndPaint(hwnd, &ps);
      return 0;
   case WM_INIT:
	  bDone = FALSE;
	  g_bSequencePassed = FALSE;
      iKey = (int)wParam;
	  if (!iKey)
		  iKeySequence = 0;
	  else
		  iKeySequence = keyNames[iKey - 1].iLast + 1;
      emptyKeyAndInstrBuffers();
      return 0;
   case WM_SYSKEYDOWN:  
   case WM_KEYDOWN:
   case WM_CHAR:
	     g_pKato->Log(LOG_DETAIL,TEXT("Char : 0x%x   0x%x"),wParam,lParam); //a-rajtha
   case WM_DEADCHAR:
   case WM_SYSCHAR:
   case WM_SYSDEADCHAR:
	   if(0x40000000 & lParam)  // skip repeats 
         return 0;
   case WM_KEYUP:
   case WM_SYSKEYUP:
	  hdc = GetDC(hwnd);
      recordKey(message, wParam, lParam);
	  showKeyEvent(hdc);
	  if (isEventEqual(message, wParam, lParam, keyEvents[iKeySequence])) {
		  if (iKeySequence == keyNames[iKey].iLast) {   // if time for next key;
			   g_bSequencePassed = TRUE;
		       bDone = TRUE;
		  }
		  else
			  iKeySequence++;
      }  // if the event was valid
	  else {
		  bDone = TRUE;
		  // g_bSequencePassed is false as default
	  }

	  ReleaseDC(hwnd, hdc);
	  return 0;

   case WM_GOAL:
      return bDone;
   
   }
   return (DefWindowProc(hwnd, message, wParam, lParam));
}
*/ 

//***********************************************************************************
void AutoDepressTest(void) {
 //  MessageBox(globalHwnd, TEXT("Make sure Caps Lock is off!"),
 //       TEXT("Checking Caps Lock..."), MB_OK | MB_ICONWARNING);
   clean();
   SendMessage(globalHwnd,WM_SWITCHPROC,KEYSEQUENCE,0);

   int iKey = 0;
   while (iKey < countof(keyNames)) {
		SendMessage(globalHwnd,WM_INIT, iKey, 0);
		doUpdate(NULL);
		clean();
		if (pump() == 2)
			break;   // timed out

		if (!g_bSequencePassed) {
			if (askMessage(TEXT("Key Sequence was invalid.  Do you want to try again?"),1))
			  continue;
		    else {
			  g_pKato->Log( LOG_FAIL, TEXT("Sequence failed: %s"), 
                 keyNames[iKey].szKeySeq);
				  
			  if (!askMessage(TEXT("Continue with rest of this test sequence?"),1)) 
				  break;
			}  // dont' want to try again
		}

		iKey++;
   }  // for
}

//***********************************************************************************
///  Entry point for sequence test
TESTPROCAPI AutoCheck_T(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE){
   NO_MESSAGES;
 
   
   /*  This works for both US and Japanese keyboards
       Check the capslock status, if it is on swith it off */
   if (GetKeyState(VK_CAPITAL)) {
       g_pKato->Log(LOG_DETAIL ,TEXT(" CAPS FOUND AND TURNING IT OFF"));
	   keybd_event (VK_SHIFT,0,0,0);
       keybd_event( VK_CAPITAL, 0, 0, 0 );
       keybd_event( VK_CAPITAL, 0, KEYEVENTF_KEYUP, 0 ); 
	   keybd_event (VK_SHIFT, 0, KEYEVENTF_KEYUP,0);
   }

   AutoDepressTest();
   return getCode();
}
