/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright  1998-2000  Microsoft Corporation.  All Rights Reserved.

Module Name:

     ratdelay.h
 
Abstract:
 
	This file includes a defines used in ratdelay.cpp 
--*/
#ifndef _RATDELAY_H
#define _RATDELAY_H

/* ------------------------------------------------------------------------
	Defines needed for setting Repeat Rate and Key Delay
------------------------------------------------------------------------ */
#define MIN_KB_DELAY	250
#define MAX_KB_DELAY	1000
#define MIN_KB_REPEAT	0
#define MAX_KB_REPEAT	30


#define DEF_KB_DELAY	(MAX_KB_DELAY / 2)
#define DEF_KB_REPEAT	(MAX_KB_REPEAT / 2)

#define szRepeateRate 	TEXT("RepeatRate")
#define szKBDelay 	    TEXT("InitialDelay")
#define szKBDispDelay   TEXT("DispDly")					// remember delay for display

#ifdef TARGET_NT
#define szKBRegistryKey TEXT("PegControlPanel\\Keyboard")
#else
#define szKBRegistryKey TEXT("ControlPanel\\Keybd")
#endif

#ifdef DEBUG_MSGS
extern TCHAR   		szDPF[180];    

#define	DPF(x)	OutputDebugString(TEXT(x))
#define	DPF1(x,y)	{ \
					wsprintf(szDPF, TEXT(x), y); \
					OutputDebugString(szDPF);	 \
					}

#define	DPF2(x,y,z) { \
					wsprintf(szDPF, TEXT(x), y,z); \
					OutputDebugString(szDPF);	 \
					}

#define	DPF3(x,y,z,w) { \
					wsprintf(szDPF, TEXT(x), y,z,w); \
					OutputDebugString(szDPF);	 \
					}
#else
#define	DPF(x)
#define	DPF1(x,y)
#define	DPF2(x,y,z) 
#define	DPF3(x,y,z,w) 
#endif


#endif _RATDELAY_H
