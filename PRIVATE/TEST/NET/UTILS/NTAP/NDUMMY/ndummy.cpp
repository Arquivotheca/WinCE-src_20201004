//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//
// NDUMMY (dummy network driver)
//
// 
// Aug, 2003
//
// a dummy NDIS miniport driver
// 4.0 driver
//



#include <ndis.h>
#include "ndummy.h"

#pragma hdrstop
#pragma NDIS_INIT_FUNCTION(DriverEntry)



#ifdef DEBUG

DBGPARAM dpCurSettings = 
{
    L"NDUMMY",
	{
        L"Init",
        L"OID_QUERY",
        L"OID_SET",
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
IN PUNICODE_STRING pszRegistryPath // registry path for this driver, "Comm\NDUMMY"
)
{
   DEBUGMSG(ZONE_COMMENT, (L"NDUMMY.DriverEntry() szRegistryPath=%s", pszRegistryPath->Buffer));
   NDIS_HANDLE hNdisWrapper = NULL;
   NdisMInitializeWrapper(&hNdisWrapper, pDriverObject, pszRegistryPath, NULL);
   NdisMRegisterUnloadHandler(hNdisWrapper, DriverUnload);

   NDIS_MINIPORT_CHARACTERISTICS ndisMChars;
   NdisZeroMemory(&ndisMChars, sizeof(NDIS_MINIPORT_CHARACTERISTICS));

   ((NDIS30_MINIPORT_CHARACTERISTICS *)&ndisMChars)->MajorNdisVersion = 4;
   ((NDIS30_MINIPORT_CHARACTERISTICS *)&ndisMChars)->MinorNdisVersion = 0;

   ((NDIS30_MINIPORT_CHARACTERISTICS *)&ndisMChars)->InitializeHandler = MiniportInitialize;
   ((NDIS30_MINIPORT_CHARACTERISTICS *)&ndisMChars)->QueryInformationHandler = MiniportQueryInformation;
   ((NDIS30_MINIPORT_CHARACTERISTICS *)&ndisMChars)->SetInformationHandler = MiniportSetInformation;
   ((NDIS30_MINIPORT_CHARACTERISTICS *)&ndisMChars)->SendHandler = MiniportSend;
   ((NDIS30_MINIPORT_CHARACTERISTICS *)&ndisMChars)->TransferDataHandler = MiniportTransferData;
   ((NDIS30_MINIPORT_CHARACTERISTICS *)&ndisMChars)->ResetHandler = MiniportReset;
   ((NDIS30_MINIPORT_CHARACTERISTICS *)&ndisMChars)->HaltHandler = MiniportHalt;

   //  Register the miniport driver
   NDIS_STATUS status = NdisMRegisterMiniport(hNdisWrapper, &ndisMChars, sizeof(ndisMChars));

   if (status != NDIS_STATUS_SUCCESS) {
      DEBUGMSG(ZONE_ERROR, (L" NDUMMY.DriverEntry: Failed register miniport 0x%08x", status));
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
   DEBUGMSG(ZONE_WARNING, (L"NDUMMY.DriverUnload()"));
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
