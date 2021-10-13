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
/*++

 
Module Name:
   passthru.c

Abstract:
   The Ndis DMUX Intermediate Miniport driver sample. This is based on passthru driver 
   which doesn't touch packets at all. The common miniport code is implemented
   in the following code.

Author:
   The original code was a NT/XP sample passthru driver.
   KarelD - add CE specific code and reformat text.

Environment:
   Windows CE .Net

Revision History:
   None.

--*/

#include "precomp.h"
#pragma hdrstop
#pragma NDIS_INIT_FUNCTION(DriverEntry)

//------------------------------------------------------------------------------

NDIS_HANDLE g_hNdisProtocol = NULL;
NDIS_HANDLE g_hNdisMiniport = NULL;
NDIS_HANDLE g_hNdisWrapper = NULL;

NDIS_MEDIUM g_aNdisMedium[] = {
   NdisMedium802_3,              // Ethernet
   NdisMedium802_5,              // Token Ring
   NdisMediumFddi                // FDDI
};

NDIS_SPIN_LOCK g_spinLock;
BINDING* g_pBindingList = NULL;
UINT g_uiBindings = 0;

#ifndef UNDER_CE

NDIS_HANDLE g_hDevice = NULL;
PDEVICE_OBJECT g_pDeviceObject = NULL;
UINT g_uiDeviceInstances = 0;

enum _DEVICE_STATE
{
   PS_DEVICE_STATE_READY = 0,                // ready for create/delete
   PS_DEVICE_STATE_CREATING,                 // create operation in progress
   PS_DEVICE_STATE_DELETING                  // delete operation in progress
} g_eDeviceState = PS_DEVICE_STATE_READY;

#endif

//------------------------------------------------------------------------------

NDIS_TIMER g_timer;

VOID
DriverRemoveItself(
   IN PVOID  pvSystemSpecific1,
   IN PVOID  hContext,
   IN PVOID  pvSystemSpecific2,
   IN PVOID  pvSystemSpecific3
)
{
   NDIS_STATUS status;
   
   if (g_hNdisMiniport != NULL) {
      NdisIMDeregisterLayeredMiniport(g_hNdisMiniport);
      g_hNdisMiniport = NULL;
   }
   if (g_hNdisProtocol != NULL) {
      NdisDeregisterProtocol(&status, g_hNdisProtocol);
      g_hNdisProtocol = NULL;
   }
}

//------------------------------------------------------------------------------

NTSTATUS
DriverEntry(
   IN PDRIVER_OBJECT  pDriverObject,
   IN PUNICODE_STRING psRegistryPath
)
/*++

Routine Description:
   First entry point to be called, when this driver is loaded.
   Register with NDIS as an intermediate driver.

Arguments:
   pDriverObject - pointer to the system's driver object structure
   psRegistryPath - system's registry path for this driver

Return Value:
   STATUS_SUCCESS if all initialization is successful
   STATUS_XXX error code if not

--*/
{
   NDIS_STATUS status = NDIS_STATUS_SUCCESS;
   NDIS_STRING sName;
   NDIS_PROTOCOL_CHARACTERISTICS ndisPChars;
   NDIS_MINIPORT_CHARACTERISTICS ndisMChars;

   NdisAllocateSpinLock(&g_spinLock);

#ifdef UNDER_CE
   // We need create subkey HKLM\Comm\PASS which will be later used as place
   // where PASSTHRU IM driver will create upper miniport device instances.
   {
      LONG rc;
      HKEY hKey = NULL;

      rc = RegCreateKeyEx(
         HKEY_LOCAL_MACHINE, DMUXMINI_REGISTRY_PATH, 0, NULL, 0, 0, NULL,
         &hKey, NULL
      );
      if (rc != ERROR_SUCCESS) {
         status = NDIS_STATUS_FAILURE;
         goto cleanUp;
      }

      RegCloseKey(hKey);
   }
#endif

   NdisMInitializeWrapper(
      &g_hNdisWrapper, pDriverObject, psRegistryPath, NULL
   );
   NdisMRegisterUnloadHandler(g_hNdisWrapper, DriverUnload);

   //
   // Register the miniport with NDIS. Note that it is the miniport
   // which was started as a driver and not the protocol. Also the miniport
   // must be registered prior to the protocol since the protocol's BindAdapter
   // handler can be initiated anytime and when it is, it must be ready to
   // start driver instances.
   //

   NdisZeroMemory(&ndisMChars, sizeof(NDIS_MINIPORT_CHARACTERISTICS));

   ndisMChars.MajorNdisVersion = DMUXMINI_MAJOR_NDIS_VERSION;
   ndisMChars.MinorNdisVersion = DMUXMINI_MINOR_NDIS_VERSION;

   ndisMChars.InitializeHandler = MiniportInitialize;
   ndisMChars.QueryInformationHandler = MiniportQueryInformation;
   ndisMChars.SetInformationHandler = MiniportSetInformation;
   ndisMChars.ResetHandler = MiniportReset;
   ndisMChars.TransferDataHandler = MiniportTransferData;
   ndisMChars.HaltHandler = MiniportHalt;

   //
   // We will disable the check for hang timeout so we do not
   // need a check for hang handler!
   //
   ndisMChars.CheckForHangHandler = NULL;
   ndisMChars.ReturnPacketHandler = MiniportReturnPacket;

   //
   // Either the Send or the SendPackets handler should be specified.
   // If SendPackets handler is specified, SendHandler is ignored
   //
   ndisMChars.SendHandler = NULL;
   ndisMChars.SendPacketsHandler = MiniportSendPackets;

#ifdef NDIS51_MINIPORT
   ndisMChars.CancelSendPacketsHandler = MiniportCancelSendPackets;
   ndisMChars.PnPEventNotifyHandler = MiniportDevicePnPEvent;
   ndisMChars.AdapterShutdownHandler = MiniportAdapterShutdown;
#endif // NDIS51_MINIPORT

   status = NdisIMRegisterLayeredMiniport(
      g_hNdisWrapper, &ndisMChars, sizeof(ndisMChars), &g_hNdisMiniport
   );
   if (status != NDIS_STATUS_SUCCESS) goto cleanUp;

   //
   // Now register the protocol.
   //
   NdisZeroMemory(&ndisPChars, sizeof(ndisPChars));
   ndisPChars.MajorNdisVersion = DMUXMINI_PROT_MAJOR_NDIS_VERSION;
   ndisPChars.MinorNdisVersion = DMUXMINI_PROT_MINOR_NDIS_VERSION;

   //
   // Make sure the protocol-name matches the service-name
   // (from the INF) under which this protocol is installed.
   // This is needed to ensure that NDIS can correctly determine
   // the binding and call us to bind to miniports below.
   //

   NdisInitUnicodeString(&sName, DMUXMINI_PROTOCOL_NAME);
   ndisPChars.Name = sName;
   ndisPChars.OpenAdapterCompleteHandler = ProtocolOpenAdapterComplete;
   ndisPChars.CloseAdapterCompleteHandler = ProtocolCloseAdapterComplete;
   ndisPChars.SendCompleteHandler = ProtocolSendComplete;
   ndisPChars.TransferDataCompleteHandler = ProtocolTransferDataComplete;
   
   ndisPChars.ResetCompleteHandler = ProtocolResetComplete;
   ndisPChars.RequestCompleteHandler = ProtocolRequestComplete;
   ndisPChars.ReceiveHandler = ProtocolReceive;
   ndisPChars.ReceiveCompleteHandler = ProtocolReceiveComplete;
   ndisPChars.StatusHandler = ProtocolStatus;
   ndisPChars.StatusCompleteHandler = ProtocolStatusComplete;
   ndisPChars.BindAdapterHandler = ProtocolBindAdapter;
   ndisPChars.UnbindAdapterHandler = ProtocolUnbindAdapter;
   ndisPChars.UnloadHandler = ProtocolUnload;

   ndisPChars.ReceivePacketHandler = ProtocolReceivePacket;
   ndisPChars.PnPEventHandler= ProtocolPNPHandler;

   NdisRegisterProtocol(
      &status, &g_hNdisProtocol, &ndisPChars, sizeof(ndisPChars)
   );

   if (status != NDIS_STATUS_SUCCESS) {
      NdisIMDeregisterLayeredMiniport(g_hNdisMiniport);
      goto cleanUp;
   }

   NdisIMAssociateMiniport(g_hNdisMiniport, g_hNdisProtocol);

   if (status != NDIS_STATUS_SUCCESS) {
      NdisTerminateWrapper(g_hNdisWrapper, NULL);
      goto cleanUp;
   }

cleanUp:
   return status;
}

//------------------------------------------------------------------------------

VOID DriverUnload(
   IN PDRIVER_OBJECT pDriverObject
)
{
   NDIS_STATUS status;
   
   if (g_hNdisMiniport != NULL) {
      NdisIMDeregisterLayeredMiniport(g_hNdisMiniport);
      g_hNdisMiniport = NULL;
   }
   if (g_hNdisProtocol != NULL) {
      NdisDeregisterProtocol(&status, g_hNdisProtocol);
      g_hNdisProtocol = NULL;
   }
}

//------------------------------------------------------------------------------

#ifndef UNDER_CE

NDIS_STATUS
RegisterDevice(
   VOID
)
/*++

Routine Description:

   Register an IOCTL interface - a device object to be used for this
   purpose is created by NDIS when we call NdisMRegisterDevice.

   This routine is called whenever a new miniport instance is
   initialized. However, we only create one global device object,
   when the first miniport instance is initialized. This routine
   handles potential race conditions with DeregisterDevice via
   the g_eDeviceState and g_uiDeviceInstances variables.

   NOTE: do not call this from DriverEntry; it will prevent the driver
   from being unloaded (e.g. on uninstall).

Arguments:
   None

Return Value:
   NDIS_STATUS_SUCCESS if we successfully register a device object.

--*/
{
   NDIS_STATUS       status = NDIS_STATUS_SUCCESS;
   UNICODE_STRING    sDeviceName;
   UNICODE_STRING    sDeviceLink;
   PDRIVER_DISPATCH  apDispatch[IRP_MJ_MAXIMUM_FUNCTION];
   UINT i;

   DBGPRINT(("==>RegisterDevice\n"));

   NdisAcquireSpinLock(&g_spinLock);

   ++g_uiDeviceInstances;
   
   if (1 == g_uiDeviceInstances) {

      ASSERT(g_eDeviceState != PS_DEVICE_STATE_CREATING);

      //
      // Another thread could be running DeregisterDevice on
      // behalf of another miniport instance. If so, wait for
      // it to exit.
      //
      while (g_eDeviceState != PS_DEVICE_STATE_READY) {
         NdisReleaseSpinLock(&g_spinLock);
         NdisMSleep(10);
         NdisAcquireSpinLock(&g_spinLock);
      }

      g_eDeviceState = PS_DEVICE_STATE_CREATING;

      NdisReleaseSpinLock(&g_spinLock);

      for (i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++) {
         apDispatch[i] = Dispatch;
      }

      NdisInitUnicodeString(&sDeviceName, PASSTHRU_DEVICE_NAME);
      NdisInitUnicodeString(&sDeviceLink, PASSTHRU_LINK_NAME);

      //
      // Create a device object and register our dispatch handlers
      //
      status = NdisMRegisterDevice(
         g_hNdisWrapper, &sDeviceName, &sDeviceLink, &apDispatch[0],
         &g_pDeviceObject, &g_hDevice
      );

      NdisAcquireSpinLock(&g_spinLock);

      g_eDeviceState = PS_DEVICE_STATE_READY;
   }

   NdisReleaseSpinLock(&g_spinLock);

   DBGPRINT(("<==RegisterDevice: %x\n", status));

   return status;
}

//------------------------------------------------------------------------------

NTSTATUS
Dispatch(
   IN PDEVICE_OBJECT pDeviceObject,
   IN PIRP pIrp
)
/*++
Routine Description:
   Process IRPs sent to this device.

Arguments:
   pDeviceObject - pointer to a device object
   pIrp - pointer to an I/O Request Packet

Return Value:
   NTSTATUS - STATUS_SUCCESS always - change this when adding
   real code to handle ioctls.

--*/
{
   PIO_STACK_LOCATION  pIrpStack;
   NTSTATUS status = STATUS_SUCCESS;

   DBGPRINT(("==>Pt Dispatch\n"));
   pIrpStack = IoGetCurrentIrpStackLocation(pIrp);
     
   switch (pIrpStack->MajorFunction) {
   case IRP_MJ_CREATE:
      break;
   case IRP_MJ_CLOSE:
      break;        
   case IRP_MJ_DEVICE_CONTROL:
      break;        
   default:
      break;
   }

   pIrp->IoStatus.status = status;
   IoCompleteRequest(pIrp, IO_NO_INCREMENT);

   DBGPRINT(("<== Pt Dispatch\n"));

   return status;

} 

//------------------------------------------------------------------------------

NDIS_STATUS
DeregisterDevice(
   VOID
)
/*++

Routine Description:
   Deregister the IOCTL interface. This is called whenever a miniport
   instance is halted. When the last miniport instance is halted, we
   request NDIS to delete the device object

Arguments:
   g_hDevice - Handle returned by NdisMRegisterDevice

Return Value:
   NDIS_STATUS_SUCCESS if everything worked ok

--*/
{
   NDIS_STATUS status = NDIS_STATUS_SUCCESS;

   DBGPRINT(("==>PassthruDeregisterDevice\n"));

   NdisAcquireSpinLock(&g_spinLock);

   ASSERT(g_uiDeviceInstances > 0);
   --g_uiDeviceInstances;
   
   if (0 == g_uiDeviceInstances) {
      //
      // All miniport instances have been halted. Deregister
      // the control device.
      //
      ASSERT(g_eDeviceState == PS_DEVICE_STATE_READY);

      //
      // Block RegisterDevice() while we release the control
      // device lock and deregister the device.
      // 
      g_eDeviceState = PS_DEVICE_STATE_DELETING;

      NdisReleaseSpinLock(&g_spinLock);

      if (g_hDevice != NULL) {
         status = NdisMDeregisterDevice(g_hDevice);
         g_hDevice = NULL;
      }

      NdisAcquireSpinLock(&g_spinLock);
      g_eDeviceState = PS_DEVICE_STATE_READY;
   }

   NdisReleaseSpinLock(&g_spinLock);

   DBGPRINT(("<== PassthruDeregisterDevice: %x\n", status));
   return status;
}

#endif // UNDER_CE

//------------------------------------------------------------------------------
