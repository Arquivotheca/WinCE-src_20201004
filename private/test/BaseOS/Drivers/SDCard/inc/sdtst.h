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
#ifndef SD_TST_H
#define SD_TST_H

#include "SDCardDDK.h"
#include <diskio.h>
#include "bldver.h"

    // need the following for POST_INIT definition
#include <devload.h>
    //////////////////////////////////////////////

#include <pm.h>
#include <storemgr.h>

#define BR_EVENT_NAME TEXT("BusRequestComplete")
#define IR_EVENT_NAME TEXT("InterruptSignaled")

typedef UCHAR SD_IOCARD_STATUS;
typedef SD_CARD_STATUS *PSD_CARD_STATUS;

#ifdef DEBUG //Note: this is a temporary Hack
    // some debug zones
#define ZONE_VERBOZE                    SDCARD_ZONE_0
#define ENABLE_ZONE_VERBOZE         ZONE_ENABLE_0
#define ZONE_COMMENT                    SDCARD_ZONE_1
#define ENABLE_ZONE_COMMENT         ZONE_ENABLE_1
#define SDIO_ZONE_ABORT             SDCARD_ZONE_2
#define ENABLE_ZONE_ABORT               ZONE_ENABLE_2
#define SDIO_ZONE_POWER             SDCARD_ZONE_3
#define ENABLE_ZONE_POWER               ZONE_ENABLE_3
#define SDIO_ZONE_TST                   SDCARD_ZONE_4
#define ENABLE_ZONE_TST             ZONE_ENABLE_4
#define SDIO_ZONE_WRAP                  SDCARD_ZONE_5
#define ENABLE_ZONE_WRAP                ZONE_ENABLE_5

#else
#define ZONE_VERBOZE                    0
#define ENABLE_ZONE_VERBOZE         0
#define ZONE_COMMENT                    0
#define ENABLE_ZONE_COMMENT         0
#define SDIO_ZONE_ABORT             0
#define ENABLE_ZONE_ABORT               0
#define SDIO_ZONE_POWER             0
#define ENABLE_ZONE_POWER               0
#define SDIO_ZONE_TST                   0
#define ENABLE_ZONE_TST             0
#define SDIO_ZONE_WRAP                  0
#define ENABLE_ZONE_WRAP                0

#ifdef ZONE_ENABLE_FUNC
#undef ZONE_ENABLE_FUNC
#define ZONE_ENABLE_FUNC                0
#endif //ZONE_ENABLE_FUNC

#ifdef SDCARD_ZONE_FUNC
#undef SDCARD_ZONE_FUNC
#define SDCARD_ZONE_FUNC                0
#endif //SDCARD_ZONE_FUNC

#ifdef ZONE_ENABLE_INFO
#undef ZONE_ENABLE_INFO
#define ZONE_ENABLE_INFO                0
#endif //ZONE_ENABLE_INFO

#ifdef SDCARD_ZONE_INFO
#undef SDCARD_ZONE_INFO
#define SDCARD_ZONE_INFO                0
#endif //SDCARD_ZONE_INFO

#ifdef ZONE_ENABLE_INIT
#undef ZONE_ENABLE_INIT
#define ZONE_ENABLE_INIT                0
#endif //ZONE_ENABLE_INIT

#ifdef SDCARD_ZONE_INIT
#undef SDCARD_ZONE_INIT
#define SDCARD_ZONE_INIT                0
#endif //SDCARD_ZONE_INIT

#ifdef ZONE_ENABLE_WARN
#undef ZONE_ENABLE_WARN
#define ZONE_ENABLE_WARN                0
#endif //ZONE_ENABLE_WARN

#ifdef SDCARD_ZONE_WARN
#undef SDCARD_ZONE_WARN
#define SDCARD_ZONE_WARN                0
#endif //SDCARD_ZONE_WARN

#ifdef ZONE_ENABLE_ERROR
#undef ZONE_ENABLE_ERROR
#define ZONE_ENABLE_ERROR               0
#endif //ZONE_ENABLE_ERROR

#ifdef SDCARD_ZONE_ERROR
#undef SDCARD_ZONE_ERROR
#define SDCARD_ZONE_ERROR               0
#endif //SDCARD_ZONE_ERROR

#endif //DEBUG

#define SD_MEMORY_TAG 'SDIO'

#define SD_BLOCK_SIZE                   512
#define DEFAULT_BLOCK_TRANSFER_SIZE 8
#define BLOCK_TRANSFER_SIZE_KEY TEXT("BlockTransferSize")
#define FOLDER_NAME_KEY TEXT("FolderName")
#define SINGLE_BLOCK_WRITES_KEY TEXT("SingleBlockWrites")
#define DISABLE_POWER_MANAGEMENT TEXT("DisablePowerManagement")
#define IDLE_TIMEOUT TEXT("IdleTimeout")
#define IDLE_POWER_STATE TEXT("IdlePowerState")

#define DEFAULT_IDLE_TIMEOUT            2000        // 2 seconds and we suspend the card
#define DEFAULT_DESELECT_RETRY_COUNT    3

    // size of manufacturer ID and serial number as ASCII strings
    // 2 chars for manufacturer ID, 8 for serial number + a nul char for each string
#define SD_SIZEOF_STORAGE_ID            12

    //Error Codes: Basically the tests will return TRUE if the IOCTL is found, or FLASE otherwise. Even if the test fails the return value from DeviceIoControl will be TRUE, thus the error codes are needed.
//#define TPR_NOT_HANDLED       0
//#define TPR_HANDLED           1
#define TPR_SKIP                    2
#define TPR_PASS                    3
#define TPR_FAIL                    4
#define TPR_ABORT                   5

#define STAT_NORMAL     0
#define STAT_ABREV          1

typedef struct {
    SD_DEVICE_HANDLE    hDevice;
    PWSTR               pRegPath;
    CRITICAL_SECTION    RemovalLock;        // removal lock critical section
    CRITICAL_SECTION    CriticalSection;
    BOOL                CardEjected;        // card has been ejected
    BOOL                CardSelected;
    BOOL                CardBeginSelectDeselect;
    PWSTR               pRegPathAlt;
    DWORD               dwError;
    HINSTANCE           hInst;
} SDCLNT_CARD_INFO, *PSDCLNT_CARD_INFO;

typedef struct _SD_PARSED_REGISTER_ALT_STATUS
{
        UCHAR ucBusWidth;
    BOOL bSecured;
    USHORT usCardType;
    DWORD dwProtectedAreaSizeInBlocks;
        UCHAR RawAltStatusRegister[64];       // raw data
} SD_PARSED_REGISTER_ALT_STATUS, *PSD_PARSED_REGISTER_ALT_STATUS;

#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))

#define AcquireLock(pDevice) EnterCriticalSection(&(pDevice)->CriticalSection)
#define ReleaseLock(pDevice) LeaveCriticalSection(&(pDevice)->CriticalSection)

#define AcquireRemovalLock(pDevice) EnterCriticalSection(&(pDevice)->RemovalLock)
#define ReleaseRemovalLock(pDevice) LeaveCriticalSection(&(pDevice)->RemovalLock)

typedef enum
{
    FT_COPY = 0,
    FT_PERM_WP = 1,
    FT_TEMP_WP = 2
} FT_TYPE;

typedef struct _ArgType
{
    UCHAR RWFLag; //RWFlag
    UCHAR Mode; //SD_IO_BLOCK_MODE/SD_IO_RW_NORMAL, or SD_IO_BYTE_MOSE/SD_IO_RW_RAW
    UCHAR FunctionNumber; // FunctionNumber
    DWORD RegisterAddress; //RegisterAddress
    UCHAR WriteData; //Only checked if Cmd52
    UCHAR OPCode; //Only checked if CMD53
    USHORT Count; //Only checked if CMD53
}ArgType, *PArgType;

#endif // SD_TST_H

