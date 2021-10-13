//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
////////////////////////////////////////////////////////////////////////////////
//
//  FLSHWEAR TUX DLL
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
