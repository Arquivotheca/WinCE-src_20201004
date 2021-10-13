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
//



#include <windows.h>
#include <usbfn.h>
#include <usbfntypes.h>
#include "config.h"

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
        
    //---set device descriptor related values---
    //vendor id ?
    pDevSetting->uVendorId = 0x045e;//<--USER INPUT
    //device id ?
    pDevSetting->uProductId = 0xffe0;//<--USER INPUT
    //USB version: 2.0 or 1.1?
    pDevSetting->ubcdUSBVer = 0x200;//<--USER INPUT
    //Max packet size of end point 0 ?
    pDevSetting->uEP0PacketSize = 0x40;//<--USER INPUT

    
    //---set configuration related values ---
    //how many configurations does this device have ?
    pDevSetting->uNumofCFs = 2;//<--USER INPUT
    //create high/full speed configuation structure
    pDevSetting->pFullSpeedCFs = (PONE_CONFIG) new ONE_CONFIG[pDevSetting->uNumofCFs];
    if(pDevSetting->pFullSpeedCFs == NULL){
        NKDbgPrintfW(L"Out of memory!");
        goto EXIT;
    }
    memset(pDevSetting->pFullSpeedCFs, 0, sizeof(ONE_CONFIG)*pDevSetting->uNumofCFs);
    
    pDevSetting->pHighSpeedCFs = (PONE_CONFIG) new ONE_CONFIG[pDevSetting->uNumofCFs];
    if(pDevSetting->pHighSpeedCFs == NULL){
        NKDbgPrintfW(L"Out of memory!");
        goto EXIT;
    }
    memset(pDevSetting->pHighSpeedCFs, 0, sizeof(ONE_CONFIG)*pDevSetting->uNumofCFs);


    //************************************************************************************
    //  set configuration 1 related values ---
    //************************************************************************************
    
    //  high speed 
   
    PONE_CONFIG pCurConfig = &pDevSetting->pHighSpeedCFs[0];
    //how many interfaces does this configuration have?
    pCurConfig->uNumofIFs = 1;//<--USER INPUT
    pCurConfig->pIFs = (PONE_IF) new ONE_IF[pCurConfig->uNumofIFs];
    if(pCurConfig->pIFs == NULL){
        NKDbgPrintfW(L"Out of memory!");
        goto EXIT;
    }
    memset(pCurConfig->pIFs, 0, sizeof(ONE_IF)*pCurConfig->uNumofIFs);

    
    PONE_IF pCurIf = &pCurConfig->pIFs[0];
    //how many endpoints there?
    pCurIf->uNumofEPs = 6;//<--USER INPUT
    pCurIf->pEPs = (PEPSETTINGS) new EPSETTINGS[pCurIf->uNumofEPs];
    if(pCurIf->pEPs == NULL){
        NKDbgPrintfW(L"Out of memory!");
        goto EXIT;
    }
    memset(pCurIf->pEPs, 0, sizeof(EPSETTINGS)*pCurIf->uNumofEPs);
    //Endpoints for  config 1  HIGH Speed
    
    //---endpoint 1---
    PUSB_ENDPOINT_DESCRIPTOR pCurEP = &pCurIf->pEPs[0].usbEP; 
    pCurEP->bEndpointAddress = 0x81; //<--USER INPUT
    pCurEP->bmAttributes = USB_ENDPOINT_TYPE_BULK;//<--USER INPUT
    pCurEP->wMaxPacketSize = 0x200;//<--USER INPUT
    pCurEP->bInterval = 0x0;//<--USER INPUT
    //will it work with another endpoint to be a loopback pair?
    pCurIf->pEPs[0].iPairAddr = 1; //<-- USER INPUT
    //---endpoint 2---
    pCurEP = &pCurIf->pEPs[1].usbEP; 
    pCurEP->bEndpointAddress = 0x02; //<--USER INPUT
    pCurEP->bmAttributes = USB_ENDPOINT_TYPE_BULK;//<--USER INPUT
    pCurEP->wMaxPacketSize = 0x200;//<--USER INPUT
    pCurEP->bInterval = 0x0;//<--USER INPUT
    //will it work with another endpoint to be a loopback pair?
    pCurIf->pEPs[1].iPairAddr = 0; //<-- USER INPUT
    //---endpoint 3---
    pCurEP = &pCurIf->pEPs[2].usbEP; 
    pCurEP->bEndpointAddress = 0x83; //<--USER INPUT
    pCurEP->bmAttributes = USB_ENDPOINT_TYPE_INTERRUPT;//<--USER INPUT
    pCurEP->wMaxPacketSize = 0x200;//<--USER INPUT
    pCurEP->bInterval = 0x1;//<--USER INPUT
    //will it work with another endpoint to be a loopback pair?
    pCurIf->pEPs[2].iPairAddr = 3; //<-- USER INPUT
    //---endpoint 4---
    pCurEP = &pCurIf->pEPs[3].usbEP; 
    pCurEP->bEndpointAddress = 0x04; //<--USER INPUT
    pCurEP->bmAttributes = USB_ENDPOINT_TYPE_INTERRUPT;//<--USER INPUT
    pCurEP->wMaxPacketSize = 0x200;//<--USER INPUT
    pCurEP->bInterval = 0x1;//<--USER INPUT
    //will it work with another endpoint to be a loopback pair?
    pCurIf->pEPs[3].iPairAddr = 2; //<-- USER INPUT
    //---endpoint 5---
    pCurEP = &pCurIf->pEPs[4].usbEP; 
    pCurEP->bEndpointAddress = 0x85; //<--USER INPUT
    pCurEP->bmAttributes = USB_ENDPOINT_TYPE_ISOCHRONOUS;//<--USER INPUT
    pCurEP->wMaxPacketSize = 0x40;//<--USER INPUT
    pCurEP->bInterval = 0x0;//<--USER INPUT
    //will it work with another endpoint to be a loopback pair?
    pCurIf->pEPs[4].iPairAddr = 5; //<-- USER INPUT
    //---endpoint 6---
    pCurEP = &pCurIf->pEPs[5].usbEP; 
    pCurEP->bEndpointAddress = 0x06; //<--USER INPUT
    pCurEP->bmAttributes = USB_ENDPOINT_TYPE_ISOCHRONOUS;//<--USER INPUT
    pCurEP->wMaxPacketSize = 0x40;//<--USER INPUT
    pCurEP->bInterval = 0x0;//<--USER INPUT
    //will it work with another endpoint to be a loopback pair?
    pCurIf->pEPs[5].iPairAddr = 4; //<-- USER INPUT

   
   //Endpoints for  config 1,  FULL Speed
    

    pCurConfig = &(pDevSetting->pFullSpeedCFs[0]);
    //how many interfaces does this configuration have?
    pCurConfig->uNumofIFs = 1;//<--USER INPUT
    pCurConfig->pIFs = (PONE_IF) new ONE_IF[pCurConfig->uNumofIFs];
    if(pCurConfig->pIFs == NULL){
        NKDbgPrintfW(L"Out of memory!");
        goto EXIT;
    }
    memset(pCurConfig->pIFs, 0, sizeof(ONE_IF)*pCurConfig->uNumofIFs);

    pCurIf = &pCurConfig->pIFs[0];
    //how many endpoints there?
    pCurIf->uNumofEPs = 6;//<--USER INPUT
    pCurIf->pEPs = (PEPSETTINGS) new EPSETTINGS[pCurIf->uNumofEPs];
    if(pCurIf->pEPs == NULL){
        NKDbgPrintfW(L"Out of memory!");
        goto EXIT;
    }
    memset(pCurIf->pEPs, 0, sizeof(EPSETTINGS)*pCurIf->uNumofEPs);

    //---endpoint 1---
    pCurEP = &pCurIf->pEPs[0].usbEP; 
    pCurEP->bEndpointAddress = 0x81; //<--USER INPUT
    pCurEP->bmAttributes = USB_ENDPOINT_TYPE_BULK;//<--USER INPUT
    pCurEP->wMaxPacketSize = 0x40;//<--USER INPUT
    pCurEP->bInterval = 0x0;//<--USER INPUT
    //will it work with another endpoint to be a loopback pair?
    pCurIf->pEPs[0].iPairAddr = 1; //<-- USER INPUT
    //---endpoint 2---
    pCurEP = &pCurIf->pEPs[1].usbEP; 
    pCurEP->bEndpointAddress = 0x02; //<--USER INPUT
    pCurEP->bmAttributes = USB_ENDPOINT_TYPE_BULK;//<--USER INPUT
    pCurEP->wMaxPacketSize = 0x40;//<--USER INPUT
    pCurEP->bInterval = 0x0;//<--USER INPUT
    //will it work with another endpoint to be a loopback pair?
    pCurIf->pEPs[1].iPairAddr = 0; //<-- USER INPUT
    //---endpoint 3---
    pCurEP = &pCurIf->pEPs[2].usbEP; 
    pCurEP->bEndpointAddress = 0x83; //<--USER INPUT
    pCurEP->bmAttributes = USB_ENDPOINT_TYPE_INTERRUPT;//<--USER INPUT
    pCurEP->wMaxPacketSize = 0x40;//<--USER INPUT
    pCurEP->bInterval = 0x1;//<--USER INPUT
    //will it work with another endpoint to be a loopback pair?
    pCurIf->pEPs[2].iPairAddr = 3; //<-- USER INPUT
    //---endpoint 4---
    pCurEP = &pCurIf->pEPs[3].usbEP; 
    pCurEP->bEndpointAddress = 0x04; //<--USER INPUT
    pCurEP->bmAttributes = USB_ENDPOINT_TYPE_INTERRUPT;//<--USER INPUT
    pCurEP->wMaxPacketSize = 0x40;//<--USER INPUT
    pCurEP->bInterval = 0x1;//<--USER INPUT
    //will it work with another endpoint to be a loopback pair?
    pCurIf->pEPs[3].iPairAddr = 2; //<-- USER INPUT
    //---endpoint 5---
    pCurEP = &pCurIf->pEPs[4].usbEP; 
    pCurEP->bEndpointAddress = 0x85; //<--USER INPUT
    pCurEP->bmAttributes = USB_ENDPOINT_TYPE_ISOCHRONOUS;//<--USER INPUT
    pCurEP->wMaxPacketSize = 0x40;//<--USER INPUT
    pCurEP->bInterval = 0x0;//<--USER INPUT
    //will it work with another endpoint to be a loopback pair?
    pCurIf->pEPs[4].iPairAddr = 5; //<-- USER INPUT
    //---endpoint 6---
    pCurEP = &pCurIf->pEPs[5].usbEP; 
    pCurEP->bEndpointAddress = 0x06; //<--USER INPUT
    pCurEP->bmAttributes = USB_ENDPOINT_TYPE_ISOCHRONOUS;//<--USER INPUT
    pCurEP->wMaxPacketSize = 0x40;//<--USER INPUT
    pCurEP->bInterval = 0x0;//<--USER INPUT
    //will it work with another endpoint to be a loopback pair?
    pCurIf->pEPs[5].iPairAddr = 4; //<-- USER INPUT

    //************************************************************************************
    // --- set configuration 2 related values ---
    //************************************************************************************


    pCurConfig = &pDevSetting->pHighSpeedCFs[1];
    //how many interfaces does this configuration have?
    pCurConfig->uNumofIFs = 1;//<--USER INPUT
    pCurConfig->pIFs = (PONE_IF) new ONE_IF[pCurConfig->uNumofIFs];
    if(pCurConfig->pIFs == NULL){
        NKDbgPrintfW(L"Out of memory!");
        goto EXIT;
    }
    memset(pCurConfig->pIFs, 0, sizeof(ONE_IF)*pCurConfig->uNumofIFs);

    //Endpoints for  config 2  HIGH Speed
    
    pCurIf = &pCurConfig->pIFs[0];
    //how many endpoints there?
    pCurIf->uNumofEPs = 6;//<--USER INPUT
    pCurIf->pEPs = (PEPSETTINGS) new EPSETTINGS[pCurIf->uNumofEPs];
    if(pCurIf->pEPs == NULL){
        NKDbgPrintfW(L"Out of memory!");
        goto EXIT;
    }
    memset(pCurIf->pEPs, 0, sizeof(EPSETTINGS)*pCurIf->uNumofEPs);

    // --- set endpoints for highspeed 
    //---endpoint 1---
    pCurEP = &pCurIf->pEPs[0].usbEP; 
    pCurEP->bEndpointAddress = 0x81; //<--USER INPUT
    pCurEP->bmAttributes = USB_ENDPOINT_TYPE_INTERRUPT;//<--USER INPUT
    pCurEP->wMaxPacketSize = 0x200;//<--USER INPUT
    pCurEP->bInterval = 0x1;//<--USER INPUT
    //will it work with another endpoint to be a loopback pair?
    pCurIf->pEPs[0].iPairAddr = 1; //<-- USER INPUT
    //---endpoint 2---
    pCurEP = &pCurIf->pEPs[1].usbEP; 
    pCurEP->bEndpointAddress = 0x02; //<--USER INPUT
    pCurEP->bmAttributes = USB_ENDPOINT_TYPE_INTERRUPT;//<--USER INPUT
    pCurEP->wMaxPacketSize = 0x200;//<--USER INPUT
    pCurEP->bInterval = 0x1;//<--USER INPUT
    //will it work with another endpoint to be a loopback pair?
    pCurIf->pEPs[1].iPairAddr = 0; //<-- USER INPUT
    //---endpoint 3---
    pCurEP = &pCurIf->pEPs[2].usbEP; 
    pCurEP->bEndpointAddress = 0x83; //<--USER INPUT
    pCurEP->bmAttributes = USB_ENDPOINT_TYPE_ISOCHRONOUS;//<--USER INPUT
    pCurEP->wMaxPacketSize = 0x200;//<--USER INPUT
    pCurEP->bInterval = 0x0;//<--USER INPUT
    //will it work with another endpoint to be a loopback pair?
    pCurIf->pEPs[2].iPairAddr = 3; //<-- USER INPUT
    //---endpoint 4---
    pCurEP = &pCurIf->pEPs[3].usbEP; 
    pCurEP->bEndpointAddress = 0x04; //<--USER INPUT
    pCurEP->bmAttributes = USB_ENDPOINT_TYPE_ISOCHRONOUS;//<--USER INPUT
    pCurEP->wMaxPacketSize = 0x200;//<--USER INPUT
    pCurEP->bInterval = 0x0;//<--USER INPUT
    //will it work with another endpoint to be a loopback pair?
    pCurIf->pEPs[3].iPairAddr = 2; //<-- USER INPUT
    //---endpoint 5---
    pCurEP = &pCurIf->pEPs[4].usbEP; 
    pCurEP->bEndpointAddress = 0x85; //<--USER INPUT
    pCurEP->bmAttributes = USB_ENDPOINT_TYPE_BULK;//<--USER INPUT
    pCurEP->wMaxPacketSize = 0x40;//<--USER INPUT
    pCurEP->bInterval = 0x0;//<--USER INPUT
    //will it work with another endpoint to be a loopback pair?
    pCurIf->pEPs[4].iPairAddr = 5; //<-- USER INPUT
    //---endpoint 6---
    pCurEP = &pCurIf->pEPs[5].usbEP; 
    pCurEP->bEndpointAddress = 0x06; //<--USER INPUT
    pCurEP->bmAttributes = USB_ENDPOINT_TYPE_BULK;//<--USER INPUT
    pCurEP->wMaxPacketSize = 0x40;//<--USER INPUT
    pCurEP->bInterval = 0x0;//<--USER INPUT
    //will it work with another endpoint to be a loopback pair?
    pCurIf->pEPs[5].iPairAddr = 4; //<-- USER INPUT

    //--- full speed config --- 
    pCurConfig = &pDevSetting->pFullSpeedCFs[1];
    //how many interfaces does this configuration have?
    pCurConfig->uNumofIFs = 1;//<--USER INPUT
    pCurConfig->pIFs = (PONE_IF) new ONE_IF[pCurConfig->uNumofIFs];
    if(pCurConfig->pIFs == NULL){
        NKDbgPrintfW(L"Out of memory!");
        goto EXIT;
    }
    memset(pCurConfig->pIFs, 0, sizeof(ONE_IF)*pCurConfig->uNumofIFs);

    // --- set interface 0 related values ---
    pCurIf = &pCurConfig->pIFs[0];
    //how many endpoints there?
    pCurIf->uNumofEPs = 6;//<--USER INPUT
    pCurIf->pEPs = (PEPSETTINGS) new EPSETTINGS[pCurIf->uNumofEPs];
    if(pCurIf->pEPs == NULL){
        NKDbgPrintfW(L"Out of memory!");
        goto EXIT;
    }
    memset(pCurIf->pEPs, 0, sizeof(EPSETTINGS)*pCurIf->uNumofEPs);

    // Endpoints for Config 2 Full Speed
    //---endpoint 1---
    pCurEP = &pCurIf->pEPs[0].usbEP; 
    pCurEP->bEndpointAddress = 0x81; //<--USER INPUT
    pCurEP->bmAttributes = USB_ENDPOINT_TYPE_INTERRUPT;//<--USER INPUT
    pCurEP->wMaxPacketSize = 0x40;//<--USER INPUT
    pCurEP->bInterval = 0x1;//<--USER INPUT
    //will it work with another endpoint to be a loopback pair?
    pCurIf->pEPs[0].iPairAddr = 1; //<-- USER INPUT
    //---endpoint 2---
    pCurEP = &pCurIf->pEPs[1].usbEP; 
    pCurEP->bEndpointAddress = 0x02; //<--USER INPUT
    pCurEP->bmAttributes = USB_ENDPOINT_TYPE_INTERRUPT;//<--USER INPUT
    pCurEP->wMaxPacketSize = 0x40;//<--USER INPUT
    pCurEP->bInterval = 0x1;//<--USER INPUT
    //will it work with another endpoint to be a loopback pair?
    pCurIf->pEPs[1].iPairAddr = 0; //<-- USER INPUT
    //---endpoint 3---
    pCurEP = &pCurIf->pEPs[2].usbEP; 
    pCurEP->bEndpointAddress = 0x83; //<--USER INPUT
    pCurEP->bmAttributes = USB_ENDPOINT_TYPE_ISOCHRONOUS;//<--USER INPUT
    pCurEP->wMaxPacketSize = 0x40;//<--USER INPUT
    pCurEP->bInterval = 0x0;//<--USER INPUT
    //will it work with another endpoint to be a loopback pair?
    pCurIf->pEPs[2].iPairAddr = 3; //<-- USER INPUT
    //---endpoint 4---
    pCurEP = &pCurIf->pEPs[3].usbEP; 
    pCurEP->bEndpointAddress = 0x04; //<--USER INPUT
    pCurEP->bmAttributes = USB_ENDPOINT_TYPE_ISOCHRONOUS;//<--USER INPUT
    pCurEP->wMaxPacketSize = 0x40;//<--USER INPUT
    pCurEP->bInterval = 0x0;//<--USER INPUT
    //will it work with another endpoint to be a loopback pair?
    pCurIf->pEPs[3].iPairAddr = 2; //<-- USER INPUT
    //---endpoint 5---
    pCurEP = &pCurIf->pEPs[4].usbEP; 
    pCurEP->bEndpointAddress = 0x85; //<--USER INPUT
    pCurEP->bmAttributes = USB_ENDPOINT_TYPE_BULK;//<--USER INPUT
    pCurEP->wMaxPacketSize = 0x40;//<--USER INPUT
    pCurEP->bInterval = 0x0;//<--USER INPUT
    //will it work with another endpoint to be a loopback pair?
    pCurIf->pEPs[4].iPairAddr = 5; //<-- USER INPUT
    //---endpoint 6---
    pCurEP = &pCurIf->pEPs[5].usbEP; 
    pCurEP->bEndpointAddress = 0x06; //<--USER INPUT
    pCurEP->bmAttributes = USB_ENDPOINT_TYPE_BULK;//<--USER INPUT
    pCurEP->wMaxPacketSize = 0x40;//<--USER INPUT
    pCurEP->bInterval = 0x0;//<--USER INPUT
    //will it work with another endpoint to be a loopback pair?
    pCurIf->pEPs[5].iPairAddr = 4; //<-- USER INPUT

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

