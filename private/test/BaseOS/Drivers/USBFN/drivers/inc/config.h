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
/*++

THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Module Name: 

        PIPEPARE.H

Abstract:

       USB Loopback driver -- class definitions for tranfer management
        
--*/

#pragma once

#include <windows.h>
#include <usbfntypes.h>
#include "ufnloopback.h"

// **************************************************************************
//                             Definitions & Variables
// **************************************************************************

//settingsf or each end point
typedef struct _EPSETTINGS{
    USB_ENDPOINT_DESCRIPTOR     usbEP;      //endpoint structure
    INT     iPairAddr;                                   //whether there will be a paring endpoing
}EPSETTINGS, *PEPSETTINGS;

//structure for one interface setting
typedef struct _ONE_IF{
    UCHAR              uNumofEPs;      //number of endpoints
    PEPSETTINGS     pEPs;               //settings of those endpoints
}ONE_IF, *PONE_IF;

//structure for one configuration 
typedef struct _ONE_CONFIG{
    UCHAR              uNumofIFs;      //number of interfaces
    PONE_IF            pIFs;               //settings of these interfaces
}ONE_CONFIG, *PONE_CONFIG;

//structure for the device
typedef struct _DEVICE_SETTING{
    UCHAR               uNumofCFs;              //number of configurations
    PONE_CONFIG     pHighSpeedCFs;        //high speed configuration
    PONE_CONFIG     pFullSpeedCFs;         //full speed configuration
    USHORT             uEP0PacketSize;       //max packet size of endpoint 0
    USHORT             ubcdUSBVer;            //usb version
    USHORT             uVendorId;              //vendorID
    USHORT             uProductId;             //productID 
       
}DEVICE_SETTING, *PDEVICE_SETTING;


#define MAX_NUM_ENDPOINTS   64

//class to manage a pipe pair, control all activities that occur on these two pipes
class CDeviceConfig{

public:
    //public functions 
    CDeviceConfig(){
        m_pDevSetting = NULL;
        memset(&m_HighSpeedDeviceDesc, 0, sizeof(USB_DEVICE_DESCRIPTOR));
        memset(&m_FullSpeedDeviceDesc, 0, sizeof(USB_DEVICE_DESCRIPTOR));
        m_uCurConfig = 0;
        m_uCurIf = 0;
        m_uNumofEPs = 0;
        m_BusSpeed = BS_HIGH_SPEED;
        for(int i = 0; i < MAX_NUM_ENDPOINTS; i++)
            m_uEPPairs[i] = 0xFF;
    };

    ~CDeviceConfig(){Cleanup();};
    BOOL InitDescriptors();
    VOID  SetDevSetting(PDEVICE_SETTING pSetting){m_pDevSetting = pSetting;};
    UCHAR                   GetNumConfigurations(){ return m_pDevSetting->uNumofCFs;};
    UCHAR                   GetCurCfgIndex(){ return m_uCurConfig;};
    UCHAR                   SetCurCfgIndex(UCHAR uNewCfg){ return m_uCurConfig = uNewCfg; };
    PUSB_DEVICE_DESCRIPTOR  GetCurHSDeviceDesc(){ return &m_HighSpeedDeviceDesc;};
    PUSB_DEVICE_DESCRIPTOR  GetCurFSDeviceDesc(){ return &m_FullSpeedDeviceDesc;};
    PUFN_CONFIGURATION        GetCurHSConfig(){return m_pHighSpeedConfig+m_uCurConfig; };
    PUFN_CONFIGURATION        GetCurFSConfig(){return m_pFullSpeedConfig+m_uCurConfig; };
    PUCHAR                            GetEPPairs(){return m_uEPPairs;};
    UCHAR                              GetNumofEPs(){return m_uNumofEPs; };
    UCHAR                              GetNumofPairs(){return m_uNumofPairs;};
    VOID                                ModifyConfig(USB_TDEVICE_RECONFIG utrc);
    VOID                                SetBusSpeed(UFN_BUS_SPEED Speed){m_BusSpeed = Speed;};
    VOID Cleanup();
        
private:
    BOOL ParingCheck();
    
    PDEVICE_SETTING                 m_pDevSetting;
    USB_DEVICE_DESCRIPTOR   m_HighSpeedDeviceDesc;
    USB_DEVICE_DESCRIPTOR   m_FullSpeedDeviceDesc;
    PUFN_CONFIGURATION         m_pHighSpeedConfig;
    PUFN_CONFIGURATION         m_pFullSpeedConfig;
    UCHAR                             m_uCurConfig;
    UCHAR                             m_uCurIf;
    UCHAR                             m_uNumofEPs;
    UCHAR                             m_uEPPairs[MAX_NUM_ENDPOINTS];
    UCHAR                             m_uNumofPairs;
    UFN_BUS_SPEED                m_BusSpeed;
};

PDEVICE_SETTING InitializeSettings();

//some debug output defines
#undef STR_MODULE
#undef SETFNAME
#undef SETFNAME_DBG
#define STR_MODULE _T("UFNLOOPBACK!")
#define SETFNAME(name) LPCTSTR pszFname = STR_MODULE name _T(":")
#ifdef DEBUG
    #define SETFNAME_DBG(name) LPCTSTR pszFname = STR_MODULE name _T(":")
#else
    #define SETFNAME_DBG(name) ((void)0) // do nothing
#endif
