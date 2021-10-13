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
// ---------------------------------------------------------------------------
//
//
// ---------------------------------------------------------------------------

#include <windows.h>
#include <memory.h>

#include <tapi.h>
#include "mcx.h"
#include "tspi.h"
#include "tspip.h"
#include "config.h"
#include "netui_kernel.h"

//
// Read the COMMPROPs to get the settings the device allows
//
void
GetSettableFields(
    PTLINEDEV   pLineDev,
    DWORD * pdwSettableBaud,
    WORD * pwSettableData,
    WORD * pwSettableStopParity
    )
{
    HANDLE hComDev;
    COMMPROP commP;

    *pdwSettableBaud = BAUD_110 | BAUD_300 | BAUD_600 | BAUD_1200 |
                               BAUD_2400 | BAUD_4800 | BAUD_9600 | BAUD_14400 |
                               BAUD_19200 | BAUD_38400 | BAUD_57600 | BAUD_115200;
    *pwSettableData = DATABITS_5 | DATABITS_6 | DATABITS_7 | DATABITS_8;
    *pwSettableStopParity = STOPBITS_10 | STOPBITS_15 | STOPBITS_20 |
                                     PARITY_NONE | PARITY_ODD | PARITY_EVEN |
                                     PARITY_SPACE | PARITY_MARK;

    if (pLineDev->hDevice_r0 != (HANDLE)INVALID_DEVICE) {
        if (!GetCommProperties(pLineDev->hDevice_r0, &commP)) {
            DEBUGMSG(ZONE_ERROR, (TEXT("UNIMODEM:GetSettableFields - GetCommProperties failed %d\n"), GetLastError()));
            return;
        }
    } else {
        if ((hComDev = CreateFile(
                            pLineDev->szDeviceName,
                            GENERIC_READ,
                            0,
                            NULL,
                            OPEN_EXISTING,
                            0,
                            0
                            ))
            != INVALID_HANDLE_VALUE) {
            if (!GetCommProperties(hComDev, &commP)) {
                DEBUGMSG(ZONE_ERROR, (TEXT("UNIMODEM:GetSettableFields - GetCommProperties failed %d\n"), GetLastError()));
                CloseHandle(hComDev);
                return;
            }
            CloseHandle(hComDev);
        } else {
            DEBUGMSG(ZONE_ERROR,
                (TEXT("UNIMODEM:GetSettableFields - CreateFile(%s) failed %d\n"), pLineDev->szDeviceName, GetLastError()));
            return;
        }
    }

    *pdwSettableBaud        = commP.dwSettableBaud;
    *pwSettableData         = commP.wSettableData;
    *pwSettableStopParity   = commP.wSettableStopParity;

}   // GetSettableFields

 
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
BOOL
TSPI_EditMiniConfig(
   HWND        hWndOwner,
   PTLINEDEV   pLineDev,
   PDEVMINICFG lpSettingsIn,
   PDEVMINICFG lpSettingsOut
   )
{
	LINECONFIGDATA	LineConfigData;
	BOOL			fRetVal = FALSE;
    DWORD           dwLen;

	DEBUGMSG (ZONE_MISC, (TEXT("UNIMODEM:+TSPI_EditMiniConfig(0x%X, 0x%X, 0x%X)\r\n"),
						  hWndOwner, lpSettingsIn, lpSettingsOut));

	// Just copy the in to the out as a default
	memcpy ((char *)lpSettingsOut, (char *)lpSettingsIn,
			sizeof(DEVMINICFG));
	
	memset ((char *)&LineConfigData, 0, sizeof(LINECONFIGDATA));
	
	// Move the data over.
	LineConfigData.dwVersion = NETUI_LCD_STRUCTVER;
	LineConfigData.dwBaudRate = lpSettingsIn->dwBaudRate;

    GetSettableFields(
        pLineDev,
    	&(LineConfigData.dwSettableBaud),
    	&(LineConfigData.wSettableData),
        &(LineConfigData.wSettableStopParity)
        );

	LineConfigData.bByteSize = lpSettingsIn->ByteSize;
	LineConfigData.bParity = lpSettingsIn->Parity;
	LineConfigData.bStopBits = lpSettingsIn->StopBits;
	LineConfigData.wWaitBong = lpSettingsIn->wWaitBong;
	LineConfigData.dwCallSetupFailTimer = lpSettingsIn->dwCallSetupFailTimer;
	
	if (lpSettingsIn->dwModemOptions & MDM_BLIND_DIAL) {
		LineConfigData.dwModemOptions |= NETUI_LCD_MDMOPT_BLIND_DIAL;
	}
	if (lpSettingsIn->dwModemOptions & MDM_FLOWCONTROL_SOFT) {
		LineConfigData.dwModemOptions |= NETUI_LCD_MDMOPT_SOFT_FLOW;
	}
	if (lpSettingsIn->dwModemOptions & MDM_FLOWCONTROL_HARD) {
		LineConfigData.dwModemOptions |= NETUI_LCD_MDMOPT_HARD_FLOW;
	}
	if (lpSettingsIn->dwModemOptions & MDM_SPEED_ADJUST) {  // DCC automatic baud rate detection
		LineConfigData.dwModemOptions |= NETUI_LCD_MDMOPT_AUTO_BAUD;
	}
	if (lpSettingsIn->fwOptions & TERMINAL_PRE) {
		LineConfigData.dwTermOptions |= NETUI_LCD_TRMOPT_PRE_DIAL;
	}
	if (lpSettingsIn->fwOptions & TERMINAL_POST) {
		LineConfigData.dwTermOptions |= NETUI_LCD_TRMOPT_POST_DIAL;
	}
	if (lpSettingsIn->fwOptions & MANUAL_DIAL) {
		LineConfigData.dwTermOptions |= NETUI_LCD_TRMOPT_MANUAL_DIAL;
	}

	// Set the maximum length
	LineConfigData.dwModMaxLen = DIAL_MODIFIER_LEN;
	wcscpy (LineConfigData.szDialModifier, lpSettingsIn->szDialModifier);

	
    try {
        if (fRetVal = CallKLineConfigEdit(hWndOwner, &LineConfigData)) {
            // Move the data back.
            lpSettingsOut->wWaitBong = LineConfigData.wWaitBong;
            lpSettingsOut->dwCallSetupFailTimer =
                LineConfigData.dwCallSetupFailTimer;

            lpSettingsOut->dwModemOptions &= ~(MDM_FLOWCONTROL_SOFT|MDM_FLOWCONTROL_HARD|MDM_SPEED_ADJUST|MDM_BLIND_DIAL);
            if (LineConfigData.dwModemOptions & NETUI_LCD_MDMOPT_SOFT_FLOW) {
                lpSettingsOut->dwModemOptions |= MDM_FLOWCONTROL_SOFT;
            }
            if (LineConfigData.dwModemOptions & NETUI_LCD_MDMOPT_HARD_FLOW) {
                lpSettingsOut->dwModemOptions |= MDM_FLOWCONTROL_HARD;
            }
            if (LineConfigData.dwModemOptions & NETUI_LCD_MDMOPT_AUTO_BAUD) {
                lpSettingsOut->dwModemOptions |= MDM_SPEED_ADJUST;
            }
            if (LineConfigData.dwModemOptions & NETUI_LCD_MDMOPT_BLIND_DIAL) {
                lpSettingsOut->dwModemOptions |= MDM_BLIND_DIAL;
            }

            lpSettingsOut->wWaitBong = LineConfigData.wWaitBong;
            lpSettingsOut->dwBaudRate = LineConfigData.dwBaudRate;

            lpSettingsOut->fwOptions &= ~(MANUAL_DIAL|TERMINAL_PRE|TERMINAL_POST);
            if (LineConfigData.dwTermOptions & NETUI_LCD_TRMOPT_MANUAL_DIAL) {
                lpSettingsOut->fwOptions |= MANUAL_DIAL;
            }
            if (LineConfigData.dwTermOptions & NETUI_LCD_TRMOPT_PRE_DIAL) {
                lpSettingsOut->fwOptions |= TERMINAL_PRE;
            }
            if (LineConfigData.dwTermOptions & NETUI_LCD_TRMOPT_POST_DIAL) {
                lpSettingsOut->fwOptions |= TERMINAL_POST;
            }

            lpSettingsOut->ByteSize = LineConfigData.bByteSize;
            lpSettingsOut->StopBits = LineConfigData.bStopBits;
            lpSettingsOut->Parity = LineConfigData.bParity;
            dwLen = wcslen(LineConfigData.szDialModifier);
            if (dwLen > DIAL_MODIFIER_LEN) {
                dwLen = DIAL_MODIFIER_LEN;
                LineConfigData.szDialModifier[DIAL_MODIFIER_LEN] = 0;
            }
            dwLen++; // add in zero terminator
            memcpy(lpSettingsOut->szDialModifier, LineConfigData.szDialModifier, dwLen * sizeof(WCHAR));

        } else {
            DEBUGMSG(ZONE_ERROR, (TEXT("UNIMODEM:TSPI_EditMiniConfig NETUI:CallLineConfigEdit failed!\n")));
        }

    } except(EXCEPTION_EXECUTE_HANDLER) {
        DEBUGMSG(ZONE_ERROR, (TEXT("UNIMODEM:TSPI_EditMiniConfig NETUI:CallLineConfigEdit exception!\n")));
        fRetVal = FALSE;
    }

	DEBUGMSG (ZONE_MISC, (TEXT("UNIMODEM:-TSPI_EditMiniConfig: %d\r\n"), fRetVal));
	return fRetVal;
}
 

//
// The following functions are used to verify setting properties of a DEVMINICONFIG via
// calls to lineDevSpecific.
//

BOOL
IsValidBaudRate(
    DWORD dwBaudRate,
    DWORD dwSettableBaud
    )
{
    //
    // Baud rates and settable bits from winbase.h
    //
    switch (dwBaudRate) {
    case CBR_110:   if (dwSettableBaud & BAUD_110)  return TRUE; break;
    case CBR_300:   if (dwSettableBaud & BAUD_300)  return TRUE; break;
    case CBR_600:   if (dwSettableBaud & BAUD_600)  return TRUE; break;
    case CBR_1200:  if (dwSettableBaud & BAUD_1200) return TRUE; break;
    case CBR_2400:  if (dwSettableBaud & BAUD_2400) return TRUE; break;
    case CBR_4800:  if (dwSettableBaud & BAUD_4800) return TRUE; break;
    case CBR_9600:  if (dwSettableBaud & BAUD_9600) return TRUE; break;
    case CBR_14400: if (dwSettableBaud & BAUD_14400)return TRUE; break;
    case CBR_19200: if (dwSettableBaud & BAUD_19200)return TRUE; break;
    case CBR_38400: if (dwSettableBaud & BAUD_38400)return TRUE; break;
    case CBR_56000: if (dwSettableBaud & BAUD_56K)  return TRUE; break;
    case CBR_57600: if (dwSettableBaud & BAUD_57600)return TRUE; break;
    case CBR_115200:if (dwSettableBaud & BAUD_115200) return TRUE; break;
    case CBR_128000:if (dwSettableBaud & BAUD_128K)   return TRUE; break;
    
    case CBR_256000:
    default: if (dwSettableBaud & BAUD_USER) return TRUE; break;
    }
    return FALSE;
}   // IsValidBaudRate


BOOL
IsValidByteSize(
    DWORD dwByteSize,
    DWORD dwSettableData
    )
{
    //
    // Settable bits from winbase.h
    //
    switch (dwByteSize) {
    case 5: if (dwSettableData & DATABITS_5) return TRUE; break;
    case 6: if (dwSettableData & DATABITS_6) return TRUE; break;
    case 7: if (dwSettableData & DATABITS_7) return TRUE; break;
    case 8: if (dwSettableData & DATABITS_8) return TRUE; break;
    case 16:if (dwSettableData & DATABITS_16) return TRUE; break;
    }
    return FALSE;
}   // IsValidByteSize


BOOL
IsValidParity(
    DWORD dwParity,
    DWORD dwSettableParity
    )
{
    //
    // Parity options and settable bits from winbase.h
    //
    switch (dwParity) {
    case NOPARITY:      if (dwSettableParity & PARITY_NONE) return TRUE; break;
    case ODDPARITY:     if (dwSettableParity & PARITY_ODD)  return TRUE; break;
    case EVENPARITY:    if (dwSettableParity & PARITY_EVEN) return TRUE; break;
    case MARKPARITY:    if (dwSettableParity & PARITY_MARK) return TRUE; break;
    case SPACEPARITY:   if (dwSettableParity & PARITY_SPACE)return TRUE; break;
    }
    return FALSE;
}   // IsValidParity


BOOL
IsValidStopBits(
    DWORD dwStopBits,
    DWORD dwSettableStopBits
    )
{
    //
    // Stop bit values and settable bits from winbase.h
    //
    switch (dwStopBits) {
    case ONESTOPBIT:    if (dwSettableStopBits & STOPBITS_10) return TRUE; break;
    case ONE5STOPBITS:  if (dwSettableStopBits & STOPBITS_15) return TRUE; break;
    case TWOSTOPBITS:   if (dwSettableStopBits & STOPBITS_20) return TRUE; break;
    }
    return FALSE;
}   // IsValidStopBits


BOOL
IsBadStringPtr(
    LPCWSTR lpStr,
    DWORD dwMaxLen
    )
{
    DWORD len;
    BOOL bRet;

    bRet = FALSE;

    try {
        len = wcslen(lpStr);
    } except (EXCEPTION_EXECUTE_HANDLER) {
        DEBUGMSG(ZONE_ERROR, (TEXT("UNIMODEM:IsBadStringPtr: Exception!\n")));        
        bRet = TRUE;
    }
    if (bRet) {
        if (len > dwMaxLen) {
            DEBUGMSG(ZONE_ERROR, (TEXT("UNIMODEM:IsBadStringPtr: String too long %d > %d\n"), len, dwMaxLen));
            bRet = TRUE;
        }
    }
    return bRet;
}   // IsBadStringPtr

#define MAX_DEVICE_CLASS_NAME_LEN 30

//
// Common parameter validation and setup for DevSpecificLineConfigEdit and DevSpecificLineConfigGet
//
LONG
DevSpecificLineConfigProlog(
    PTLINEDEV   pLineDev,
    PUNIMDM_CHG_DEVCFG pChg,
    PDEVMINICFG * ppMiniCfg,
    LPVARSTRING * ppDevCfg
    )
{
    LONG rc = 0;
    WCHAR        szDeviceClass[MAX_DEVICE_CLASS_NAME_LEN];
    PDEVMINICFG  pDevMiniCfg;
    LPVARSTRING  lpDevConfig;

    DEBUGMSG(ZONE_FUNC, (TEXT("UNIMODEM:+DevSpecificLineConfigProlog\n")));

    if (S_OK != StringCchCopy(szDeviceClass, MAX_DEVICE_CLASS_NAME_LEN, pChg->lpszDeviceClass)) {
        DEBUGMSG(ZONE_ERROR, (TEXT("UNIMODEM:DevSpecificLineConfigProlog: Invalid lpszDeviceClass\n")));
        rc = LINEERR_INVALPOINTER;
        goto exitPoint;
    }

    if (!ValidateDevCfgClass(szDeviceClass)) {
        DEBUGMSG(ZONE_ERROR, (TEXT("UNIMODEM:DevSpecificLineConfigProlog: LINEERR_INVALDEVICECLASS\n")));
        rc = LINEERR_INVALDEVICECLASS;
        goto exitPoint;
    }
    
    //
    // Validate the buffer pointers
    //
    lpDevConfig = (LPVARSTRING)pChg->lpDevConfig;
    if (lpDevConfig == NULL) {
        DEBUGMSG(ZONE_ERROR, (TEXT("UNIMODEM:DevSpecificLineConfigProlog: LINEERR_INVALPOINTER\n")));
        rc = LINEERR_INVALPOINTER;
        goto exitPoint;
    }

    pDevMiniCfg = (PDEVMINICFG)(((LPBYTE)lpDevConfig) + sizeof(VARSTRING));

    try {
        //
        // Validate the buffer size
        //
        lpDevConfig->dwNeededSize = sizeof(VARSTRING) + sizeof(DEVMINICFG);
        if (lpDevConfig->dwTotalSize < sizeof(VARSTRING) + sizeof(DEVMINICFG)) {
            DEBUGMSG(ZONE_ERROR, (TEXT("UNIMODEM:DevSpecificLineConfigProlog: LINEERR_STRUCTURETOOSMALL\n")));
            rc = LINEERR_STRUCTURETOOSMALL;
        } else {
            lpDevConfig->dwUsedSize   = sizeof(VARSTRING) + sizeof(DEVMINICFG);
        }

    } except (EXCEPTION_EXECUTE_HANDLER) {
          rc = LINEERR_INVALPOINTER;
    }

    if (rc) {
        goto exitPoint;
    }

    //
    // If they specify version 0, then they want the default devconfig. 
    //
    if (pDevMiniCfg->wVersion == 0) {
        getDefaultDevConfig( pLineDev, pDevMiniCfg );
    }

    if (pLineDev->DevMiniCfg.wVersion != (pDevMiniCfg->wVersion)) {
        DEBUGMSG(ZONE_ERROR, (TEXT("UNIMODEM:DevSpecificLineConfigProlog: Invalid DevConfig\n")));
        rc =  LINEERR_INVALPARAM;
        goto exitPoint;
    }

    *ppDevCfg = lpDevConfig;
    *ppMiniCfg = pDevMiniCfg;

exitPoint:
    return rc;

}   // DevSpecificLineConfigProlog

//
// Function to edit the specific property of a PDEVMINICONFIG
//
LONG
DevSpecificLineConfigEdit(
    PTLINEDEV   pLineDev,
    PUNIMDM_CHG_DEVCFG pChg
    )
{
    DWORD dwSettableBaud;
    DWORD dwSettableData;
    DWORD dwSettableStopParity;
    DWORD rc;

    PDEVMINICFG  pDevMiniCfg;
    LPVARSTRING  lpDevConfig;

    DEBUGMSG(ZONE_FUNC, (TEXT("UNIMODEM:+DevSpecificLineConfigEdit\n")));

    if (rc = DevSpecificLineConfigProlog(pLineDev, pChg, &pDevMiniCfg, &lpDevConfig)) {
        DEBUGMSG(ZONE_ERROR, (TEXT("UNIMODEM:DevSpecificLineConfigEdit: DevSpecificLineConfigProlog failed\n")));
        goto exitPoint;
    }

    switch (pChg->dwOption) {
    case UNIMDM_OPT_BAUDRATE:
    case UNIMDM_OPT_BYTESIZE:
    case UNIMDM_OPT_PARITY:
    case UNIMDM_OPT_STOPBITS:
        dwSettableData = dwSettableStopParity = 0;

        GetSettableFields(
            pLineDev,
            &dwSettableBaud,
            (WORD *)&dwSettableData,
            (WORD *)&dwSettableStopParity
            );
    }

    rc = LINEERR_INVALPARAM;

    switch (pChg->dwOption) {
    case UNIMDM_OPT_BAUDRATE:
        if (IsValidBaudRate(pChg->dwValue, dwSettableBaud)) {
            pDevMiniCfg->dwBaudRate = pChg->dwValue;
        } else {
            DEBUGMSG(ZONE_ERROR, (TEXT("UNIMODEM:DevSpecificLineConfigEdit: Unsupported baud rate\n")));
            goto exitPoint;
        }
        break;

    case UNIMDM_OPT_BYTESIZE:
        if (IsValidByteSize(pChg->dwValue, dwSettableData)) {
            pDevMiniCfg->ByteSize = (BYTE)pChg->dwValue;
        } else {
            DEBUGMSG(ZONE_ERROR, (TEXT("UNIMODEM:DevSpecificLineConfigEdit: Unsupported byte size\n")));
            goto exitPoint;
        }
        break;

    case UNIMDM_OPT_PARITY:
        if (IsValidParity(pChg->dwValue, dwSettableStopParity)) {
            pDevMiniCfg->Parity = (BYTE)pChg->dwValue;
        } else {
            DEBUGMSG(ZONE_ERROR, (TEXT("UNIMODEM:DevSpecificLineConfigEdit: Unsupported parity\n")));
            goto exitPoint;
        }
        break;

    case UNIMDM_OPT_STOPBITS:
        if (IsValidStopBits(pChg->dwValue, dwSettableStopParity)) {
            pDevMiniCfg->StopBits = (BYTE)pChg->dwValue;
        } else {
            DEBUGMSG(ZONE_ERROR, (TEXT("UNIMODEM:DevSpecificLineConfigEdit: Unsupported stop bits\n")));
            goto exitPoint;
        }
        break;

    case UNIMDM_OPT_WAITBONG:
        pDevMiniCfg->wWaitBong = (WORD)pChg->dwValue;
        break;

    case UNIMDM_OPT_MDMOPTIONS: // MDM_* values from mcx.h
        //
        // Currently CE unimodem only looks at:
        // MDM_BLIND_DIAL
        // MDM_FLOWCONTROL_HARD
        // MDM_FLOWCONTROL_SOFT
        // MDM_SPEED_ADJUST (enables DCC client automatic baud rate detection)
        //
        if (pChg->dwValue > (MDM_V23_OVERRIDE * 2 - 1)) {
            DEBUGMSG(ZONE_ERROR, (TEXT("UNIMODEM:DevSpecificLineConfigEdit: Invalid modem option\n")));
            goto exitPoint;
        }
        pDevMiniCfg->dwModemOptions = pChg->dwValue;
        break;

    case UNIMDM_OPT_TIMEOUT:
        pDevMiniCfg->dwCallSetupFailTimer = pChg->dwValue;
        break;

    case UNIMDM_OPT_TERMOPTIONS:    // NETUI_LCD_TRMOPT_* values from netui.h
        if (pChg->dwValue == 0) {
            pDevMiniCfg->fwOptions = TERMINAL_NONE;
        } else {
            pDevMiniCfg->fwOptions = 0;
            if (pChg->dwValue & NETUI_LCD_TRMOPT_MANUAL_DIAL) {
                pDevMiniCfg->fwOptions |= MANUAL_DIAL;
            }
            if (pChg->dwValue & NETUI_LCD_TRMOPT_PRE_DIAL) {
                pDevMiniCfg->fwOptions |= TERMINAL_PRE;
            }
            if (pChg->dwValue & NETUI_LCD_TRMOPT_POST_DIAL) {
                pDevMiniCfg->fwOptions |= TERMINAL_POST;
            }
        }
        break;

    case UNIMDM_OPT_DIALMOD:
        if (S_OK != StringCchCopy(pDevMiniCfg->szDialModifier, DIAL_MODIFIER_LEN, (LPWSTR)pChg->dwValue)) {
            DEBUGMSG(ZONE_ERROR, (TEXT("UNIMODEM:DevSpecificLineConfigEdit: Invalid dial modifier\n")));
            rc = LINEERR_INVALPOINTER;
            goto exitPoint;
        }
        break;

    case UNIMDM_OPT_DRVNAME:
        if (S_OK != StringCchCopy(pDevMiniCfg->wszDriverName, MAX_NAME_LENGTH, (LPWSTR)pChg->dwValue)) {
            DEBUGMSG(ZONE_ERROR, (TEXT("UNIMODEM:DevSpecificLineConfigEdit: Invalid dynamic modem driver name\n")));
            rc = LINEERR_INVALPOINTER;
            goto exitPoint;
        }
        break;

    case UNIMDM_OPT_CFGBLOB:
        if (!CeSafeCopyMemory(pDevMiniCfg->pConfigBlob, (LPBYTE)pChg->dwValue, MAX_CFG_BLOB)) {
            DEBUGMSG(ZONE_ERROR, (TEXT("UNIMODEM:DevSpecificLineConfigEdit: Invalid dynamic modem config blob\n")));
            rc = LINEERR_INVALPOINTER;
            goto exitPoint;
        }
        break;

    default:
        DEBUGMSG(ZONE_ERROR, (TEXT("UNIMODEM:DevSpecificLineConfigEdit: Invalid dwOption\n")));
        goto exitPoint;
    }
    
    rc = 0;

exitPoint:
    DEBUGMSG(ZONE_FUNC, (TEXT("UNIMODEM:-DevSpecificLineConfigEdit: returning 0x%x\n"), rc));
    return rc;
}   // DevSpecificLineConfigEdit


//
// Function to retrieve a specific property of a PDEVMINICONFIG
//
LONG
DevSpecificLineConfigGet(
    PTLINEDEV   pLineDev,
    PUNIMDM_CHG_DEVCFG pChg
    )
{
    DWORD rc;

    PDEVMINICFG  pDevMiniCfg;
    LPVARSTRING  lpDevConfig;

    DEBUGMSG(ZONE_FUNC, (TEXT("UNIMODEM:+DevSpecificLineConfigGet\n")));

    if (rc = DevSpecificLineConfigProlog(pLineDev, pChg, &pDevMiniCfg, &lpDevConfig)) {
        DEBUGMSG(ZONE_ERROR, (TEXT("UNIMODEM:DevSpecificLineConfigGet: DevSpecificLineConfigProlog failed\n")));
        goto exitPoint;
    }

    switch (pChg->dwOption) {
    case UNIMDM_OPT_BAUDRATE:
        pChg->dwValue = pDevMiniCfg->dwBaudRate;
        break;

    case UNIMDM_OPT_BYTESIZE:
        pChg->dwValue = pDevMiniCfg->ByteSize;
        break;

    case UNIMDM_OPT_PARITY:
        pChg->dwValue = pDevMiniCfg->Parity;
        break;

    case UNIMDM_OPT_STOPBITS:
        pChg->dwValue = pDevMiniCfg->StopBits;
        break;

    case UNIMDM_OPT_WAITBONG:
        pChg->dwValue = pDevMiniCfg->wWaitBong;
        break;

    case UNIMDM_OPT_MDMOPTIONS: // MDM_* values from mcx.h
        pChg->dwValue = pDevMiniCfg->dwModemOptions;
        break;

    case UNIMDM_OPT_TIMEOUT:
        pChg->dwValue = pDevMiniCfg->dwCallSetupFailTimer;
        break;

    case UNIMDM_OPT_TERMOPTIONS:    // NETUI_LCD_TRMOPT_* values from netui.h
        pChg->dwValue = 0;
        if (pDevMiniCfg->fwOptions & MANUAL_DIAL) {
            pChg->dwValue |= NETUI_LCD_TRMOPT_MANUAL_DIAL;
        }
        if (pDevMiniCfg->fwOptions & TERMINAL_PRE) {
            pChg->dwValue |= NETUI_LCD_TRMOPT_PRE_DIAL;
        }
        if (pDevMiniCfg->fwOptions & TERMINAL_POST) {
            pChg->dwValue |= NETUI_LCD_TRMOPT_POST_DIAL;
        }
        break;

    case UNIMDM_OPT_DIALMOD:
        if (S_OK != StringCchCopy((LPWSTR)pChg->dwValue, DIAL_MODIFIER_LEN, pDevMiniCfg->szDialModifier)) {
            DEBUGMSG(ZONE_ERROR, (TEXT("UNIMODEM:DevSpecificLineConfigGet: Invalid dial modifier\n")));
            rc = LINEERR_INVALPOINTER;
            goto exitPoint;
        }
        break;

    case UNIMDM_OPT_DRVNAME:
        if (S_OK != StringCchCopy((LPWSTR)pChg->dwValue, MAX_NAME_LENGTH, pDevMiniCfg->wszDriverName)) {
            DEBUGMSG(ZONE_ERROR, (TEXT("UNIMODEM:DevSpecificLineConfigGet: Invalid dynamic modem driver name\n")));
            rc = LINEERR_INVALPOINTER;
            goto exitPoint;
        }
        break;

    case UNIMDM_OPT_CFGBLOB:
        if (!CeSafeCopyMemory((LPBYTE)pChg->dwValue, pDevMiniCfg->pConfigBlob, MAX_CFG_BLOB)) {
            DEBUGMSG(ZONE_ERROR, (TEXT("UNIMODEM:DevSpecificLineConfigGet: Invalid dynamic modem config blob\n")));
            rc = LINEERR_INVALPOINTER;
            goto exitPoint;
        }
        break;

    default:
        DEBUGMSG(ZONE_ERROR, (TEXT("UNIMODEM:DevSpecificLineConfigGet: Invalid dwOption\n")));
        rc = LINEERR_INVALPARAM;
        goto exitPoint;
    }
    
    rc = 0;

exitPoint:
    DEBUGMSG(ZONE_FUNC, (TEXT("UNIMODEM:-DevSpecificLineConfigGet: returning 0x%x\n"), rc));
    return rc;
}   // DevSpecificLineConfigGet

