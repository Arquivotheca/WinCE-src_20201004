#if 0
-------------------------------------------------------------------------------
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright  1997  Microsoft Corporation.  All Rights Reserved.

Module Name:

	globals.cpp

Abstract:

	Causes the header globals.h to actually define the global variables.

Notes:

-------------------------------------------------------------------------------
#endif


#include "main.h"

// By defining the symbol __GLOBALS_CPP__, we force the file globals.h to
// define, instead of declare, all global variables.

#define __GLOBALS_CPP__
#include "globals.h"

// By defining the symbol __DEFINE_FTE__, we force the file ft.h to define
// the function table

#define __DEFINE_FTE__
#include "ft.h"
