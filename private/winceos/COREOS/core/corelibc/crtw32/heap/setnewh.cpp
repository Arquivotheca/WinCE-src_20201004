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
*setnewh.cpp - defines C++ set_new_handler() routine
*
*
*Purpose:
*       Defines C++ set_new_handler() routine.
*
*******************************************************************************/

#include <corecrt.h>
#include <new.h>
#include <coredll.h>   // define debug zone information

/***
*new_handler set_new_handler - set the ANSI C++ new handler
*
*Purpose:
*       Set the ANSI C++ per-thread new handler.
*
*Entry:
*       Pointer to the new handler to be installed.
*
*       WARNING: set_new_handler is a stub function that is provided to
*       allow compilation of the Standard Template Library (STL).
*
*       Do NOT use it to register a new handler. Use _set_new_handler instead.
*
*       However, it can be called to remove the current handler:
*
*           set_new_handler(NULL); // calls _set_new_handler(NULL)
*
*Return:
*       Previous ANSI C++ new handler
*
*******************************************************************************/

new_handler set_new_handler (
        new_handler new_p
        )
{
        // cannot use stub to register a new handler
        if (new_p != 0) {
#ifdef _CRTBLD
        printf("Error - ::set_new_handler() is a stub function.\n");
        printf("      Invoke only with argument NULL, to remove current handler.\n");
#else
        RETAILMSG(1, (TEXT("Error - ::set_new_handler() is a stub function.\r\n")));
        RETAILMSG(1, (TEXT("      Invoke only with argument NULL, to remove current handler.\r\n")));
#endif
        return 0;
    }

        // remove current handler
        _set_new_handler(0);

        return 0;
}
