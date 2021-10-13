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
#include <sdcommon.h>
#include <SDCardDDK.h>
#include <sdcard.h>
#include <sd_tst_cmn.h>
#include <sddrv.h>

#define OUTPUT_BYTES_PER_LINE 16

void buildLine(UINT lineNum, __out_ecount(lineLength) LPTSTR line, DWORD lineLength, LPCTSTR hexStr, LPCTSTR charStr)
{
    UINT c;
    UINT len = 0;

    TCHAR tcHex[OUTPUT_BYTES_PER_LINE * 3];
    TCHAR tcChar[OUTPUT_BYTES_PER_LINE + 1];
    if ((line == NULL) || (lineLength == 0))
    {
        goto DONE;
    }
    #pragma prefast(suppress:419, "Not a bug.  ZeroMemory will not cause overflow.")
    ZeroMemory(line, lineLength*sizeof(TCHAR));
    if ((hexStr ==  NULL) || (charStr ==  NULL))
    {
        goto DONE;
    }
    ZeroMemory(tcHex, (OUTPUT_BYTES_PER_LINE * 3) * sizeof(TCHAR));
    ZeroMemory(tcChar, (OUTPUT_BYTES_PER_LINE + 1) * sizeof(TCHAR));

    StringCchCopyN(tcHex, _countof(tcHex), hexStr + (lineNum * (OUTPUT_BYTES_PER_LINE * 3) ), (OUTPUT_BYTES_PER_LINE *3) - 1);
    StringCchCopyN(tcChar, _countof(tcChar), charStr + (lineNum * OUTPUT_BYTES_PER_LINE ), OUTPUT_BYTES_PER_LINE);
//    _stprintf(line, TEXT(" 0x%08X  %- 47s     %- 16s \r\n"),(lineNum * OUTPUT_BYTES_PER_LINE ), tcHex, tcChar);
//    _stprintf(line, TEXT(" 0x%08X  %- 47s\t%- 16s \r\n"),(lineNum * OUTPUT_BYTES_PER_LINE ), tcHex, tcChar);
    len = _tcslen(tcChar);
    switch (len)
    {
        case 1:
            __fallthrough;
        case 2:
            __fallthrough;
        case 3:
            __fallthrough;
        case 4:
            __fallthrough;
        case 5:
            StringCchPrintf(line, lineLength, TEXT(" 0x%08X  %- 47s\t\t\t%- 16s \r\n"), (lineNum * OUTPUT_BYTES_PER_LINE ), tcHex, tcChar);
            break;
        case 6:
            __fallthrough;
        case 7:
            __fallthrough;
        case 8:
            __fallthrough;
        case 9:
            __fallthrough;
        case 10:
            __fallthrough;
        case 11:
            __fallthrough;
        case 12:
            __fallthrough;
        case 13:
            StringCchPrintf(line, lineLength, TEXT(" 0x%08X  %- 47s\t\t%- 16s \r\n"), (lineNum * OUTPUT_BYTES_PER_LINE ), tcHex, tcChar);
            break;
        case 14:
            __fallthrough;
        case 15:
            __fallthrough;
        case 16:
            StringCchPrintf(line, lineLength, TEXT(" 0x%08X  %- 47s\t%- 16s \r\n"), (lineNum * OUTPUT_BYTES_PER_LINE ), tcHex, tcChar);
    }
//    _stprintf(line, TEXT(" 0x%08X\t%s\t\t%s\r\n"),(lineNum * OUTPUT_BYTES_PER_LINE ), tcHex, tcChar);
DONE:
    c = 0;
}

BOOL GenerateOutputSim(UINT indent, PTEST_PARAMS  pTestParams, PUCHAR inBuff, DWORD inBuffSize, LPTSTR *ptszOut)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    LPTSTR lptstrhex = NULL;
    LPTSTR lptstrchar = NULL;
    LPTSTR lptstr = NULL;
    LPTSTR lptstrline = NULL;
    BOOL bRet = FALSE;
    DWORD buffSize;
    DWORD numLines; //number of whole lines (16 chars)
    UINT c;

    LogDebug(indent, SDIO_ZONE_TST, TEXT("GenSim: Generating simulation text foe SDOutputBuffer..."));
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("GenSim: Verifying input parameters..."));
    if (inBuff == NULL)
    {
        LogFail(indent2,TEXT("GenSim: inBuff == NULL, Bailing"));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Test error. Bad parameter passed into GenerateOutputSim inBuff = NULL"));
        goto DONE;
    }
    if (inBuffSize == 0)
    {
        LogFail(indent2,TEXT("GenSim: inBuffSize == 0, Bailing"));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Test error. Bad parameter passed into GenerateOutputSim inBuffSize = 0"));
        goto DONE;
    }
    if (ptszOut == NULL)
    {
        LogFail(indent2,TEXT("GenSim: *ptszOut == NULL, Bailing"));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Test error. Bad parameter passed into GenerateOutputSim *ptszOut = NULL"));
        goto DONE;
    }
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("GenSim: Allocating buffers..."));
    if (AllocateTCString(indent2, STRING_HEX, inBuff, inBuffSize, &lptstrhex) == FALSE)
    {
        LogFail(indent2,TEXT("GenSim: Failure Allocating necessary TCHAR string of %u bytes, this is most likely due to a lack of available memory, bailing..."), (inBuffSize * 3 * sizeof(TCHAR)));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Memory allocation failure due to a probable lack of memory.\nUnable to create a necessary TCHAR buffer for SDOutputDebug simulation"));
        goto DONE;
    }
    if (AllocateTCString(indent1, STRING_TEXT, inBuff, inBuffSize, &lptstrchar) == FALSE)
    {
        LogFail(indent2,TEXT("GenSim: Failure Allocating necessary TCHAR string of %u bytes, this is most likely due to a lack of available memory, bailing..."), ((inBuffSize + 1)* sizeof(TCHAR)));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Memory allocation failure due to a probable lack of memory.\nUnable to create a necessary TCHAR buffer for SDOutputDebug simulation"));
        goto DONE;
    }
    if (AllocateTCString(indent1, STRING_ZERO, NULL, 256, &lptstrline) == FALSE)
    {
        LogFail(indent2,TEXT("GenSim: Failure Allocating necessary TCHAR string of %u bytes, this is most likely due to a lack of available memory, bailing..."), (256 * sizeof(TCHAR)));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Memory allocation failure due to a probable lack of memory.\nUnable to create a necessary TCHAR buffer for SDOutputDebug simulation"));
        goto DONE;
    }
    numLines = dwRndUp(inBuffSize, OUTPUT_BYTES_PER_LINE);
    buffSize = 256 * (numLines+ 9);
    if (AllocateTCString(indent1, STRING_ZERO, NULL, buffSize, &lptstr) == FALSE)
    {
        LogFail(indent2,TEXT("GenSim: Failure Allocating necessary TCHAR string of %u bytes, this is most likely due to a lack of available memory, bailing..."), (buffSize * sizeof(TCHAR)));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Memory allocation failure due to a probable lack of memory.\nUnable to create a necessary TCHAR buffer for SDOutputDebug simulation"));
        goto DONE;
    }
    bRet = TRUE;
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("GenSim: Filling simulation buffer..."));
    StringCchCopy(lptstr, buffSize, TEXT("\r\n\r\n---------------------------------------------------------------------------------------- \r\n"));
    StringCchPrintf(lptstrline, 256, TEXT("\r\n SDOutputBuffer: 0x%08p , Size: %u bytes \r\n\r\n"),(PVOID)inBuff, inBuffSize);
    StringCchCopy(lptstr, buffSize, lptstrline);
    for (c  = 0; c < (numLines); c++)
    {
        buildLine(c, lptstrline, 256, lptstrhex, lptstrchar);
        StringCchCopy(lptstr, buffSize, lptstrline);
    }
    StringCchCopy(lptstr, buffSize, TEXT("\r\n\r\n---------------------------------------------------------------------------------------- \r\n"));

DONE:
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("GenSim: Cleaning up..."));
    if (ptszOut) *ptszOut = lptstr;
    DeAllocTCString(&lptstrhex);
    DeAllocTCString(&lptstrchar);
    DeAllocTCString(&lptstrline);
    return bRet;
}

// Shifts pbInput down by dwBitOffset.
static
VOID
ShiftBytes(BYTE const *pbInput, ULONG cbInput, DWORD dwBitOffset, __out_ecount(sizeof(DWORD)) PBYTE pbOutput)
{
    PREFAST_DEBUGCHK(pbInput);
    PREFAST_DEBUGCHK(pbOutput);

    DWORD dwByteIndex = dwBitOffset / 8;
    dwBitOffset %= 8;

    DWORD dwRemainderShift = 8 - dwBitOffset;

    // Only copy 4 bytes max.
    DWORD dwEndIndex = min(dwByteIndex + sizeof(DWORD), cbInput);
    DWORD dwCurrOutputIndex = 0;
    while (dwByteIndex < dwEndIndex && dwCurrOutputIndex < sizeof(DWORD)) {
        DEBUGCHK(dwCurrOutputIndex < sizeof(DWORD));
        DEBUGCHK(dwByteIndex < cbInput);

        pbOutput[dwCurrOutputIndex] = pbInput[dwByteIndex] >> dwBitOffset;

        ++dwByteIndex;

        if (dwByteIndex != cbInput) {
            BYTE bTemp = pbInput[dwByteIndex];
            bTemp <<= dwRemainderShift;
            pbOutput[dwCurrOutputIndex] |= bTemp;
        }

        ++dwCurrOutputIndex;
    }
}

DWORD myGetBits(UINT indent, UCHAR const *pBuff, DWORD buffSize, DWORD offset, UCHAR numBits, PBOOL pbSuccess)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    DWORD dwRet = 0;
    DWORD numBytes = 0;
    BOOL bSuccess = FALSE;
//1 Precheck
    LogDebug(indent, SDIO_ZONE_TST, TEXT("GetBitsSim: Checking input parameters..."));

    if (pBuff == NULL)
    {
        LogDebug(indent1, SDIO_ZONE_TST, TEXT("GetBitsSim: pBuff = NULL. One cannot get bits from a NULL buffer. Bailing..."));
        goto DONE;
    }
    if (numBits > 32)
    {
        LogDebug(indent1, SDIO_ZONE_TST, TEXT("GetBitsSim: Too many bits are requested. NumBits = %u. max value = 32. Bailing..."), numBits);
        goto DONE;
    }
    if (numBits == 0)
    {
         LogDebug(indent1, SDIO_ZONE_TST, TEXT("GetBitsSim:  NumBits = 0. Bailing..."));
        bSuccess = TRUE;
        goto DONE;
    }
    //SafeDW(dwBitOffset) + SafeDW(numBits)) > (SafeDW(cbBuffer) * 8)
    if ((numBits + offset) > (buffSize * 8) )
    {
        LogDebug(indent1, SDIO_ZONE_TST, TEXT("GetBitsSim:  The bits requested have overrun the available number of bits. offset = %u, numBits = %u, Total # bits (in pBuff) = %u. Bailing..."), offset, numBits, numBytes * 8);
        goto DONE;
    }
    UCHAR rgbShifted[4] = { 0 };

    // Shift the pBuff down by dwBitOffset bits.
    ShiftBytes(pBuff, buffSize, offset, rgbShifted);

    DWORD dwUsedBytes; // How many bytes have valid data.

    if (numBits % 8 == 0) {
        // Return a byte multiple.
        dwUsedBytes = numBits / 8;
    }
    else {
        // Clear the last used byte of upper bits.
        DWORD dwLastByteIndex = (numBits - 1) / 8;
        DWORD dwRemainderShift = 8 - (numBits % 8);
        rgbShifted[dwLastByteIndex] <<= dwRemainderShift;
        rgbShifted[dwLastByteIndex] >>= dwRemainderShift;
        dwUsedBytes = dwLastByteIndex + 1;
    }

    // Clear the unused bytes.
    if (dwUsedBytes != sizeof(rgbShifted)) {
        memset(rgbShifted + dwUsedBytes, 0, sizeof(rgbShifted) - dwUsedBytes);
    }
    memcpy(&dwRet, rgbShifted, sizeof(dwRet));
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("GetBitsSim: DWORD = 0x%x."), dwRet);
    bSuccess = TRUE;

DONE:

    if (pbSuccess)
    {
        *pbSuccess = bSuccess;
    }
    return dwRet;
}

