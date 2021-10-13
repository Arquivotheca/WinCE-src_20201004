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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
//******************************************************************************
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//******************************************************************************
//
// Module Name:  
//    subcfg1.cpp
//
// Abstract:  This file includes parameters for configuration 1
//

    // --- high speed config (if your device only support full/low speed, you still need to present high speed config---
    pCurConfig = &pDevSetting->pHighSpeedCFs[1];
    //how many interfaces does this configuration have?
    pCurConfig->uNumofIFs = 1;//<--USER INPUT
    pCurConfig->pIFs = (PONE_IF) new ONE_IF[pCurConfig->uNumofIFs];
    OUT_OF_MEMORY_CHECK(pCurConfig->pIFs);
    memset(pCurConfig->pIFs, 0, sizeof(ONE_IF)*pCurConfig->uNumofIFs);

    // --- set interface 0 related values ---
    pCurIf = &pCurConfig->pIFs[0];
    //how many endpoints there?
    pCurIf->uNumofEPs = 6;//<--USER INPUT
    pCurIf->pEPs = (PEPSETTINGS) new EPSETTINGS[pCurIf->uNumofEPs];
    OUT_OF_MEMORY_CHECK(pCurIf->pEPs);
    memset(pCurIf->pEPs, 0, sizeof(EPSETTINGS)*pCurIf->uNumofEPs);

    // --- set endpoints for highspeed config 1, interface 0 ---

    //---endpoint 1---
    CREATE_ENDPOINT_DESCRIPTOR(0,0x81,USB_ENDPOINT_TYPE_ISOCHRONOUS,0x1400,0x0,1);
    //---endpoint 2---
    CREATE_ENDPOINT_DESCRIPTOR(1,0x02,USB_ENDPOINT_TYPE_ISOCHRONOUS,0x1400,0x0,0);

    //---endpoint 3---
    CREATE_ENDPOINT_DESCRIPTOR(2,0x83,USB_ENDPOINT_TYPE_BULK,0x200,0x0,3);
    //---endpoint 4---
    CREATE_ENDPOINT_DESCRIPTOR(3,0x04,USB_ENDPOINT_TYPE_BULK,0x200,0x0,2);

    //---endpoint 5---
    CREATE_ENDPOINT_DESCRIPTOR(4,0x85,USB_ENDPOINT_TYPE_INTERRUPT,0x40,0x1,5);
    //---endpoint 6---
    CREATE_ENDPOINT_DESCRIPTOR(5,0x06,USB_ENDPOINT_TYPE_INTERRUPT,0x40,0x1,4);


    //--- full speed config --- 
    pCurConfig = &pDevSetting->pFullSpeedCFs[1];
    //how many interfaces does this configuration have?
    pCurConfig->uNumofIFs = 1;//<--USER INPUT
    pCurConfig->pIFs = (PONE_IF) new ONE_IF[pCurConfig->uNumofIFs];
    OUT_OF_MEMORY_CHECK(pCurConfig->pIFs);
    memset(pCurConfig->pIFs, 0, sizeof(ONE_IF)*pCurConfig->uNumofIFs);

    // --- set interface 0 related values ---
    pCurIf = &pCurConfig->pIFs[0];
    //how many endpoints there?
    pCurIf->uNumofEPs = 6;//<--USER INPUT
    pCurIf->pEPs = (PEPSETTINGS) new EPSETTINGS[pCurIf->uNumofEPs];
    OUT_OF_MEMORY_CHECK(pCurIf->pEPs);
    memset(pCurIf->pEPs, 0, sizeof(EPSETTINGS)*pCurIf->uNumofEPs);

    // --- set endpoints for full speed config 1, interface 0 ---

    //---endpoint 1---
    CREATE_ENDPOINT_DESCRIPTOR(0,0x81,USB_ENDPOINT_TYPE_ISOCHRONOUS,0x200,0x0,1);
    //---endpoint 2---
    CREATE_ENDPOINT_DESCRIPTOR(1,0x02,USB_ENDPOINT_TYPE_ISOCHRONOUS,0x200,0x0,0);

    //---endpoint 3---
    CREATE_ENDPOINT_DESCRIPTOR(2,0x83,USB_ENDPOINT_TYPE_BULK,0x40,0x0,3);
    //---endpoint 4---
    CREATE_ENDPOINT_DESCRIPTOR(3,0x04,USB_ENDPOINT_TYPE_BULK,0x40,0x0,2);

    //---endpoint 5---
    CREATE_ENDPOINT_DESCRIPTOR(4,0x85,USB_ENDPOINT_TYPE_INTERRUPT,0x40,0x1,5);
    //---endpoint 6---
    CREATE_ENDPOINT_DESCRIPTOR(5,0x06,USB_ENDPOINT_TYPE_INTERRUPT,0x40,0x1,4);

