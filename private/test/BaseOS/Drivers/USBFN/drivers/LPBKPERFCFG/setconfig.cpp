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
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//******************************************************************************
// 
// Module Name:  
//    setconfig.cpp
// 
// Abstract:  This file includes a function that let user to input device-specific information
//            
//     
// Notes: 
// Configurations for Performance Tests
//


#include <windows.h>
#include <usbfn.h>
#include <usbfntypes.h>
#include "config.h"


#define OUT_OF_MEMORY_CHECK(_argument_) if( (_argument_) == NULL) { \
        NKDbgPrintfW(L"Out of memory!"); \
        goto EXIT; }

#define CREATE_ENDPOINT_DESCRIPTOR(_EP_INDEX_,_EP_ADDR_,_EP_TYPE_,_EP_MAXPACKSIZE_,_EP_INTERVAL_,_EP_PAIR_INDEX_) { \
    pCurEP = &pCurIf->pEPs[_EP_INDEX_].usbEP;               \
    pCurEP->bEndpointAddress = _EP_ADDR_;                   \
    pCurEP->bmAttributes = _EP_TYPE_;                       \
    pCurEP->wMaxPacketSize = _EP_MAXPACKSIZE_;              \
    pCurEP->bInterval = _EP_INTERVAL_;                      \
    pCurIf->pEPs[_EP_INDEX_].iPairAddr = _EP_PAIR_INDEX_;   \
}


//****************************************************************************
//This function is to input device specific setting values
//
//This function is the ONLY function that user needs to change if he/she wants 
//to modify the loopback driver to fit into any other function devices
//****************************************************************************

PDEVICE_SETTING InitializeSettings(){

    PDEVICE_SETTING pDevSetting = (PDEVICE_SETTING) new DEVICE_SETTING;
    if(pDevSetting == NULL)
        return NULL;

    BOOL bConfig = FALSE;

    PONE_CONFIG pCurConfig;
    PONE_IF pCurIf;
    PUSB_ENDPOINT_DESCRIPTOR pCurEP;

    //---set device descriptor related values---
    //vendor id ?
    pDevSetting->uVendorId = 0x045e;//<--USER INPUT
    //device id ?
    pDevSetting->uProductId = 0xffe0;//<--USER INPUT
    //USB version: 2.0 or 1.1?
    pDevSetting->ubcdUSBVer = 0x200;//<--USER INPUT
    //Max packet size of end point 0 ?
    pDevSetting->uEP0PacketSize = 0x40;//<--USER INPUT

    NKDbgPrintfW(L"\r\n\t*** LpBkPerfCfg!InitializeSettings() ven=0x045E prod=0xFFE0 v.2 ***\r\n");

    //---set configuration related values ---
    //how many configurations does this device have ?
    pDevSetting->uNumofCFs = 2;//<--USER INPUT

    //create high/full speed configuation structures
    pDevSetting->pHighSpeedCFs = (PONE_CONFIG) new ONE_CONFIG[pDevSetting->uNumofCFs];
    OUT_OF_MEMORY_CHECK(pDevSetting->pHighSpeedCFs);
    memset(pDevSetting->pHighSpeedCFs, 0, sizeof(ONE_CONFIG)*pDevSetting->uNumofCFs);

    pDevSetting->pFullSpeedCFs = (PONE_CONFIG) new ONE_CONFIG[pDevSetting->uNumofCFs];
    OUT_OF_MEMORY_CHECK(pDevSetting->pFullSpeedCFs);
    memset(pDevSetting->pFullSpeedCFs, 0, sizeof(ONE_CONFIG)*pDevSetting->uNumofCFs);

    //************************************************************************************
    // --- set configuration 0 related values ---
    //************************************************************************************
#include "subcfg0.cpp"


    //************************************************************************************
    // --- set configuration 1 related values ---
    //************************************************************************************
#include "subcfg1.cpp"


    bConfig = TRUE;

EXIT:

    if(bConfig == FALSE && pDevSetting != NULL){//sth. wrong, need to cleanup
        //clear device settings
        if(pDevSetting->pHighSpeedCFs != NULL){
            PONE_CONFIG pCurCFs = pDevSetting->pHighSpeedCFs;
            for(int i = 0; i < pDevSetting->uNumofCFs; i++){
                if(pCurCFs[i].pIFs != NULL){
                    PONE_IF pCurIFs = pCurCFs[i].pIFs;
                    for(int j = 0; j < pCurCFs[i].uNumofIFs; j++){
                        if(pCurIFs[j].pEPs != NULL){
                            delete[] pCurIFs[j].pEPs;
                        }
                    }
                    delete[] pCurIFs;
                }
            }
            delete[] pCurCFs;
        }
        if(pDevSetting->pFullSpeedCFs != NULL){
            PONE_CONFIG pCurCFs = pDevSetting->pFullSpeedCFs;
            for(int i = 0; i < pDevSetting->uNumofCFs; i++){
                if(pCurCFs[i].pIFs != NULL){
                    PONE_IF pCurIFs = pCurCFs[i].pIFs;
                    for(int j = 0; j < pCurCFs[i].uNumofIFs; j++){
                        if(pCurIFs[j].pEPs != NULL){
                            delete[] pCurIFs[j].pEPs;
                        }
                    }
                    delete[] pCurIFs;
                }
            }
            delete[] pCurCFs;
        }
        
        delete pDevSetting;
        pDevSetting = NULL;
    }

    return pDevSetting;
}

