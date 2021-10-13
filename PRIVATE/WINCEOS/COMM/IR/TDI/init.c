//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++


 Module Name:    init.c

 Abstract:       Contains IrDAStk initialization routines.

 Contents:

--*/

#include "irdatdi.h"

// 
// TDI Globals.
//

BOOL             DscvInProgress;
HANDLE           DscvConfEvent;
HANDLE			 DscvIndEvent;
PDEVICELIST      pDscvDevList;
int              DscvNumDevices;
DWORD            DscvCallersPermissions;
int              DscvRepeats;
CRITICAL_SECTION csDscv;

BOOL             IASQueryInProgress;
HANDLE           IASQueryConfEvent;
PIAS_QUERY       pIASQuery;
int              IASQueryStatus;
CRITICAL_SECTION csIasQuery;

int              OpenSocketCnt;

IAS_QUERY        ConnectIASQuery;

CRITICAL_SECTION  csIrObjList;
LIST_ENTRY        IrAddrObjList;
// Maintains connections which are not yet associated with address.
LIST_ENTRY        IrConnObjList;

PVOID            IrdaMsgPool;

#ifdef MEMTRACKING
DWORD IrdaMemTypeId;
#endif // MEMTRACKING

// NDIS dynamic linking.
HINSTANCE               g_hInstNdisDll;

// For registry configuration.
#define IRDA_NAME           L"IrDA"
#define IRDA_REG            L"Comm\\IrDA"
#define IRDA_LINKAGE        L"Comm\\IrDA\\Linkage"
#define IRDA_MAX_BINDNAME   128

//
// In irlap\irndis.c. Loads the driver specified.
//

VOID IrdaBindAdapter(
    OUT PNDIS_STATUS            pStatus,
    IN  NDIS_HANDLE             BindContext,
    IN  PNDIS_STRING            AdapterName,
    IN  PVOID                   SystemSpecific1,
    IN  PVOID                   SystemSpecific2
    );

//
// Local functions prototypes.
//

static DWORD IrdaTdiInitialize();

//
// Debug settings.
//

#ifdef DEBUG
DBGPARAM dpCurSettings = 
{
    TEXT("IrDA"), 
    {
        TEXT("Init"),       TEXT("WshIrda"),    
        TEXT("IrTdi"),      TEXT("IrLmp"),
        TEXT("IrLap"),      TEXT("IrNdis"),   
        TEXT("Discovery"),  TEXT("Socket Count"),
        TEXT("Timer"),      TEXT("Ref"),     
        TEXT("Lock"),       TEXT("Misc"),
        TEXT("Alloc"),      TEXT("Function"), 
        TEXT("Warning"),    TEXT("Error") 
    },
    0x00008000
};
#endif // DEBUG

//
// TDI dispatch table to register entry points with AFD.
//

const 
TDIDispatchTable IRLMP_DispatchTable =
{
    IRLMP_OpenAddress,
    IRLMP_CloseAddress,
    IRLMP_OpenConnection,
    IRLMP_CloseConnection,
    IRLMP_AssociateAddress,
    IRLMP_DisAssociateAddress,
    IRLMP_Connect,
    IRLMP_Disconnect,
    IRLMP_Listen,
    IRLMP_Accept,
    IRLMP_Receive,
    IRLMP_Send,
    IRLMP_SendDatagram,
    IRLMP_ReceiveDatagram,
    IRLMP_SetEvent,
    IRLMP_QueryInformation,
    IRLMP_SetInformation,
    IRLMP_Action,
    IRLMP_QueryInformationEx,
    IRLMP_SetInformationEx,
    IRLMP_GetSockaddrType,
    IRLMP_GetWildcardSockaddr,
    IRLMP_GetSocketInformation,
    IRLMP_GetWinsockMapping,
    IRLMP_Notify,
    IRLMP_OpenSocket,
    IRLMP_SetSocketInformation,
    IRLMP_EnumProtocols
};

/*++

 Function:       dllentry

 Description:    DLL entry point.

 Arguments:

 Returns:

 Comments:

--*/

BOOL
DllEntry(HANDLE hinstDLL,
         DWORD  Op,
         LPVOID lpvReserved)
{
    //int i;

    switch (Op)
    {
      case DLL_PROCESS_ATTACH:
        DEBUGREGISTER(hinstDLL);

        DEBUGMSG(ZONE_INIT, 
            (TEXT("IRDASTK.DLL dllentry(0x%X,DLL_PROCESS_ATTACH,0x%X)\r\n"),
            (unsigned) hinstDLL, lpvReserved));

        CTEInitialize();

    #ifdef MEMTRACKING
        IrdaMemTypeId = RegisterTrackedItem(TEXT("IrDA Memory"));
    #endif // MEMTRACKING
		
        DisableThreadLibraryCalls ((HMODULE)hinstDLL);

        break;
      
      case DLL_PROCESS_DETACH:
        DEBUGMSG(ZONE_INIT, 
            (TEXT("IRDASTK.DLL dllentry(0x%X,DLL_PROCESS_DETACH,0x%X)\r\n"),
            (unsigned) hinstDLL, lpvReserved));
        break;

      default:
        DEBUGMSG(ZONE_INIT, 
            (TEXT("IRDASTK.DLL dllentry(0x%X,0x%X,0x%X)\r\n"),
            (unsigned) hinstDLL, Op, lpvReserved));
        break;
  }
    return(TRUE);
}

/*++

 Function:       Register

 Description:    AFD entry point to register protocol.

 Arguments:

    pRegister   - Function pointer to register TDI dispatch table with AFD.
    
    pAfdCS      - Must be NULL. Not required anymore by AFD.

 Returns:

    None.

 Comments:

--*/

void
Register(
    PWSRegister pRegister, 
    CRITICAL_SECTION *pAfdCS
    )
{
    NTSTATUS       status;
    UNICODE_STRING ProtocolName;
    UNICODE_STRING RegistryPath;

    DWORD  dwLazyDscvInterval;

	DEBUGMSG(ZONE_INIT, (TEXT("IRDASTK.DLL Register(0x%X)\r\n"), pRegister));

    ASSERT(pAfdCS == NULL);

    RtlInitUnicodeString(&ProtocolName, IRDA_NAME);
    RtlInitUnicodeString(&RegistryPath, IRDA_REG);

    // Initialize IrDA Winsock Helper.
    status = WshIrdaInit();

    if (status != STATUS_SUCCESS)
    {
        RETAILMSG(1, (TEXT("IRDASTK: Failed to initialize (WSHIrda).\r\n")));
        goto done;
    }

    // Initialize TDI.
    status = IrdaTdiInitialize();

    if (status != STATUS_SUCCESS)
    {
        RETAILMSG(1, (TEXT("IRDASTK: Failed to initialize (TDI).\r\n")));
        goto done;
    }

    // Initialize LAP/LMP/MAC.
    status = IrdaInitialize(
        &ProtocolName,
        &RegistryPath,
        &dwLazyDscvInterval
        );
    ASSERT(dwLazyDscvInterval == 0);

    if (status != STATUS_SUCCESS)
    {
        RETAILMSG(1, (TEXT("IRDASTK: Failed to initialize (LMP/LAP).\r\n")));
        goto done;
    }

done:

    if (status == NDIS_STATUS_SUCCESS)
    {
        pRegister(IRLMP_TRANSPORT_NAME, (TDIDispatchTable *)&IRLMP_DispatchTable);
    }
    else
    {
        RETAILMSG(1, (TEXT("IRDASTK: Failed to initialize IrDA.\r\n")));
    }

    return;
}


HANDLE g_hIrdaHeap;

/*++

 Function:       IrdaTdiInitialize

 Description:    Initializes the IrDA TDI layer.

 Arguments:

    None.

 Returns:

    DWORD - status.

        Success - STATUS_SUCCESS.
        
        Failure - STATUS_INSUFFICIENT_RESOURCES.

 Comments:

--*/

DWORD
IrdaTdiInitialize()
{
    DWORD status = STATUS_SUCCESS;

	DEBUGMSG(ZONE_INIT, (L"IRDASTK:+TdiInitialize\r\n"));

    if ((g_hIrdaHeap = HeapCreate(0, 0, 0)) == NULL)
    {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    DEBUGMSG(ZONE_INIT, (L"IRDASTK:TdiInitialize g_hIrdaHeap == 0x%x\n", g_hIrdaHeap));

    if ((DscvConfEvent = CreateEvent(NULL, FALSE, FALSE, NULL)) == NULL)
    {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto error10;
    }

	// Make the DscvIndEvent manual, this let's all threads waiting
	// have a chance to release
    if ((DscvIndEvent = CreateEvent(NULL, TRUE,  FALSE, NULL)) == NULL)
    {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto error20;
    }

    if ((IASQueryConfEvent = CreateEvent(NULL, FALSE, FALSE, NULL)) == NULL)
    {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto error30;
    }

    IrdaMsgPool = CreateIrdaBufPool(IRDA_MSG_DATA_SIZE_INTERNAL, '1DRI');

    if (IrdaMsgPool == NULL)
    {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto error40;
    }

    InitializeCriticalSection(&csDscv);
    InitializeCriticalSection(&csIasQuery);
    InitializeCriticalSection(&csIrObjList);

    InitializeListHead(&IrAddrObjList);
    InitializeListHead(&IrConnObjList);

    goto exit;

error40:
    CloseHandle(IASQueryConfEvent);
error30:
    CloseHandle(DscvIndEvent);
error20:
    CloseHandle(DscvConfEvent);
error10:
    HeapDestroy(g_hIrdaHeap);
exit:

	DEBUGMSG(ZONE_INIT, (L"IRDASTK:-TdiInitialize %d\r\n", status));
    return(status);
}


void
IrdaCloseEventHandle(
    HANDLE hEvent
    )
{
    if (hEvent) {
        SetEvent(hEvent);
        CloseHandle(hEvent);
    }
}

/*++

 Function:       TdiDeinitialize

 Description:    Undo TdiInitialize. Clean up resources.

 Arguments:

    None.

 Returns:

    STATUS_SUCCESS.

 Comments:

--*/

DWORD
TdiDeinitialize()
{
    

	DEBUGMSG(ZONE_INIT, (L"IRDASTK:+TdiDeinitialize\r\n"));

    IrdaCloseEventHandle(DscvConfEvent);
    IrdaCloseEventHandle(DscvIndEvent);
    IrdaCloseEventHandle(IASQueryConfEvent);

    DeleteCriticalSection(&csDscv);
    DeleteCriticalSection(&csIasQuery);
    DeleteCriticalSection(&csIrObjList);

    if (IrdaMsgPool) { DeleteIrdaBufPool(IrdaMsgPool); }

    HeapDestroy(g_hIrdaHeap);

	DEBUGMSG(ZONE_INIT, (L"IRDASTK:-TdiDeinitialize\r\n"));
    return (STATUS_SUCCESS);
}
