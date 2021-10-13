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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
//
// XNIC (Experimental Network Interface Card)
//

//
//
//
// Use of this source code is subject to the terms of the Microsoft end-user
// license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
// If you did not accept the terms of the EULA, you are not authorized to use
// this source code. For a copy of the EULA, please see the LICENSE.RTF on your
// install media.
//
/*++


Module Name:
   xnic.cpp

Abstract:
   NDIS 4.0 driver

Author:


Environment:
   Windows CE

Revision History:
   2003-12-23 first version
 
--*/




#include <ndis.h>
#include "xnic.h"

#pragma hdrstop
#pragma NDIS_INIT_FUNCTION(DriverEntry)



#ifdef DEBUG

DBGPARAM dpCurSettings = 
{
    L"XNIC",
   {
        L"Init",
        L"OID_SER/QUERY",
        L"SEND",
        L"TRANSFER",

        L"",
        L"",
        L"",
        L"",

        L"",
        L"",
        L"",
        L"",

        L"",
        L"Comment",
        L"Warning",
        L"Error"
    },

    0xE000
};

#endif






NTSTATUS
DriverEntry
//
// NDIS calls DriverEntry() when loading the miniport driver.
// we register this dll as a miniport driver.
//
// Return
//   STATUS_SUCCESS if all initialization is successful
//   STATUS_XXX error code if not
//
(
IN PDRIVER_OBJECT  pDriverObject,  // pointer to the system's driver object structure
IN PUNICODE_STRING pszRegistryPath // registry path for this driver, "Comm\XNIC"
)
{
   DEBUGMSG(ZONE_COMMENT, (L"XNIC.DriverEntry()"));
   NDIS_HANDLE hNdisWrapper = NULL;
   NdisMInitializeWrapper(&hNdisWrapper, pDriverObject, pszRegistryPath, NULL);
   NdisMRegisterUnloadHandler(hNdisWrapper, DriverUnload);

   NDIS_MINIPORT_CHARACTERISTICS ndisMChars;
   NdisZeroMemory(&ndisMChars, sizeof(NDIS_MINIPORT_CHARACTERISTICS));

   ((NDIS30_MINIPORT_CHARACTERISTICS *)&ndisMChars)->MajorNdisVersion = 4;
   ((NDIS30_MINIPORT_CHARACTERISTICS *)&ndisMChars)->MinorNdisVersion = 0;

   ((NDIS30_MINIPORT_CHARACTERISTICS *)&ndisMChars)->InitializeHandler = MiniportInitialize;
   ((NDIS30_MINIPORT_CHARACTERISTICS *)&ndisMChars)->QueryInformationHandler = (W_QUERY_INFORMATION_HANDLER) MiniportQueryInformation;
   ((NDIS30_MINIPORT_CHARACTERISTICS *)&ndisMChars)->SetInformationHandler = (W_SET_INFORMATION_HANDLER) MiniportSetInformation;
   ((NDIS30_MINIPORT_CHARACTERISTICS *)&ndisMChars)->SendHandler = (W_SEND_HANDLER) MiniportSend;
   ((NDIS30_MINIPORT_CHARACTERISTICS *)&ndisMChars)->TransferDataHandler = (W_TRANSFER_DATA_HANDLER) MiniportTransferData;
   ((NDIS30_MINIPORT_CHARACTERISTICS *)&ndisMChars)->ResetHandler = (W_RESET_HANDLER) MiniportReset;
   ((NDIS30_MINIPORT_CHARACTERISTICS *)&ndisMChars)->HaltHandler = (W_HALT_HANDLER) MiniportHalt;

   //  Register the miniport driver
   NDIS_STATUS status = NdisMRegisterMiniport(hNdisWrapper, &ndisMChars, sizeof(ndisMChars));

   if (status != NDIS_STATUS_SUCCESS) {
      DEBUGMSG(ZONE_ERROR, (L" XNIC.DriverEntry: Failed register miniport 0x%08x", status));
      NdisTerminateWrapper(hNdisWrapper, NULL);
   }

   return status;
}





void
DriverUnload
(
IN PDRIVER_OBJECT pDriverObject
)
{
   DEBUGMSG(ZONE_WARNING, (L"XNIC.DriverUnload()"));
}






//
// DllEntry.
//

BOOL WINAPI
DllMain(
HANDLE hinstDll,  // Instance pointer.
DWORD dwReason,   // Reason why this routine is called.
LPVOID lpReserved // reserved.
)
{
    if ( dwReason == DLL_PROCESS_ATTACH )
    {
        DisableThreadLibraryCalls((HMODULE) hinstDll);
#ifdef DEBUG			
        DEBUGREGISTER((HMODULE)hinstDll);
#endif                       
    }
    return TRUE;
}
