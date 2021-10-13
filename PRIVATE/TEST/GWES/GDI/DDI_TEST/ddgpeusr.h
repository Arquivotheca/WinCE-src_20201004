//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
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
