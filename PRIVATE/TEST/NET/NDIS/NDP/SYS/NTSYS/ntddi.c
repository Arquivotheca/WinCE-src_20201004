//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
NT Support routines.
ntddi.c
*/
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef UNDER_CE

#include "Ntddi.h"
#include "NDP.h"

NDP_PROTOCOL g_sNtProto = {0};

#define ACQUIRE_GLOBAL_LOCK()       NdisAcquireSpinLock(&g_sNtProto.Lock)
#define RELEASE_GLOBAL_LOCK()       NdisReleaseSpinLock(&g_sNtProto.Lock)

#pragma NDIS_INIT_FUNCTION(DriverEntry)

NTSTATUS DriverEntry(
   IN  PDRIVER_OBJECT      pDriverObject,
   IN  PUNICODE_STRING     pRegistryPath
   )
/*++

Routine Description:
    This is the "init" routine, called by the system when the NDP
    module is loaded. We initialize all our global objects, fill in our
    Dispatch and Unload routine addresses in the driver object, and create
    a device object for receiving I/O requests.

Arguments:
    pDriverObject   - Pointer to the driver object created by the system.
    pRegistryPath   - Pointer to our global registry path. This is ignored.

Return Value:
    NT Status code: STATUS_SUCCESS if successful, error code otherwise.

--*/
{
   NDIS_STATUS                     Status;
   NDIS_PROTOCOL_CHARACTERISTICS   PChars;
   PNDIS_CONFIGURATION_PARAMETER   Param;
   PDEVICE_OBJECT                  pDeviceObject;
   UNICODE_STRING                  DeviceName;
   UNICODE_STRING                  SymbolicName;
   HANDLE                          ThreadHandle;
   int                             i;

   TraceIn(DriverEntry);
/*
#if DBG
   DbgPrint("AtmSmDebugFlag:  Address = %p  Value = 0x%x\n", 
      &AtmSmDebugFlag, AtmSmDebugFlag);
#endif
*/

//   DbgLoud(("Sizeof TCHAR = %hu\n",sizeof(TCHAR)));

   
   NtProtoInitializeGlobal(pDriverObject);

   //
   // Initialize the debug memory allocator
   //
//   ATMSM_INITIALIZE_AUDIT_MEM();
   
   //
   //  Initialize the Driver Object.
   //
   pDriverObject->DriverUnload   = NtProtoUnload;
   pDriverObject->FastIoDispatch = NULL;

   for(i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++)
   {
      pDriverObject->MajorFunction[i] = NtProtoDispatch;
   }

   do
   { // break off loop

      //
      // Create a device object for atm sample module.  The device object
      // is used by the user mode app for I/O
      //
      RtlInitUnicodeString(&DeviceName, NTPROTO_DEVICE_NAME);

      Status = IoCreateDevice(
                  pDriverObject,
                  0,
                  &DeviceName,
                  FILE_DEVICE_NETWORK,
                  FILE_DEVICE_SECURE_OPEN,
                  FALSE,
                  &pDeviceObject
                  );
      if(NDIS_STATUS_SUCCESS != Status)
      {
		  /*
         DbgErr(("Failed to create device object - Error - 0x%X\n",
            Status)); */ 
         break;
      }

      // we are doing buffered and not direct IO for sends and recvs
      pDeviceObject->Flags        |= DO_BUFFERED_IO; //DO_DIRECT_IO;

      // Save the device object
      g_sNtProto.pDeviceObject    = pDeviceObject;

      g_sNtProto.ulInitSeqFlag   |= CREATED_IO_DEVICE;

      //
      // Set up a symbolic name for interaction with the user-mode
      // application.
      //
      RtlInitUnicodeString(&SymbolicName, NTPROTO_SYMBOLIC_NAME);
      IoCreateSymbolicLink(&SymbolicName, &DeviceName);

      g_sNtProto.ulInitSeqFlag   |= REGISTERED_SYM_NAME;

      //
      //  We are doing direct I/O.
      //
      pDeviceObject->Flags |= DO_DIRECT_IO;

      //
      // Now register the protocol.
      //
      Status = NDPInitProtocol(&g_sNtProto);

      if(NDIS_STATUS_SUCCESS != Status)
      {
		  /*
         DbgErr(("Failed to Register protocol - Error - 0x%X\n",
            Status));
			*/
         break;
      }

      g_sNtProto.ulInitSeqFlag   |= REGISTERED_WITH_NDIS;

   }while(FALSE);


   if(NDIS_STATUS_SUCCESS != Status)
   {
      //
      //  Clean up will happen in Unload routine
      //
   }

   TraceOut(DriverEntry);

   return(Status);
}


VOID NtProtoInitializeGlobal(
   IN  PDRIVER_OBJECT      pDriverObject
   )
{
   NdisZeroMemory(&g_sNtProto, sizeof(NDP_PROTOCOL));

   g_sNtProto.magicCookie = NDP_PROTOCOL_COOKIE;

   g_sNtProto.pDriverObject = pDriverObject;

   NdisAllocateSpinLock(&g_sNtProto.Lock);
}


VOID NtProtoCleanupGlobal()
{
   NdisFreeSpinLock(&g_sNtProto.Lock);
}


VOID NtProtoUnload(
   IN  PDRIVER_OBJECT              pDriverObject
   )
/*++

Routine Description:
    This routine is called by the system prior to unloading us.
    Currently, we just undo everything we did in DriverEntry,
    that is, de-register ourselves as an NDIS protocol, and delete
    the device object we had created.

Arguments:
    pDriverObject   - Pointer to the driver object created by the system.

Return Value:
    None

--*/
{
   UNICODE_STRING          SymbolicName;
   NDIS_STATUS             Status;

   TraceIn(Unload);

   // call the shutdown handler
   NtProtoShutDown();

   // Remove the Symbolic Name created by us
   if(0 != (g_sNtProto.ulInitSeqFlag & REGISTERED_SYM_NAME))
   {
      RtlInitUnicodeString(&SymbolicName, NTPROTO_SYMBOLIC_NAME);
      IoDeleteSymbolicLink(&SymbolicName);
   }

   // Remove the Device created by us
   if(0 != (g_sNtProto.ulInitSeqFlag & CREATED_IO_DEVICE))
   {
      IoDeleteDevice(g_sNtProto.pDeviceObject);
   }

//   ATMSM_SHUTDOWN_AUDIT_MEM();

   NtProtoCleanupGlobal();

   TraceOut(Unload);

   return;
}


NTSTATUS NtProtoDispatch(
   IN  PDEVICE_OBJECT  pDeviceObject,
   IN  PIRP            pIrp
   )
/*++

Routine Description:

    This is the common dispath routine for user Ioctls

Arguments:

Return Value:

    None

--*/
{
   NTSTATUS            Status = NDIS_STATUS_SUCCESS;
   ULONG               ulBytesWritten = 0;
   PIO_STACK_LOCATION  pIrpSp;

   TraceIn(AtmSmDispatch);

   //
   // Get current Irp stack location
   //
   pIrpSp = IoGetCurrentIrpStackLocation(pIrp);

   pIrp->IoStatus.Information = 0;

   switch(pIrpSp->MajorFunction)
   {

      case IRP_MJ_CREATE: {

		    NDP_PROTOCOL* pDevice = &g_sNtProto;
			NDP_ADAPTER* pAdapter = NULL;

            //DbgLoud(("IRP_MJ_CREATE\n"));
			// Allocate instance structure
			if (NDIS_STATUS_SUCCESS != NdisAllocateMemoryWithTag(&pAdapter, sizeof(NDP_ADAPTER),NDP_ADAPTER_COOKIE))
			{
				break;
			}

			NdisZeroMemory(pAdapter, sizeof(NDP_ADAPTER));
			pAdapter->magicCookie = NDP_ADAPTER_COOKIE;
			
			// Link instance back to device
			pAdapter->pProtocol = pDevice;
			
			NDPInitAdapter(pAdapter);

			pDevice->pAdapterList = pAdapter;
            InterlockedIncrement(&pDevice->ulNumCreates);
            Status = STATUS_SUCCESS;

            break;
         }

      case IRP_MJ_CLOSE: {

            //DbgLoud(("IRP_MJ_CLOSE\n"));

            Status = STATUS_SUCCESS;

            break;
         }

      case IRP_MJ_CLEANUP: {

		    NDP_ADAPTER* pAdapter = g_sNtProto.pAdapterList;

            //DbgLoud(("IRP_MJ_CLEANUP\n"));

			NDPDeInitAdapter(pAdapter);
			// Delete instance object
			NdisFreeMemory(pAdapter, sizeof(NDP_ADAPTER), 0);

            InterlockedDecrement(&g_sNtProto.ulNumCreates);
			g_sNtProto.pAdapterList = NULL;

			Status = STATUS_SUCCESS;

            break;
         }

      case IRP_MJ_DEVICE_CONTROL: {

		    //Note that since our all the codes are based on "METHOD_BUFFERED", the following is according to
		    //Buffered_IO.
		    ULONG lengthIn = pIrpSp->Parameters.DeviceIoControl.InputBufferLength;
			ULONG lengthOut = pIrpSp->Parameters.DeviceIoControl.OutputBufferLength;
			ULONG code = pIrpSp->Parameters.DeviceIoControl.IoControlCode;
			PBYTE pBufferIn = pIrp->AssociatedIrp.SystemBuffer;
			PBYTE pBufferOut = pIrp->AssociatedIrp.SystemBuffer;
			UINT size;
			ULONG * pActualLengthOut = &ulBytesWritten;
			PNDP_OID_MP pOid;

			BOOL ok = FALSE;
			NDP_ADAPTER* pAdapter = g_sNtProto.pAdapterList;
			
			ASSERT(pAdapter != NULL);
			if (pAdapter == NULL)
				break;

			switch (code)
			{
			case IOCTL_NDP_OPEN_MINIPORT:
				ok = ndp_OpenAdapter(pAdapter, (LPCTSTR)pBufferIn);
				if (ok)
					Status = STATUS_SUCCESS;
				break;
			case IOCTL_NDP_CLOSE_MINIPORT:
				ok = ndp_CloseAdapter(pAdapter);
				break;
			case IOCTL_NDP_SEND_PACKET:
				// Check input buffer size
				if (
					pAdapter->hAdapter == NULL ||
					lengthIn < sizeof(NDP_SEND_PACKET_INP) - 1
					) break;
				size = sizeof(NDP_SEND_PACKET_INP) - 1;
				size += ((NDP_SEND_PACKET_INP*)pBufferIn)->dataSize;
				if (lengthIn < size) break;
				// Execute         
				ok = ndp_SendPacket(pAdapter, (NDP_SEND_PACKET_INP*)pBufferIn);
				break;
			case IOCTL_NDP_LISTEN:
				// Check input/output buffer size
				if (
					pAdapter->hAdapter == NULL ||
					lengthIn != sizeof(NDP_LISTEN_INP)
					) break;
				// Execute         
				ok = ndp_Listen(pAdapter, (NDP_LISTEN_INP*)pBufferIn);
				break;
			case IOCTL_NDP_RECV_PACKET:
				// Check input/output buffer size
				if (
					pAdapter->hAdapter == NULL ||
					lengthIn != sizeof(NDP_RECV_PACKET_INP) ||
					lengthOut < sizeof(NDP_RECV_PACKET_OUT) - 1
					) break;
				size = lengthOut - sizeof(NDP_RECV_PACKET_OUT) + 1;
				((NDP_RECV_PACKET_OUT*)pBufferOut)->dataSize = size;
				// Execute         
				ok = ndp_RecvPacket(
					pAdapter, (NDP_RECV_PACKET_INP*)pBufferIn, 
					(NDP_RECV_PACKET_OUT*)pBufferOut
					);
				size = ((NDP_RECV_PACKET_OUT*)pBufferOut)->dataSize;
				size += sizeof(NDP_RECV_PACKET_OUT) - 1;
				*pActualLengthOut = size;
				break;
			case IOCTL_NDP_STRESS_SEND:
				// Check input/output buffer size
				if (
					pAdapter->hAdapter == NULL ||
					lengthIn != sizeof(NDP_STRESS_SEND_INP) ||
					lengthOut != sizeof(NDP_STRESS_SEND_OUT)
					) break;
				// Execute         
				ok = ndp_StressSend(
					pAdapter, (NDP_STRESS_SEND_INP*)pBufferIn, 
					(NDP_STRESS_SEND_OUT*)pBufferOut
					);
				*pActualLengthOut = sizeof(NDP_STRESS_SEND_OUT);
				break;
			case IOCTL_NDP_STRESS_RECV:
				// Check input/output buffer size
				if (
					pAdapter->hAdapter == NULL ||
					lengthIn != sizeof(NDP_STRESS_RECV_INP) ||
					lengthOut < sizeof(NDP_STRESS_RECV_OUT) - 1
					) break;
				// Set maximal data size
				size = lengthOut - sizeof(NDP_STRESS_RECV_OUT) + 1;
				((NDP_STRESS_RECV_OUT*)pBufferOut)->dataSize = size;
				// Execute         
				ok = ndp_StressReceive(
					pAdapter, (NDP_STRESS_RECV_INP*)pBufferIn, 
					(NDP_STRESS_RECV_OUT*)pBufferOut
					);
				// Get actual return size         
				size = ((NDP_STRESS_RECV_OUT*)pBufferOut)->dataSize;
				size += sizeof(NDP_STRESS_RECV_OUT) - 1;
				*pActualLengthOut = size;
				break;
				
			case IOCTL_NDP_MP_OID:
				pOid = (PNDP_OID_MP) pBufferIn;
				// Check input/output buffer size
				if (
					(pAdapter->hAdapter == NULL) ||
					(lengthIn != sizeof(NDP_OID_MP)) ||
					(pBufferIn == NULL)
					) break;
				// Execute
				if (pOid->eType == QUERY)
				{//If its Query OID.
					Status = QueryOid(pAdapter,pOid->oid,pOid->lpInOutBuffer,pOid->pnInOutBufferSize);
					if (Status == NDIS_STATUS_SUCCESS) ok = TRUE;
				}
				else
				{
					if (pOid->eType == SET)
					{//If its SET OID.
						Status = SetOid(pAdapter,pOid->oid,pOid->lpInOutBuffer,pOid->pnInOutBufferSize);
						if (Status == NDIS_STATUS_SUCCESS) ok = TRUE;
					}
					else
						break;
				}
				
				pOid->status = Status;
				//Just for now.
				*pActualLengthOut = sizeof(NDP_OID_MP);
				break;
				
				
			default :
				ok = TRUE;
				Status = STATUS_INVALID_DEVICE_REQUEST;
				break;
			}

			if (!ok)
			{
				Status = NDIS_STATUS_FAILURE;
			}

            break;
         }

      default: {

            /*DbgErr(("Unknown IRP_MJ_XX - %x\n",pIrpSp->MajorFunction));*/

            Status = STATUS_INVALID_PARAMETER;

            break;
         }
   }


   if(STATUS_PENDING != Status)
   {
      pIrp->IoStatus.Status = Status;
	  pIrp->IoStatus.Information = ulBytesWritten;
      IoCompleteRequest(pIrp, IO_NETWORK_INCREMENT);
   }

   TraceOut(AtmSmDispatch);

   return Status;
}



VOID NtProtoShutDown()
/*++

Routine Description:
    Called when the system is being shutdown.
    Here we unbind all the adapters that we bound to previously.

Arguments:
    None

Return Value:
    None

--*/
{
   NDP_ADAPTER*  pAdapt, pNextAdapt;
   NDIS_STATUS     Status;
#if DBG
   KIRQL           EntryIrql, ExitIrql;
#endif

   TraceIn(NtProtoShutDown);

   NDP_GET_ENTRY_IRQL(EntryIrql);

   // Deregister from NDIS
   if(0 != (g_sNtProto.ulInitSeqFlag & REGISTERED_WITH_NDIS))
	   NDPDeInitProtocol(&g_sNtProto);

   NDP_CHECK_EXIT_IRQL(EntryIrql, ExitIrql);

   TraceOut(NtProtoShutDown);
}


#endif