//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2002 BSQUARE Corporation. All rights reserved.
// Copyright (c) Microsoft Corporation.  All rights reserved.//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
// --------------------------------------------------------------------
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// --------------------------------------------------------------------
//
#ifndef SD_TST_CMN_H
#define SD_TST_CMN_H

#include <sdcardddk.h>
#include "model_cmn.h"

#ifndef _PREFAST_

#pragma warning(disable:4068)

#endif

//2 Macros

#ifndef ZeroMemory
#define ZeroMemory(Destination,Length) memset((Destination),0,(Length))
#endif

#ifndef HighMemory
#define HighMemory(Destination,Length) memset((Destination),0xFF,(Length))
#endif

typedef enum
{
    SDDEVICE_SD = 0,
    SDDEVICE_SDHC = 1,
    SDDEVICE_MMC = 2,
    SDDEVICE_MMCHC = 3,
    SDDEVICE_SDIO = 4,
    SDDEVICE_UKN = 5
} SDTST_DEVICE_TYPE;

typedef enum  _SD_CMD_ARG {
    SD_CMD_ARG_INVALID = -1,
    CMD_ARG_NULL  = 0,
    CMD_ARG_STUFF  = 1,
    CMD_ARG_DSR = 2,
    CMD_ARG_RCA  = 3,
    CMD_ARG_BLOCK_LEN  = 4,
    CMD_ARG_DATA_ADDRESS  = 5,
    CMD_ARG_WRITE_PROTECT_DATA_ADDRESS  = 6,
    CMD_ARG_RD_WR  = 7,
    CMD_ARG_BUS_WIDTH  = 8,
    CMD_ARG_NUM_BLOCKS  = 9,
    CMD_ARG_OCR_WO_BUSY  = 10,
    CMD_ARG_SET_CD = 11,
    CMD_ARG_RANDOM  = 12
} SD_CMD_ARG, *PSD_CMD_ARG;
//2 Flags

#define SD_FLAG_SYNCHRONOUS 0x00000001      //used to switch to synchronous bus command on test from async
#define SD_FLAG_ALT_CMD     0x00000002      //used to shift command to app cmd meaning only  checked on CMD13/ACMD13 & CMD42/ACMD42
#define SD_FLAG_BYTE_RAW        0x00000004      //used to switch between raw and normal or block and byte CMD52s and CMD53s
#define SD_FLAG_WRITE           0x00000008      //used only in RWRegDirect tests to switch between read and write tests, & Odd Size RW Tests
#define SD_FLAG_CI_CLOCK_RATE   0x00000010      //used in SDSetCardFeature test to switch between testing Card Interface Clock Rate or Card Interface Mode (bus width)
#define SD_FLAG_MISALIGN        0x00000020      //used in Odd Size RW tests

//2 Defines

// registry related defines

#define SDTST_CUSTOM_DRV_REG  TEXT("Drivers\\SDCARD\\ClientDrivers\\Custom")

#define SDTST_MARS_REG      TEXT("MANF-2211-CARDID-55AA-FUNC-1")
#define SDTST_MARS_REG2         TEXT("MANF-0388-CARDID-0000-FUNC-1")
#define SDTST_MARS_REG_PATH     TEXT("Drivers\\SDCARD\\ClientDrivers\\Custom\\MANF-2211-CARDID-55AA-FUNC-1")
#define SDTST_MARS_REG2_PATH    TEXT("Drivers\\SDCARD\\ClientDrivers\\Custom\\MANF-0388-CARDID-0000-FUNC-1")

#define SDTST_MARS_DLL          TEXT("SDIOTst.dll")
#define SDTST_MARS_PREFIX       TEXT("SIO")

#define MESSAGE_BUFFER_LENGTH 5000
#define EVNT_NAME TEXT("DriverLoaded")

// Define Macro to use block addressing (for 2.0) for SDHC & MMCHC
#define IS_HIGHCAPACITY_MEDIA(x)  (((SDTST_DEVICE_TYPE)x == SDDEVICE_SDHC) || ((SDTST_DEVICE_TYPE)x == SDDEVICE_MMCHC))

typedef struct
{
  int sd_bus_cmd;
  SD_CMD_ARG cmdArg;
  int transferClass;
  int responseType;
  BOOL SyncCmd;
  int sd_end_state;
  SD_API_STATUS APIStatus;
}SD_COMMAND_STRUCT, *PSD_COMMAND_STRUCT;

typedef struct
{
  TCHAR name[500];
  TCHAR value[500];
}SD_STATE_STRUCT, *PSD_STATE_STRUCT;

//2 test parameter structure
typedef struct testParam
{
    DWORD   TestFlags;          // Test Flags
    int iResult;
    UCHAR BusRequestCode;       //if test uses bus requests
    TCHAR   MessageBuffer[MESSAGE_BUFFER_LENGTH + 1];   // error message buffer
    BOOL    bMessageOverflow;
    DWORD dwSeed;
    BOOL    bHasDisplay;
//4 SDONLY
    SD_INFO_TYPE iT;
//4 SDIO only
    UCHAR TupleCode;
    SD_SET_FEATURE_TYPE ft;     //only used by setcard feature tests
    SD_COMMAND_STRUCT sdCmdStruct;
    SD_STATE_STRUCT sdStateStruct;
    SDTST_DEVICE_TYPE sdDeviceType;
    BOOL b_useFastpath;
    testParam(){sdDeviceType = SDDEVICE_UKN;}
    ~testParam(){}
}TEST_PARAMS, *PTEST_PARAMS;

#endif //SDTST_CMN_H

