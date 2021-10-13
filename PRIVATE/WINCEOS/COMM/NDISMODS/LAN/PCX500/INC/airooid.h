//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
// Copyright 2001, Cisco Systems, Inc.  All rights reserved.
// No part of this source, or the resulting binary files, may be reproduced,
// transmitted or redistributed in any form or by any means, electronic or
// mechanical, for any purpose, without the express written permission of Cisco.
//
//---------------------------------------------------------------------------
// airooid.h
//---------------------------------------------------------------------------
// Description:
//
// Revision History:
//
// Date         
//---------------------------------------------------------------------------
// 10/05/00     jbeaujon        -added OID_X500_SOFT_RESET
// 11/10/00     jbeaujon        -added OID_X500_GET_ACTIVE_PROFILE
//                                     OID_X500_SET_ACTIVE_PROFILE
//                                     OID_X500_SELECT_PROFILE
//                                     OID_X500_GET_PROFILE
//                                     OID_X500_SET_PROFILE
// 
//---------------------------------------------------------------------------

//  AiroOid.h
#ifndef __AiroOid_h__ 
#define __AiroOid_h__

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#ifdef  IOCTL_NDIS_QUERY_GLOBAL_STATS
#undef  IOCTL_NDIS_QUERY_GLOBAL_STATS
#endif

#define IOCTL_NDIS_QUERY_GLOBAL_STATS               0x00170002
#define IOCTL_NDIS_WRITE_GLOBAL_PARAMS              IOCTL_NDIS_QUERY_GLOBAL_STATS
//                                                  
#define AWCN_IOCTL_NDIS_QUERY_GLOBAL_PARAMS         IOCTL_NDIS_QUERY_GLOBAL_STATS
#define AWCN_IOCTL_NDIS_WRITE_GLOBAL_PARAMS         IOCTL_NDIS_QUERY_GLOBAL_STATS
#define LM3000_IOCTL_NDIS_QUERY_GLOBAL_PARAMS       IOCTL_NDIS_QUERY_GLOBAL_STATS
#define LM3000_IOCTL_NDIS_WRITE_GLOBAL_PARAMS       IOCTL_NDIS_QUERY_GLOBAL_STATS
#define LM3500_IOCTL_NDIS_QUERY_GLOBAL_PARAMS       IOCTL_NDIS_QUERY_GLOBAL_STATS
#define LM3500_IOCTL_NDIS_WRITE_GLOBAL_PARAMS       IOCTL_NDIS_QUERY_GLOBAL_STATS
#define X500_IOCTL_NDIS_QUERY_GLOBAL_PARAMS     IOCTL_NDIS_QUERY_GLOBAL_STATS
#define X500_IOCTL_NDIS_WRITE_GLOBAL_PARAMS     IOCTL_NDIS_QUERY_GLOBAL_STATS

#ifndef STFLASH_PROGRESS_DEFINED
#define STFLASH_PROGRESS_DEFINED

#define ERROR_SET_EVENT_HANDLE_FAILED           -1
#define ERROR_SET_EVENT_HANDLE_PENDING          0
#define ERROR_SET_EVENT_HANDLE_SUCCESS          1
            
typedef enum{
    FLASHING_PROGRESS_NOTFLASHING           = 0,
    FLASHING_PROGRESS_OPEN                  = 1,
    FLASHING_PROGRESS_ERASING               = 2,
    FLASHING_PROGRESS_WRITING               = 4,
    FLASHING_PROGRESS_REBOOTING             = 6,
    FLASHING_PROGRESS_FAILED                = 7,
    FLASHING_PROGRESS_SUCCESS               = 8,
}FLASH_PROGRESS;
    
typedef struct{
    FLASH_PROGRESS      progress;
    int                 percent_complete;                               
    int                 mseconds_remain;
}STFLASH_PROGRESS;
#endif

typedef enum _OID_COMMON_DEVICEIOCTL {
    OID_COMMON_BASE         =0xff000000,
    OID_ENUM_DEVICE,                        
    OID_GET_VXDVERSION,
//
    OID_RADIO_ON,           
    OID_RADIO_OFF,          
    OID_QUERY_RADIO_STATE,  
    OID_GETVERSION,                 
//
    OID_GET_REGISTRATION_STATUS,
    OID_GET_REGISTERED_ROUTER,
//
    OID_GET_CONFIGUATION,
    OID_SET_CONFIGUATION,
    OID_GET_STATUS,
    OID_GET_STATISTICS,
    OID_RESET_STATISTICS,

//  VXD OIDS
    OID_EXTEND_BASE         =0xff000100,
    OID_OPEN_CARD,
    OID_OPEN_FIRSTCARD,
    OID_OPEN_NEXTCARD,
    OID_OPEN_PREVIOUS_CARD,
    OID_GET_DRIVER,
}OID_COMMON_DEVICEIOCTL;

typedef enum _OID_LM3000_DEVICEIOCTL { 
//
    OID_LM3000_BASE         =0xff010000,
//
//  
    OID_LM3000_GET_ESSID,      
    OID_LM3000_GET_BSSID,     
    OID_LM3000_GET_REGAP,      
    OID_LM3000_GET_RSSI,       
    OID_LM3000_GET_MODE,       
    OID_LM3000_GET_STATUS,     
    //
    //  --------------------------- add code here ---------------------------
    //  end of additional code ------------------------------------------------
    //
    OID_LM3000_OID_BASE_PUBLUC  =OID_LM3000_BASE + 100,
    OID_LM3000_CLOSE_FILE,          
    OID_LM3000_GET_ESTATUS,         
    OID_LM3000_GET_CONFIG,          
    OID_LM3000_SET_CONFIG,          
    OID_LM3000_FLASH_IMAGEFILE,         
    OID_LM3000_FLASH_IMAGBUFF,          
    OID_LM3000_FLASH_QUEIMAGEBUF_STAT,
    OID_LM3000_FLASH_QUEIMAGEBUF_MID,
    OID_LM3000_FLASH_QUEIMAGEBUF_END,
    OID_LM3000_FLASH_RESETADAPTER,
    OID_LM3000_GET_ASYNCINFO,
    OID_LM3000_GET_STATS,
    OID_LM3000_RESET_STATS,
    OID_LM3000_GET_PHYSTAT_BUF,
            

    //
    //  --------------------------- add code here ---------------------------

    //  end of additional code ------------------------------------------------
    //
//  OID_LM3000_GETVXDVERSION = 0,       
    //
}OID_LM3000_DEVICEIOCTL;


typedef  enum _OID_AWCN_DEVICEIOCTL{ 
//
    OID_AWCN_BASE = 0xff010000,
//
    OID_AWCN_GET_SIGNAL_STRENGTH,
    OID_AWCN_GET_SIGNAL_QUALITY,
    OID_AWCN_GET_REGISTERED_ROUTER,
    OID_AWCN_GET_REGISTRATION_STATUS,
    OID_AWCN_GET_SYSTEM_ID,
    OID_AWCN_GOTO_SLOW_POLL,
    OID_AWCN_GET_CONFIGURATION,
    OID_AWCN_SET_CONFIGURATION,
    OID_AWCN_GET_STATISTICS,
    OID_AWCN_GET_STATUS,
    OID_AWCN_RESET_STATS,
    OID_AWCN_FLASH_OPEN,
    OID_AWCN_FLASH_ERASE,
    OID_AWCN_FLASH_WRITE,
    OID_AWCN_FLASH_CLOSE,
    OID_QUERY_RADIO_TYPE,
    OID_QUERY_ADAPTER_TYPE,
    OID_AWCN_WRITE_LOADER,
    OID_AWCN_GET_CARDDATA,
    OID_AWCN_SET_CARDDATA,
//
}OID_AWCN_DEVICEIOCTL;


typedef  enum _OID_LM3500_DEVICEIOCTL{ 
//
    OID_LM3500_BASE = 0xff010000,
//
    OID_LM3500_GET_SIGNAL_STRENGTH,
    OID_LM3500_GET_SIGNAL_QUALITY,
    OID_LM3500_GET_SIGNAL_PARAMS,
    OID_LM3500_GET_REGISTERED_ROUTER,
    OID_LM3500_GET_REGISTRATION_STATUS,
    OID_LM3500_GET_SYSTEM_ID,
    OID_LM3500_GOTO_SLOW_POLL,
    OID_LM3500_GET_CONFIGURATION,
    OID_LM3500_SET_CONFIGURATION,
    OID_LM3500_GET_APS,
    OID_LM3500_SET_APS,
    OID_LM3500_GET_SSIDS,
    OID_LM3500_SET_SSIDS,
    OID_LM3500_GET_STATISTICS,
    OID_LM3500_GET_STATUS,
    OID_LM3500_RESET_STATS,
    OID_LM3500_GET_CAPS,
    OID_LM3500_FLASH_ADAPTER,
    OID_LM3500_FLASH_OPEN,
    OID_LM3500_FLASH_PROGRESS,
    OID_LM3500_FLASH_WRITE,
    OID_LM3500_FLASH_CLOSE,
    OID_LM3500_QUERY_RADIO_TYPE,
    OID_LM3500_QUERY_ADAPTER_TYPE,
    OID_LM3500_HARD_RESET,
    OID_LM3500_ISINSERTED,
//
}OID_LM3500_DEVICEIOCTL;

typedef  enum _OID_X500_DEVICEIOCTL{ 
//
    OID_X500_BASE = 0xff010000,
//
    OID_X500_GET_SIGNAL_STRENGTH,
    OID_X500_GET_SIGNAL_QUALITY,
    OID_X500_GET_SIGNAL_PARAMS,
    OID_X500_GET_REGISTERED_ROUTER,
    OID_X500_GET_REGISTRATION_STATUS,
    OID_X500_GET_SYSTEM_ID,
    OID_X500_GOTO_SLOW_POLL,
    OID_X500_GET_CONFIGURATION,
    OID_X500_SET_CONFIGURATION,
    OID_X500_GET_APS,
    OID_X500_SET_APS,
    OID_X500_GET_SSIDS,
    OID_X500_SET_SSIDS,
    OID_X500_GET_STATISTICS,
    OID_X500_GET_STATUS,
    OID_X500_RESET_STATS,
    OID_X500_GET_CAPS,
    OID_X500_FLASH_ADAPTER,
    OID_X500_FLASH_OPEN,
    OID_X500_FLASH_PROGRESS,
    OID_X500_FLASH_WRITE,
    OID_X500_FLASH_CLOSE,
    OID_X500_QUERY_RADIO_TYPE,
    OID_X500_QUERY_ADAPTER_TYPE,
    OID_X500_HARD_RESET,
    OID_X500_ISINSERTED,
//
    OID_X500_GET_32STATISTICS,
    OID_X500_RESET_32STATISTICS,
    OID_X500_READ_RID,
    OID_X500_WRITE_RID,             // can't write config RID, use config function
    OID_X500_GET_BUSTYPE,
    OID_X500_AWAKEN,
    OID_X500_SLEEP,
    OID_X500_ISAWAKE,
    OID_X500_KEEPAWAKE,     
    OID_X500_ISMAXPOWERSAVEON,
    OID_X500_READBUF,
    OID_X500_WRITEBUF,
    OID_X500_READAUXBUF,
    OID_X500_WRITEAUXBUF,
//
    OID_X500_EE_GET_CONFIGURATION,
    OID_X500_EE_SET_CONFIGURATION,
    OID_X500_EE_GET_APS,
    OID_X500_EE_SET_APS,
    OID_X500_EE_GET_SSIDS,
    OID_X500_EE_SET_SSIDS,
//
    OID_X500_GET_COREDUMP,
    OID_X500_WRITE_RID_IGNORE_MAC,
    OID_X500_SET_EVENT_HANDLE,

    OID_X500_SOFT_RESET,

    OID_X500_GET_ACTIVE_PROFILE,
    OID_X500_SET_ACTIVE_PROFILE,
    OID_X500_SELECT_PROFILE,
    OID_X500_GET_PROFILE,
    OID_X500_SET_PROFILE,
    OID_X500_ENABLE_AUTO_PROFILE_SWITCH,
    OID_X500_DISABLE_AUTO_PROFILE_SWITCH,

    OID_X500_SET_PROFILES_V2,
    OID_X500_GET_ACTIVE_PROFILE_V2,
    OID_X500_SET_ACTIVE_PROFILE_V2,
    OID_X500_GET_CAPABILITY,

}OID_X500_DEVICEIOCTL;

#ifdef __cplusplus
}
#endif // __cplusplus

#endif// __AiroOid_h__ 
