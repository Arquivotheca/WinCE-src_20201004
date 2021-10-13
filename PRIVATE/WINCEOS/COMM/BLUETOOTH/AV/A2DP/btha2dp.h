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
/**
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED 
WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.


Abstract:
    Windows CE Bluetooth A2DP_t layer 

**/
#ifndef __BTHA2DP_HPP_INCLUDED__
#define __BTHA2DP_HPP_INCLUDED__

#include <windows.h>
#include "sbc.hxx"


#define OUT_CHANNELS (2)
#define BITS_PER_SAMPLE 16

// Macallan Dublin Compat
#define DRVM_MAPPER 0x2000
#define DRVM_MAPPER_PREFERRED_GET (DRVM_MAPPER+21) 
#define DRVM_MAPPER_PREFERRED_SET (DRVM_MAPPER+22)

const DWORD SAMPLE_RATE_DEFAULT = 44100;                    
const BYTE BIT_POOL_DEFAULT = 39;                           
const DWORD SUBBANDS = 8;                                   //not configurable
const BYTE MAX_BIT_POOL = 0xfa;                             //not configurable
const BYTE MIN_BIT_POOL = 2;                                //not configurable
const BYTE MAX_SUPPORTED_BIT_POOL_DEFAULT = 80;
const BYTE MIN_SUPPORTED_BIT_POOL_DEFAULT = 14;             //anything less than this has 
                                                            //very poor sound quality


const DWORD BLOCK_SIZE = 16;                                //not configurable
const BYTE ALLOCATION_METHOD = 0;//SBC_ALLOCATION_LOUDNESS  //not configurable
const DWORD SUSPEND_DELAY_MS_DEFAULT = 5000;
extern DWORD SAMPLE_RATE;
extern UINT32 INVSAMPLERATE;
extern BYTE BIT_POOL;                                       //configurable
extern DWORD SUSPEND_DELAY_MS;                              //configurable  
extern BYTE MAX_SUPPORTED_BIT_POOL;                         //configurable
extern BYTE MIN_SUPPORTED_BIT_POOL;                         //configurable
extern DWORD CHANNEL_MODE;                                  //configurable


//
// A2DP_t-specific user events
//

#define A2DP_LINK_DISCONNECT      1000
#define MAX_SEIDS 16
#define NUM_CAPABILITIES 10



//
// Callbacks given to A2DP_t layer during binding
//

typedef int (*A2DP_CALLBACK_DataPacketUp)        (void *pUserData, USHORT hConnection, PUCHAR pBuffer, DWORD dwBufferLen);
typedef int (*A2DP_CALLBACK_DataPacketDown)      (void *pUserData, void *pPacketUserContext, USHORT hConnection, DWORD dwError);
typedef int (*A2DP_CALLBACK_PacketsCompleted)    (void *pUserData, USHORT hConnection, USHORT nCompletedPackets);
typedef int (*A2DP_CALLBACK_StackEvent)          (void *pUserData, int iEvent);

typedef struct _A2DP_CALLBACKS
{
    A2DP_CALLBACK_StackEvent         StackEvent;
} A2DP_CALLBACKS, *PA2DP_CALLBACKS;


//
// Interface received from A2DP_t layer during binding
//

// values for dwUserCallType



#define UCT_ABORT                           0x00000001
#define UCT_DISCOVER                        0x00000002
#define UCT_GET_CAPABILITIES                0x00000003
#define UCT_CONFIGURE                       0x00000004
#define UCT_OPEN                            0x00000005
#define UCT_START                           0x00000006
#define UCT_SUSPEND                         0x00000007
#define UCT_CLOSE                           0x00000008
#define UCT_WRITE                           0x00000009



#define UCT_IOCTL_GET_BASEBAND_CONNECTIONS  0x40000001
#define UCT_IOCTL_GET_A2DP_PARAMETERS        0x40000002


typedef struct _A2DP_USER_CALL
{
    DWORD   dwUserCallType;
    HANDLE  hEvent;
    int     iResult;

    union {
        struct {
            IN      PBYTE   pDataBuffer;
            IN      DWORD   DataBufferByteCount;
        } Write;
        
        struct {
        } OtherCalls; //no params

    } uParameters;

} A2DP_USER_CALL, *PA2DP_USER_CALL;

//
// A2DP_t interface
//

typedef int (*A2DP_INTERFACE_Call)           (A2DP_USER_CALL* pUserCall);
typedef int (*A2DP_INTERFACE_AbortCall)      (A2DP_USER_CALL* pUserCall);
typedef int (*A2DP_INTERFACE_DataPacketDown) (void* pPacketUserContext, USHORT hConnection, PUCHAR pBuffer, DWORD dwBufferLen);

typedef struct _A2DP_INTERFACE
{
    A2DP_INTERFACE_Call              Call;
    A2DP_INTERFACE_AbortCall         AbortCall;
    A2DP_INTERFACE_DataPacketDown    DataPacketDown;
} A2DP_INTERFACE, *PA2DP_INTERFACE;


int a2dp_InitializeOnce(void);
int a2dp_UninitializeOnce(void);

int a2dp_CreateDriverInstance(void);
int a2dp_CloseDriverInstance(void);

int a2dp_Bind(void *pUserData, A2DP_INTERFACE* pInterface);
int a2dp_Unbind();
BOOL a2dp_IsReadyToWrite();
BOOL a2dp_HasBeenOpened();


#define BSS_DECPARAM(x)             DEBUGMSG(ZONE_FUNCTION && ZONE_PARAMS,  (TEXT("BTA2DP::\tPARAM : ")TEXT( #x )TEXT(" = %d\r\n"), x))
#define BSS_HEXPARAM(x)             DEBUGMSG(ZONE_FUNCTION && ZONE_PARAMS,  (TEXT("BTA2DP::\tPARAM : ")TEXT( #x )TEXT(" = 0x%X\r\n"), x))
#define BSS_FUNC_WMDD(x)            DEBUGMSG(ZONE_FUNCTION && ZONE_MDD,     (TEXT("BTA2DP::\t[FUNC] %a()\r\n"), x))
#define BSS_FUNC_WPDD(x)            DEBUGMSG(ZONE_FUNCTION && ZONE_PDD,     (TEXT("BTA2DP::\t\t[FUNC] %a()\r\n"), x))
#define BSS_VERBOSE(str)            DEBUGMSG(ZONE_VERBOSE,  (TEXT("BTA2DP:: ")TEXT( #str )TEXT("\r\n")))
#define BSS_VERBOSE1(str,x1)        DEBUGMSG(ZONE_VERBOSE,  (TEXT("BTA2DP:: ")TEXT( #str )TEXT("\r\n"), x1))
#define BSS_VERBOSE2(str,x1,x2)     DEBUGMSG(ZONE_VERBOSE,  (TEXT("BTA2DP:: ")TEXT( #str )TEXT("\r\n"), x1, x2))
#define BSS_ERRMSG(str)             DEBUGMSG(ZONE_ERROR,    (TEXT("BTA2DP:: [ERROR] ")TEXT( #str )TEXT("\r\n")))
#define BSS_ERRMSG1(str,x1)         DEBUGMSG(ZONE_ERROR,    (TEXT("BTA2DP:: [ERROR] ")TEXT( #str )TEXT("\r\n"), x1))
#define BSS_ERRMSG2(str,x1,x2)      DEBUGMSG(ZONE_ERROR,    (TEXT("BTA2DP:: [ERROR] ")TEXT( #str )TEXT("\r\n"), x1, x2))
#define BSS_INTMSG(str)             DEBUGMSG(ZONE_INTERRUPT,(TEXT("BTA2DP:: [INTR] ")TEXT( #str )TEXT("\r\n")))
#define BSS_INTMSG1(str,x1)         DEBUGMSG(ZONE_INTERRUPT,(TEXT("BTA2DP:: [INTR] ")TEXT( #str )TEXT("\r\n"), x1))
#define BSS_INTMSG2(str,x1,x2)      DEBUGMSG(ZONE_INTERRUPT,(TEXT("BTA2DP:: [INTR] ")TEXT( #str )TEXT("\r\n"), x1, x2))
#define BSS_WODM(str)               DEBUGMSG(ZONE_WODM,     (TEXT("BTA2DP:: [WODM] ")TEXT( #str )TEXT("\r\n")))
#define BSS_WIDM(str)               DEBUGMSG(ZONE_WIDM,     (TEXT("BTA2DP:: [WIDM] ")TEXT( #str )TEXT("\r\n")))
#define BSS_FUNCTION(str)           DEBUGMSG(ZONE_FUNCTION, (TEXT("BTA2DP:: [FUNC] ")TEXT( #str )TEXT("\r\n")))
#define BSS_FUNCTION1(str,x1)       DEBUGMSG(ZONE_FUNCTION, (TEXT("BTA2DP:: [FUNC] ")TEXT( #str )TEXT("\r\n"), x1))
#define BSS_FUNCTION2(str,x1,x2)    DEBUGMSG(ZONE_FUNCTION, (TEXT("BTA2DP:: [FUNC] ")TEXT( #str )TEXT("\r\n"), x1, x2))
#define BSS_FUNCTION3(str,x1,x2,x3) DEBUGMSG(ZONE_FUNCTION, (TEXT("BTA2DP:: [FUNC] ")TEXT( #str )TEXT("\r\n"), x1, x2,x3))
#define BSS_MISC1(str,x1)           DEBUGMSG(ZONE_MISC,     (TEXT("BTA2DP:: [MISC] ")TEXT( #str )TEXT("\r\n"), x1))
#define BSS_MISC2(str,x1,x2)        DEBUGMSG(ZONE_MISC,     (TEXT("BTA2DP:: [MISC] ")TEXT( #str )TEXT("\r\n"), x1, x2))
#define BSS_MISC3(str,x1,x2,x3)     DEBUGMSG(ZONE_MISC,     (TEXT("BTA2DP:: [MISC] ")TEXT( #str )TEXT("\r\n"), x1, x2,x3))
#define BSS_MISC4(str,x1,x2,x3,x4)  DEBUGMSG(ZONE_MISC,     (TEXT("BTA2DP:: [MISC] ")TEXT( #str )TEXT("\r\n"), x1, x2,x3,x4))

#define INVALID_CODEC_TYPE                  0xc1
#define NOT_SUPPORTED_CODEC_TYPE            0xc2
#define INVALID_SAMPLING_FREQUENCY          0xc3
#define NOT_SUPPORTED_SAMPLING_FREQUENCY    0xc4
#define INVALID_CHANNEL_MODE                0xc5
#define NOT_SUPPORTED_CHANNEL_MODE          0xc6
#define INVALID_SUBBANDS                    0xc7
#define NOT_SUPPORTED_SUBBANDS              0xc8
#define INVALID_ALLOCATION_METHOD           0xc9
#define NOT_SUPPORTED_ALLOCATION_METHOD     0xca
#define INVALID_MINIMUM_BITPOOL_VALUE       0xcb
#define NOT_SUPPORTED_MINIMUM_BITPOOL_VALUE 0xcc
#define INVALID_MAXIMUM_BITPOOL_VALUE       0xcd
#define NOT_SUPPORTED_MAXIMUM_BITPOOL_VALUE 0xce
#define INVALID_LAYER                       0xcf
#define NOT_SUPPORTED_LAYER                 0xd0
#define NOT_SUPPORTED_CRC                   0xd1
#define NOT_SUPPORTED_MPF                   0xd2
#define NOT_SUPPORTED_VBR                   0xd3
#define INVALID_BIT_RATE                    0xd4
#define NOT_SUPPORTED_BIT_RATE              0xd5
#define INVALID_OBJECT_TYPE                 0xd6
#define NOT_SUPPORTED_OBJECT_TYPE           0xd7
#define INVALID_CHANNELS                    0xd8
#define NOT_SUPPORTED_CHANNELS              0xd9
#define INVALID_VERSION                     0xda
#define NOT_SUPPORTED_VERSION               0xdb
#define NOT_SUPPORTED_MAXIMUM_SUL           0xdc
#define INVALID_BLOCK_LENGTH                0xdd
#define INVALID_CP_TYPE                     0xe0
#define INVALID_CP_FORMAT                   0xe1
//implementation specific
#define INVALID_TRANSPORT_TYPE                  0x01
#define INVALID_CODEC_SERVICE_CATEGORY_LEN      0x02
#endif      
