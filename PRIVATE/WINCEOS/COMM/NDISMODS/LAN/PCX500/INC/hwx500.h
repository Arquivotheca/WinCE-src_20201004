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
//  HWX500.h
#ifndef __HWX500_H__
#define __HWX500_H__
// 
//
#ifndef u8_u16_u32
#define u8_u16_u32
typedef unsigned char   u8;
typedef unsigned short  u16;
typedef unsigned long   u32;
typedef u8              MacAddr[6];
#endif
//
//.......................
//
#pragma pack( push, struct_pack1 )
#pragma pack( 1 )
typedef struct _CFG_X500{
    u16     size;               // 0x00 struct length 
    u16     OpMode;             // 0x02 operation mode 
    u16     RxMode;             // 0x04 receive mode 
    u16     FragThreshhold;     // 0x06
    u16     RTSThreshhold;      // 0x08
    MacAddr     MacId;          // 0x0a station mac id
    u8      SupRates[8];        // 0x10 
    u16     ShortRetryLimit;    // 0x18 low bits of retry limits
    u16     LongRetryLimit;     // 0x1A high bits of retry limits
    u16     TxMSDULifetime;     // 0x1c     
    u16     RxMSDULifetime;     // 0x1e     
    u16     stationary;         // 0x20 !roaming    
    u16     ordering;           // 0x22
    u16     DeviceType;         // 0x24 set to zero for clients
    u8      RESERVED1[10];      // 0x26 ---------------------
    //----------------------------------------------------
    u16     ScanMode;           // 0x30
    u16     ProbeDelay;         // 0x32         
    u16     ProbeEnergyTO;      // 0x34
    u16     ProbeRespTO;        // 0x36
    u16     BeaconListenTO;     // 0x38
    u16     JoinNetTO;          // 0x3a
    u16     AuthenTO;           // 0x3c
    u16     AuthenType;         // 0x3e
    u16     AssociationTO;      // 0x40
    u16     SpecAPTO;           // 0x42
    u16     OfflineScanInterv;  // 0x44
    u16     OfflineScanDuar;    // 0x46
    u16     LinkLossDelay;      // 0x48,        0x4a
    u16     MaxBeaconLT;        // 0x4a,        0x4c
    u16     ResfreshInterv;     // 0x4c,        0x4e
    u16     WorldMode;          // 0x4e,        new             
    //----------------------------------------------------
    u16     PowerSaveMode;      // 0x50
    u16     RcvDTIM;            // 0x52
    u16     ListenInterv;       // 0x54
    u16     FastListenInterv;   // 0x56 
    u16     ListenDecay;        // 0x58
    u16     FastListenDecay;    // 0x5a
    u8      RESERVED3[4];       // 0x5c
    //----------------------------------------------------
    u16     BeaconPeriod;       // 0x60
    u16     AtimDuration;       // 0x62
    u16     HopPeriod;          // 0x64
    union   {
        u16     HopSet;         // 0x66
        u16     DSChannel;      // 0x66 the selected DS channel
    };  
    u16     HopPattern;         // 0x68
    u16     DTIMPeriod;         // 0x6a
    u16     Distance;           // 0x6c 
    u16     RadioID;            // 0x6e set to zero for clients
    //----------------------------------------------------
    u16     RadioType;          // 0x70
    u16     Diversity;          // 0x72
    u16     TransmitPower;      // 0x74
    u16     RSSIThreshhold;     // 0x76
    u16     Modulation;         // 0x78 1=CCK 2=MOK  (4800 ONLY!!!)
    u16     ShortHeaders;       // 0x7A
    u16     HomeConfiguration;  // 0x7C
    u8      RESERVED5[2];       // 0x7E
    //----------------------------------------------------
    u8      NodeName[16];       // 0x80
    u16     ARLThreshhold;      // 0x90
    u16     ARLDecay;           // 0x92
    u16     ARLDelay;           // 0x94
    //----------------------------------------------------
    u16     Reserved1;          // 0x96
    u16     MagicMode;          // 0x98
    u16     MaxPowerSave;       // 0x9A
 }CFG_X500;

typedef struct _STSSID {
    u16     num;
    u16     Len1;
    u8      ID1[32];
    u16     Len2;
    u8      ID2[32];
    u16     Len3;
    u8      ID3[32];
}STSSID;

typedef struct _STAPLIST {
    u16     num;
    MacAddr AP1;
    MacAddr AP2;
    MacAddr AP3;
    MacAddr AP4;
}STAPLIST;

////////////////////////////////////////////////
//  PROFILE Version 1.0
////////////////////////////////////////////////

// 
// All zflags including checksum
// 
typedef struct STZFLAGS {
    u16             sum;
    CFG_X500        cfg;
    STSSID          SSIDList;
    STAPLIST        APList;
} STZFLAGS;

// 
// All zflags excluding checksum
// 
struct ZFLAGS {
    CFG_X500        cfg;
    STSSID          SSIDList;
    STAPLIST        APList;
};

// 
// profile properties Version 1
// 
struct PROFILE_PROPERTIES {
    char    name[33];
    u8      autoSelectable;
    u8      isDefault;
};

// 
// Profile Version 1.  Combination of properties and zflags 
// 
struct PROFILE {
    ZFLAGS              zFlags;
    PROFILE_PROPERTIES  properties;
};

// 
// Profile for driver's use -- includes valid flag.
// 
struct STPROFILE {
    STZFLAGS            zFlags;
    PROFILE_PROPERTIES  properties;
    u8                  isValid;
};

////////////////////////////////////////////////
//  PROFILE Version 2.0
////////////////////////////////////////////////

#define MAX_NUM_PROFILES                16
#define PROFILE_MAGIC_NUMBER            0xA1B2C3D4
#define PROFILE_VERSION                 0x02


// profile flags
#define PROFILE_IS_DEAFULT              0x0001
#define PROFILE_IS_FACTORY_DEAFULT      0x0002
#define PROFILE_IS_AUTOSELECTABLE       0x0004
#define PROFILE_ALLOW_EDIT              0x0010
#define PROFILE_ALLOW_EXPORT            0x0020
#define PROFILE_ALLOW_EDIT_WEPKEY       0x0040
#define PROFILE_IS_CHANGED              0x1000
#define PROFILE_IS_VALID                0x8000

// 
// profile properties
// 
struct PROFILE_PROPERTIES_V2 {
    char    name[80];
    u32     flags;             // 0x0001    IsDefault         
                               // 0x0002    IsFacturyDefault  
                               // 0x0004    IsAutoSwitchable  
                               // 0x0008                     
                               // 0x0010    AllowEdit         
                               // 0x0020    AllowExport       
                               // 0x0040    AllowEditWepKey   
                               // 0x0080                      
                               // 0x1000    IsChanged                 
                               // 0x8000    IsValid
    u8      reserve[40];
};

struct WEP_KEY {
    char    IsTransmitKey;      // set if selected
    char    keyLen;             // length in bytes of the Wep key
    char    keyData[128];       // buffer containing encrypted Wep key padded with random characters
    char    MacId[6];           // mac id may be used later
};

struct LEAP_PROPERTY {
    u8      type;               // 0x0  Use Windows User Name and Password
                                // 0x1  Use Separate User Name and Password
                                // 0x2  Use Saved User Name and Password
    char    username[32];
    char    password[32];
    u16     timeout;            // timeout value, default is 60 seconds
    u16     flags;              // 0x01 Include Domain Name with User Name
                                // 0x02 Not Disassociate after Logoff
};


// 
// Profile.  Combination of properties and zflags 
// 
struct PROFILE_V2 {
    u32                     MagicNumber;    // 0xA1B2C3D4
    u8                      Version;
    u32                     Size;
    PROFILE_PROPERTIES_V2   Property;
    CFG_X500                Config;
    STSSID                  SSIDList;
    STAPLIST                APList;
    WEP_KEY                 Keys[4];
    LEAP_PROPERTY           Leap;
};

// 
// Profiles to send down to driver
// 
struct STPROFILES {
    u32     Size;               // size of the structure, including profile data
    u16     NumProfiles;        // number of profiles in the structure
    u8      Data[1];            // array of profile data
};

// 
// Encrypted Profile
// 
struct ENCRYPTED_PROFILE {
    u16         CheckSum;
    char        EncryptionKey[16];
    PROFILE_V2  Profile;
};

////////////////////////////////////////////////
//  END OF PROFILE 
////////////////////////////////////////////////

typedef struct _STCAPS{
    u16     u16RidLen;                  //0x0000
    u8      au8OUI[3];                  //0x0002
    u8      nothing;                    //0x0005
    u16     ProuctNum;                  //0x0006
    u8      au8ManufacturerName[32];    //0x0008
    u8      au8ProductName[16];         //0x0028
    u8      au8ProductVersion[8];       //0x0038
    u8      au8FactoryAddress[6];       //0x0040
    u8      au8AironetAddress[6];       //0x0046
    u16     u16RadioType;               //0x004C
    u16     u16RegDomain;               //0x004E
    u8      au8Callid[6];               //0x0050
    u8      au8SupportedRates[8];       //0x0056
    u8      u8RxDiversity;              //0x005E
    u8      u8TxDiversity;              //0x005F
    u16     au16TxPowerLevels[8];       //0x0060
    u16     u16HardwareVersion;         //0x0070
    u16     u16HardwareCapabilities;    //0x0072
    u16     u16TemperatureRange;        //0x0074
    u16     u16SoftwareVersion;         //0x0076
    u16     u16SoftwareSubVersion;      //0x0078
    u16     u16InterfaceVersion ;       //0x007A
    u16     u16SoftwareCapabilities;    //0x007C
    u16     u16BootBlockVersion;        //0x007E
    u16     u16SupportedHardwareRev;    //0x0080
}STCAPS;

typedef struct _STSTATUS{
    u16     u16RidLen;                  //0x0000
    u8      au8MacAddress[6];           //0x0002
    u16     u16OperationalMode;         //0x0008
    u16     u16ErrorCode;               //0x000A
    u16     u16SignalStrength;          //0x000C
    u16     SSIDlength;                 //0x000E
    u8      SSID[32];                   //0x0010
    u8      au8ApName[16];              //0x0030
    u8      au8CurrentBssid[6];         //0x0040
    u8      au8PreviousBssid1[6];       //0x0046
    u8      au8PreviousBssid2[6];       //0x004C
    u8      au8PreviousBssid3[6];       //0x0052
    u16     u16BeaconPeriod;            //0x0058
    u16     u16DtimPeriod;              //0x005A
    u16     u16AtimDuration;            //0x005C
    u16     u16HopPeriod;               //0x005E
    union {
            u16     u16DsChannel;       //0x0060
            u16     u16HopSet;          //0x0060
            };
    u16     u16HopPattern;              //0x0062
    u16     u16HopsToBackbone;          //0x0064
    u16     u16ApTotalLoad;             //0x0066
    u16     u16OurGeneratedLoad;        //0x0068
    u16     u16AccumulatedArl;          //0x006A
    u16     u16SignalQuality;           //0x006C
    u16     u16CurrentTxRate;           //0x006E
    u16     u16APDeviceType;            //0x0070
    u16     u16NormalizedSignalStrength;//0x0072
    u16     u16UsingShortRFHeaders;     //0x0074
    u8      AccessPointIPAddress[4];    //0x0076
    u16     u16MaxNoiseLevelLastSecond; //0x007A
    u16     u16AvgNoiseLevelLastMinute; //0x007C
    u16     u16MaxNoiseLevelLastMinute; //0x007E
    u16     u16CurrentAPPacketLoad;     //0x0080
    u8      AdoptedCarrierSet[4];       //0x0082
    u16     AssociationStatus;          //0x0086
}STSTATUS;
//
typedef struct _STATISTICS {
    u16     u16RidLen;                  //0x0000
    u16     RxOverrunErr;       //; no buffer to handle rx
    u16     RxPlcpCrcErr;       //; plcp Hec errors
    u16     RxPlcpFormatErr;    //; plcp format errors
    u16     RxPlcpLengthErr;    //; plcp length is too long
    u16     RxMacCrcErr;        //; mac crc32 errors all rates
    u16     RxMacCrcOk;         //; mac crc32 ok all rates
    u16     RxWepErr;           //; wep errors
    u16     RxWepOk;            //; wep ok
    u16     RetryLong;          //; Long frame retries
    u16     RetryShort;         //; Short frame retries
    u16     MaxRetries;         //; packet failures
    u16     NoAck;              //; no ack received
    u16     NoCts;              //; no cts received
    u16     RxAck;              //; ack received
    u16     RxCts;              //; cts received
    u16     TxAck;              //; ack transmitted
    u16     TxRts;              //; rts transmitted
    u16     TxCts;              //; cts transmitted
    u16     TxMc;               //; using Address 1 (or Flag)
    u16     TxBc;           
    u16     TxUcFrags;          //; counts frags transmitted
    u16     TxUcPackets;        //; counts complete unicast packet tx
    u16     TxBeacon;           //; beacons transmitted
    u16     RxBeacon;           //; beacons received
    u16     TxSinColl;          //; Transmit single collisions
    u16     TxMulColl;          //; Transmit multiple collisions
    u16     DefersNo;           //; Frames sent with no deferral
    u16     DefersProt;         //; Frames deferred due to protocol
    u16     DefersEngy;         //; Frames deferred due to energy detect
    u16     DupFram;            //; Duplicate frames an fragments
    u16     RxFragDisc;         //; Received partial frames
    u16     TxAged;             //; Transmit packets aged
    u16     RxAged;             //; Receive packets aged
    //
    //---- Hmac loss of sync tallies -------
    //
    u16     LostSync_MaxRetry;      //  DS  2   ; C6 -- lost sync due to max retries
    u16     LostSync_MissedBeacons; //  DS  2   ; C6 -- lost sync due to missed beacons
    u16     LostSync_ArlExceeded;   //  DS  2   ; C6 -- lost sync due to arl
    u16     LostSync_Deauthed;      //  DS  2   ; C6 -- deauth received
    u16     LostSync_Disassoced;    //  DS  2   ; C6 -- deauth received
    u16     LostSync_TsfTiming;     //  DS  2   ; C6 -- tsf timing error
    //
    //---- Host initiated packet tallies ---
    //
    u16     HostTxMc;           //; C7 -- host transmitted multicast (DA)
    u16     HostTxBc;           //; C7 -- host transmitted broadcast (DA)
    u16     HostTxUc;           //; C7 -- host transmitted unicast (DA)
    u16     HostTxFail;         //; C7 -- host transmission failures

    u16     HostRxMc;           //; C7 -- host received multicast (DA)
    u16     HostRxBc;           //; C7 -- host received broadcast (DA)
    u16     HostRxUc;           //; C7 -- host received unicast (DA)
    u16     HostRxDiscard;      //; C7 -- host received discarded
    //
    //---- Hmac initiated packet tallies ---
    u16     HmacTxMc;           //; Hmac transmitted multicast (DA)
    u16     HmacTxBc;           //; Hmac transmitted broadcast (DA)
    u16     HmacTxUc;           //; Hmac transmitted unicast (DA)
    u16     HmacTxFail;         //; Hmac transmission failures
    //
    u16     HmacRxMc;           //; Hmac received multicast (DA)
    u16     HmacRxBc;           //; Hmac received broadcast (DA)
    u16     HmacRxUc;           //; Hmac received unicast (DA)
    u16     HmacRxDiscard;      //; Hmac discarded...
    u16     HmacRxAccepted;     //; Hmac accepted...

    u16     SsidMismatch;       //; C6 <airorx.asm>
    u16     ApMismatch;         //; specified ap mismatch
    u16     RatesMismatch;      //; rates mismatch
    u16     AuthReject;         //
    u16     AuthTimeout;        //
    u16     AssocReject;        //
    u16     AssocTimeout;       //

    u16     ReasonOutsideTable; //  ; C6 <airorx.asm>
    u16     ReasonStatus[19];
//
    u16     RxMan;
    u16     TxMan;
    u16     RxRefresh;
    u16     TxRefresh;
    u16     RxPoll;
    u16     TxPoll;
//
    u16     HostRetries;        //  DS  2   ; C6 -- host retries on transmits
    u16     LostSync_HostReq;   //  DS  2   ; C6 -- host requested loss of sync
    u16     HostTxBytes;        //  DS  2   ;    -- host transmitted bytes
    u16     HostRxBytes;        //  DS  2   ;    -- host received bytes
    u16     ElapsedUsec;        //  DS  2   ;    -- elapsed usec
    u16     ElapsedSec;         //  DS  2   ;    -- elapsed seconds
    u16     LostSyncBetterAP;   //  DS  2   ;    -- found a better AP
    u16     PrivacyMismatch;    //  DS  2   ;    -- capability privacy mismatch
    u16     Jammed;             //  DS  2   ;    -- jammer recovery
    u16     RxDiscWEPOff;       //  DS  2   ;    -- unencrypted packets discarded
    u16     PhyElementMismatch; //  DS  2   ;    -- phy element of beacon/probe resp bad
    u16     LeapSuccess;
    u16     LeapFailure;
    u16     LeapTimeout;
    u16     Spare[26];
}STSTATISTICS;

//typedef STATISTICS STSTATISTICS;

typedef struct _STSTATISTICS32 {
    u32     u16RidLen;                  //0x0000
    u32     RxOverrunErr;       //; no buffer to handle rx
    u32     RxPlcpCrcErr;       //; plcp Hec errors
    u32     RxPlcpFormatErr;    //; plcp format errors
    u32     RxPlcpLengthErr;    //; plcp length is too long
    u32     RxMacCrcErr;        //; mac crc32 errors all rates
    u32     RxMacCrcOk;         //; mac crc32 ok all rates
    u32     RxWepErr;           //; wep errors
    u32     RxWepOk;            //; wep ok
    u32     RetryLong;          //; Long frame retries
    u32     RetryShort;         //; Short frame retries
    u32     MaxRetries;         //; packet failures
    u32     NoAck;              //; no ack received
    u32     NoCts;              //; no cts received
    u32     RxAck;              //; ack received
    u32     RxCts;              //; cts received
    u32     TxAck;              //; ack transmitted
    u32     TxRts;              //; rts transmitted
    u32     TxCts;              //; cts transmitted
    u32     TxMc;               //; using Address 1 (or Flag)
    u32     TxBc;           
    u32     TxUcFrags;          //; counts frags transmitted
    u32     TxUcPackets;        //; counts complete unicast packet tx
    u32     TxBeacon;           //; beacons transmitted
    u32     RxBeacon;           //; beacons received
    u32     TxSinColl;          //; Transmit single collisions
    u32     TxMulColl;          //; Transmit multiple collisions
    u32     DefersNo;           //; Frames sent with no deferral
    u32     DefersProt;         //; Frames deferred due to protocol
    u32     DefersEngy;         //; Frames deferred due to energy detect
    u32     DupFram;            //; Duplicate frames an fragments
    u32     RxFragDisc;         //; Received partial frames
    u32     TxAged;             //; Transmit packets aged
    u32     RxAged;             //; Receive packets aged
    //
    //---- Hmac loss of sync tallies -------
    //
    u32     LostSync_MaxRetry;      //  DS  2   ; C6 -- lost sync due to max retries
    u32     LostSync_MissedBeacons; //  DS  2   ; C6 -- lost sync due to missed beacons
    u32     LostSync_ArlExceeded;   //  DS  2   ; C6 -- lost sync due to arl
    u32     LostSync_Deauthed;      //  DS  2   ; C6 -- deauth received
    u32     LostSync_Disassoced;    //  DS  2   ; C6 -- deauth received
    u32     LostSync_TsfTiming;     //  DS  2   ; C6 -- tsf timing error
    //
    //---- Host initiated packet tallies ---
    //
    u32     HostTxMc;           //; C7 -- host transmitted multicast (DA)
    u32     HostTxBc;           //; C7 -- host transmitted broadcast (DA)
    u32     HostTxUc;           //; C7 -- host transmitted unicast (DA)
    u32     HostTxFail;         //; C7 -- host transmission failures

    u32     HostRxMc;           //; C7 -- host received multicast (DA)
    u32     HostRxBc;           //; C7 -- host received broadcast (DA)
    u32     HostRxUc;           //; C7 -- host received unicast (DA)
    u32     HostRxDiscard;      //; C7 -- host received discarded
    //
    //---- Hmac initiated packet tallies ---
    u32     HmacTxMc;           //; Hmac transmitted multicast (DA)
    u32     HmacTxBc;           //; Hmac transmitted broadcast (DA)
    u32     HmacTxUc;           //; Hmac transmitted unicast (DA)
    u32     HmacTxFail;         //; Hmac transmission failures
    //
    u32     HmacRxMc;           //; Hmac received multicast (DA)
    u32     HmacRxBc;           //; Hmac received broadcast (DA)
    u32     HmacRxUc;           //; Hmac received unicast (DA)
    u32     HmacRxDiscard;      //; Hmac discarded...
    u32     HmacRxAccepted;     //; Hmac accepted...

    u32     SsidMismatch;       //; C6 <airorx.asm>
    u32     ApMismatch;         //; specified ap mismatch
    u32     RatesMismatch;      //; rates mismatch
    u32     AuthReject;         //
    u32     AuthTimeout;        //
    u32     AssocReject;        //
    u32     AssocTimeout;       //

    u32     ReasonOutsideTable; //  ; C6 <airorx.asm>
    u32     ReasonStatus[19];
//
    u32     RxMan;
    u32     TxMan;
    u32     RxRefresh;
    u32     TxRefresh;
    u32     RxPoll;
    u32     TxPoll;
//
    u32     HostRetries;        //  DS  2   ; C6 -- host retries on transmits
    u32     LostSync_HostReq;   //  DS  2   ; C6 -- host requested loss of sync
    u32     HostTxBytes;        //  DS  2   ;    -- host transmitted bytes
    u32     HostRxBytes;        //  DS  2   ;    -- host received bytes
    u32     ElapsedUsec;        //  DS  2   ;    -- elapsed usec
    u32     ElapsedSec;         //  DS  2   ;    -- elapsed seconds
    u32     LostSyncBetterAP;   //  DS  2   ;    -- found a better AP
    u32     PrivacyMismatch;    //  DS  2   ;    -- capability privacy mismatch
    u32     Jammed;             //  DS  2   ;    -- jammer recovery
    u32     RxDiscWEPOff;       //  DS  2   ;    -- unencrypted packets discarded
    u32     PhyElementMismatch; //  DS  2   ;    -- phy element of beacon/probe resp bad
    u32     LeapSuccess;
    u32     LeapFailure;
    u32     LeapTimeout;
    u32     Spare[26];
}STSTATISTICS32;

typedef struct
{
    char    *BufPtr;
    int     BufLen;
    USHORT  RID;
}STRIDACCESS;

typedef struct
{
    STRIDACCESS     Junk;
    ULONG           Size;
    USHORT          RID;
    int             BufOffset;
    int             BufLen;
}STRIDACCESS_V2;

//
typedef struct
{
    int     Regoffset;
    void    *BufPtr;
    int     BufLen;         // in bytes
}STREGACCESS;

typedef struct
{
    STREGACCESS     Junk;
    ULONG           Size;
    int             Regoffset;
    int             BufOffset;
    int             BufLen;
}STREGACCESS_V2;

typedef struct _STDRIVERCAPS{
    u16     Size;                   //0x0000
    u8      VendorName[32];         //0x0002
    u8      DriverVersion[16];      //0x0034
    u8      ProfileVersion;         //0x004A
    u8      RIDInterfaceVersion;    //0x004B
    u8      AutoProfile;            //0x004C
    u8      MaxPSP;                 //0x004D
    u8      MagicPacket;            //0x004E 
    u8      Reserve[64];            //0x004F
}STDRIVERCAPS;


#pragma pack( pop, struct_pack1 )

//////////////////////////////////


#endif  //__HWX500_H__
