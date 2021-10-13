//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

// 
// This file contains definitions for debug zones used by the device manager.
//

#pragma once

//
// Debug zones
//
#define ZONE_ERROR      DEBUGZONE(0)
#define ZONE_WARNING    DEBUGZONE(1)
#define ZONE_FUNCTION   DEBUGZONE(2)
#define ZONE_INIT       DEBUGZONE(3)
#define ZONE_ROOT       DEBUGZONE(4)
#define ZONE_ACTIVE     DEBUGZONE(6)
#define ZONE_RESMGR     DEBUGZONE(7)
#define ZONE_FSD        DEBUGZONE(8)
#define ZONE_DYING      DEBUGZONE(9)
#define ZONE_BOOTSEQ    DEBUGZONE(10)
#define ZONE_PNP        DEBUGZONE(11)
