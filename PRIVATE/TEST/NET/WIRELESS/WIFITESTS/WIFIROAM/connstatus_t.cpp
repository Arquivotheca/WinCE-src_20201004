//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
// ----------------------------------------------------------------------------
//
// Use of this source code is subject to the terms of the Microsoft end-user
// license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
// If you did not accept the terms of the EULA, you are not authorized to use
// this source code. For a copy of the EULA, please see the LICENSE.RTF on your
// install media.
//
// ----------------------------------------------------------------------------
//
// Implementation of the ConnStatus_t class.
//
// ----------------------------------------------------------------------------

#include "ConnStatus_t.hpp"

#include <assert.h>

using namespace ce::qa;
    
// ----------------------------------------------------------------------------
//
// Constructor:
//
ConnStatus_t::
ConnStatus_t(void)
    : firstCycle(0),
      currentCycle(0),
      cycleTransitions(0),
      connStatus(0),
      cellConnectionHandle(NULL), 
      netChangesSeen(0),
      longConnErrorNetChanges(0),
      multiConnErrorNetChanges(0),
      numberMultiConnErrorsSeen(0),
      numberErrorsInThisCycle(0),
      fErrorLoggedInThisCycle(false),
      failedNoConn(0),        
      failedLongConn(0),    
      failedMultiConn(0),   
      failedUnknownAP(0),    
      failedCellGlitch(0),
      countHomeWiFiConn(0),  
      countOfficeWiFiConn(0), 
      countHotSpotWiFiConn(0),
      countOfficeWiFiRoam(0),  
      countDTPTConn(0),   
      countCellConn(0),
      timeOfLastConnection(0),
      lastCellConnectionTime(0),
      lastCellConnectionClosed(0)
{
    connStatusString[0] = TEXT('\0');
    cellNetType[0]      = TEXT('\0');
    cellNetDetails[0]   = TEXT('\0');
}

// ----------------------------------------------------------------------------
