//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/***
*new_mode.cpp - defines C++ setHandler mode
*
*
*Purpose:
*   Defines routines to set and to query the C++ new handler mode.
*
*   The new handler mode flag determines whether malloc() failures will
*   call the new() failure handler, or whether malloc will return NULL.
*
*******************************************************************************/

#include <corecrt.h>
#include <new.h>
#include <internal.h>

int _newmode;

int __cdecl _set_new_mode( int nhm )
{
    int nhmOld;

    /*
     * The only valid inputs are 0 and 1
     */

    if ( ( nhm & 01 ) != nhm )
        return -1;

    /*
     * Set the new mode and return the old
     */
    nhmOld = _newmode;
    _newmode = nhm;

    return nhmOld;
}

int __cdecl _query_new_mode ( void )
{
    return _newmode;
}
