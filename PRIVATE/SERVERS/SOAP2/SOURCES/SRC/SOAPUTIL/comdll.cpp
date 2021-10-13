//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//+---------------------------------------------------------------------------------
//
//
// File:
//      ComDll.cpp
//
// Contents:
//
//      Defines standard DllCanUnloadNow implementation
//
//----------------------------------------------------------------------------------

#include "Headers.h"

STDAPI CanUnloadNow()
{
    static HRESULT s_result[] = { S_OK, S_FALSE };
    return s_result[g_cLock || g_cObjects];
}
