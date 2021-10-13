//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/***
*woutput.c - wprintf style output to a FILEX (wchar_t version)
*
*
*Purpose:
*   This file defines the symbol UNICODE and then #includes the file
*   "output.c" in order to implement _woutput(), the helper for the
*   wide character versions of *wprintf() family of functions.
*
*******************************************************************************/

#define CRT_UNICODE
#include "output.c"

