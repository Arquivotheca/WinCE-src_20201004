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
#include <SDCardDDK.h>
#include <sdcard.h>
#include "misc_hlp.h"
#include "drv_dlg.h"
#include "resource.h"
#include <sd_tst_cmn.h>
#include <sddrv.h>
#include <ceddk.h>

void InitStuffTest(UINT indent, PTEST_PARAMS  pTestParams,  PSDCLNT_CARD_INFO pClientInfo)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    UINT indent3 = indent + 3;
    PREFAST_ASSERT(indent3 > indent);
    UINT indent4 = indent + 4;
    PREFAST_ASSERT(indent4 > indent);
    BOOL bSuccess;
    LPCTSTR lead = TEXT("InitTst");
    TCHAR buff[100];
    SD_API_STATUS rStat;
    SD_CARD_STATUS crdStatus;
    HKEY hKey = NULL;
    BOOL bGoodVal = FALSE;
    LogDebug(indent, SDIO_ZONE_TST, TEXT("InitTst: Entering Driver Test Function..."));
//1 SDDetDeviceHandle
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("InitTst: Testing SDGetDeviceHandle..."));
    //2 hDevice
    LogDebug(indent2, SDIO_ZONE_TST, TEXT("InitTst: Does hDevice seem valid?"));
    LogDebug(indent3, SDIO_ZONE_TST, TEXT("InitTst: Is it non-Null?"));
    bSuccess = ReportPassFail(pClientInfo->hDevice != NULL, indent4, lead, TEXT("hDevice equals NULL!!"));
    if (!bSuccess)
    {
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The device handle returned by SDGetDeviceHandle during SMC_Init is NULL. "));
        goto REG1;
    }
    LogDebug(indent3, SDIO_ZONE_TST, TEXT("InitTst: Can I use it to do something?"));
    LogDebug(indent4, SDIO_ZONE_TST, TEXT("InitTst: Calling SDCardInfoQuery..."));
    rStat = wrap_SDCardInfoQuery(pClientInfo->hDevice, SD_INFO_CARD_STATUS, &crdStatus, sizeof(SD_CARD_STATUS));
    ZeroMemory(buff, 100 * sizeof(TCHAR));
    StringCchPrintf(buff, _countof(buff), TEXT("SDCardInfoQuery returned %s (0x%x)"),TranslateErrorCodes(rStat), rStat);
    bSuccess = ReportPassFail((SD_API_SUCCESS(rStat)), indent4, lead, buff);
    if (!bSuccess)
    {
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The device handle returned by SDGetDeviceHandle may be non Null\n However, when I attempt to use it to get the card's status via SDCardInfoQuery, I get an error\nError = %s (0x%x). "),TranslateErrorCodes(rStat),rStat);
    }
REG1:
    //2 Reg Path
    LogDebug(indent2, SDIO_ZONE_TST, TEXT("InitTst: Does pRegPath  point to valid key?"));
    LogDebug(indent3, SDIO_ZONE_TST, TEXT("InitTst: Is the key Non-Null?"));
    bSuccess = ReportPassFail(pClientInfo->pRegPath != NULL, indent4, lead, TEXT("pRegPath equals NULL!!"));
    if (!bSuccess)
    {
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The registry path returned by SDGetDeviceHandle during SMC_Init is NULL. "));
        goto REG2;
    }
    if (pClientInfo->pRegPath == NULL) goto REG2;//put in for prefast happiness
    LogDebug(indent3, SDIO_ZONE_TST, TEXT("InitTst: Is the key of memory class?"));
//  _stprintf(buff, TEXT(""),TranslateErrorCodes(rStat), rStat);

    switch (pTestParams->sdDeviceType) {
        case SDDEVICE_SD:
            bGoodVal = _tcscmp(pClientInfo->pRegPath, TEXT("\\Drivers\\SDCARD\\ClientDrivers\\Class\\SDMemory_Class")) == 0;
            break;
        case SDDEVICE_SDHC:
            bGoodVal = _tcscmp(pClientInfo->pRegPath, TEXT("\\Drivers\\SDCARD\\ClientDrivers\\Class\\SDMemory_Class\\High_Capacity")) == 0;
            break;

        case SDDEVICE_MMC:
            bGoodVal = _tcscmp(pClientInfo->pRegPath, TEXT("\\Drivers\\SDCARD\\ClientDrivers\\Class\\MMC_Class")) == 0;
            break;

        case SDDEVICE_MMCHC:
            bGoodVal = _tcscmp(pClientInfo->pRegPath, TEXT("\\Drivers\\SDCARD\\ClientDrivers\\Class\\MMC_Class\\High_Capacity")) == 0;
            break;

    }
    bSuccess = ReportPassFail(bGoodVal, indent4, lead, TEXT("pRegPath does not appear to be a valid memory class driver"));
    if (!bSuccess)
    {
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The registry path returned from SDGetDeviceHandle does not appear to be a valid memory class driver.\n "));
        goto REG2;
    }
    LogDebug(indent3, SDIO_ZONE_TST, TEXT("InitTst: Does the key exist in the registry?"));
    LogDebug(indent4, SDIO_ZONE_TST, TEXT("InitTst: Attempting RegOpenKeyEx..."));
    bSuccess = ReportPassFail((RegOpenKeyEx(HKEY_LOCAL_MACHINE,pClientInfo->pRegPath,0, 0,&hKey) == ERROR_SUCCESS), indent4, lead, TEXT("pRegPath looks like a valid memory class driver, however I can not find it in the registry."));
    if (!bSuccess)
    {
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("RegOpenKeyEx failed on the registry key path retuned from SDGetDeviceHandle."));
    }
REG2:
//1 SDRegPathFromInitContext
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("InitTst: Testing SDRegPathFromInitContext..."));
    if (bGoodVal)
    {
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("InitTst: Is the key Non-Null?"));
        bSuccess = ReportPassFail(pClientInfo->pRegPathAlt != NULL, indent4, lead, TEXT("pRegPathAlt equals NULL!!"));
        if (!bSuccess)
        {
            AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The registry path returned by SDRegPathFromInitContext during SMC_Init is NULL. "));
            goto DONE;
        }
        if (pClientInfo->pRegPathAlt == NULL) goto DONE;//put in for prefast happiness
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("InitTst: Does pRegPathAlt point to same  key as pRegPath?"));

        switch (pTestParams->sdDeviceType) {
            case SDDEVICE_SD:
                bGoodVal = _tcscmp(pClientInfo->pRegPathAlt, TEXT("\\Drivers\\SDCARD\\ClientDrivers\\Class\\SDMemory_Class")) == 0;
                break;
            case SDDEVICE_SDHC:
                bGoodVal = _tcscmp(pClientInfo->pRegPathAlt, TEXT("\\Drivers\\SDCARD\\ClientDrivers\\Class\\SDMemory_Class\\High_Capacity")) == 0;
                break;

            case SDDEVICE_MMC:
                bGoodVal = _tcscmp(pClientInfo->pRegPathAlt, TEXT("\\Drivers\\SDCARD\\ClientDrivers\\Class\\MMC_Class")) == 0;
                break;

            case SDDEVICE_MMCHC:
                bGoodVal = _tcscmp(pClientInfo->pRegPathAlt, TEXT("\\Drivers\\SDCARD\\ClientDrivers\\Class\\MMC_Class\\High_Capacity")) == 0;
                break;
        }

        bSuccess = ReportPassFail(bGoodVal, indent4, lead, TEXT("pRegPathAlt != pRegPath"));
        if (!bSuccess)
        {
            AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The registry path returned from SDRegPathFromInitContext does not equal the registry path returned from SDGetDeviceHandle\npRegPath (SDGetDeviceHandle) = %s; pRegPathAlt (SDRegPathFromInitContext) = %s "), pClientInfo->pRegPath, pClientInfo->pRegPathAlt);
            goto DONE;
        }

    }
    else
    {
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("InitTst: Does pRegPathAlt  point to valid key?"));
        LogDebug(indent3, SDIO_ZONE_TST, TEXT("InitTst: Is the key Non-Null?"));
        bSuccess = ReportPassFail(pClientInfo->pRegPathAlt != NULL, indent4, lead, TEXT("pRegPathAlt equals NULL!!"));
        if (!bSuccess)
        {
            AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The registry path returned by SDRegPathFromInitContext during SMC_Init is NULL. "));
            goto DONE;
        }
        if (pClientInfo->pRegPathAlt == NULL) goto DONE;//put in for prefast happiness

        LogDebug(indent3, SDIO_ZONE_TST, TEXT("InitTst: Is the key of memory class?"));
//      _stprintf(buff, TEXT(""),TranslateErrorCodes(rStat), rStat);

        switch (pTestParams->sdDeviceType) {
            case SDDEVICE_SD:
                bGoodVal = _tcscmp(pClientInfo->pRegPathAlt, TEXT("\\Drivers\\SDCARD\\ClientDrivers\\Class\\SDMemory_Class")) == 0;
                break;
            case SDDEVICE_SDHC:
                bGoodVal = _tcscmp(pClientInfo->pRegPathAlt, TEXT("\\Drivers\\SDCARD\\ClientDrivers\\Class\\SDMemory_Class\\High_Capacity")) == 0;
                break;

            case SDDEVICE_MMC:
                bGoodVal = _tcscmp(pClientInfo->pRegPathAlt, TEXT("\\Drivers\\SDCARD\\ClientDrivers\\Class\\MMC_Class")) == 0;
                break;

            case SDDEVICE_MMCHC:
                bGoodVal = _tcscmp(pClientInfo->pRegPathAlt, TEXT("\\Drivers\\SDCARD\\ClientDrivers\\Class\\MMC_Class\\High_Capacity")) == 0;
                break;
        }

        bSuccess = ReportPassFail(bGoodVal, indent4, lead, TEXT("pRegPathAlt does not appear to be a valid memory class driver"));
        if (!bSuccess)
        {
            AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The registry path returned from SDRegPathFromInitContext does not appear to be a valid memory class driver.\n"));
            goto DONE;
        }
        LogDebug(indent3, SDIO_ZONE_TST, TEXT("InitTst: Does the key exist in the registry?"));
        LogDebug(indent4, SDIO_ZONE_TST, TEXT("InitTst: Attempting RegOpenKeyEx..."));
        //3 Note: If I am here I know that the registry returned already = HKEY_LOCAL_MACHINE\...\SDMemory_Class so I will hard code it here.
        bSuccess = ReportPassFail((RegOpenKeyEx(HKEY_LOCAL_MACHINE,pClientInfo->pRegPathAlt,0,0,&hKey) == ERROR_SUCCESS), indent4, lead, TEXT("pRegPathAlt looks like a valid memory class driver, however I can not find it in the registry."));
        if (!bSuccess)
        {
            AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("RegOpenKeyEx failed on the registry key path retuned from SDGetDeviceHandleSDGetDeviceHandle."));
        }
    }
DONE:
    LogDebug(indent, SDIO_ZONE_TST, TEXT("InitTst: Exiting Driver Test Function..."));
    if (hKey)
    {
        RegCloseKey(hKey);
        hKey = NULL;
    }

}

BOOL GBSCompare(UINT indent, PTEST_PARAMS  pTestParams, SD_DEVICE_HANDLE /*hDevice*/, DWORD dwBufferSize, DWORD offset, UCHAR numBits, BOOL const *pbNoCriticalErr)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    PUCHAR buff = NULL;

    DWORD dwGBS = 0;
    DWORD dwMygetBS = 0;
    BOOL bNoCriticalErrors = TRUE;
    BOOL bSuccess = FALSE;
//1 Allocating Random Buffer
    LogDebug(indent, SDIO_ZONE_TST, TEXT("GBSCompare: Allocating Memory..."));
    bNoCriticalErrors = AllocBuffer(indent1, dwBufferSize, BUFF_RANDOM_NOSET, &buff);

    if (bNoCriticalErrors == FALSE)
    {
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Failure Allocating memory for PUCHAR buffer that was to be sliced. This might have been because of a lack of available memory. See debug logging for more details."));
        goto DONE;
    }
//1 Initial Logging
    LogDebug(indent, SDIO_ZONE_TST, TEXT("GBSCompare: Dumping buffer information..."));
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("GBSCompare: Buffer = %s\t\t(%u bytes)."), GenerateHexString(buff, dwBufferSize), dwBufferSize);
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("GBSCompare: Offset = %u\t\tNumer of Bits = %u."),  offset, numBits);
//1 GetBitSlice
    LogDebug(indent, SDIO_ZONE_TST, TEXT("GBSCompare: Performing GetBitSlice..."));
    dwGBS = wrap_GetBitSlice(buff, dwBufferSize, offset, numBits);
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("GBSCompare: GetBitSlice returns 0x%x\n"),  dwGBS);
//1 Simulation of GetBitSlice
    LogDebug(indent, SDIO_ZONE_TST, TEXT("GBSCompare: Independently simulating GetBitSlice..."));
    dwMygetBS = myGetBits(indent1, buff, dwBufferSize, offset, numBits, &bNoCriticalErrors);
    if (bNoCriticalErrors == FALSE)
    {
        LogFail(indent, TEXT("GBSCompare: Failure when running simulation of GetBitSlice. This could be due to a parameter error, or more likely a memory allocation problem."));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT(" Failure when running simulation of GetBitSlice. This could be due to a parameter error, or more likely a memory allocation problem.\n\tBuffer : %s \t\t(%u bytes)\n\tOffset : %u;\t\t# Bits : %u"), GenerateHexString(buff, dwBufferSize), dwBufferSize, offset, numBits);
        goto DONE;
    }
//1 Comparing Results
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("GBSCompare: Simulation of GetBitSlice returns 0x%x\n"),  dwMygetBS);
    if (dwGBS != dwMygetBS)
    {
        LogFail(indent, TEXT("Value returned from GetBitSlice differs from value returned in simulation code"));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Different values are returned from GetBitSlice and the simulation code.\n\tBuffer : %s \t\t(%u bytes)\n\tOffset : %u;\t\t# Bits : %u\n\tGetBitSlice returned: 0x%x\n\tSimulation : 0x%x"), GenerateHexString(buff, dwBufferSize), dwBufferSize, offset, numBits, dwGBS, dwMygetBS);
    }
    bSuccess = TRUE;
DONE:
    DeAllocBuffer(&buff);
    pbNoCriticalErr = &bNoCriticalErrors;
    return bSuccess;
}

void GetBitSliceTest(UINT indent, PTEST_PARAMS  pTestParams,  PSDCLNT_CARD_INFO pClientInfo)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    UINT indent3 = indent + 3;
    PREFAST_ASSERT(indent3 > indent);
    UINT indent4 = indent + 4;
    PREFAST_ASSERT(indent4 > indent);
    UINT indent5 = indent + 5;
    PREFAST_ASSERT(indent5 > indent);
    BOOL bNoCrit = TRUE;
    LogDebug(indent, SDIO_ZONE_TST, TEXT("GBST: Entering Driver Test Function..."));
//1 Calling srand
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("GBST: calling srand. Seed = %u..."), pTestParams->dwSeed);
    srand(pTestParams->dwSeed);
//1 Perform GetBitSlice & Simulation
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("GBST: Performing GetBitSlices and Simulations..."));
    //2Byte Alignment, Whole Byte
    LogDebug(indent2, SDIO_ZONE_TST, TEXT("GBST: Total Byte alignment (offsets, byte aligned, and # bits in multiples of 8..."));
    //3 1-Byte
    LogDebug(indent3, SDIO_ZONE_TST, TEXT("GBST: 1-Byte..."));
    //4 From Beginning
    LogDebug(indent4, SDIO_ZONE_TST, TEXT("GBST: First Byte..."));
    GBSCompare(indent5, pTestParams, pClientInfo->hDevice, 6, 0, 8, &bNoCrit);
    if (bNoCrit == FALSE)
    {
        LogDebug(indent5, SDIO_ZONE_TST, TEXT("GBST: Bailing..."));
        goto DONE;
    }
    //4 From End
    LogDebug(indent4, SDIO_ZONE_TST, TEXT("GBST: Last Byte..."));
    GBSCompare(indent5, pTestParams, pClientInfo->hDevice, 6, 40, 8, &bNoCrit);
    if (bNoCrit == FALSE)
    {
        LogDebug(indent5, SDIO_ZONE_TST, TEXT("GBST: Bailing..."));
        goto DONE;
    }
    //4 From Middle
    LogDebug(indent4, SDIO_ZONE_TST, TEXT("GBST: Middle Byte..."));
    GBSCompare(indent5, pTestParams, pClientInfo->hDevice, 10, 32, 8, &bNoCrit);
    if (bNoCrit == FALSE)
    {
        LogDebug(indent5, SDIO_ZONE_TST, TEXT("GBST: Bailing..."));
        goto DONE;
    }
    //3 >1-Byte, but <4-Byte
    LogDebug(indent3, SDIO_ZONE_TST, TEXT("GBST: > 1-Byte, < 3-Byte..."));
    GBSCompare(indent4, pTestParams, pClientInfo->hDevice, 4, 16, 16, &bNoCrit);
    if (bNoCrit == FALSE)
    {
        LogDebug(indent4, SDIO_ZONE_TST, TEXT("GBST: Bailing..."));
        goto DONE;
    }
    //3 Full DWORD
    LogDebug(indent3, SDIO_ZONE_TST, TEXT("GBST: Full DWORD..."));
    GBSCompare(indent4, pTestParams, pClientInfo->hDevice, 6, 8, 32, &bNoCrit);
    if (bNoCrit == FALSE)
    {
        LogDebug(indent4, SDIO_ZONE_TST, TEXT("GBST: Bailing..."));
        goto DONE;
    }
    //2 Byte Alignment, Number of Bits Non-Byte Sized
    LogDebug(indent2, SDIO_ZONE_TST, TEXT("GBST: Offset Byte alignment (offsets, byte aligned, but number of bits not multiples of 8..."));
    //3 < 1-Byte
    LogDebug(indent3, SDIO_ZONE_TST, TEXT("GBST: < 1-Byte..."));
    //4 From Beginning
    LogDebug(indent4, SDIO_ZONE_TST, TEXT("GBST: First Byte..."));
    GBSCompare(indent5, pTestParams, pClientInfo->hDevice, 5, 0, 6, &bNoCrit);
    if (bNoCrit == FALSE)
    {
        LogDebug(indent5, SDIO_ZONE_TST, TEXT("GBST: Bailing..."));
        goto DONE;
    }
    //4 At end
    LogDebug(indent4, SDIO_ZONE_TST, TEXT("GBST: Last Byte..."));
    GBSCompare(indent5, pTestParams, pClientInfo->hDevice, 5, 32, 6, &bNoCrit);
    if (bNoCrit == FALSE)
    {
        LogDebug(indent5, SDIO_ZONE_TST, TEXT("GBST: Bailing..."));
        goto DONE;
    }
    //4 from middle
    LogDebug(indent4, SDIO_ZONE_TST, TEXT("GBST: Middle Byte..."));
    GBSCompare(indent5, pTestParams, pClientInfo->hDevice, 5, 27, 5, &bNoCrit);
    if (bNoCrit == FALSE)
    {
        LogDebug(indent5, SDIO_ZONE_TST, TEXT("GBST: Bailing..."));
        goto DONE;
    }
    //3 > 1-Byte, < 3-Byte
    LogDebug(indent3, SDIO_ZONE_TST, TEXT("GBST: > 1-Byte, < 3-Byte..."));
    GBSCompare(indent4, pTestParams, pClientInfo->hDevice, 10, 32, 22, &bNoCrit);
    if (bNoCrit == FALSE)
    {
        LogDebug(indent4, SDIO_ZONE_TST, TEXT("GBST: Bailing..."));
        goto DONE;
    }
    //3 > 3-Byte
    LogDebug(indent3, SDIO_ZONE_TST, TEXT("GBST: > 3-Byte..."));
    GBSCompare(indent4, pTestParams, pClientInfo->hDevice, 10, 48, 29, &bNoCrit);
    if (bNoCrit == FALSE)
    {
        LogDebug(indent4, SDIO_ZONE_TST, TEXT("GBST: Bailing..."));
        goto DONE;
    }
    //2 Whole Bytes but not aligned
    LogDebug(indent2, SDIO_ZONE_TST, TEXT("GBST: Whole Bytes, but not Aligned..."));
    //3 1-Byte
    LogDebug(indent3, SDIO_ZONE_TST, TEXT("GBST: 1-Byte..."));
    GBSCompare(indent4, pTestParams, pClientInfo->hDevice, 14, 85, 8, &bNoCrit);
    if (bNoCrit == FALSE)
    {
        LogDebug(indent4, SDIO_ZONE_TST, TEXT("GBST: Bailing..."));
        goto DONE;
    }
    //3  3-Byte
    LogDebug(indent3, SDIO_ZONE_TST, TEXT("GBST: 3-Byte..."));
    GBSCompare(indent4, pTestParams, pClientInfo->hDevice, 7, 3, 27, &bNoCrit);
    if (bNoCrit == FALSE)
    {
        LogDebug(indent4, SDIO_ZONE_TST, TEXT("GBST: Bailing..."));
        goto DONE;
    }
    //3 4-Byte
    LogDebug(indent3, SDIO_ZONE_TST, TEXT("GBST: 4-Byte..."));
    GBSCompare(indent4, pTestParams, pClientInfo->hDevice, 20, 100, 32, &bNoCrit);
    if (bNoCrit == FALSE)
    {
        LogDebug(indent4, SDIO_ZONE_TST, TEXT("GBST: Bailing..."));
        goto DONE;
    }
    //2 Non-Alignment, Non-Byte Sized
    LogDebug(indent2, SDIO_ZONE_TST, TEXT("GBST: Not Whole Bytes, and not Aligned..."));
    //3 < 1-Byte
    LogDebug(indent3, SDIO_ZONE_TST, TEXT("GBST: < 1-Byte..."));
    //4  From Middle
    LogDebug(indent4, SDIO_ZONE_TST, TEXT("GBST: From Middle..."));
    GBSCompare(indent5, pTestParams, pClientInfo->hDevice, 8, 40, 4, &bNoCrit);
    if (bNoCrit == FALSE)
    {
        LogDebug(indent5, SDIO_ZONE_TST, TEXT("GBST: Bailing..."));
        goto DONE;
    }
    //4 From End
    LogDebug(indent4, SDIO_ZONE_TST, TEXT("GBST: From End..."));
    GBSCompare(indent5, pTestParams, pClientInfo->hDevice, 15, 118, 2, &bNoCrit);
    if (bNoCrit == FALSE)
    {
        LogDebug(indent5, SDIO_ZONE_TST, TEXT("GBST: Bailing..."));
        goto DONE;
    }
    //3 > 1-Byte, < 3-Byte
    LogDebug(indent3, SDIO_ZONE_TST, TEXT("GBST: > 1-Byte, < 3-Byte..."));
    GBSCompare(indent4, pTestParams, pClientInfo->hDevice, 6, 5, 19, &bNoCrit);
    if (bNoCrit == FALSE)
    {
        LogDebug(indent4, SDIO_ZONE_TST, TEXT("GBST: Bailing..."));
        goto DONE;
    }
    //3 > 3-Byte
    LogDebug(indent3, SDIO_ZONE_TST, TEXT("GBST: > 3-Byte..."));
    GBSCompare(indent4, pTestParams, pClientInfo->hDevice, 9, 17, 30, &bNoCrit);
    if (bNoCrit == FALSE)
    {
        LogDebug(indent4, SDIO_ZONE_TST, TEXT("GBST: Bailing..."));
        goto DONE;
    }
    //2 Single Bit Cases
    LogDebug(indent2, SDIO_ZONE_TST, TEXT("GBST: Not Whole Bytes, and not Aligned..."));
    //3 Small Buffer
    LogDebug(indent3, SDIO_ZONE_TST, TEXT("GBST: From Very Small Buffer (1-Byte)..."));
    //4 First Bit
    LogDebug(indent4, SDIO_ZONE_TST, TEXT("GBST: First Bit..."));
    GBSCompare(indent5, pTestParams, pClientInfo->hDevice, 1, 0, 1, &bNoCrit);
    if (bNoCrit == FALSE)
    {
        LogDebug(indent5, SDIO_ZONE_TST, TEXT("GBST: Bailing..."));
        goto DONE;
    }
    //4 Last Bit
    LogDebug(indent4, SDIO_ZONE_TST, TEXT("GBST: Last Bit..."));
    GBSCompare(indent5, pTestParams, pClientInfo->hDevice, 1, 7, 1, &bNoCrit);
    if (bNoCrit == FALSE)
    {
        LogDebug(indent5, SDIO_ZONE_TST, TEXT("GBST: Bailing..."));
        goto DONE;
    }
    //4 Middle Bit
    LogDebug(indent4, SDIO_ZONE_TST, TEXT("GBST: Middle Bit..."));
    GBSCompare(indent5, pTestParams, pClientInfo->hDevice, 2, 9, 1, &bNoCrit);
    if (bNoCrit == FALSE)
    {
        LogDebug(indent5, SDIO_ZONE_TST, TEXT("GBST: Bailing..."));
        goto DONE;
    }
    //3  Large Buffer
    LogDebug(indent3, SDIO_ZONE_TST, TEXT("GBST: From Large Buffer..."));
    //4 First Bit
    LogDebug(indent4, SDIO_ZONE_TST, TEXT("GBST: First Bit..."));
    GBSCompare(indent5, pTestParams, pClientInfo->hDevice, 10, 0, 1, &bNoCrit);
    if (bNoCrit == FALSE)
    {
        LogDebug(indent5, SDIO_ZONE_TST, TEXT("GBST: Bailing..."));
        goto DONE;
    }
    //4 Last Bit
    LogDebug(indent4, SDIO_ZONE_TST, TEXT("GBST: Last Bit..."));
    GBSCompare(indent5, pTestParams, pClientInfo->hDevice, 10, 79, 1, &bNoCrit);
    if (bNoCrit == FALSE)
    {
        LogDebug(indent5, SDIO_ZONE_TST, TEXT("GBST: Bailing..."));
        goto DONE;
    }
    //4 Middle Bit
    LogDebug(indent4, SDIO_ZONE_TST, TEXT("GBST: Middle Bit..."));
    GBSCompare(indent5, pTestParams, pClientInfo->hDevice, 8, 20, 1, &bNoCrit);
    if (bNoCrit == FALSE)
    {
        LogDebug(indent5, SDIO_ZONE_TST, TEXT("GBST: Bailing..."));
        goto DONE;
    }
    //2 Whole or Most of Buffer
    LogDebug(indent2, SDIO_ZONE_TST, TEXT("GBST: Whole or Most of Buffer..."));
    //3 Whole Buffer
    LogDebug(indent3, SDIO_ZONE_TST, TEXT("GBST: Whole  Buffer..."));
    //4 < 4-Byte
    LogDebug(indent4, SDIO_ZONE_TST, TEXT("GBST: Buffer < 4 bytes..."));
    GBSCompare(indent5, pTestParams, pClientInfo->hDevice, 1, 0, 8, &bNoCrit);
    if (bNoCrit == FALSE)
    {
        LogDebug(indent5, SDIO_ZONE_TST, TEXT("GBST: Bailing..."));
        goto DONE;
    }
    //4 4-Byte
    LogDebug(indent4, SDIO_ZONE_TST, TEXT("GBST: 4 Byte Buffer..."));
    GBSCompare(indent5, pTestParams, pClientInfo->hDevice, 4, 0, 32, &bNoCrit);
    if (bNoCrit == FALSE)
    {
        LogDebug(indent5, SDIO_ZONE_TST, TEXT("GBST: Bailing..."));
        goto DONE;
    }
    //3 Most of buffer
    LogDebug(indent3, SDIO_ZONE_TST, TEXT("GBST: Most of Buffer..."));
    //4 < 4-Byte
    LogDebug(indent4, SDIO_ZONE_TST, TEXT("GBST: Buffer < 4 bytes..."));
    GBSCompare(indent5, pTestParams, pClientInfo->hDevice, 2, 1, 15, &bNoCrit);
    if (bNoCrit == FALSE)
    {
        LogDebug(indent5, SDIO_ZONE_TST, TEXT("GBST: Bailing..."));
        goto DONE;
    }
    //4 4-Byte
    LogDebug(indent4, SDIO_ZONE_TST, TEXT("GBST: 4  Byte Buffer..."));
    GBSCompare(indent5, pTestParams, pClientInfo->hDevice, 4, 2, 29, &bNoCrit);
    if (bNoCrit == FALSE)
    {
        LogDebug(indent5, SDIO_ZONE_TST, TEXT("GBST: Bailing..."));
        goto DONE;
    }
    //2 From Extremely Large Buffer
    LogDebug(indent2, SDIO_ZONE_TST, TEXT("GBST: From Extremely Large Buffer..."));
    //3 Towards Middle
    LogDebug(indent3, SDIO_ZONE_TST, TEXT("GBST: From middle of buffer..."));
    GBSCompare(indent4, pTestParams, pClientInfo->hDevice, 50, 130, 13, &bNoCrit);
    if (bNoCrit == FALSE)
    {
        LogDebug(indent4, SDIO_ZONE_TST, TEXT("GBST: Bailing..."));
        goto DONE;
    }
    //3 Towards End
    LogDebug(indent3, SDIO_ZONE_TST, TEXT("GBST: Near end of buffer..."));
    GBSCompare(indent4, pTestParams, pClientInfo->hDevice, 50, 394, 6, &bNoCrit);
    if (bNoCrit == FALSE)
    {
        LogDebug(indent4, SDIO_ZONE_TST, TEXT("GBST: Bailing..."));
        goto DONE;
    }
    //2Invalid
    LogDebug(indent2, SDIO_ZONE_TST, TEXT("GBST: Non-Valid Cases..."));
    //3 0 bits
    LogDebug(indent3, SDIO_ZONE_TST, TEXT("GBST: Zero Bits..."));
    GBSCompare(indent4, pTestParams, pClientInfo->hDevice, 4, 1, 0, &bNoCrit);
    if (bNoCrit == FALSE)
    {
        LogDebug(indent4, SDIO_ZONE_TST, TEXT("GBST: Bailing..."));
        goto DONE;
    }

DONE:

    LogDebug(indent, SDIO_ZONE_TST, TEXT("GBST: Exiting Driver Test Function..."));
}

void SDOutputBufferTest(UINT indent, PTEST_PARAMS  pTestParams,  PSDCLNT_CARD_INFO pClientInfo)
{
    UNREFERENCED_PARAMETER(indent);
    UNREFERENCED_PARAMETER(pClientInfo);
#ifdef DEBUG
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    UINT indent3 = indent + 3;
    PREFAST_ASSERT(indent3 > indent);
    PUCHAR lpstr = NULL;
    LPTSTR lptstrSim = NULL;
    BOOL bSuccess = TRUE;
    YesNoText YNT;
    int iRes = 0;
    UINT bl = 40;
    LogDebug(indent, SDIO_ZONE_TST, TEXT("OutBuffTst: Entering Driver Test Function..."));
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("OutBuffTst: Creating random buffer to dump..."));
//  bSuccess = AllocBuffer(indent2, bl, BUFF_ALPHANUMERIC, &lpstr, pTestParams->dwSeed);
    bSuccess = AllocBuffer(indent2, bl, BUFF_RANDOM, &lpstr, pTestParams->dwSeed);
    if (bSuccess == FALSE)
    {
        LogDebug(indent3, SDIO_ZONE_TST, TEXT("InitTst: Bailing..."));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Failure allocation memory for buffer to dump. This is probably due to a  lack of memory..."));
        goto DONE;
    }
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("OutBuffTst: Generating simulation for output dumped via SDOutputBuffer ..."));
    bSuccess = GenerateOutputSim(indent2, pTestParams, lpstr, bl, &lptstrSim);
    if (!bSuccess)
    {
        LogDebug(indent3, SDIO_ZONE_TST, TEXT("OutBuffTst: Bailing..."));
        goto DONE;
    }

    LogDebug(indent1, SDIO_ZONE_TST, TEXT("OutBuffTst: Dumping buffer to debugger with SDOutputBuffer..."));
    wrap_SDOutputBuffer(lpstr, bl);
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("OutBuffTst: Dumping simulation text to dialog box..."));
    ZeroMemory (&YNT, sizeof(YesNoText));
    YNT.question = TEXT("Does the test in the box below match what is displayed in the debugging window?");
    YNT.text = lptstrSim;
    YNT.time = 60;
    YNT.msgRate = 5;
    iRes = DialogBoxParam(pClientInfo->hInst, MAKEINTRESOURCE(IDD_YesNo), NULL, YesNoProc, (LPARAM)&YNT);
    if (iRes == -1)
    {
        LogFail(indent2, TEXT("OutBuffTst: The Dialog box failed. Are you sure you have a display?"));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("DialogBoxParam failed."));
    }
    else if (iRes == 0)
    {
        LogFail(indent2, TEXT("OutBuffTst: The user said that the output of SDOutputBuffer in the debugger did not match the output in the dialog box."));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The user indicated that the output of SDOutputBuffer in the debugger did not match the output in the dialog box."));
    }
DONE:
    DeAllocBuffer(&lpstr);
    DeAllocTCString(&lptstrSim);
    GenerateSucessMessage(pTestParams);
    LogDebug(indent, SDIO_ZONE_TST, TEXT("OutBuffTst: Exiting Driver Test Function..."));
#endif DEBUG
#ifndef DEBUG
        AppendMessageBufferAndResult(pTestParams, TPR_SKIP, TEXT("This test has been compiled under retail. As it is testing a debugging API, the test will be skipped."));
#endif // !DEBUG
}

void GetBusNamePrefixTest(UINT /*indent*/, PTEST_PARAMS /*pTestParams*/, PSDCLNT_CARD_INFO /*pClientInfo*/)
{
}
