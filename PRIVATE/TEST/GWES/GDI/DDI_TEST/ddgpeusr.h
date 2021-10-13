//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#ifndef __GPEDDHALUSER_H__
#define __GPEDDHALUSER_H__

// some macros to help in debugging

#define DEBUGENTER(func) 										\
	{															\
		DEBUGMSG(GPE_ZONE_ENTER,(TEXT("Entering function %s\r\n"),TEXT(#func)));		\
	}

#define DEBUGLEAVE(func) 										\
	{															\
		DEBUGMSG(GPE_ZONE_ENTER,(TEXT("Leaving function %s\r\n"),TEXT( #func )));	\
	}


// HARDWARE-SPECIFIC Required #defines
#define IN_VBLANK           (GetTickCount() & 0x1)
#define IS_BUSY				(!(GetTickCount() & 0x1))



#endif //__GPEDDHALUSER_H__
