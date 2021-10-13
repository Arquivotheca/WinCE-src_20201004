//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>
#include <initguid.h>
#if defined (UNDER_CE) || defined (WINCE_EMULATION)
#include "bt_sdp.h"
#else
#include "bt_100.h"
#endif

#include "bthguid.h"
