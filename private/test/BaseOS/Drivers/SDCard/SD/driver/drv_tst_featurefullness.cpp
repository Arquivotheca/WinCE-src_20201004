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
#include <sddrv.h>

BOOL GetHostInterface(PSDCLNT_CARD_INFO pClientInfo,PBYTE /*pBufIn*/, DWORD /*dwLenIn*/,PBYTE pBufOut,DWORD dwLenOut,PDWORD pdwActualOut)
{
    BOOL bRet = TRUE;
    if(pdwActualOut)
    {
        *pdwActualOut = 0;
    }
    if(pBufOut == NULL || dwLenOut != sizeof(SD_CARD_INTERFACE_EX))
    {
      SetLastError(ERROR_INVALID_PARAMETER);
      return FALSE;
    }
    PSD_CARD_INTERFACE_EX pCardInterface = (PSD_CARD_INTERFACE_EX)pBufOut;

    DWORD rStat = wrap_SDCardInfoQuery(pClientInfo->hDevice, SD_INFO_HOST_IF_CAPABILITIES, pCardInterface, sizeof(SD_CARD_INTERFACE_EX));

    if(rStat != SD_API_STATUS_SUCCESS)
    {
        bRet = FALSE;
    }

    if(pdwActualOut)
    {
        *pdwActualOut = sizeof(SD_CARD_INTERFACE_EX);
    }
    return bRet;
}

BOOL GetCardInterface(PSDCLNT_CARD_INFO pClientInfo,PBYTE /*pBufIn*/, DWORD /*dwLenIn*/,PBYTE pBufOut,DWORD dwLenOut,PDWORD pdwActualOut)
{
    BOOL bRet = TRUE;
    if(pdwActualOut)
    {
        *pdwActualOut = 0;
    }
    if(pBufOut == NULL || dwLenOut != sizeof(SD_CARD_INTERFACE_EX))
    {
      SetLastError(ERROR_INVALID_PARAMETER);
      return FALSE;
    }
    PSD_CARD_INTERFACE_EX pCardInterface = (PSD_CARD_INTERFACE_EX)pBufOut;

    DWORD rStat = wrap_SDCardInfoQuery(pClientInfo->hDevice, SD_INFO_CARD_INTERFACE_EX, pCardInterface, sizeof(SD_CARD_INTERFACE_EX));

     if(rStat != SD_API_STATUS_SUCCESS)
    {
        bRet = FALSE;
    }

    if(pdwActualOut)
    {
        *pdwActualOut = sizeof(SD_CARD_INTERFACE_EX);
    }
    return bRet;
}
