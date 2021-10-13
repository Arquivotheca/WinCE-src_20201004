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
/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Module Name:

wince.cpp

Abstract:

Windows CE specific functions for the Cisco 1.x NDIS miniport driver.

Functions:
    DllEntry
    AddKeyValues
    Install_Driver
    FindDetectKey
    DetectPcx500


--*/
#include <windows.h>
#include <ndis.h>
#include "aironet.h"
#include "airodef.h"
#include <cardserv.h>
#include <cardapi.h>
#include <tuple.h>
#include "version.h"
#include "AiroDef.h"


LPWSTR FindDetectKey(VOID);

#ifdef DEBUG

//
// These defines must match the ZONE_* defines in AIRODEF.H
//

#define DBG_ERROR      1
#define DBG_WARN       2
#define DBG_FUNCTION   4
#define DBG_INIT       8
#define DBG_INTR       16
#define DBG_RCV        32
#define DBG_XMIT       64

DBGPARAM dpCurSettings = 
{
    TEXT("PCX500"), 
	{
		TEXT("Errors"),
		TEXT("Warnings"),
		TEXT("Functions"),
		TEXT("Init"),
		TEXT("Interrupts"),
		TEXT("Receives"),
		TEXT("Transmits"),
		TEXT("Query"),
		TEXT("Set"),
		TEXT("Undefined"),
		TEXT("Undefined"),
		TEXT("Undefined"),
		TEXT("Undefined"),
		TEXT("Undefined"),
		TEXT("Undefined"),
		TEXT("Undefined") 
	},
    
	0x000f
};
#endif  // DEBUG


typedef struct _REG_VALUE_DESCR 
{
    LPWSTR val_name;
    DWORD  val_type;
    PBYTE  val_data;
} REG_VALUE_DESCR, * PREG_VALUE_DESCR;


//
//	Values for [HKEY_LOCAL_MACHINE\Drivers\PCMCIA\Detect\40]
//

REG_VALUE_DESCR DetectKeyValues[] = 
{
    (TEXT("Dll")), REG_SZ, (PBYTE)(TEXT("pcx500.dll")),
    (TEXT("Entry")), REG_SZ, (PBYTE)(TEXT("DetectPCX500")),
    NULL, 0, NULL
};


//
//	Values for [HKEY_LOCAL_MACHINE\Drivers\PCMCIA\Cisco]
//

REG_VALUE_DESCR PcmKeyValues[] = {
   (TEXT("Dll")), REG_SZ, (PBYTE)(TEXT("ndis.dll")),
   (TEXT("Prefix")), REG_SZ, (PBYTE)(TEXT("NDS")),
   (TEXT("Miniport")), REG_SZ, (PBYTE)(TEXT("Cisco")),
    NULL, 0, NULL
};


//
// Values for [HKEY_LOCAL_MACHINE\Comm\Cisco]
// and [HKEY_LOCAL_MACHINE\Comm\Cisco1]
//

REG_VALUE_DESCR CommKeyValues[] = 
{
   (TEXT("DisplayName")), REG_SZ, (PBYTE)(TEXT("Cisco Wireless LAN Adapter")),
   (TEXT("Group")), REG_SZ, (PBYTE)(TEXT("NDIS")),
   (TEXT("ImagePath")), REG_SZ, (PBYTE)(TEXT("pcx500.dll")),
    NULL, 0, NULL
};


//
//	Values for [HKEY_LOCAL_MACHINE\Comm\Cisco1\Parms]
//

REG_VALUE_DESCR ParmKeyValues[] = 
{
   (TEXT("BusNumber")), REG_DWORD, (PBYTE)0,
   (TEXT("BusType")), REG_DWORD, (PBYTE)8,
   (TEXT("InterruptNumber")), REG_DWORD, (PBYTE)03,
   (TEXT("IoBaseAddress")), REG_DWORD, (PBYTE)768,		// 0x0300
   (TEXT("InfrastructureMode")), REG_DWORD, (PBYTE)1,	// Infrastructure
   (TEXT("TransmitPower")), REG_DWORD, (PBYTE)100,
   (TEXT("PowerSaveMode")), REG_DWORD, (PBYTE)0,		//CAM
   (TEXT("CardType")), REG_DWORD, (PBYTE)6,			    //4800
   (TEXT("DriverMajorVersion")), REG_DWORD, (PBYTE)DRIVER_MAJOR_VERSION,
   (TEXT("DriverMinorVersion")), REG_DWORD, (PBYTE)DRIVER_MINOR_VERSION,
   (TEXT("Associated")), REG_DWORD, (PBYTE)0,
    NULL, 0, NULL
};


//
//	Values for [HKEY_LOCAL_MACHINE\Comm\Cisco1]
//

REG_VALUE_DESCR LinkageKeyValues[] = 
{
   (TEXT("Route")), REG_MULTI_SZ, (PBYTE)(TEXT("Cisco1")),
    NULL, 0, NULL
};

PREG_VALUE_DESCR Values[] = 
{
    PcmKeyValues,
    CommKeyValues,
    CommKeyValues,
    ParmKeyValues,
    LinkageKeyValues
};

LPWSTR KeyNames[] = 
{
    (TEXT("Drivers\\PCMCIA\\Cisco")),
    (TEXT("Comm\\Cisco")),
    (TEXT("Comm\\Cisco1")),
    (TEXT("Comm\\Cisco1\\Parms")),
    (TEXT("Comm\\Cisco\\Linkage"))
};



////////////////////////////////////////////////////////////////////////////////
//	DllEntry()
//
//	Routine Description:
//
//		Standard Windows DLL entrypoint.
//		Since Windows CE NDIS miniports are implemented as DLLs, a DLL 
//		entrypoint is needed.
//
//	Arguments:
//	
//		hDLL		::	Instance pointer.
//		dwReason	::	Reason routine is called.
//		lpReserved	::	System parameter.
//	
//	Return Value:
//	
//		TRUE / FALSE.
//

BOOL __stdcall
DllEntry(
  HANDLE	hDLL,
  DWORD		dwReason,
  LPVOID	lpReserved)
{
    switch (dwReason) 
	{
		case DLL_PROCESS_ATTACH:
#ifdef DEBUG
	        DEBUGREGISTER((HMODULE)hDLL);
#endif
            DisableThreadLibraryCalls ((HMODULE)hDLL);
		    DEBUGMSG(ZONE_INIT, (TEXT("Cisco DLL_PROCESS_ATTACH Handle=0x%lx\n"),hDLL));
			break;
    
		case DLL_PROCESS_DETACH:
			DEBUGMSG(ZONE_INIT, (TEXT("Cisco DLL_PROCESS_DETACH\n")));
			break;
    }
    return TRUE;

}	//	DllEntry()



////////////////////////////////////////////////////////////////////////////////
//	AddKeyValues()
//
//	Routine Description:
//
//		Add registry key: KeyName and set it with value: Vals.
//
//	Arguments:
//	
//		KeyName	::	The registry key name.
//		Vals	::	The value for this registry key entry.
//	
//	Return Value:
//	
//		TRUE / FALSE.
//

BOOL
AddKeyValues(
    LPWSTR				KeyName,
    PREG_VALUE_DESCR	Vals
    )
{
	PREG_VALUE_DESCR pValue;
    
	DWORD	Status;
    DWORD	dwDisp;
    HKEY	hKey;    
    DWORD	ValLen;
    PBYTE	pVal;
    DWORD	dwVal;
    LPWSTR	pStr;

    Status = RegCreateKeyEx(
                 HKEY_LOCAL_MACHINE,
                 KeyName,
                 0,
                 NULL,
                 REG_OPTION_NON_VOLATILE,
                 0,
                 NULL,
                 &hKey,
                 &dwDisp);

    if (Status != ERROR_SUCCESS) 
	{
        return FALSE;
    }

    pValue = Vals;
    
	while (pValue->val_name) 
	{
        switch (pValue->val_type) 
		{
			case REG_DWORD:
				pVal = (PBYTE)&dwVal;
				dwVal = (DWORD)pValue->val_data;
				ValLen = sizeof(DWORD);
				break;

			case REG_SZ:
				pVal = (PBYTE)pValue->val_data;
				ValLen = (wcslen((LPWSTR)pVal) + 1)*sizeof(WCHAR);
				break;

			case REG_MULTI_SZ:
				dwVal = wcslen((LPWSTR)pValue->val_data);
				ValLen = (dwVal+2)*sizeof(WCHAR);
				pVal = (PBYTE)LocalAlloc(LPTR, ValLen);
				if (pVal == NULL) 
				{
	                goto akv_fail;
		        }
				wcscpy((LPWSTR)pVal, (LPWSTR)pValue->val_data);
				pStr = (LPWSTR)pVal + dwVal;
				pStr[1] = 0;
				break;
        }
        
		Status = RegSetValueEx(
                     hKey,
                     pValue->val_name,
                     0,
                     pValue->val_type,
                     pVal,
                     ValLen);
        
		if (pValue->val_type == REG_MULTI_SZ) 		
            LocalFree(pVal);        


akv_fail:
        if (Status != ERROR_SUCCESS) 
		{
            RegCloseKey(hKey);
            return FALSE;
        }
        
		pValue++;
    }
    
	RegCloseKey(hKey);
    
	return TRUE;

}   // AddKeyValues



////////////////////////////////////////////////////////////////////////////////
//	Install_Driver()
//
//	Routine Description:
//
//		Install_Driver function for the Cisco NDIS miniport driver.
//		This function sets up the registry keys and values required to install 
//		this DLL as a Windows CE plug and play driver.
//
//	Arguments:
//	
//		lpPnpId	::	The device's plug and play identifier string.  An install
//					function can use lpPnpId to set up a key
//					HKEY_LOCAL_MACHINE\Drivers\PCMCIA\<lpPnpId> under the 
//					assumption that the user will continue to use the same 
//					device that generates the same plug and play id string.  
//					If there is a general detection method for the card, then 
//					lpPnpId can be ignored and a detection function can be 
//					registered under HKEY_LOCAL_MACHINE\Drivers\PCMCIA\Detect.
//
//		lpRegPath::	Buffer to contain the newly installed driver's device key
//					under HKEY_LOCAL_MACHINE in the registry.
//					Windows CE will attempt to load the the newly installed 
//					device driver upon completion of its Install_Driver 
//					function.
//
//		cRegPathSize:: Number of bytes in lpRegPath.
//	
//	Return Value:
//	
//		Returns lpRegPath if successful, NULL for failure.
//

#ifdef __cplusplus
extern "C" {
LPWSTR
Install_Driver(
    LPWSTR	lpPnpId,
    LPWSTR	lpRegPath,
    DWORD	cRegPathSize);
}
#endif

LPWSTR
Install_Driver(
    LPWSTR	lpPnpId,
    LPWSTR	lpRegPath,
    DWORD	cRegPathSize)
{
	DWORD	i;
    LPWSTR	lpDetectKeyName;

    DEBUGMSG(ZONE_INIT,
        (TEXT("Cisco Install_Driver(%s, %s, %d)\r\n"),
        lpPnpId, lpRegPath, cRegPathSize));	

    if ((lpDetectKeyName = FindDetectKey()) == NULL)	
        return NULL;    

    if (!AddKeyValues(lpDetectKeyName, DetectKeyValues))
		goto wid_fail1;

    for (i = 0; i < (sizeof(KeyNames)/sizeof(LPWSTR)); i++)
	{
        if (!AddKeyValues(KeyNames[i], Values[i])) 		
            goto wid_fail;        
    }
    
	LocalFree(lpDetectKeyName);

    //
    // Return "Drivers\\PCMCIA\\Cisco"
    //
    
	wcscpy(lpRegPath, KeyNames[0]);
    return lpRegPath;


wid_fail:

    //
    // Clean up after ourself on failure.
    //

    for (i = 0; i < (sizeof(KeyNames)/sizeof(LPWSTR)); i++)
		RegDeleteKey(HKEY_LOCAL_MACHINE,KeyNames[i]);
    

wid_fail1:

    RegDeleteKey(HKEY_LOCAL_MACHINE, lpDetectKeyName);
    LocalFree(lpDetectKeyName);
	DEBUGMSG(ZONE_INIT | ZONE_ERROR, (TEXT("Cisco InstallDriver Failed\n")));
    
	return NULL;

}	//	Install_Driver()



////////////////////////////////////////////////////////////////////////////////
//	FindDetectKey()
//
//	Routine Description:
//
//		Find an unused Detect key number and return the registry path 
//		name for it.
//
//	Arguments:
//	
//		None.
//
//	Return Value:
//	
//		Return NULL for failure
//

LPWSTR
FindDetectKey(VOID)
{
    HKEY	hDetectKey;
    DWORD	dwKeyNum;
    DWORD	dwDisp;
    DWORD	Status;
    WCHAR	*pKeyName;
    LPWSTR	pKeyNum;

    pKeyName = (LPTCH)LocalAlloc(LPTR, sizeof(WCHAR) * 255);
    
	if (pKeyName == NULL)
        return NULL;
    
    wcscpy(pKeyName, (TEXT("Drivers\\PCMCIA\\Detect\\")));

    pKeyNum = pKeyName + wcslen(pKeyName);

    //
    // Find a detect number and create the detect key.
    //

    for (dwKeyNum = 40; dwKeyNum < 99; dwKeyNum++) 
	{
        wsprintf(pKeyNum, (TEXT("%02d")), dwKeyNum);
        Status = RegCreateKeyEx(
                     HKEY_LOCAL_MACHINE,
                     pKeyName,
                     0,
                     NULL,
                     REG_OPTION_NON_VOLATILE,
                     0,
                     NULL,
                     &hDetectKey,
                     &dwDisp);
        
		if (Status == ERROR_SUCCESS) 
		{
            RegCloseKey(hDetectKey);

            if (dwDisp == REG_CREATED_NEW_KEY)
				return pKeyName;
		}
    }

    LocalFree(pKeyName);

    return NULL;

}	//	FindDetectKey()



////////////////////////////////////////////////////////////////////////////////
//	DetectPCX500()
//
//	Routine Description:
//
//		Windows CE Plug and play detection function for the CISCO PCMCIA 
//		network adapter.
//
//	Arguments:
//	
//		hSock		::
//		DevType		::	Device Type (we need: PCCARD_TYPE_NETWORK).
//		DevKey		::
//		DevKeyLen	::
//
//	Return Value:
//	
//		Return NULL for failure
//
#ifdef __cplusplus
extern "C" 
LPWSTR
DetectPCX500(
    CARD_SOCKET_HANDLE	hSock,
    UCHAR				DevType,
    LPWSTR				DevKey,
    DWORD				DevKeyLen
    );
#endif


LPWSTR
DetectPCX500(
    CARD_SOCKET_HANDLE	hSock,
    UCHAR				DevType,
    LPWSTR				DevKey,
    DWORD				DevKeyLen)
{
    CARD_CLIENT_HANDLE	hPcmcia	   = NULL;
    HMODULE				hPcmciaDll = NULL;
    DWORD				cerr;
    LPWSTR				lpRet=NULL;

	struct {
		CARD_DATA_PARMS	TupleParms ;
		UINT16			aTPLMID_MANF;
		UINT16			aTPLMID_CARD;
	} TupleBuffer ;

    GETFIRSTTUPLE		pfnGetFirstTuple;
    GETTUPLEDATA		pfnGetTupleData;

	DEBUGMSG(ZONE_INIT, (TEXT("Cisco DetectCisco Routine Handle=0x%lx\n"),hSock));

    if (DevType != PCCARD_TYPE_NETWORK) 
	{
		DEBUGMSG(ZONE_INIT | ZONE_ERROR, (TEXT("Cisco DevType %ld Not Valid\n"),DevType));
        return NULL;
    }

	hPcmciaDll = LoadLibrary(TEXT("PCMCIA.DLL"));

	DEBUGMSG(ZONE_INIT, (TEXT("Cisco Loaded Pcmcia, Handle=0x%lx\n"),hPcmciaDll));
    
	if (hPcmciaDll == NULL) 
	{
		DEBUGMSG(ZONE_INIT | ZONE_ERROR,(TEXT("Cisco Could Not Load PCMCIA.DLL\n")));
        return NULL;
    }

    pfnGetFirstTuple = (GETFIRSTTUPLE) GetProcAddress(hPcmciaDll, TEXT("CardGetFirstTuple"));
    pfnGetTupleData  = (GETTUPLEDATA)  GetProcAddress(hPcmciaDll, TEXT("CardGetTupleData" ));

    lpRet = NULL;

	if ((pfnGetFirstTuple == NULL) ||
        (pfnGetTupleData  == NULL)) 
	{
        DEBUGMSG(ZONE_INIT | ZONE_ERROR, (TEXT("Cisco PcmciaDll GetProcAddress Failed\n")));
        goto dn_fail1;
    }

    //
    //	Get Manuf ID Tuple
    //

	TupleBuffer.TupleParms.hSocket		 = hSock;
	TupleBuffer.TupleParms.fAttributes	 = 0;
	TupleBuffer.TupleParms.uDesiredTuple = CISTPL_MANFID ;

	if( CERR_SUCCESS != (cerr=pfnGetFirstTuple((PCARD_TUPLE_PARMS)&TupleBuffer))) 
	{
        DEBUGMSG(ZONE_INIT | ZONE_ERROR,
            (TEXT("Cisco GetFirstTuple Failed %d\n"),cerr));
        goto dn_fail1;
    }

	TupleBuffer.TupleParms.uBufLen		= sizeof(TupleBuffer) - sizeof(CARD_DATA_PARMS);
	TupleBuffer.TupleParms.uTupleOffset = 0;

	if( CERR_SUCCESS != (cerr=pfnGetTupleData((PCARD_DATA_PARMS)&TupleBuffer))) 
	{
        DEBUGMSG(ZONE_INIT | ZONE_ERROR,
            (TEXT("Cisco GetTupleData Failed %d\n"),cerr));
        goto dn_fail1;
    }

	if(TupleBuffer.aTPLMID_MANF != 0x015F) 
	{
		DEBUGMSG(ZONE_INIT | ZONE_ERROR,
            (TEXT("Cisco: Not Cisco Manuf ID\r\n")));
		goto dn_fail1;
	}

	if(!((TupleBuffer.aTPLMID_CARD == 0x04)   ||
	     (TupleBuffer.aTPLMID_CARD == 0x05)   ||
	     (TupleBuffer.aTPLMID_CARD == 0x06)   ||
	     (TupleBuffer.aTPLMID_CARD == 0x07)   ||
	     (TupleBuffer.aTPLMID_CARD == 0x09)   ||
		 (TupleBuffer.aTPLMID_CARD == 0x0A))) 
	{
		DEBUGMSG(ZONE_INIT,
            (TEXT("Cisco: Not Correct Cisco Product %d\n"),TupleBuffer.aTPLMID_CARD));
		goto dn_fail1;
	}

	DEBUGMSG(ZONE_INIT, (TEXT("Cisco: Detected Cisco Card\n")));
    lpRet = DevKey;
    wcscpy(DevKey, TEXT("CISCO"));

dn_fail1:
    FreeLibrary(hPcmciaDll);
    return lpRet;

}	//	DetectPCX500()

