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
#ifndef CMN_DRV_TRANS_H
#define CMN_DRV_TRANS_H

#include <windows.h>
#include <sdcardddk.h>
#include "sdtst.h"
#include <sd_tst_cmn.h>

LPCTSTR TranslateCommandCodes(UCHAR code, BOOL bAppCMD=FALSE);

LPCTSTR TranslateErrorCodes(SD_API_STATUS stat);

LPCTSTR TranslateState(DWORD dwState);

LPCTSTR TranslateInfoTypes(SD_INFO_TYPE it);

LPCTSTR TranslateTransferClass(SD_TRANSFER_CLASS tc);

LPCTSTR TranslateResponseType(SD_RESPONSE_TYPE rt);

LPCTSTR TranslateBusReqFlags(DWORD dwFlags);

LPCTSTR TranslateDeviceType(SDTST_DEVICE_TYPE devType);

LPCTSTR TransState(SD_CARD_STATUS cardStatus);

LPCTSTR GetNumberSuffix(UINT i);

LPCTSTR Supported(BOOL b);

LPCTSTR Supported(UCHAR uc);

LPCTSTR YesNo(SD_CARD_STATUS input, DWORD setBit);

LPCTSTR YesNo(SD_IOCARD_STATUS input, UCHAR setBit);

LPCTSTR YesNo(BOOL b);

LPCTSTR TransBOOLTF(BOOL b);

LPCTSTR TranslateTupleCode(DWORD dwCode);

LPCTSTR TranslateInterfaceMode(SD_INTERFACE_MODE_EX im);

UINT GetInterfaceValue(SD_INTERFACE_MODE_EX im);

void SetInterfaceMode(SD_INTERFACE_MODE_EX& im /*set*/, SD_INTERFACE_MODE inMode);

SD_INTERFACE_MODE GetInterfaceMode(SD_INTERFACE_MODE_EX im);

void DumpStatus(UINT indent, SD_CARD_STATUS cardStatus, DWORD st = STAT_NORMAL);

void DumpStatusRegister(UINT indent, SD_DEVICE_HANDLE hDevice, PTEST_PARAMS pTP);

void DumpStatusFromResponse(UINT indent, PSD_COMMAND_RESPONSE psdResp);

void DumpAltStatus(UINT indent, PSD_PARSED_REGISTER_ALT_STATUS pAltStat);

BOOL ParseAltStatus(const UCHAR *pBits, PSD_PARSED_REGISTER_ALT_STATUS pAltStat);

LPCTSTR TranslateCardFeature(SD_SET_FEATURE_TYPE ft);

LPCTSTR TranslateIOTransferType(SD_IO_TRANSFER_TYPE tt);

void DumpArgument(DWORD dwArg, UCHAR ucCmdCode, LPCTSTR txt);

#endif //CMN_DRV_TRANS_H

