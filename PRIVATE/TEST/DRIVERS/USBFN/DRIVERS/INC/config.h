//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

#ifndef __CONFIG_H__
#define __CONFIG_H__

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
    UCHAR               uNumofCFPresents; //number of configurations that will be exposed. normally it will be 1.
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
        memset(&m_HighSpeedConfig, 0, sizeof(UFN_CONFIGURATION));
        memset(&m_FullSpeedConfig, 0, sizeof(UFN_CONFIGURATION));
        m_uCurConfig = 0;
        m_uCurIf = 0;
        m_uNumofEPs = 0;
        m_BusSpeed = BS_HIGH_SPEED;
        for(int i = 0; i < MAX_NUM_ENDPOINTS; i++)
            m_uEPPairs[i] = 0xFF;
    };

    ~CDeviceConfig(){Cleanup();};
    BOOL InitDescriptors(UCHAR uConfig, UCHAR uInterface);
    VOID  SetDevSetting(PDEVICE_SETTING pSetting){m_pDevSetting = pSetting;};
    PUSB_DEVICE_DESCRIPTOR  GetCurHSDeviceDesc(){ return &m_HighSpeedDeviceDesc;};
    PUSB_DEVICE_DESCRIPTOR  GetCurFSDeviceDesc(){ return &m_FullSpeedDeviceDesc;};
    PUFN_CONFIGURATION        GetCurHSConfig(){return &m_HighSpeedConfig; };
    PUFN_CONFIGURATION        GetCurFSConfig(){return &m_FullSpeedConfig; };
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
    UFN_CONFIGURATION         m_HighSpeedConfig;
    UFN_CONFIGURATION         m_FullSpeedConfig;
    UCHAR                             m_uCurConfig;
    UCHAR                             m_uCurIf;
    UCHAR                             m_uNumofEPs;
    UCHAR                             m_uEPPairs[MAX_NUM_ENDPOINTS];
    UCHAR                             m_uNumofPairs;
    UFN_BUS_SPEED                m_BusSpeed;
};

PDEVICE_SETTING InitializeSettings();

//some debug output defines
#ifndef SHIP_BUILD
#undef STR_MODULE
#undef SETFNAME
#define STR_MODULE _T("UFNLOOPBACK!")
#define SETFNAME(name) LPCTSTR pszFname = STR_MODULE name _T(":")
#else
#define SETFNAME(name)
#endif


#endif


