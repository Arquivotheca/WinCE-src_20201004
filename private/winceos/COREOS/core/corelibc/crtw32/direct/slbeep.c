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
/***
*slbeep.c - Sleep and beep
*
*   Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*   defines _sleep() and _beep()
*
*Revision History:
*   08-22-91  BWM   Wrote module.
*   09-29-93  GJF   Resurrected for compatibility with NT SDK (which had
*           the function). Replaced _CALLTYPE1 with __cdecl and
*           removed Cruiser support.
*
*******************************************************************************/

#include <cruntime.h>
#include <oscalls.h>
#include <stdlib.h>

/***
*void _sleep(duration) - Length of sleep
*
*Purpose:
*
*Entry:
*   unsigned long duration - length of sleep in milliseconds or
*   one of the following special values:
*
*       _SLEEP_MINIMUM - Sends a yield message without any delay
*       _SLEEP_FOREVER - Never return
*
*Exit:
*   None
*
*Exceptions:
*
*******************************************************************************/

void __cdecl _sleep(unsigned long dwDuration)
{

    if (dwDuration == 0) {
    dwDuration++;
    }
    Sleep(dwDuration);

}

/***
*void _beep(frequency, duration) - Length of sleep
*
*Purpose:
*
*Entry:
*   unsigned frequency - frequency in hertz
*   unsigned duration - length of beep in milliseconds
*
*Exit:
*   None
*
*Exceptions:
*
*******************************************************************************/

void __cdecl _beep(unsigned dwFrequency, unsigned dwDuration)
{
#ifdef _WIN32_WCE // CE OS doesn't support Beep
    return;  
#else
    Beep(dwFrequency, dwDuration);
#endif // _WIN32_WCE 
}
