//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __STDAFX_H
#define __STDAFX_H

//------------------------------------------------------------------------------

#include "windows.h"
#include "tchar.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "pkfuncs.h"
#include "ndis.h"
#include "NDT_Ext.h"

#ifdef __cplusplus
}
#endif

void *operator new(size_t nSize, void *p);

//------------------------------------------------------------------------------

#endif
