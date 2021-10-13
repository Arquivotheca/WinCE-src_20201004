////////////////////////////////////////////////////////////////////////////////
//
//  TUXTEST TUX DLL
//  Copyright (c) Microsoft Corporation
//
//  Module: globals.cpp
//          Causes the header globals.h to actually define the global variables.
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////

#include "main.h"

// By defining the symbol __GLOBALS_CPP__, we force the file globals.h to
// define, instead of declare, all global variables.
#define __GLOBALS_CPP__
#include "globals.h"

// By defining the symbol __DEFINE_FTE__, we force the file ft.h to define
#define __DEFINE_FTE__
#include "ft.h"
