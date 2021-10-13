//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/***
*user.cpp - E.H. functions only called by the client programs
*
*
*Purpose:
*       Exception handling functions only called by the client programs,
*       not by the C/C++ runtime itself.
*
*       Entry Points:
*       * set_terminate
*       * set_unexpected
*       * _set_seh_translator
*       * _set_inconsistency
*
****/

#include <windows.h>
#include <corecrt.h>
#include <mtdll.h>

#include <exception>
#include <ehhooks.h>

#pragma hdrstop

/////////////////////////////////////////////////////////////////////////////
//
// set_terminate - install a new terminate handler (ANSI Draft 17.1.2.1.3)
//

_CRTIMP std::terminate_handler __cdecl
std::set_terminate( std::terminate_handler pNew )
{
    std::terminate_handler pOld = __pTerminate;
    __pTerminate = pNew;
    return pOld;
}


/////////////////////////////////////////////////////////////////////////////
//
// set_unexpected - install a new unexpected handler (ANSI Draft 17.1.2.1.3)
//

_CRTIMP std::unexpected_handler __cdecl
std::set_unexpected( std::unexpected_handler pNew )
{
    std::unexpected_handler pOld = __pUnexpected;
    __pUnexpected = pNew;
    return pOld;
}




/////////////////////////////////////////////////////////////////////////////
//
// _set_inconsistency - install a new inconsistency handler(Internal Error)
//
// (This function is currently not documented for end-users.  At some point,
//  it might be advantageous to allow end-users to "catch" internal errors
//  from the EH CRT, but for now, they will terminate(). )

_inconsistency_function __cdecl
__set_inconsistency( _inconsistency_function pNew)
{
    _inconsistency_function pOld = __pInconsistency;
    __pInconsistency = pNew;
    return pOld;
}
