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
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2002 BSQUARE Corporation. All rights reserved.
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
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
#include <SDCardDDK.h>
#include <sdcard.h>
#include "br_hlp.h"
#include <sddrv.h>

BOOL VerifySetCardFeatureCR(UINT indent, PTEST_PARAMS  pTestParams, SD_DEVICE_HANDLE hDevice, PSD_CARD_INTERFACE_EX pciSet, PSD_CARD_INTERFACE_EX pciExpect, LPCTSTR msg,...)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    UINT indent3 = indent + 3;
    PREFAST_ASSERT(indent3 > indent);
    BOOL                        bNoCriticalErr = TRUE;
    ULONG                       ulTmp = 0;
    SD_CARD_INTERFACE_EX           ciGet;
    SD_API_STATUS               rStat = 0;
//  BOOL bVal;
    TCHAR buff[10];
    va_list pVal;
    TCHAR buff2[257];

    va_start(pVal, msg);
    ZeroMemory(&ciGet, sizeof(SD_CARD_INTERFACE_EX));
    ZeroMemory(buff, 10* sizeof(TCHAR));
    ZeroMemory(buff2, 257* sizeof(TCHAR));
    StringCchVPrintf(buff2, _countof(buff2), msg, pVal);
//1 Setting Clock Rate to Specified Value
    ZeroMemory(&ciGet, sizeof(SD_CARD_INTERFACE_EX));
    ulTmp = pciSet->ClockRate;
    LogDebug(indent, SDIO_ZONE_TST, TEXT("Verify_SCF: %s"), buff2);
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("Verify_SCF: Setting Clock Rate to %u Hz"), pciSet->ClockRate);
    rStat = wrap_SDSetCardFeature(hDevice, SD_SET_CARD_INTERFACE_EX, pciSet, sizeof(SD_CARD_INTERFACE_EX));
    if (!SD_API_SUCCESS(rStat))
    {
        LogFail(indent2, TEXT("Verify_SCF: SDSetCardFeature Failed. Error = %s (0x%x)"),TranslateErrorCodes(rStat), rStat);
        SetMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("SDSetCardFeature failed when a clock rate was set at %u Hz. Error = %s (0x%x)."), ulTmp, TranslateErrorCodes(rStat), rStat);
        bNoCriticalErr = FALSE;
        goto DONE;
    }

    LogDebug(indent1, SDIO_ZONE_TST, TEXT("Verify_SCF: Getting Card Interface via SDCardInfoQuery..."), pciSet->ClockRate);
    rStat = wrap_SDCardInfoQuery(hDevice, SD_INFO_CARD_INTERFACE_EX, &ciGet, sizeof(SD_CARD_INTERFACE_EX));
    if (!SD_API_SUCCESS(rStat))
    {
        LogFail(indent2, TEXT("Verify_SCF: SDCardInfoQuery Failed. Error = %s (0x%x)"),TranslateErrorCodes(rStat), rStat);
        SetMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("SDSetCardFeature failed when a clock rate was set at %u Hz. Error = %s (0x%x)."), ulTmp, TranslateErrorCodes(rStat), rStat);
        bNoCriticalErr = FALSE;
        goto DONE;
    }
    LogDebug(indent2, SDIO_ZONE_TST, TEXT("Verify_SCF: Checking returned Card Interface..."), pciSet->ClockRate);
    if (ciGet.ClockRate != pciExpect->ClockRate)
    {
        LogFail(indent3, TEXT("Verify_SCF: The Clock Rate returned by SDCardInfoQuery was not adjusted to the expected value. Clock Rate Retuned = %u Hz, and Clock Rate Expected= %u Hz"), ciGet.ClockRate, pciExpect->ClockRate);
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The Clock Rate returned by SDCardInfoQuery was not adjusted to the expected value. Clock Rate Retuned = %u Hz, and Clock Rate Expected= %u Hz"), ciGet.ClockRate, pciExpect->ClockRate);
//      bNoCriticalErr = FALSE;
    }
        else
    {
        LogDebug(indent3, SDIO_ZONE_TST, TEXT("Verify_SCF: The Clock Rate returned by SDCardInfoQuery went to %u Hz as expected"), pciSet->ClockRate);
    }

DONE:
    return bNoCriticalErr;
}

ULONGLONG GetDiff(LARGE_INTEGER li0, LARGE_INTEGER li1)
{
    ULONGLONG ull0, ull1, ullOut;
    ull0 = (ULONGLONG) li0.QuadPart;
    ull1 = (ULONGLONG) li1.QuadPart;
    if (ull0 <= ull1)
    {
        ullOut = ull1 - ull0;
    }
    else
    {
        ullOut = (0xffffffffffffffff  - ull0) + ull1;
    }
    return ullOut;
}

DWORD GetTime(LARGE_INTEGER listart, LARGE_INTEGER listop, LARGE_INTEGER lifreq, PBOOL pbSuccess)
{
    DWORD dwRet = 0;
    ULONGLONG ulldiff;
    ULONGLONG ulltime;
    if (pbSuccess == NULL)
    {
        goto DONE;
    }
    *pbSuccess = FALSE;
    if (lifreq.QuadPart == 0)
    {
        goto DONE;
    }
    ulldiff = GetDiff(listart, listop);
    ulltime = (ulldiff * 1000000) / ((ULONGLONG)(lifreq.QuadPart));
    if ((ulltime >> 32) == 0)
    {

        dwRet = (DWORD) (ulltime & 0xffffffff);
        *pbSuccess = TRUE;
        return dwRet;
    }
DONE:
    return dwRet;
}

BOOL TimeWrites(UINT indent, SD_DEVICE_HANDLE hDevice, __out_ecount(dwArrSize) LPDWORD pdwTimeArray, DWORD dwArrSize, PUCHAR pbuff, DWORD buffSize, PTEST_PARAMS pTestParams)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    BOOL bSuccess = FALSE;
    DWORD c;
//  SD_API_STATUS rStat[20];
    SD_API_STATUS rStat;
    LARGE_INTEGER lifreq, listart, listop;
    lifreq.QuadPart = 0;
    listart.QuadPart = 0;
    listop.QuadPart = 0;
    DWORD dwNumBlocks;
    PSD_COMMAND_RESPONSE pResp = NULL;
    DWORD dwTime;
    LogDebug(indent, SDIO_ZONE_TST, TEXT("TimeWrites: Checking Input Parameters..."));

    if ( (hDevice == NULL) || (pdwTimeArray == NULL) || (dwArrSize == 0) || (pbuff == NULL) || (buffSize == 0) )
    {
        LogFail(indent1, TEXT("TimeWrites: You have input invalid parameters. Test Error. Bailing..."));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("You have input invalid parameters in the Time Writes function. This is a test error. Unable to proceed"));
        goto DONE;
    }
    LogDebug(indent, SDIO_ZONE_TST, TEXT("TimeWrites: Calculating number of blocks..."));
    dwNumBlocks= buffSize / 512;//min((buffSize / 512), 30);
    if (dwNumBlocks == 0)
    {
        LogFail(indent1, TEXT("TimeWrites: You have input a buffer that is smaller than one block (512 byte). This function requires that the buffer be at least one full block. Bailing..."));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("You have input a buffer that is smaller than one block (512 byte). This function requires that the buffer be at least one full block. Unable to proceed"));
        goto DONE;
    }
    LogDebug(indent, SDIO_ZONE_TST, TEXT("TimeWrites: Getting the frequency of the performance counter..."));
    bSuccess = QueryPerformanceFrequency(&lifreq);
    if (bSuccess == FALSE)
    {
        LogFail(indent1, TEXT("TimeWrites: The performance counter does not appear to be supported on this platform. Bailing..."));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("It looks like there is no OEM support for a performance counter on this platform."));
        goto DONE;
    }
    LogDebug(indent, SDIO_ZONE_TST, TEXT("TimeWrites: Allocating the response buffer ..."));
    AllocateResponseBuff(indent1, pTestParams, &pResp, FALSE);
    LogDebug(indent, SDIO_ZONE_TST, TEXT("TimeWrites: Performing 1 writes into the %u first blocks of memory %u times..."), dwNumBlocks, dwArrSize);
    for (c = 0; c < dwArrSize; c++)
    {
        LogDebug(indent1, SDIO_ZONE_TST, TEXT("TimeWrites: Multi-block Write Number %u (Note: during the write  there will be no debug logging (including wrappers) from the test to avoid slowing down the operation ..."), c);
        QueryPerformanceCounter(&listart);
/*      for (k = 0; k < dwNumBlocks; k++)
        {
//          rStat[k] = wrap_SDSynchronousBusRequest(hDevice, SD_CMD_WRITE_BLOCK, k * 512, SD_WRITE, ResponseR1, pResp, 1, 512, pbuff + (k*512), 0);
            rStat[k] = SDSynchronousBusRequest(hDevice, SD_CMD_WRITE_BLOCK, k * 512, SD_WRITE, ResponseR1, pResp, 1, 512, pbuff + (k*512), 0);
        }*/
//      rStat = wrap_SDSynchronousBusRequest(hDevice, SD_CMD_WRITE_MULTIPLE_BLOCK, 0, SD_WRITE, ResponseR1, pResp, dwNumBlocks, 512, pbuff, SD_AUTO_ISSUE_CMD12);
        rStat = SDSynchronousBusRequest(hDevice, SD_CMD_WRITE_MULTIPLE_BLOCK, 0, SD_WRITE, ResponseR1, pResp, dwNumBlocks, 512, pbuff, SD_AUTO_ISSUE_CMD12);

        QueryPerformanceCounter(&listop);
        bSuccess = TRUE;
/*      LogDebug(indent1, SDIO_ZONE_TST, TEXT("TimeWrites: Checking results of bus requests for Write Cycle %u..."), c);
        for (k = 0; k < dwNumBlocks; k++)
        {
            if (!SD_API_SUCCESS(rStat[k]))
            {
                LogFail(indent2, TEXT("TimeWrites: The %u write failed. SDSynchronousBusRequest returned %s (0x%x)."), k, TranslateErrorCodes(rStat[k]), rStat[k]);
                AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The  write %u failed in  write cycle %u. SDSynchronousBusRequest returned %s (0x%x)."), k, c, TranslateErrorCodes(rStat[k]), rStat[k]);
                bSuccess = FALSE;
            }
        }*/
        LogDebug(indent1, SDIO_ZONE_TST, TEXT("TimeWrites: Checking results of bus request for Write Number %u..."), c);
        if (!SD_API_SUCCESS(rStat))
        {
            LogFail(indent2, TEXT("TimeWrites: The multi-block write failed. SDSynchronousBusRequest returned %s (0x%x)."), TranslateErrorCodes(rStat), rStat);
            AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The  multi-block write %u failed. SDSynchronousBusRequest returned %s (0x%x)."), c, TranslateErrorCodes(rStat), rStat);
            bSuccess = FALSE;
        }

        if (bSuccess == FALSE)
        {
            goto DONE;
        }
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("TimeWrites: The multi-block write number %u succeeded."), dwArrSize);
        LogDebug(indent1, SDIO_ZONE_TST, TEXT("TimeWrites: Calculating time to perform  multi-block write of %u blocks..."), dwNumBlocks);
        dwTime = GetTime(listart, listop, lifreq, &bSuccess);
        if (bSuccess == FALSE)
        {
            LogFail(indent2, TEXT("TimeWrites: Unable to calculate the time for the %u block write. This is probably due to the number of bits in the start and stop time differences exceeding 32 bits. Bailing..."), dwNumBlocks);
            AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Unable to calculate the time for the %u bus requests. This is probably due to the number of bits in the start and stop time differences exceeding 32 bits.\nIt could also be due to a parameter error in the function call."), dwNumBlocks);
            goto DONE;
        }
        pdwTimeArray[c] = dwTime;
        dwTime = 0;
    }
DONE:
    if (pResp)
    {
        free(pResp);
        pResp = NULL;
    }
    return bSuccess;
}

DWORD DWAvg(DWORD const *lpdwArray, DWORD dwSize)
{
    DWORD c;
    DWORD dwHigh = 0;
    DWORD dwLow = 0;
    DWORD dwSum = 0;
    DWORD dwRet = 0;
    if (lpdwArray == NULL) return 0;
    if (dwSize  == 0) return 0;
    else if (dwSize > 4)
    {
        if (lpdwArray[0] > lpdwArray[1] )
        {
            dwHigh = lpdwArray[0];
            dwLow = lpdwArray[1];
        }
        else
        {
            dwHigh = lpdwArray[1];
            dwLow = lpdwArray[0];
        }
        dwSum = dwHigh + dwLow;
        for (c = 2; c < dwSize; c++)
        {
            dwSum = dwSum + lpdwArray[c];
            if (lpdwArray[c] > dwHigh) dwHigh = lpdwArray[c];
            else if (lpdwArray[c] < dwLow) dwLow = lpdwArray[c];
        }
        dwSum = dwSum - (dwHigh + dwLow);
        dwRet = (dwSum / (dwSize - 2));
    }
    else
    {
        for (c = 0; c < dwSize; c++)
        {
            dwSum = dwSum + lpdwArray[c];
        }
        dwRet =  (dwSum / dwSize);
    }
    return dwRet;
}

BOOL CompareVals(DWORD dwFirst, DWORD dwLast, DWORD dwPlusMinus)
{
    if (dwFirst > dwLast)
    {
        if (( dwFirst - dwLast) < dwPlusMinus) return TRUE;
    }
    else
    {
        if (( dwLast - dwFirst) < dwPlusMinus) return TRUE;
    }
    return FALSE;
}

SD_API_STATUS GetBusWidthsSupported(UINT indent, SD_DEVICE_HANDLE hDevice, PTEST_PARAMS pTestParams, PSD_COMMAND_RESPONSE pResp, PUCHAR pBusWidths)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    SD_API_STATUS               rStat = 0;
    SD_CARD_RCA                 RCA;
    PUCHAR                      pSCR_Bits = NULL;

    LogDebug(indent, SDIO_ZONE_TST, TEXT("MultiBusSupport: Prechecking input parameters..."));
    if (pBusWidths == NULL)
    {
        LogFail(indent1,TEXT("MultiBusSupport: pBusWidths = NULL."));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("Test Error: Invalid parameter pBusWidths = NULL. Cannot proceed."));
        rStat = SD_API_STATUS_INVALID_PARAMETER;
        goto DONE;
    }

    //2 First allocate memory for SCR bits

    LogDebug(indent, SDIO_ZONE_TST, TEXT("MultiBusSupport: Allocating Memory for SCR Register bits..."));
    pSCR_Bits = (PUCHAR)malloc(sizeof(UCHAR)*SD_SCR_REGISTER_SIZE);
    if (!pSCR_Bits)
    {
        LogFail(indent1,TEXT("MultiBusSupport: Insufficient memory to get the contents of the SCR Register."));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("Unable to allocate memory for a buffer to contain the raw SCR bits. Cannot proceed."));
        rStat = SD_API_STATUS_NO_MEMORY;
        goto DONE;
    }
    ZeroMemory(pSCR_Bits, sizeof(UCHAR)*SD_SCR_REGISTER_SIZE);

    //2 Next Determine if Multiple Bus Widths are supported
    LogDebug(indent, SDIO_ZONE_TST, TEXT("MultiBusSupport: Determining if 4 bit width is supported..."));
    //3 Get the RCA
    rStat = GetRelativeAddress(indent1, hDevice, pTestParams, &RCA);
    if (!SD_API_SUCCESS(rStat))
    {
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("MultiBusSupport: Bailing..."));
        goto DONE;
    }

    //3 Send the a request to go to the application commands

    rStat = AppCMD(indent1, hDevice, pTestParams, pResp, RCA);
    if (!SD_API_SUCCESS(rStat))
    {
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("MultiBusSupport: Bailing..."));
        goto DONE;
    }

    //3 Send a request to get the SCR register
    LogDebug(indent, SDIO_ZONE_TST, TEXT("MultiBusSupport: Sending bus request to get SCR register..."));
    rStat = wrap_SDSynchronousBusRequest(hDevice, SD_ACMD_SEND_SCR, 0, SD_READ, ResponseR1, pResp, 1, SD_SCR_REGISTER_SIZE, pSCR_Bits, 0, TRUE);
    if (!SD_API_SUCCESS(rStat))
    {
        LogFail(indent1,TEXT("MultiBusSupport: Unable to get the card's SD Configuration Register. Error Code: %s (0x%x) returned."),TranslateErrorCodes(rStat), rStat);
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT("Failure to get the SD Card's SCR register. Without it, one cannot tell if 4 bit bus widths are supported. Error %s (0x%x) returned."), TranslateErrorCodes(rStat), rStat);
        DumpStatusFromResponse(indent2, pResp);
        goto DONE;
    }
    LogDebug(indent, SDIO_ZONE_TST, TEXT("MultiBusSupport: Reading bit that indicates if 4 bit bus widths are supported..."));
    //3 Determine what widths are supported
    *pBusWidths = pSCR_Bits[1] & 0xf;
DONE:
    if (pSCR_Bits)
    {
        free(pSCR_Bits);
        pSCR_Bits = NULL;
    }
    return rStat;
}

SD_INTERFACE_MODE HighestBusWidthSupportAtHost(UINT indent, SD_DEVICE_HANDLE hDevice, PTEST_PARAMS pTestParams, SD_API_STATUS *pRStat)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    SD_API_STATUS rStat;
    SD_CARD_INTERFACE_EX ci;

    ZeroMemory(&ci, sizeof(SD_CARD_INTERFACE_EX));
    LogDebug(indent, SDIO_ZONE_TST, TEXT("MultiBusHostSupport: Getting Card Interface Caps..."));
    rStat = wrap_SDCardInfoQuery(hDevice, SD_INFO_HOST_IF_CAPABILITIES, &ci, sizeof(SD_CARD_INTERFACE_EX));
    if (!SD_API_SUCCESS(rStat))
    {
        LogFail(indent1,TEXT("MultiBusHostSupport: Unable to get the card's Host Interface Capsr.  SDCardInfoQuery returned %s (0x%x)."),TranslateErrorCodes(rStat), rStat);
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL,TEXT(" Unable to get the card's Host Interface Caps. SDCardInfoQuery returned %s (0x%x)."), TranslateErrorCodes(rStat), rStat);
    }
    else
    {
        LogDebug(indent1, SDIO_ZONE_TST, TEXT("HighBusWidthSupportAtHost: Bus Mode supported - %s"), TranslateInterfaceMode(ci.InterfaceModeEx) );
    }
    if (pRStat) *pRStat = rStat;
    return GetInterfaceMode(ci.InterfaceModeEx);
}

