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
//     config.cpp
// 
// Abstract:  This file includes functions that related the function driver initialization
//                reset, close, and other events' handling.
//            
//     
// Notes: 
//


#include <windows.h>
#include <usbfn.h>
#include <usbfntypes.h>
#include "config.h"


BOOL 
CDeviceConfig::InitDescriptors(){

    SETFNAME(_T("DeviceConfig::InitDescriptors()"));
    FUNCTION_ENTER_MSG();

    if(m_pDevSetting->uNumofCFs == 0){
        ERRORMSG(1, (_T("%s Device has no configurations!\r\n"), pszFname));
        goto FXNEXIT;
    }

    BOOL bRet = FALSE;
    UCHAR uIndex;
    UCHAR uConfig;
    UCHAR uInterface = 1;
    PUFN_ENDPOINT   pFSEndPoints = NULL;
    PUFN_ENDPOINT   pHSEndPoints = NULL;
    PUFN_INTERFACE  pUFNFSIf = NULL;
    PUFN_INTERFACE  pUFNHSIf = NULL;
   
    //************************************************************
    // Device Descriptor Settings 
    //************************************************************
    
    //---set highspeed device descriptor---
    m_HighSpeedDeviceDesc.bLength = sizeof(USB_DEVICE_DESCRIPTOR);
    m_HighSpeedDeviceDesc.bDescriptorType = USB_DEVICE_DESCRIPTOR_TYPE;
    m_HighSpeedDeviceDesc.bcdUSB = m_pDevSetting->ubcdUSBVer;
    m_HighSpeedDeviceDesc.bMaxPacketSize0 = (UCHAR)m_pDevSetting->uEP0PacketSize;
    m_HighSpeedDeviceDesc.idVendor = m_pDevSetting->uVendorId;
    m_HighSpeedDeviceDesc.idProduct = m_pDevSetting->uProductId;
    m_HighSpeedDeviceDesc.bNumConfigurations =  m_pDevSetting->uNumofCFs;
    //others are all zero

    //---set fullspeed device descriptor---
    //currently we assume there's no difference between them
    memcpy(&m_FullSpeedDeviceDesc, &m_HighSpeedDeviceDesc, sizeof(USB_DEVICE_DESCRIPTOR));

    //Allocate Config Descriptors 

    m_pFullSpeedConfig = (PUFN_CONFIGURATION) new UFN_CONFIGURATION[m_pDevSetting->uNumofCFs];
    m_pHighSpeedConfig = (PUFN_CONFIGURATION) new UFN_CONFIGURATION[m_pDevSetting->uNumofCFs];

    memset(m_pFullSpeedConfig,0,sizeof(UFN_CONFIGURATION)*m_pDevSetting->uNumofCFs);
    memset(m_pHighSpeedConfig,0,sizeof(UFN_CONFIGURATION)*m_pDevSetting->uNumofCFs);

    //For All Configs
    for( uConfig=0; uConfig<m_pDevSetting->uNumofCFs; uConfig++ ) {

        //************************************************************
        // Configuration settings for high speed
        //************************************************************

        //---create high speed endpints---
        PONE_CONFIG pCurHSCf = &m_pDevSetting->pHighSpeedCFs[uConfig];
        if((pCurHSCf == NULL) || (pCurHSCf->uNumofIFs == 0) || pCurHSCf->pIFs == NULL){
            ERRORMSG(1, (_T("%s Invalid parameters!\r\n"), pszFname));
            return FALSE;
        }

        //if( uInterface < 1 || uInterface > pCurHSCf->uNumofIFs)
        //    uInterface = 1;

        PONE_IF pCurHSIf = &(pCurHSCf->pIFs[uInterface-1]);
        if(pCurHSIf == NULL || pCurHSIf->pEPs == NULL || pCurHSIf->uNumofEPs == 0){
            ERRORMSG(1, (_T("%s Invalid parameters!\r\n"), pszFname));
            return FALSE;
        }

        //---create and fill up high speed endpoints array---
        pHSEndPoints = (PUFN_ENDPOINT) new UFN_ENDPOINT[pCurHSIf->uNumofEPs];
        if(pHSEndPoints == NULL){
            ERRORMSG(1, (_T("%s Out of memory!\r\n"), pszFname));
            return FALSE;
        }
        memset(pHSEndPoints, 0, sizeof(UFN_ENDPOINT)*pCurHSIf->uNumofEPs);
        for(uIndex = 0; uIndex < pCurHSIf->uNumofEPs; uIndex++){
            PUFN_ENDPOINT pUFNEP = &pHSEndPoints[uIndex];
            memcpy(&pUFNEP->Descriptor, &(pCurHSIf->pEPs[uIndex].usbEP), sizeof(USB_ENDPOINT_DESCRIPTOR));
            pUFNEP->Descriptor.bLength = sizeof(USB_ENDPOINT_DESCRIPTOR);
            pUFNEP->Descriptor.bDescriptorType = USB_ENDPOINT_DESCRIPTOR_TYPE;
            pUFNEP->dwCount = sizeof(UFN_ENDPOINT);
            m_uEPPairs[uIndex] = (UCHAR)pCurHSIf->pEPs[uIndex].iPairAddr;
            if(m_uEPPairs[uIndex] != 0xFF && 
              pCurHSIf->pEPs[uIndex].usbEP.bDescriptorType != pCurHSIf->pEPs[m_uEPPairs[uIndex]].usbEP.bDescriptorType){
                ERRORMSG(1, (_T("%s Endpoint Pair(%d, %d) 's type don't match!!!\r\n"), pszFname, uIndex, m_uEPPairs[uIndex]));
                return FALSE;
            }
            //NKDbgPrintfW(_T("\t\t HSEP[%u.%u.%u]  %02x:%02x:%04x:%02x ~%u"),
            //    uConfig,uInterface,uIndex,
            //    pUFNEP->Descriptor.bmAttributes,
            //    pUFNEP->Descriptor.bEndpointAddress,
            //    pUFNEP->Descriptor.wMaxPacketSize,
            //    pUFNEP->Descriptor.bInterval,
            //    pCurHSIf->pEPs[uIndex].iPairAddr);

        }

        m_uNumofEPs = pCurHSIf->uNumofEPs;
        if(ParingCheck() == FALSE){
            ERRORMSG(1, (_T("%s Failed on paring check!\r\n"), pszFname));
            goto CLEANUP;
        }

        //--- create high speed interface---
        pUFNHSIf = (PUFN_INTERFACE) new UFN_INTERFACE[pCurHSCf->uNumofIFs];
        if(pUFNHSIf == NULL){
            ERRORMSG(1, (_T("%s Out of memory!\r\n"), pszFname));
            goto CLEANUP;
        } 
        memset(pUFNHSIf, 0, sizeof(UFN_INTERFACE)*pCurHSCf->uNumofIFs);
        pUFNHSIf->Descriptor.bLength = sizeof(USB_INTERFACE_DESCRIPTOR);
        pUFNHSIf->Descriptor.bDescriptorType = USB_INTERFACE_DESCRIPTOR_TYPE;
        pUFNHSIf->Descriptor.bNumEndpoints = pCurHSIf->uNumofEPs;
        pUFNHSIf->Descriptor.bInterfaceClass        = 0x0F;
        pUFNHSIf->Descriptor.bInterfaceSubClass  =  0xff;
        pUFNHSIf->Descriptor.bInterfaceProtocol    =    0xff;
        pUFNHSIf->dwCount = sizeof(UFN_INTERFACE);
        pUFNHSIf->pEndpoints = pHSEndPoints;


        //--- filling the config info---

        m_pHighSpeedConfig[uConfig].dwCount = sizeof(UFN_CONFIGURATION);
        m_pHighSpeedConfig[uConfig].Descriptor.bLength = sizeof(USB_CONFIGURATION_DESCRIPTOR);
        m_pHighSpeedConfig[uConfig].Descriptor.bDescriptorType = USB_CONFIGURATION_DESCRIPTOR_TYPE;
        m_pHighSpeedConfig[uConfig].Descriptor.wTotalLength = 18 + 7 * m_uNumofEPs;
        m_pHighSpeedConfig[uConfig].Descriptor.bNumInterfaces = pCurHSCf->uNumofIFs;
        m_pHighSpeedConfig[uConfig].Descriptor.bConfigurationValue = uConfig+1;
        m_pHighSpeedConfig[uConfig].Descriptor.bmAttributes = USB_CONFIG_SELF_POWERED;
        m_pHighSpeedConfig[uConfig].pInterfaces = pUFNHSIf;


        //************************************************************
        // Config settings for fullspeed
        //************************************************************

        //---create full speed endpoints---
        PONE_CONFIG pCurFSCf = &m_pDevSetting->pFullSpeedCFs[uConfig];
        if((pCurFSCf == NULL) || (pCurFSCf->uNumofIFs == 0) || pCurFSCf->pIFs == NULL){
            ERRORMSG(1, (_T("%s Invalid parameters!\r\n"), pszFname));
            goto CLEANUP;
        }

        //if( uInterface < 1 || uInterface > pCurFSCf->uNumofIFs)
        //    uInterface = 1;

        PONE_IF pCurFSIf = &(pCurFSCf->pIFs[uInterface-1]);
        if(pCurFSIf == NULL || pCurFSIf->pEPs == NULL || pCurFSIf->uNumofEPs == 0){
            ERRORMSG(1, (_T("%s Invalid parameters!\r\n"), pszFname));
            goto CLEANUP;
        }

        //---create and fill up high speed endpoints array---
        pFSEndPoints = (PUFN_ENDPOINT) new UFN_ENDPOINT[pCurHSIf->uNumofEPs];
        if(pFSEndPoints == NULL){
            ERRORMSG(1, (_T("%s Out of memory! \r\n"), pszFname));
            goto CLEANUP;
        }
        memset(pFSEndPoints, 0, sizeof(UFN_ENDPOINT)*pCurFSIf->uNumofEPs);
        for(uIndex = 0; uIndex < pCurFSIf->uNumofEPs; uIndex++){
            PUFN_ENDPOINT pUFNEP = &pFSEndPoints[uIndex];
            memcpy(&pUFNEP->Descriptor, &(pCurFSIf->pEPs[uIndex].usbEP), sizeof(USB_ENDPOINT_DESCRIPTOR));
            pUFNEP->Descriptor.bLength = sizeof(USB_ENDPOINT_DESCRIPTOR);
            pUFNEP->Descriptor.bDescriptorType = USB_ENDPOINT_DESCRIPTOR_TYPE;
            pUFNEP->dwCount = sizeof(UFN_ENDPOINT);

            //NKDbgPrintfW(_T("\t\t fsEP[%u.%u.%u]  %02x:%02x:%04x:%02x"),
            //    uConfig,uInterface,uIndex,
            //    pUFNEP->Descriptor.bmAttributes,
            //    pUFNEP->Descriptor.bEndpointAddress,
            //    pUFNEP->Descriptor.wMaxPacketSize,
            //    pUFNEP->Descriptor.bInterval);
        }

        //--- create full speed interface---
        pUFNFSIf = (PUFN_INTERFACE) new UFN_INTERFACE[pCurFSCf->uNumofIFs];
        if(pUFNFSIf == NULL){
            ERRORMSG(1, (_T("%s Out of memory! \r\n"), pszFname));
            goto CLEANUP;
        }
        memset(pUFNFSIf, 0, sizeof(UFN_INTERFACE)*pCurFSCf->uNumofIFs);
        pUFNFSIf->Descriptor.bLength = sizeof(USB_INTERFACE_DESCRIPTOR);
        pUFNFSIf->Descriptor.bDescriptorType = USB_INTERFACE_DESCRIPTOR_TYPE;
        pUFNFSIf->Descriptor.bNumEndpoints = pCurFSIf->uNumofEPs;
        pUFNFSIf->Descriptor.bInterfaceClass    = 0x0F;
        pUFNFSIf->Descriptor.bInterfaceSubClass = 0xff;
        pUFNFSIf->Descriptor.bInterfaceProtocol = 0xff;
        pUFNFSIf->dwCount = sizeof(UFN_INTERFACE);
        pUFNFSIf->pEndpoints = pFSEndPoints;

        //--- filling the config info---
        m_pFullSpeedConfig[uConfig].dwCount = sizeof(UFN_CONFIGURATION);
        m_pFullSpeedConfig[uConfig].Descriptor.bLength = sizeof(USB_CONFIGURATION_DESCRIPTOR);
        m_pFullSpeedConfig[uConfig].Descriptor.bDescriptorType = USB_CONFIGURATION_DESCRIPTOR_TYPE;
        m_pFullSpeedConfig[uConfig].Descriptor.wTotalLength = 18 + 7 * m_uNumofEPs;
        m_pFullSpeedConfig[uConfig].Descriptor.bNumInterfaces = 1;
        m_pFullSpeedConfig[uConfig].Descriptor.bConfigurationValue = uConfig+1;
        m_pFullSpeedConfig[uConfig].Descriptor.bmAttributes = USB_CONFIG_SELF_POWERED;
        m_pFullSpeedConfig[uConfig].pInterfaces = pUFNFSIf;

    } //For All Configs

    // Initially, both configuration and interface indices are set to ZERO
    m_uCurConfig = 0;
    m_uCurIf = 0;

    bRet = TRUE;
    
CLEANUP:

    if(bRet == FALSE) {
        
        //free any data structures allocated in this function
        for(uConfig = 0 ; uConfig < m_pDevSetting->uNumofCFs ;uConfig++) {
            if(m_pFullSpeedConfig[uConfig].pInterfaces){
                PUFN_INTERFACE pDelIF = m_pFullSpeedConfig[uConfig].pInterfaces;
                if(pDelIF->pEndpoints != NULL){
                    delete[] pDelIF->pEndpoints;
                }
                else if(pFSEndPoints){
                    delete[] pFSEndPoints;
                }
                delete[] pDelIF;
            }
            else if (pUFNFSIf){
                delete[] pUFNFSIf;
            }

            if(m_pHighSpeedConfig[uConfig].pInterfaces){
                PUFN_INTERFACE pDelIF = m_pHighSpeedConfig[uConfig].pInterfaces;
                if(pDelIF->pEndpoints != NULL){
                    delete[] pDelIF->pEndpoints;
                }
                else if(pHSEndPoints){
                    delete[] pHSEndPoints;
                }
                delete[] pDelIF;
            }
            else if(pUFNHSIf){
                delete[] pUFNHSIf;
            }
        }

        if(m_pFullSpeedConfig) {
            delete[] m_pFullSpeedConfig;
            m_pFullSpeedConfig = NULL;
        }
        if(m_pHighSpeedConfig) {
            delete[] m_pHighSpeedConfig;
            m_pHighSpeedConfig = NULL;
        }
    }

FXNEXIT:
    FUNCTION_LEAVE_MSG();
    return bRet;
}

BOOL
CDeviceConfig::ParingCheck(){
    UCHAR uChecked[MAX_NUM_ENDPOINTS] = {0};
    BOOL bError = FALSE;

    SETFNAME_DBG(_T("CDeviceConfig::ParingCheck()"));
    FUNCTION_ENTER_MSG();

    m_uNumofPairs = 0;
    for(int i = 0; i < m_uNumofEPs; i ++){
        if(uChecked[i] != 0)
            continue;
            
        UCHAR uTemp = m_uEPPairs[i];
        if(uTemp == 0xFF || uTemp > 63){//single endpoint
            uChecked[i] = 1;
            continue;
        }
        else{//paired endpoint, it's couterpart should not have not been touched yet
            if(uChecked[uTemp] != 0){
                bError = TRUE;
                break;
            }
            else{
                uChecked[uTemp] = 1;
                uChecked[i] = 1;
                m_uNumofPairs ++;
            }
        }
    }

    //error found in paring. dump paring info
    if(bError == TRUE){
        NKDbgPrintfW(_T("Error found in paring endpoints, please check:"));
        for(int j = 0;  j < m_uNumofEPs; j ++){
            NKDbgPrintfW(_T("\tEP %d:\t Paired EP: %d"), j, m_uEPPairs[j]);
        }
    }

    FUNCTION_LEAVE_MSG();
    return !bError;
}

VOID
CDeviceConfig::Cleanup(){

    SETFNAME_DBG(_T("CDeviceConfig::Cleanup()"));
    FUNCTION_ENTER_MSG();

    //clear UFN_configs
  for(UCHAR uConfig = 0 ; uConfig < m_pDevSetting->uNumofCFs ;uConfig++)
  {
    PUFN_INTERFACE pUFNIFs = NULL;

    if(m_pHighSpeedConfig != NULL){
        if(m_pHighSpeedConfig[uConfig].pInterfaces != NULL){
            pUFNIFs = m_pHighSpeedConfig[uConfig].pInterfaces;
            for(int i = 0; i < m_pHighSpeedConfig[uConfig].Descriptor.bNumInterfaces; i++){
                if(pUFNIFs[i].pEndpoints)
                    delete[] pUFNIFs[i].pEndpoints;
            }
            delete[] pUFNIFs;
        }
    }
    if(m_pFullSpeedConfig != NULL){
        if(m_pFullSpeedConfig[uConfig].pInterfaces != NULL){
            pUFNIFs =  m_pFullSpeedConfig[uConfig].pInterfaces;
            for(int i = 0; i < m_pFullSpeedConfig[uConfig].Descriptor.bNumInterfaces; i++){
                if(pUFNIFs[i].pEndpoints)
                    delete[] pUFNIFs[i].pEndpoints;
            }
            delete[] pUFNIFs;
        }
    }
  }

    if(m_pFullSpeedConfig) {
      delete[] m_pFullSpeedConfig;
      m_pFullSpeedConfig = NULL;
    }

    if(m_pHighSpeedConfig) {
      delete[] m_pHighSpeedConfig;
      m_pHighSpeedConfig = NULL;
    }

    if(m_pDevSetting != NULL) {

       if(m_pDevSetting->pHighSpeedCFs != NULL){
            PONE_CONFIG pCurCFs = m_pDevSetting->pHighSpeedCFs;
            for(int i = 0; i < m_pDevSetting->uNumofCFs; i++){
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
        if(m_pDevSetting->pFullSpeedCFs != NULL){
            PONE_CONFIG pCurCFs = m_pDevSetting->pFullSpeedCFs;
            for(int i = 0; i < m_pDevSetting->uNumofCFs; i++){
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

        delete m_pDevSetting;
        m_pDevSetting = NULL;
    }
  
    //reset all member variables, ready for change config or unplug/replug event
    memset(&m_HighSpeedDeviceDesc, 0, sizeof(USB_DEVICE_DESCRIPTOR));
    memset(&m_FullSpeedDeviceDesc, 0, sizeof(USB_DEVICE_DESCRIPTOR));

    m_uCurConfig = 0;
    m_uCurIf = 0;
    m_uNumofEPs = 0;
    for(int i = 0; i < MAX_NUM_ENDPOINTS; i++)
        m_uEPPairs[i] = 0xFF;

   FUNCTION_LEAVE_MSG();
}

VOID
CDeviceConfig::ModifyConfig(USB_TDEVICE_RECONFIG utrc)
{

    SETFNAME_DBG(_T("CDeviceConfig::ModifyConfig()"));
    FUNCTION_ENTER_MSG();

   //Modify the Packet Sizes for all Configs
    for(UCHAR uConfig = 0 ; uConfig < m_pDevSetting->uNumofCFs ;uConfig++)
  {
    
    PUFN_ENDPOINT pEndPionts = NULL;
    if(utrc.ucSpeed == SPEED_FULL_SPEED){
        pEndPionts = m_pFullSpeedConfig[uConfig].pInterfaces->pEndpoints;
    }
    else{
        pEndPionts = m_pHighSpeedConfig[uConfig].pInterfaces->pEndpoints;
    }

    for(UCHAR uIndex = 0; uIndex < m_uNumofEPs; uIndex++){
        if(pEndPionts[uIndex].Descriptor.bmAttributes == USB_ENDPOINT_TYPE_BULK){
            if(utrc.wBulkPkSize != 0){
                pEndPionts[uIndex].Descriptor.wMaxPacketSize = utrc.wBulkPkSize;
            }
            continue;
        }
        if(pEndPionts[uIndex].Descriptor.bmAttributes == USB_ENDPOINT_TYPE_CONTROL){
            if(utrc.wControlPkSize != 0){
                pEndPionts[uIndex].Descriptor.wMaxPacketSize = utrc.wControlPkSize;
            }
            continue;
        }
        if(pEndPionts[uIndex].Descriptor.bmAttributes == USB_ENDPOINT_TYPE_INTERRUPT){
            if(utrc.wIntrPkSize != 0){
                pEndPionts[uIndex].Descriptor.wMaxPacketSize = utrc.wIntrPkSize;
            }
            continue;
        }
        if(pEndPionts[uIndex].Descriptor.bmAttributes == USB_ENDPOINT_TYPE_ISOCHRONOUS){
            if(utrc.wIsochPkSize != 0){
                pEndPionts[uIndex].Descriptor.wMaxPacketSize = utrc.wIsochPkSize;
            }
            continue;
        }

    }
  }

    FUNCTION_LEAVE_MSG();
}
