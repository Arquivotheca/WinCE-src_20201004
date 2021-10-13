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
   protocol.c

Abstract:
   The Ndis DMUX Intermediate Miniport driver sample. This is based on passthru driver 
   which doesn't touch packets at all. The protocol API is implemented
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

//------------------------------------------------------------------------------

#define MAX_PACKET_POOL_SIZE     4096
#define MIN_PACKET_POOL_SIZE     64

//------------------------------------------------------------------------------

#define PASSTHRU_DMUX_MEMORY_TAG            'Dmux'
#define DMUX_MINIPORT_SUFFIX           L"\\DMUX"

// The following size also includes instance number <1,2,3 etc.>
#define DMUX_MINIPORT_SUFFIX_SIZE      (6)

//------------------------------------------------------------------------------

UCHAR* GetRandomAddrEx(NDIS_MEDIUM ndisMedium, DWORD dwVar, PUCHAR pucAddr)
{
   if (pucAddr == NULL)
	   return NULL;
   
   switch (ndisMedium) {
   case NdisMedium802_3:
   case NdisMediumFddi:
   case NdisMedium802_5:
	  // Hoping that CeGenRandom may be relying on the initial values of buffer content.
	  *(DWORD *)&pucAddr[1] = dwVar;
	   
	  if (CeGenRandom(6,pucAddr))
	  {
		  pucAddr[0] = pucAddr[0] & 0xFE; // Make it unicast
		  pucAddr[0] = pucAddr[0] | 0x02; // make it local
	  }
	  else
	  {
          pucAddr[0] = 0x02; pucAddr[5] = 0x06; 
	  }

      break;
   }
   return pucAddr;
}

//------------------------------------------------------------------------------

WCHAR wszDigitToWchar[] = { L'0', L'1', L'2', L'3', L'4', L'5',
							L'6', L'7', L'8', L'9' };

//------------------------------------------------------------------------------


BOOL NumberToWchar(DWORD dwNum, PWCHAR pwsz, PDWORD pdwszSize)
{
	DWORD dw,j,i=0;

	if (dwNum > 999)
		return FALSE;

	while (dwNum)
	{
		dw = dwNum % 10;
		pwsz[i] = wszDigitToWchar[dw];
		dwNum = dwNum / 10;
		++i;
	}

	for (j=0;j<(i/2);++j)
	{
		WCHAR ch = pwsz[j];
		pwsz[j] = pwsz[i-j-1];
		pwsz[i-j-1] = ch;
	}

	*pdwszSize = i;
	return TRUE;
}

//------------------------------------------------------------------------------

// Unicast packet : Only to one interface: pDMUXIf = Pointer to interface
// Broadcast packet : Indicate to all interfaces: pDMUXIf = 0xFFFFFFFF
// Mcast packet : Indicate to those interfaces which have mcast addresses
// Badcast packet : if packet is not to be indicated pDMUXIf = NULL.
void FilterOnDstMacAddr(PBYTE pPktDstMacAddr, BINDING * pBinding, PsDMUX * ppDMUXIf)
{
	DWORD i;
	BYTE pbBroadcastAddr[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
	*ppDMUXIf = NULL;
	
	//Lets first verify if the packet is unicast/broadcast/multicast
   if (NdisEqualMemory(
      pbBroadcastAddr, pPktDstMacAddr, 6)) 
   {
	   *ppDMUXIf = (PsDMUX)0xFFFFFFFF;
	   return;
   }

   if (pPktDstMacAddr[0] & 0x1)
   {
	   // This is multicast address. We'll have to do extra things so for now ignore.
	   return;	   
   }

	for (i=0;i < pBinding->sPASSDMUXSet.dwDMUXnos; ++i)
	{
		PsDMUX psDMUX;
		psDMUX = pBinding->sPASSDMUXSet.ArrayDMUX[i];

		if (psDMUX->hMPBinding == NULL) continue;

		if (NdisEqualMemory(psDMUX->pbMacAddr, pPktDstMacAddr, 6))
		{
			*ppDMUXIf = psDMUX;
			return;
		}
	}
}

//------------------------------------------------------------------------------

void FilterRecvPacket(PNDIS_PACKET pPacket, BINDING * pBinding, PsDMUX * ppDMUXIf)
{
	PBYTE pbBuff = NULL;
	PNDIS_BUFFER pBuffd = (PNDIS_BUFFER) pPacket->Private.Head;
	UINT Length;

	NdisQueryBuffer(pBuffd, &pbBuff, &Length);

	FilterOnDstMacAddr(pbBuff,pBinding, ppDMUXIf);
}

//------------------------------------------------------------------------------

VOID
ProtocolBindAdapter(
   OUT PNDIS_STATUS pStatus,
   IN NDIS_HANDLE hBindContext,
   IN PNDIS_STRING psAdapterName,
   IN PVOID pvSystemSpecific1,
   IN PVOID pvSystemSpecific2
)
/*++

Routine Description:
   Called by NDIS to bind to a miniport below.

Arguments:
   pStatus - Return status of bind here.
   hBindContext - Can be passed to NdisCompleteBindAdapter if this call is 
      pended.
   psAdapterName - Device name to bind to. This is passed to NdisOpenAdapter.
   pvSystemSpecific1 - Can be passed to NdisOpenProtocolConfiguration to read 
      per-binding information
   pvSystemSpecific2 - Unused

Return Value:
   NDIS_STATUS_PENDING - If this call is pended. In this case call
      NdisCompleteBindAdapter to complete.
   NDIS_xxxx - Completes this call synchronously

--*/
{
   NDIS_STATUS status;
   NDIS_STATUS statusOpen;
   NDIS_HANDLE hConfig = NULL;
   NDIS_STRING sUnbind = NDIS_STRING_CONST("Unbind");
   PNDIS_CONFIGURATION_PARAMETER pValue = NULL;
   BINDING* pBinding = NULL;
   UINT uiMediumIndex;
   ULONG ulSize;

   //BOOLEAN bVMiniportDMUXed = FALSE;
   DWORD dwNosOfDMUX = 4;
   DWORD cbCounter = 1;
   DWORD i=0;
   DWORD dwMacSeed = 0x5388;

#ifdef UNDER_CE
   BOOLEAN bProtocolUnbind = FALSE;
   LPWSTR mszProtocols = NULL;
   TCHAR mszOldProtocols[256] = _T("");
   DWORD dwOldProtocolsSize = 0;
   WCHAR wszSkipTheseNICs[][16] = { {L"L2TP"},{L"PPTP"},{L"ASYNCMAC"},{L"IRSIR"},{L"PPPOE"},{L'\0'} };
#endif

   DBGPRINT(("==>DMUXMINI::ProtocolBindAdapter %S\n", psAdapterName->Buffer));

#ifdef UNDER_CE

   // We have to avoid recursive binding
   if (NdisEqualMemory(
      psAdapterName->Buffer, DMUXMINI_MINIPORT_PREFIX, 
      DMUXMINI_MINIPORT_PREFIX_SIZE * sizeof(WCHAR)
   )) {
      status = NDIS_STATUS_NOT_ACCEPTED;
      goto cleanUp;
   }

   //
   // This is to skip certain adapters to which passthru can not bind, which is required mostly when
   // passthru is loaded dynamically using "s ndisconfig miniport add passthru". At boot time 
   // passthru is invoked to bind before any protocol driver & hence following is not required for
   // that case. Note that adapters like IRSIR1 don't like even getting unbound from IRSIR & 
   // hence best way is to avoid binding to such adapters.
   // 
   while (wszSkipTheseNICs[i][0] != L'\0')
   {
	   NDIS_STRING sSkipThis;
	   NdisInitUnicodeString(&sSkipThis,wszSkipTheseNICs[i]);

	   //Using NdisEqualMemory instead of NdisEqualString to avoid instance number 
	   //Eg. IRSIR1 compared with IRSIR
	   if (NdisEqualMemory(
		   psAdapterName->Buffer, sSkipThis.Buffer,sSkipThis.Length)) {
		   status = NDIS_STATUS_NOT_ACCEPTED;
		   goto cleanUp;
	   }
	   ++i;
   }

   //Lets read config. value for DMUXMINI
   do
   {
	   WCHAR szPASSPath[128];
	   WCHAR szBindTo[256];
	   LONG rc;
	   HKEY hKey1 = NULL;
	   DWORD dwSize = 0;
	   DWORD dwValue = 0;

   	   swprintf(szPASSPath, L"Comm\\%s", DMUXMINI_PROTOCOL_NAME);
	   rc = RegOpenKeyEx(HKEY_LOCAL_MACHINE, szPASSPath, 0, 0, &hKey1);
	   if (rc != ERROR_SUCCESS) {
		   status = NDIS_STATUS_FAILURE;
		   goto cleanUp;
	   }
		   
	   dwValue = 0;dwSize = sizeof(DWORD);
	   rc = RegQueryValueEx(
		   hKey1, L"Instances", NULL, NULL, (PBYTE)&dwValue, &dwSize);
	   if (rc == ERROR_SUCCESS) {
		   cbCounter = dwValue;
	   }

	   dwValue = 0;dwSize = sizeof(DWORD);
	   rc = RegQueryValueEx(
		   hKey1, L"MacSeed", NULL, NULL, (PBYTE)&dwValue, &dwSize);
	   if (rc == ERROR_SUCCESS) {
		   dwMacSeed = dwValue;
	   }

	   dwSize = sizeof(szBindTo);
	   rc = RegQueryValueEx(
		   hKey1, L"BindTo", NULL, NULL, (PBYTE)szBindTo, &dwSize
		   );

	   if (rc == ERROR_SUCCESS) {
		   NDIS_STRING nszBindTo;
		   NdisInitUnicodeString(&nszBindTo,szBindTo);

		   if(!NdisEqualString(&nszBindTo,psAdapterName,TRUE) )
		   {
			   status = NDIS_STATUS_NOT_ACCEPTED;
			   goto cleanUp;
		   }
	   }

	   RegCloseKey(hKey1);
	   
   } while (0);

#endif

   //
   // Access the configuration section for our binding-specific
   // parameters. If there is no protocol configuration use default settings.
   //
   NdisOpenProtocolConfiguration(&status, &hConfig, pvSystemSpecific1);
   if (status == NDIS_STATUS_SUCCESS) {
   
      //
      // Read the "Unbind" reserved key that can control which protocols will
      // be unbinded from the adapter after it's open
      //
      NdisReadConfiguration(
         &status, &pValue, hConfig, &sUnbind,  NdisParameterMultiString
      );
      if (status != NDIS_STATUS_SUCCESS) pValue = NULL;
   }
   
   //
   // Allocate memory for the Adapter structure. This represents both the
   // protocol context as well as the adapter structure when the miniport
   // is initialized.
   //
   // In addition to the base structure, allocate space for the device
   // instance string.
   //
   ulSize = sizeof(BINDING);
/*
   ulSize += DMUXMINI_MINIPORT_PREFIX_SIZE * sizeof(WCHAR);
   ulSize += psAdapterName->MaximumLength;
*/

   NdisAllocateMemoryWithTag(&pBinding, ulSize, DMUXMINI_MEMORY_TAG);
   if (pBinding == NULL) {
      status = NDIS_STATUS_RESOURCES;
      goto cleanUp;
   }

   NdisInterlockedIncrement(&pBinding->nRef);

   //
   // Initialize the adapter structure. We should create there adapter name 
   // for IM instance. We should save it as well, because we may need to use it
   // in a call to NdisIMCancelInitializeDeviceInstance. We only prepare name
   // and finish it after we succesfully open the underlying adapter.
   //
   NdisZeroMemory(pBinding, ulSize);

/*
   NdisMoveMemory(
      (UCHAR*)pBinding + sizeof(BINDING), DMUXMINI_MINIPORT_PREFIX, 
      DMUXMINI_MINIPORT_PREFIX_SIZE * sizeof(WCHAR)
   );
   NdisMoveMemory(
      (UCHAR*)pBinding + sizeof(BINDING) + 
      DMUXMINI_MINIPORT_PREFIX_SIZE * sizeof(WCHAR),
      psAdapterName->Buffer, psAdapterName->MaximumLength
   );
   pBinding->sDeviceName.MaximumLength = psAdapterName->MaximumLength
      + DMUXMINI_MINIPORT_PREFIX_SIZE * sizeof(WCHAR);
   pBinding->sDeviceName.Length = psAdapterName->Length 
      + DMUXMINI_MINIPORT_PREFIX_SIZE * sizeof(WCHAR);
   pBinding->sDeviceName.Buffer = (LPWSTR)((UCHAR*)pBinding + sizeof(BINDING));
*/

   NdisInitializeEvent(&pBinding->hEvent);

   //
   // Allocate a packet pool for sends. We need this to pass sends down.
   // We cannot use the same packet descriptor that came down to our send
   // handler (see also NDIS 5.1 packet stacking).
   //
   NdisAllocatePacketPoolEx(
      &status, &pBinding->hSendPacketPool, MIN_PACKET_POOL_SIZE,
      MAX_PACKET_POOL_SIZE - MIN_PACKET_POOL_SIZE, sizeof(PASSTHRU_PR_SEND)
   );
   if (status != NDIS_STATUS_SUCCESS) goto cleanUp;

   //
   // Allocate a packet pool for receives. We need this to indicate receives.
   // Same consideration as sends (see also NDIS 5.1 packet stacking). Because
   // NdisMIndicateReceivePacket is used we need to allocate at least 
   // PROTOCOL_RESERVED_SIZE_IN_PACKET.
   //
   NdisAllocatePacketPoolEx(
      &status, &pBinding->hRecvPacketPool, MIN_PACKET_POOL_SIZE,
      MAX_PACKET_POOL_SIZE - MIN_PACKET_POOL_SIZE, 
      PROTOCOL_RESERVED_SIZE_IN_PACKET + sizeof(PASSTHRU_PR_RECV)
   );
   if (status != NDIS_STATUS_SUCCESS) goto cleanUp;

#ifdef UNDER_CE
   //
   // There we should unbind other protocols from an adapter
   //
   if (pValue != NULL) mszProtocols = pValue->ParameterData.StringData.Buffer;

   if (mszProtocols == NULL || *mszProtocols != _T('\0')) {

      // We try to save current binding to registry for rebind on unload
      NdisGetAdapterBindings(
         &status, psAdapterName->Buffer, (UCHAR*)mszOldProtocols, 
         sizeof(mszOldProtocols), &dwOldProtocolsSize
      );
      if (status != NDIS_STATUS_SUCCESS) dwOldProtocolsSize = 0;

      // And we unbind      
      NdisUnbindProtocolsFromAdapter(
         &status, psAdapterName->Buffer, mszProtocols
      );
      if (status != NDIS_STATUS_SUCCESS) goto cleanUp;
      bProtocolUnbind = TRUE;
   }
#endif

   //
   // Now open the adapter below and complete the initialization
   //
   NdisOpenAdapter(
      &status, &statusOpen, &pBinding->hPTBinding, &uiMediumIndex, 
      g_aNdisMedium, sizeof(g_aNdisMedium)/sizeof(NDIS_MEDIUM), g_hNdisProtocol, 
      pBinding, psAdapterName, 0, NULL
   );
   if (status == NDIS_STATUS_PENDING) {
      NdisWaitEvent(&pBinding->hEvent, 0);
      status = pBinding->status;
   }
   if (status != NDIS_STATUS_SUCCESS) goto cleanUp;

   pBinding->medium = g_aNdisMedium[uiMediumIndex];

   pBinding->sPASSDMUXSet.dwDMUXnos = 0;
   //cbCounter = dwNosOfDMUX;
   
   /*
   //bVMiniportDMUXed & dwNosOfDMUX will be read from registry or predefined constants
   //if (bVMiniportDMUXed)
   
	 {
	 sDMUXSet * psDMUXSet = NULL;
	 
	   DWORD dw = sizeof(sDMUX);
	   dw = sizeof(sDMUXSet);
	   
		 //Allocate memory for sDMUXSet.
		 NdisAllocateMemoryWithTag(&psDMUXSet, dw, PASSTHRU_DMUX_MEMORY_TAG);
		 if (pBinding == NULL) {
		 status = NDIS_STATUS_RESOURCES;
		 goto cleanUp;
		 }
		 
		   // Let pBinding knows that we have DMUX
		   pBinding->pulDMUXSet = (PULONG)psDMUXSet;
		   }
   */
   
   //
   // Set the flag below to synchronize with a possible call
   // to our protocol Unbind handler that may come in before
   // our miniport initialization happens.
   //
   
   pBinding->bMiniportInitPending = TRUE;
   // Reset event because it can be set above and we want to reuse it
   NdisResetEvent(&pBinding->hEvent);

   for (i=0;(i<cbCounter) && (i<MAX_DMUX_INTERFACES);++i)
   {
	   LONG rc;
	   WCHAR szPath1[128], szPath2[128], szUpperBind[256];
	   HKEY hKey1 = NULL, hKey2 = NULL;
	   DWORD dwValue = 0;
	   DWORD dwSize = sizeof(dwValue);
	   sDMUXSet * psDMUXSet = &(pBinding->sPASSDMUXSet);
	   
	   DWORD dw = 0;
	   NDIS_STRING * psVMiniPortInstanceName = NULL;
	   PsDMUX pDMUXIf = NULL;
	   PBYTE pb = NULL;
	   
	   //if (bVMiniportDMUXed)
	   {
		   dw = sizeof(sDMUX);

		   //First allocate the memory for DMUX adapter
		   NdisAllocateMemoryWithTag(&pDMUXIf, dw, PASSTHRU_DMUX_MEMORY_TAG);
		   if (pDMUXIf == NULL) {
			   status = NDIS_STATUS_RESOURCES;
			   goto cleanUp;
		   }

		   // Now lets set the name for DMUX adapter
		   // DMUX\<adapter instance>1 , DMUX\<adapter instance>2 etc.
		   psDMUXSet->ArrayDMUX[i] = pDMUXIf;
		   psVMiniPortInstanceName = &(pDMUXIf->sDeviceName);
		   
		   //Zero all the content of sDMUX If
		   NdisZeroMemory(pDMUXIf, sizeof(sDMUX));
		   
		   // First sets the pointer to pBinding
		   pDMUXIf->pBinding = pBinding;
		   pBinding->dwDMUXIfIndex = i;
		   
		   // Now name.size. If the string is NULL terminated, Length does not include the trailing NULL.
		   pDMUXIf->sDeviceName.Length = psAdapterName->Length + (DMUXMINI_MINIPORT_PREFIX_SIZE) * sizeof(WCHAR);

		   // Now name.MaxSize. The maximum length in bytes of Buffer
		   pDMUXIf->sDeviceName.MaximumLength = 61 * sizeof(WCHAR);
		   
		   // Verify following.
		   ASSERT(pDMUXIf->sDeviceName.MaximumLength > pDMUXIf->sDeviceName.Length);
		   
		   // Now name.buffer
		   pDMUXIf->sDeviceName.Buffer = pDMUXIf->pwzcMacName;

		   dw = DMUXMINI_MINIPORT_PREFIX_SIZE * sizeof(WCHAR);
		   pb = (PBYTE) pDMUXIf->sDeviceName.Buffer;

		   NdisMoveMemory(pb , DMUXMINI_MINIPORT_PREFIX, dw);

		   pb = pb + dw;
		   dw = psAdapterName->Length;

		   NdisMoveMemory(pb, psAdapterName->Buffer, dw);

		   pb = pb + dw;
		   dw = 3; // Here 3 wide chars

		   // Get instance number in name.
		   NumberToWchar(i+1, ((WCHAR *)pb), &dw);

   		   // account for instance number in length.
		   pDMUXIf->sDeviceName.Length += ((USHORT)dw) * sizeof(WCHAR);

		   //((WCHAR *)pb)[0] = wszDigitToWchar[i+1];
	   
		   // Set the Mac addr
		   GetRandomAddrEx(NdisMedium802_3, ++dwMacSeed, pDMUXIf->pbMacAddr);
	   }

#ifdef UNDER_CE
	   
	   //
	   // Following code is workaround about fact that CE requires 
	   // HKLM\Comm\DMUX\<InstanceName>\Parms registry key with DWORD values 
	   // named BusNumber and BusType - so we have to create them if they don't
	   // exist. We manage also situation when there is an UpperBind definition
	   // in underlaying miniport - in such case we copy it to IM virtual miniport.
	   // Because ProtocolBindAdapeter can be called inside NDIS HKLM\Comm
	   // enumeration we can't add key to HKLM\Comm so IM miniport instance key
	   // must be created with prefix (which move IM miniport instance key to prefix
	   // subkey.
	   //
	   // Remark that previous sample version does use prefix $ for miniport name.
	   // It caused problems on enumeration made by NDIS on HKLM\Comm when it is
	   // loaded.
	   //
	   //
	   // If there already is the parameters key skip all
	   //
	   swprintf(szPath1, L"Comm\\%s\\Parms", psVMiniPortInstanceName->Buffer);
	   rc = RegOpenKeyEx(HKEY_LOCAL_MACHINE, szPath1, 0, 0, &hKey1);
	   if (rc != ERROR_SUCCESS) {
		   
		   rc = RegCreateKeyEx(
			   HKEY_LOCAL_MACHINE, szPath1, 0, NULL, 0, 0, NULL, &hKey1, NULL
			   );
		   if (rc != ERROR_SUCCESS) {
			   status = NDIS_STATUS_FAILURE;
			   goto cleanUp;
		   }
		   
		   dwValue = 0;
		   rc = RegSetValueEx(
			   hKey1, L"BusNumber", 0, REG_DWORD, (PBYTE)&dwValue, sizeof(dwSize)
			   );
		   if (rc != ERROR_SUCCESS) {
			   RegCloseKey(hKey1);
			   status = NDIS_STATUS_FAILURE;
			   goto cleanUp;
		   }      
		   
		   dwValue = 0;
		   rc = RegSetValueEx(
			   hKey1, L"BusType", 0, REG_DWORD, (PBYTE)&dwValue, sizeof(dwSize)
			   );
		   if (rc != ERROR_SUCCESS) {
			   RegCloseKey(hKey1);
			   status = NDIS_STATUS_FAILURE;
			   goto cleanUp;
		   }      
		   
		   //
		   // Save original protocol binding for rebind on unload
		   //
		   if (dwOldProtocolsSize > 0) {
			   RegSetValueEx(
				   hKey1, L"OriginalUpperBind", 0, REG_MULTI_SZ, 
				   (BYTE*)mszOldProtocols, dwOldProtocolsSize
				   );
		   }
		   
		   //
		   // If there is protocol binding registry set for adapter move it
		   // to IM registry
		   //
		   swprintf(szPath2, L"Comm\\%s\\Parms", psAdapterName->Buffer);
		   rc = RegCreateKeyEx(
			   HKEY_LOCAL_MACHINE, szPath2, 0, NULL, 0, 0, NULL, &hKey2, NULL
			   );
		   if (rc != ERROR_SUCCESS) {
			   RegCloseKey(hKey1);
			   status = NDIS_STATUS_FAILURE;
			   goto cleanUp;
		   }      
		   
		   szUpperBind[0] = L'\0';
		   dwSize = sizeof(szUpperBind);
		   rc = RegQueryValueEx(
			   hKey2, L"UpperBind", NULL, NULL, (PBYTE)szUpperBind, &dwSize
			   );
		   if (rc != ERROR_SUCCESS) {
			   dwSize = sizeof(szUpperBind);
			   rc = RegQueryValueEx(
				   hKey2, L"ProtocolsToBindTo", NULL, NULL, (PBYTE)szUpperBind,
				   &dwSize
				   );
		   }
		   
		   if (szUpperBind[0] != L'\0') {
			   rc = RegSetValueEx(
				   hKey1, L"UpperBind", 0, REG_MULTI_SZ, (PBYTE)szUpperBind, dwSize
				   );
			   if (rc != ERROR_SUCCESS) {
				   RegCloseKey(hKey2);
				   RegCloseKey(hKey1);
				   status = NDIS_STATUS_FAILURE;
				   goto cleanUp;
			   }            
		   }
		   
		   if (i == (cbCounter -1))
		   {
			   //DO THIS JUST ONCE & THAT TOO IN THE END.

			   //
			   // Allow the miniport bind only to PASSTHRU protocol
			   //
			   NdisMoveMemory(
				   szUpperBind, DMUXMINI_PROTOCOL_NAME, 
				   DMUXMINI_PROTOCOL_NAME_SIZE * sizeof(WCHAR)
				   );
			   szUpperBind[DMUXMINI_PROTOCOL_NAME_SIZE] = 0;
			   szUpperBind[DMUXMINI_PROTOCOL_NAME_SIZE + 1] = 0;
			   
			   rc = RegSetValueEx(
				   hKey2, L"UpperBind", 0, REG_MULTI_SZ, (PBYTE)szUpperBind,
				   (DMUXMINI_PROTOCOL_NAME_SIZE + 2) * sizeof(WCHAR)
				   );
			   if (rc = ERROR_SUCCESS) {
				   RegCloseKey(hKey2);
				   RegCloseKey(hKey1);
				   status = NDIS_STATUS_FAILURE;
				   goto cleanUp;
			   }
		   }

		   RegCloseKey(hKey2);
		   RegCloseKey(hKey1);
      }
	  
#endif
	  
	  //
	  // Now ask NDIS to initialize our miniport (upper) edge.

	  status = NdisIMInitializeDeviceInstanceEx(
		  g_hNdisMiniport, psVMiniPortInstanceName, pBinding
		  );

	  if (status != NDIS_STATUS_SUCCESS) goto cleanUp;
   }

   //
   // Add this adapter to the global pBinding List
   //
   NdisAcquireSpinLock(&g_spinLock);
   pBinding->pNext = g_pBindingList;
   g_pBindingList = pBinding;
   NdisReleaseSpinLock(&g_spinLock);

   //
   // If we had received an UnbindAdapter notification on the underlying
   // adapter, we would have blocked that thread waiting for the IM Init
   // process to complete. Wake up any such thread.
   //
   ASSERT(pBinding->bMiniportInitPending == TRUE);

   pBinding->bMiniportInitPending = FALSE;
   NdisSetEvent(&pBinding->hEvent);


cleanUp:
   //
   // Close the configuration handle now - see comments above with
   // the call to NdisIMInitializeDeviceInstanceEx.
   //
   if (hConfig != NULL) NdisCloseConfiguration(hConfig);

   if (status != NDIS_STATUS_SUCCESS) {

#ifdef UNDER_CE
      //
      // There we should re-bind other protocols to the adapter
      //
      //if (bProtocolUnbind) {
      //   if (mszProtocols == NULL || *mszProtocols != _T('\0')) {
      //      NdisBindProtocolsToAdapter(
      //         &status, psAdapterName->Buffer, mszProtocols
      //      );
      //   }
      //}
#endif
      if (pBinding != NULL) {
         if (pBinding->hPTBinding != NULL) {
            //
            // Close the binding we opened above.
            //
            NdisCloseAdapter(&status, pBinding->hPTBinding);
            pBinding->hPTBinding = NULL;
            if (status == NDIS_STATUS_PENDING) {
               NdisWaitEvent(&pBinding->hEvent, 0);
               status = pBinding->status;
            }
         }
         if (pBinding->hSendPacketPool != NULL) {
            NdisFreePacketPool(pBinding->hSendPacketPool);
         }
         if (pBinding->hRecvPacketPool != NULL) {
            NdisFreePacketPool(pBinding->hRecvPacketPool);
         }

		 for (i=0;i<cbCounter;++i)
		 {
			 PsDMUX pDMUXIf = NULL;
			 //To do
			 // We need to call NdisIMDeInitializeDeviceInstance as many times
			 // as needed for successful instantiated virtual interfaces.
			 pDMUXIf = pBinding->sPASSDMUXSet.ArrayDMUX[i];

			 if (pDMUXIf)
				 NdisFreeMemory(pDMUXIf, sizeof(sDMUXSet), 0);
		 }

         NdisFreeMemory(pBinding, 0, 0);
         pBinding = NULL;
      }
   }

   DBGPRINT((
      "<==DMUXMINI::ProtocolBindAdapter: pBinding %p, status %x\n", 
      pBinding, status
   ));
   *pStatus = status;
}

//------------------------------------------------------------------------------

VOID
ProtocolUnbindAdapter(
   OUT NDIS_STATUS* pStatus,
   IN NDIS_HANDLE hProtocolBindingContext,
   IN NDIS_HANDLE hUnbindContext
)
/*++

Routine Description:
   Called by NDIS when we are required to unbind to the adapter below.
   This functions shares functionality with the miniport's HaltHandler.
   The code should ensure that NdisCloseAdapter and NdisFreeMemory is called
   only once between the two functions

Arguments:
   pStatus - Placeholder for return status
   hProtocolBindingContext - Pointer to the binding structure
   hUnbindContext - Context for NdisUnbindComplete() if this pends

Return Value:
   status for NdisIMDeinitializeDeviceContext

--*/
{
   BINDING* pBinding =(BINDING*)hProtocolBindingContext;
   NDIS_HANDLE hMPBinding = pBinding->hPTBinding;
   NDIS_STATUS status;

   DBGPRINT(("==>DMUXMINI::ProtocolUnbindAdapter: pBinding %p\n", pBinding));

   if (pBinding->bQueuedRequest == TRUE) {
      pBinding->bQueuedRequest = FALSE;
      ProtocolRequestComplete(
         pBinding, &pBinding->request, NDIS_STATUS_FAILURE
      );
   }

   //
   // Check if we had called NdisIMInitializeDeviceInstanceEx and
   // we are awaiting a call to MiniportInitialize.
   //
   if (pBinding->bMiniportInitPending == TRUE) {

      //
      // Try to cancel the pending IMInit process.
      //
      status = NdisIMCancelInitializeDeviceInstance(
         g_hNdisMiniport, &pBinding->sDeviceName
      );

      if (status == NDIS_STATUS_SUCCESS) {
         //
         // Successfully cancelled IM Initialization; our
         // Miniport Initialize routine will not be called
         // for this device.
         //
         pBinding->bMiniportInitPending = FALSE;
         ASSERT(pBinding->hMPBinding == NULL);
      } else {
         //
         // Our Miniport Initialize routine will be called
         // (may be running on another thread at this time).
         // Wait for it to finish.
         //
         NdisWaitEvent(&pBinding->hEvent, 0);
         ASSERT(pBinding->bMiniportInitPending == FALSE);
      }

   }

   //
   // Call NDIS to remove our device-instance. We do most of the work
   // inside the HaltHandler.
   //
   // The Handle will be NULL if our miniport Halt Handler has been called or
   // if the IM device was never initialized
   //
   
   if (pBinding->hMPBinding != NULL) {

      status = NdisIMDeInitializeDeviceInstance(pBinding->hMPBinding);
      if (status != NDIS_STATUS_SUCCESS) status = NDIS_STATUS_FAILURE;

   } else {

      //
      // We need to do some work here. 
      // Close the binding below us 
      // and release the memory allocated.
      //
      if (pBinding->hPTBinding != NULL) {

         NdisResetEvent(&pBinding->hEvent);
         NdisCloseAdapter(&status, pBinding->hPTBinding);

         //
         // Wait for it to complete
         //
         if (status == NDIS_STATUS_PENDING) {
            NdisWaitEvent(&pBinding->hEvent, 0);
            status = pBinding->status;
         }
         
      } else {
         //
         // Both Our MiniportHandle and Binding Handle  should not be NULL.
         //
         status = NDIS_STATUS_FAILURE;
         ASSERT(0);
      }

   }

   DBGPRINT(("<==DMUXMINI::ProtocolUnbindAdapter: pBinding %p\n", pBinding));
}

//------------------------------------------------------------------------------

VOID
ProtocolOpenAdapterComplete(
   IN NDIS_HANDLE hProtocolBindingContext,
   IN NDIS_STATUS status,
   IN NDIS_STATUS openStatus
   )
/*++

Routine Description:
   Completion routine for NdisOpenAdapter issued from within 
   the ProtocolBindAdapter. Simply unblock the caller.

Arguments:
   hProtocolBindingContext - Pointer to the binding structure
   status -  Status of the NdisOpenAdapter call
   openStatus - Secondary status(ignored by us)

Return Value:
   None

--*/
{
   BINDING* pBinding = (BINDING*)hProtocolBindingContext;

   DBGPRINT((
      "==>DMUXMINI::ProtocolOpenAdapterComplete: pBinding %p, status %x\n",
      pBinding, status
   ));

   pBinding->status = status;
   NdisSetEvent(&pBinding->hEvent);

   DBGPRINT(("<==DMUXMINI::ProtocolOpenAdapterComplete"));
}


//------------------------------------------------------------------------------

VOID
ProtocolCloseAdapterComplete(
   IN NDIS_HANDLE hProtocolBindingContext,
   IN NDIS_STATUS status
)
/*++

Routine Description:
   Completion for the CloseAdapter call.

Arguments:
   hProtocolBindingContext - Pointer to the binding structure
   status - Completion status

Return Value:
   None

--*/
{
   BINDING* pBinding = (BINDING*)hProtocolBindingContext;

   DBGPRINT((
      "==>DMUXMINI::CloseAdapterComplete: pBinding %p, status %x\n", 
      pBinding, status
   ));
   pBinding->status = status;
   NdisSetEvent(&pBinding->hEvent);

   DBGPRINT(("<==DMUXMINI::CloseAdapterComplete"));
}

//------------------------------------------------------------------------------

VOID
ProtocolResetComplete(
   IN NDIS_HANDLE hProtocolBindingContext,
   IN NDIS_STATUS status
)
/*++

Routine Description:
   Completion for the reset.

Arguments:
   hProtocolBindingContext - Pointer to the binding structure
   status - Completion status

Return Value:
   None

--*/
{
   BINDING* pBinding = (BINDING*)hProtocolBindingContext;

   //
   // We never issue a reset, so we should not be here.
   //
   ASSERT(0);
}

//------------------------------------------------------------------------------

VOID
ProtocolPostQueryPNPCapabilities(
   IN PsDMUX pDMUXIf,
   OUT PNDIS_STATUS pStatus
)
/*++

Routine Description:
   Postprocess a request for OID_PNP_CAPABILITIES that was forwarded
   down to the underlying miniport, and has been completed by it.

Arguments:
   pBinding - Pointer to the adapter structure
   pStatus - Place to return final status

Return Value:
   None

--*/

{
   PNDIS_PNP_CAPABILITIES pPNPCapabilities;
   PNDIS_PM_WAKE_UP_CAPABILITIES pPMWakeUp;
   PNDIS_REQUEST pRequest = &pDMUXIf->request;

   if (pRequest->DATA.QUERY_INFORMATION.InformationBufferLength >= sizeof(NDIS_PNP_CAPABILITIES)) {
      pPNPCapabilities = (PNDIS_PNP_CAPABILITIES)pRequest->DATA.QUERY_INFORMATION.InformationBuffer;

      //
      // The following fields must be overwritten by an IM driver.
      //
      pPMWakeUp = &pPNPCapabilities->WakeUpCapabilities;
      pPMWakeUp->MinMagicPacketWakeUp = NdisDeviceStateUnspecified;
      pPMWakeUp->MinPatternWakeUp = NdisDeviceStateUnspecified;
      pPMWakeUp->MinLinkChangeWakeUp = NdisDeviceStateUnspecified;
      *pDMUXIf->pulBytesUsed = sizeof(NDIS_PNP_CAPABILITIES);
      *pDMUXIf->pulBytesNeeded = sizeof(NDIS_PNP_CAPABILITIES);

      //
      // Setting our internal flags
      // Default, device is ON
      //
      pDMUXIf->MPDeviceState = NdisDeviceStateD0;
      pDMUXIf->pBinding->PTDeviceState = NdisDeviceStateD0;

      *pStatus = NDIS_STATUS_SUCCESS;
   } else {
      *pDMUXIf->pulBytesNeeded = sizeof(NDIS_PNP_CAPABILITIES);
      *pStatus = NDIS_STATUS_RESOURCES;
   }
}

//------------------------------------------------------------------------------

VOID
ProtocolRequestComplete(
   IN NDIS_HANDLE hProtocolBindingContext,
   IN PNDIS_REQUEST pNdisRequest,
   IN NDIS_STATUS status
)
/*++

Routine Description:
   Completion handler for the previously posted request. All OIDS
   are completed by and sent to the same miniport that they were requested for.
   If Oid == OID_PNP_QUERY_POWER then the data structure needs to returned
   with all entries = NdisDeviceStateUnspecified

Arguments:
   hProtocolBindingContext - Pointer to the binding structure
   pNdisRequest - The posted request pointer
   status - Completion status

Return Value:
   None

--*/
{
   BINDING* pBinding = (BINDING*)hProtocolBindingContext;
   // Note that we send down pDMUXIf->NdisRequest request so when completed
   // we get the same back.
   PsDMUX pDMUXIf = (PsDMUX)pNdisRequest;
   NDIS_OID Oid = pDMUXIf->request.DATA.SET_INFORMATION.Oid;

   //
   // Since our request is not outstanding anymore
   //
   pDMUXIf->bOutstandingRequests = FALSE;

   //
   // Complete the Set or Query, and fill in the buffer
   // for OID_PNP_CAPABILITIES, if need be.
   //
   switch (pNdisRequest->RequestType) {

   case NdisRequestQueryInformation:
      //
      // We never pass OID_PNP_QUERY_POWER down
      //
      ASSERT(Oid != OID_PNP_QUERY_POWER);
      if ((Oid == OID_PNP_CAPABILITIES) && (status == NDIS_STATUS_SUCCESS)) {
         ProtocolPostQueryPNPCapabilities(pDMUXIf, &status);
      }

      *pDMUXIf->pulBytesUsed = pNdisRequest->DATA.QUERY_INFORMATION.BytesWritten;
      *pDMUXIf->pulBytesNeeded = pNdisRequest->DATA.QUERY_INFORMATION.BytesNeeded;

       if ((Oid == OID_GEN_MAC_OPTIONS) && (status == NDIS_STATUS_SUCCESS)) {
            //
            // Remove the no-loopback bit from mac-options. In essence we are
            // telling NDIS that we can handle loopback. We don't, but the
            // interface below us does. If we do not do this, then loopback
            // processing happens both below us and above us. This is wasteful
            // at best and if Netmon is running, it will see multiple copies
            // of loopback packets when sniffing above us.
            //
            // Only the lowest miniport is a stack of layered miniports should
            // ever report this bit set to NDIS.
            //
            *(PULONG)pNdisRequest->DATA.QUERY_INFORMATION.InformationBuffer 
               &= ~NDIS_MAC_OPTION_NO_LOOPBACK;
         }

         NdisMQueryInformationComplete(pDMUXIf->hMPBinding, status);
         break;

   case NdisRequestSetInformation:
      ASSERT(Oid != OID_PNP_SET_POWER);
      *pDMUXIf->pulBytesUsed = pNdisRequest->DATA.SET_INFORMATION.BytesRead;
      *pDMUXIf->pulBytesNeeded = pNdisRequest->DATA.SET_INFORMATION.BytesNeeded;
      NdisMSetInformationComplete(pDMUXIf->hMPBinding, status);
      break;

   default:
      ASSERT(0);
      break;
   }
   
}

//------------------------------------------------------------------------------

VOID
ProtocolStatus(
   IN NDIS_HANDLE hProtocolBindingContext,
   IN NDIS_STATUS status,
   IN PVOID pvStatusBuffer,
   IN UINT uiStatusBufferSize
)
/*++

Routine Description:
   Status handler for the protocol interface

Arguments:
   hProtocolBindingContext - Pointer to the binding structure
   status - Status code
   pvStatusBuffer - Status buffer
   uiStatusBufferSize - Size of the status buffer

Return Value:
   None

--*/
{
   BINDING* pBinding = (BINDING*)hProtocolBindingContext;
   DWORD i;

   for (i=0;i<pBinding->sPASSDMUXSet.dwDMUXnos;++i)
   {
	   PsDMUX pDMUXIf = pBinding->sPASSDMUXSet.ArrayDMUX[i];
	   
	   //
	   // Pass up this indication only if the upper edge miniport is initialized
	   // and powered on. Also ignore indications that might be sent by the lower
	   // miniport when it isn't at D0.
	   //
	   if (
		   (pDMUXIf->hMPBinding != NULL)  &&
		   (pDMUXIf->MPDeviceState == NdisDeviceStateD0) &&
		   (pBinding->PTDeviceState == NdisDeviceStateD0)
		   ) {
		   
		   if (
			   (status == NDIS_STATUS_MEDIA_CONNECT) || 
			   (status == NDIS_STATUS_MEDIA_DISCONNECT)
			   ) {
			   pDMUXIf->statusLastIndicated = status;
		   }
		   NdisMIndicateStatus(
			   pDMUXIf->hMPBinding, status, pvStatusBuffer, uiStatusBufferSize
			   );
		   
	   } else {
		   
		   //
		   // Save the last indicated media status 
		   //
		   if (
			   (pDMUXIf->hMPBinding != NULL) && (
			   (status == NDIS_STATUS_MEDIA_CONNECT) || 
			   (status == NDIS_STATUS_MEDIA_DISCONNECT)
			   )) {
			   pDMUXIf->statusLatestUnIndicate = status;
		   }
	   }
   }
    
}

//------------------------------------------------------------------------------

VOID
ProtocolStatusComplete(
   IN NDIS_HANDLE hProtocolBindingContext
)
/*++

Routine Description:


Arguments:
   hProtocolBindingContext - Pointer to the binding structure

Return Value:
   None

--*/
{
   BINDING* pBinding = (BINDING*)hProtocolBindingContext;
   DWORD i;

   for (i=0;i<pBinding->sPASSDMUXSet.dwDMUXnos;++i)
   {
	   PsDMUX pDMUXIf = pBinding->sPASSDMUXSet.ArrayDMUX[i];
	   
	   //
	   // Pass up this indication only if the upper edge miniport is initialized
	   // and powered on. Also ignore indications that might be sent by the lower
	   // miniport when it isn't at D0.
	   //
	   if (
		   (pDMUXIf->hMPBinding != NULL)  &&
		   (pDMUXIf->MPDeviceState == NdisDeviceStateD0) &&
		   (pBinding->PTDeviceState == NdisDeviceStateD0)
		   ) {
		   NdisMIndicateStatusComplete(pDMUXIf->hMPBinding);
	   }
   }
}

//------------------------------------------------------------------------------

VOID
ProtocolSendComplete(
   IN NDIS_HANDLE hProtocolBindingContext,
   IN PNDIS_PACKET pPacket,
   IN NDIS_STATUS status
   )
/*++

Routine Description:
   Called by NDIS when the miniport below had completed a send. We should
   complete the corresponding upper-edge send this represents.

Arguments:
   hProtocolBindingContext - Pointer to the binding structure
   pPacket - Low level packet being completed
   status - status of send

Return Value:
   None

--*/
{
   BINDING* pBinding = (BINDING*)hProtocolBindingContext;
   PNDIS_PACKET pOriginalPacket;
   PsDMUX pDMUXIf = NULL;
#ifdef NDIS51
   NDIS_HANDLE hPacketPool;

   //
   // Packet stacking:
   //
   // Determine if the packet we are completing is the one we allocated. If so,
   // then get the original packet from the reserved area and completed it and
   // free the allocated packet. If this is the packet that was sent down to us,
   // then just complete it.
   //
   hPacketPool = NdisGetPoolFromPacket(pPacket);
   if (hPacketPool != pBinding->hSendPacketPool) {
	   
	  PNDIS_PACKET_STACK pStack;
	  BOOLEAN bRemaining;

      //
      // We had passed down a packet belonging to the protocol above us.
      //
	  pStack = NdisIMGetCurrentPacketStack(pPacket, &bRemaining); 

	  ASSERT(bRemaining == TRUE);
	  pDMUXIf = (PsDMUX) (pStack->IMReserved[0]);

      NdisMSendComplete(pDMUXIf->hMPBinding, pPacket, status);

   } else
#endif // NDIS51
   {
      PASSTHRU_PR_SEND* pProtocolSend;

      pProtocolSend = (PASSTHRU_PR_SEND *)(pPacket->ProtocolReserved);
      pOriginalPacket = pProtocolSend->pOriginalPacket;
      NdisIMCopySendCompletePerPacketInfo(pOriginalPacket, pPacket);
      NdisDprFreePacket(pPacket);
      NdisMSendComplete(pBinding->hMPBinding, pOriginalPacket, status);
   }
}     

//------------------------------------------------------------------------------

VOID
ProtocolTransferDataComplete(
   IN NDIS_HANDLE hProtocolBindingContext,
   IN PNDIS_PACKET pPacket,
   IN NDIS_STATUS status,
   IN UINT uiBytesTransferred
)
/*++

Routine Description:
   Entry point called by NDIS to indicate completion of a call by us
   to NdisTransferData. See notes under SendComplete.

Arguments:
   hProtocolBindingContext - Pointer to the binding structure

Return Value:
   None
   
--*/
{
   BINDING* pBinding = (BINDING*)hProtocolBindingContext;

   ASSERT(0);

   if (pBinding->hMPBinding) {
      NdisMTransferDataComplete(
         pBinding->hMPBinding, pPacket, status, uiBytesTransferred
      );
   }
}

//------------------------------------------------------------------------------

NDIS_STATUS
ProtocolReceive(
   IN NDIS_HANDLE hProtocolBindingContext,
   IN NDIS_HANDLE hMacReceiveContext,
   IN PVOID pvHeaderBuffer,
   IN UINT uiHeaderBufferSize,
   IN PVOID pvLookAheadBuffer,
   IN UINT uiLookAheadBufferSize,
   IN UINT uiPacketSize
   )
/*++

Routine Description:
   Handle receive data indicated up by the miniport below. We pass
   it along to the protocol above us.
   If the miniport below indicates packets, NDIS would more
   likely call us at our ReceivePacket handler. However we
   might be called here in certain situations even though
   the miniport below has indicated a receive packet, e.g.
   if the miniport had set packet status to NDIS_STATUS_RESOURCES.
      
Arguments:
   <see DDK ref page for ProtocolReceive>

Return Value:
   NDIS_STATUS_SUCCESS if we processed the receive successfully,
   NDIS_STATUS_XXX error code if we discarded it.

--*/
{
   BINDING* pBinding = (BINDING*)hProtocolBindingContext;
   NDIS_PACKET* pRecvPacket;
   NDIS_PACKET* pPacket;
   NDIS_STATUS status = NDIS_STATUS_SUCCESS;
   PsDMUX  pDMUXIf = NULL;
   DWORD i;

   //
   // Lets find out whom should we indicate this packet up.
   //
   FilterOnDstMacAddr((PBYTE)pvHeaderBuffer, pBinding,&pDMUXIf);

   if(pDMUXIf == NULL)
	   return status; // Packet does not belong to us, exit.
   //
   // Get at the packet, if any, indicated up by the miniport below.
   //
   pRecvPacket = NdisGetReceivedPacket(
      pBinding->hPTBinding, hMacReceiveContext
   );

   if (pRecvPacket != NULL) {
      //
      // The miniport below did indicate up a packet. Use information
      // from that packet to construct a new packet to indicate up.
      //

#ifdef NDIS51
      //
      // NDIS 5.1 NOTE: Do not reuse the original packet in indicating
      // up a receive, even if there is sufficient packet stack space.
      // If we had to do so, we would have had to overwrite the
      // status field in the original packet to NDIS_STATUS_RESOURCES,
      // and it is not allowed for protocols to overwrite this field
      // in received packets.
      //
#endif // NDIS51

      //
      // Get a packet off the pool and indicate that up
      //
      NdisAllocatePacket(&status, &pPacket, pBinding->hRecvPacketPool);
      if (status == NDIS_STATUS_SUCCESS) {

         //
         // Make our packet point to data from the original
         // packet. NOTE: this works only because we are
         // indicating a receive directly from the context of
         // our receive indication. If we need to queue this
         // packet and indicate it from another thread context,
         // we will also have to allocate a new buffer and copy
         // over the packet contents, OOB data and per-packet
         // information. This is because the packet data
         // is available only for the duration of this
         // receive indication call.
         //
         pPacket->Private.Head = pRecvPacket->Private.Head;
         pPacket->Private.Tail = pRecvPacket->Private.Tail;
         //
         // Get the original packet (it could be the same packet as the
         // one received or a different one based on the number of layered
         // miniports below) and set it on the indicated packet so the OOB
         // data is visible correctly at protocols above.
         //
         NDIS_SET_ORIGINAL_PACKET(
            pPacket, NDIS_GET_ORIGINAL_PACKET(pRecvPacket)
         );
         NDIS_SET_PACKET_HEADER_SIZE(pPacket, uiHeaderBufferSize);

         //
         // Copy packet flags.
         //
         NdisSetPacketFlags(pPacket, NdisGetPacketFlags(pRecvPacket));

         //
         // Force protocols above to make a copy if they want to hang
         // on to data in this packet. This is because we are in our
         // Receive handler (not ReceivePacket) and we can't return a
         // ref count from here.
         //
         NDIS_SET_PACKET_STATUS(pPacket, NDIS_STATUS_RESOURCES);

		 //
		 // By setting NDIS_STATUS_RESOURCES, we also know that we can reclaim
		 // this packet as soon as the call to NdisMIndicateReceivePacket
		 // returns.
		 //
		 if (pDMUXIf == (PsDMUX)0xFFFFFFFF)
		 {
			 for (i=0;i<pBinding->sPASSDMUXSet.dwDMUXnos;++i)
			 {
				 pDMUXIf = pBinding->sPASSDMUXSet.ArrayDMUX[i];
				 if (  (pDMUXIf->hMPBinding == NULL)  ||  (!pDMUXIf->dwState)  )
					 continue;
				 NdisMIndicateReceivePacket(pDMUXIf->hMPBinding, &pPacket, 1);
			 }
		 }
		 else
		 {
			 NdisMIndicateReceivePacket(pDMUXIf->hMPBinding, &pPacket, 1);
		 }

         //
         // Reclaim the indicated packet. Since we had set its status
         // to NDIS_STATUS_RESOURCES, we are guaranteed that protocols
         // above are done with it.
         //
         NdisFreePacket(pPacket);

         // We are done so exit
         goto cleanUp;
      }

   }

   
   //
   // Fall through if the miniport below us has either not
   // indicated a packet or we could not allocate one
   //
   switch (pBinding->medium) {

   case NdisMedium802_3:
   case NdisMediumWan:

	   if (pDMUXIf == (PsDMUX)0xFFFFFFFF)
	   {
		   for (i=0;i<pBinding->sPASSDMUXSet.dwDMUXnos;++i)
		   {
			   pDMUXIf = pBinding->sPASSDMUXSet.ArrayDMUX[i];
			   if (  (pDMUXIf->hMPBinding == NULL)  ||  (!pDMUXIf->dwState)  )
				   continue;

			   pDMUXIf->bIndicateRecvComplete = TRUE;

			   NdisMEthIndicateReceive(
				   pDMUXIf->hMPBinding, hMacReceiveContext, pvHeaderBuffer,
				   uiHeaderBufferSize, pvLookAheadBuffer, uiLookAheadBufferSize,
				   uiPacketSize
				   );
		   }
	   }
	   else
	   {
		   pDMUXIf->bIndicateRecvComplete = TRUE;
		   NdisMEthIndicateReceive(
			   pDMUXIf->hMPBinding, hMacReceiveContext, pvHeaderBuffer,
			   uiHeaderBufferSize, pvLookAheadBuffer, uiLookAheadBufferSize,
			   uiPacketSize
			   );
	   }

      break;

   case NdisMedium802_5:

	   if (pDMUXIf == (PsDMUX)0xFFFFFFFF)
	   {
		   for (i=0;i<pBinding->sPASSDMUXSet.dwDMUXnos;++i)
		   {
			   pDMUXIf = pBinding->sPASSDMUXSet.ArrayDMUX[i];
			   if (  (pDMUXIf->hMPBinding == NULL)  ||  (!pDMUXIf->dwState)  )
				   continue;

			   pDMUXIf->bIndicateRecvComplete = TRUE;
			   
			   NdisMTrIndicateReceive(
				   pDMUXIf->hMPBinding, hMacReceiveContext, pvHeaderBuffer,
				   uiHeaderBufferSize, pvLookAheadBuffer, uiLookAheadBufferSize,
				   uiPacketSize
				   );
		   }
	   }
	   else
	   {
		   pDMUXIf->bIndicateRecvComplete = TRUE;
		   NdisMTrIndicateReceive(
			   pDMUXIf->hMPBinding, hMacReceiveContext, pvHeaderBuffer,
			   uiHeaderBufferSize, pvLookAheadBuffer, uiLookAheadBufferSize,
			   uiPacketSize
			   );
	   }


      break;

   case NdisMediumFddi:

	   if (pDMUXIf == (PsDMUX)0xFFFFFFFF)
	   {
		   for (i=0;i<pBinding->sPASSDMUXSet.dwDMUXnos;++i)
		   {
			   pDMUXIf = pBinding->sPASSDMUXSet.ArrayDMUX[i];

			   if (  (pDMUXIf->hMPBinding == NULL)  ||  (!pDMUXIf->dwState)  )
				   continue;

			   pDMUXIf->bIndicateRecvComplete = TRUE;
			   
			   NdisMFddiIndicateReceive(
				   pDMUXIf->hMPBinding, hMacReceiveContext, pvHeaderBuffer,
				   uiHeaderBufferSize, pvLookAheadBuffer, uiLookAheadBufferSize,
				   uiPacketSize
				   );
		   }
	   }
	   else
	   {
		   pDMUXIf->bIndicateRecvComplete = TRUE;
		   NdisMFddiIndicateReceive(
			   pDMUXIf->hMPBinding, hMacReceiveContext, pvHeaderBuffer,
			   uiHeaderBufferSize, pvLookAheadBuffer, uiLookAheadBufferSize,
			   uiPacketSize
			   );
	   }

      break;

   default:
      ASSERT(0);
      break;
   }

cleanUp:
   return status;
}

//------------------------------------------------------------------------------

VOID
ProtocolReceiveComplete(
   IN NDIS_HANDLE hProtocolBindingContext
)
/*++

Routine Description:
   Called by the adapter below us when it is done indicating a batch of
   received packets.

Arguments:
   <see DDK ref page for ProtocolReceiveComplete>

Return Value:
   None

--*/
{
   BINDING* pBinding = (BINDING*)hProtocolBindingContext;
   PsDMUX  pDMUXIf = NULL;
   DWORD i;

   for (i=0;i<pBinding->sPASSDMUXSet.dwDMUXnos;++i)
   {
	   pDMUXIf = pBinding->sPASSDMUXSet.ArrayDMUX[i];
	   
	   if ((pDMUXIf->hMPBinding) && (pDMUXIf->bIndicateRecvComplete) &&
		   (pDMUXIf->dwState))
	   {
		   
		   switch (pBinding->medium) {
		   case NdisMedium802_3:
		   case NdisMediumWan:
			   NdisMEthIndicateReceiveComplete(pDMUXIf->hMPBinding);
			   break;
			   
		   case NdisMedium802_5:
			   NdisMTrIndicateReceiveComplete(pDMUXIf->hMPBinding);
			   break;
			   
		   case NdisMediumFddi:
			   NdisMFddiIndicateReceiveComplete(pDMUXIf->hMPBinding);
			   break;
			   
		   default:
			   ASSERT(0);
			   break;
		   }
	   }

	   pDMUXIf->bIndicateRecvComplete = FALSE;
   }
   
}

//------------------------------------------------------------------------------

INT
ProtocolReceivePacket(
   IN NDIS_HANDLE hProtocolBindingContext,
   IN PNDIS_PACKET pPacket
)
/*++

Routine Description:
   ReceivePacket handler. Called by NDIS if the miniport below supports
   NDIS 4.0 style receives. Re-package the buffer chain in a new packet
   and indicate the new packet to protocols above us. Any context for
   packets indicated up must be kept in the MiniportReserved field.

   NDIS 5.1 - packet stacking - if there is sufficient "stack space" in
   the packet passed to us, we can use the same packet in a receive
   indication.

Arguments:
   ProtocolBindingContext - Pointer to our adapter structure.
   Packet - Pointer to the packet

Return Value:
   == 0 -> We are done with the packet
   != 0 -> We will keep the packet and call NdisReturnPackets() this
           many times when done.
--*/
{
   BINDING* pBinding = (BINDING*)hProtocolBindingContext;
   INT iCode = 0;
   NDIS_STATUS status;
   PNDIS_PACKET pIMPacket;
#ifdef NDIS51
   BOOLEAN bRemaining;
#endif
   PsDMUX  pDMUXIf = NULL;
   DWORD i;

   //
   // Lets find out whom should we indicate this packet up.
   //
   FilterRecvPacket(pPacket, pBinding,&pDMUXIf);

   if(pDMUXIf == NULL)
	   return iCode; // Packet does not belong to us, exit.

#ifdef NDIS51
   //
   // Check if we can reuse the same packet for indicating up.
   // See also: ProtocolReceive(). 
   //
   (VOID)NdisIMGetCurrentPacketStack(pPacket, &bRemaining);
   if (bRemaining) {
      //
      // We can reuse "Packet". Indicate it up and be done with it.
      //
      status = NDIS_GET_PACKET_STATUS(pPacket);

	  if (pDMUXIf == (PsDMUX)0xFFFFFFFF)
	  {
		  for (i=0;i<pBinding->sPASSDMUXSet.dwDMUXnos;++i)
		  {
			  pDMUXIf = pBinding->sPASSDMUXSet.ArrayDMUX[i];

			  //
			  // Drop the packet silently if the upper miniport edge isn't initialized
			  //
			  if ( (!pDMUXIf->hMPBinding) || (!pDMUXIf->dwState) ) continue;
			  
			  NdisMIndicateReceivePacket(pDMUXIf->hMPBinding, &pPacket, 1);
		  }
	  }
	  else
	  {
		  //
		  // Drop the packet silently if the upper miniport edge isn't initialized
		  //
		  
		  if (!pDMUXIf->hMPBinding) goto cleanUp;
		  NdisMIndicateReceivePacket(pDMUXIf->hMPBinding, &pPacket, 1);
	  }

      iCode = (status != NDIS_STATUS_RESOURCES) ? 1 : 0;
      goto cleanUp;
   }
#endif // NDIS51

   //
   // Get a packet off the pool and indicate that up
   //
   NdisAllocatePacket(&status, &pIMPacket, pBinding->hRecvPacketPool);

   if (status == NDIS_STATUS_SUCCESS) {

      PASSTHRU_PR_RECV* pProtocolRecv;
      
      pProtocolRecv = (PASSTHRU_PR_RECV*)(pIMPacket->MiniportReserved);
      pProtocolRecv->pOriginalPacket = pPacket;

      pIMPacket->Private.Head = pPacket->Private.Head;
      pIMPacket->Private.Tail = pPacket->Private.Tail;

      //
      // Get the original packet (it could be the same packet as the one
      // received or a different one based on the number of layered miniports
      // below) and set it on the indicated packet so the OOB data is visible
      // correctly to protocols above us.
      //
      NDIS_SET_ORIGINAL_PACKET(pIMPacket, NDIS_GET_ORIGINAL_PACKET(pPacket));

      //
      // Set Packet Flags
      //
      NdisSetPacketFlags(pIMPacket, NdisGetPacketFlags(pPacket));

      status = NDIS_GET_PACKET_STATUS(pPacket);
      NDIS_SET_PACKET_STATUS(pIMPacket, status);

      NDIS_SET_PACKET_HEADER_SIZE(
         pIMPacket, NDIS_GET_PACKET_HEADER_SIZE(pPacket)
      );

      NdisMIndicateReceivePacket(pBinding->hMPBinding, &pIMPacket, 1);

      //
      // Check if we had indicated up the packet with NDIS_STATUS_RESOURCES
      // NOTE -- do not use NDIS_GET_PACKET_STATUS(MyPacket) for this since
      // it might have changed! Use the value saved in the local variable.
      //
      if (status == NDIS_STATUS_RESOURCES) {
         //
         // Our ReturnPackets handler will not be called for this packet.
         // We should reclaim it right here.
         //
         NdisFreePacket(pIMPacket);
      }

      iCode = (status != NDIS_STATUS_RESOURCES) ? 1 : 0;

   }

cleanUp:
   return iCode;
}

//------------------------------------------------------------------------------

NDIS_STATUS
ProtocolPnPNetEventReconfigure(
   IN BINDING* pBinding,
   IN PNET_PNP_EVENT pNetPnPEvent
)
/*++

Routine Description:
   This routine is called from NDIS to notify our protocol edge of a
   reconfiguration of parameters for either a specific binding (pBinding
   is not NULL), or global parameters if any (pBinding is NULL).

Arguments:
   pBinding - Pointer to our binding structure.
   pNetPnPEvent - the reconfigure event

Return Value:
   NDIS_STATUS_SUCCESS

--*/
{
   NDIS_STATUS status = NDIS_STATUS_SUCCESS;

   //
   // Is this is a global reconfiguration notification ?
   //
   if (pBinding == NULL) {

      //
      // An important event that causes this notification to us is if
      // one of our upper-edge miniport instances was enabled after being
      // disabled earlier, e.g. from Device Manager in Win2000. Note that
      // NDIS calls this because we had set up an association between our
      // miniport and protocol entities by calling NdisIMAssociateMiniport.
      //
      // Since we would have torn down the lower binding for that miniport,
      // we need NDIS' assistance to re-bind to the lower miniport. The
      // call to NdisReEnumerateProtocolBindings does exactly that.
      //
      NdisReEnumerateProtocolBindings(g_hNdisProtocol);      

   } else {

#ifdef NDIS51
      //
      // Pass on this notification to protocol(s) above before doing anything
      // with it.
      //

	   PsDMUX  pDMUXIf = NULL;
	   DWORD i;
	   
	   for (i=0;i<pBinding->sPASSDMUXSet.dwDMUXnos;++i)
	   {
		   pDMUXIf = pBinding->sPASSDMUXSet.ArrayDMUX[i];
		   
		   if (pDMUXIf->hMPBinding != NULL) {
			   status = NdisIMNotifyPnPEvent(pDMUXIf->hMPBinding, pNetPnPEvent);
		   }
	   }
#endif // NDIS51

   }
   return status;
}

//------------------------------------------------------------------------------

NDIS_STATUS
ProtocolPnPNetEventSetPower(
   IN BINDING* pBinding,
   IN NET_PNP_EVENT* pNetPnPEvent
)
/*++

Routine Description:
   This is a notification to our protocol edge of the power state
   of the lower miniport. If it is going to a low-power state, we must
   wait here for all outstanding sends and requests to complete.

   NDIS 5.1:  Since we use packet stacking, it is not sufficient to
   check usage of our local send packet pool to detect whether or not
   all outstanding sends have completed. For this, use the new API
   NdisQueryPendingIOCount.

   NDIS 5.1: Use the 5.1 API NdisIMNotifyPnPEvent to pass on PnP
   notifications to upper protocol(s).

Arguments:
   pBinding - Pointer to our binding structure.
   pNetPnPEvent - The Net Pnp Event. This contains the new device state.

Return Value:
   NDIS_STATUS_SUCCESS or the status returned by upper-layer protocols.

--*/
{
   NDIS_DEVICE_POWER_STATE* pDeviceState;
   NDIS_DEVICE_POWER_STATE prevDeviceState = pBinding->PTDeviceState;  
   NDIS_STATUS  status;
   NDIS_STATUS                ReturnStatus;
#ifdef NDIS51
   ULONG ulPendingIoCount = 0;
#endif // NDIS51
   PsDMUX  pDMUXIf = NULL;
   DWORD i;


   pDeviceState = (NDIS_DEVICE_POWER_STATE*)(pNetPnPEvent->Buffer);

   ReturnStatus = NDIS_STATUS_SUCCESS;

   //
   // Set the Internal Device State, this blocks all new sends or receives
   //
   pBinding->PTDeviceState = *pDeviceState;

   //
   // Check if the miniport below is going to a low power state.
   //
   if (*pDeviceState > NdisDeviceStateD0) {

#ifdef NDIS51
      //
      // Notify upper layer protocol(s) first.
      //
      for (i=0;i<pBinding->sPASSDMUXSet.dwDMUXnos;++i)
	  {
		  pDMUXIf = pBinding->sPASSDMUXSet.ArrayDMUX[i];
		  
		  if (pDMUXIf->hMPBinding != NULL) {
			  status = NdisIMNotifyPnPEvent(pDMUXIf->hMPBinding, pNetPnPEvent);
		  }
	  }  

#endif // NDIS51

      //
      // If the miniport below is going to standby, fail all incoming requests
      //
      if (prevDeviceState == NdisDeviceStateD0) pBinding->bStandingBy = TRUE;

      //
      // Wait for outstanding sends and requests to complete.
      //
#ifdef NDIS51
      do {
         status = NdisQueryPendingIOCount(
            pBinding->hPTBinding, &ulPendingIoCount
         );
         if ((status != NDIS_STATUS_SUCCESS) || (ulPendingIoCount == 0)) break;
         NdisMSleep(2);
      } while (TRUE);
#else
      while (NdisPacketPoolUsage(pBinding->hSendPacketPool) != 0) NdisMSleep(2);
      //
      // Sleep till outstanding requests complete
      //
      do
	  {
		  for (i=0;i<pBinding->sPASSDMUXSet.dwDMUXnos;++i)
		  {
			  pDMUXIf = pBinding->sPASSDMUXSet.ArrayDMUX[i];
			  if (pDMUXIf->hMPBinding != NULL) {
				  while(pDMUXIf->bOutstandingRequests == TRUE) NdisMSleep(2);
			  }
			  ASSERT(pDMUXIf->bOutstandingRequests == FALSE);
		  }
	  }while(0);

#endif // NDIS51

      ASSERT(NdisPacketPoolUsage(pBinding->hSendPacketPool) == 0);

   } else {

      //
      // The device below is being turned on. If we had a request
      // pending, send it down now.
      //
	   for (i=0;i<pBinding->sPASSDMUXSet.dwDMUXnos;++i)
	   {
		   pDMUXIf = pBinding->sPASSDMUXSet.ArrayDMUX[i];
		   if ( (pDMUXIf->hMPBinding != NULL) && (pDMUXIf->bQueuedRequest == TRUE) )
		   {
			   pDMUXIf->bQueuedRequest = FALSE;
			   NdisRequest(&status, pBinding->hPTBinding, &pDMUXIf->request);
			   if (status != NDIS_STATUS_PENDING) {
				   ProtocolRequestComplete(pBinding, &pDMUXIf->request, status);
			   }
		   }
      }

      //
      // If the physical miniport is powering up (from Low power state to D0), 
      // clear the flag
      //
      if (prevDeviceState > NdisDeviceStateD0) pBinding->bStandingBy = FALSE;

#ifdef NDIS51
      //
      // Pass on this notification to protocol(s) above
      //
	  for (i=0;i<pBinding->sPASSDMUXSet.dwDMUXnos;++i)
	  {
		  pDMUXIf = pBinding->sPASSDMUXSet.ArrayDMUX[i];
		  if (pDMUXIf->hMPBinding != NULL)
		  {
			  status = NdisIMNotifyPnPEvent(pDMUXIf->hMPBinding, pNetPnPEvent);
		  }
	  }

#endif // NDIS51

   }

   return status;
}

//------------------------------------------------------------------------------

NDIS_STATUS
ProtocolPNPHandler(
   IN NDIS_HANDLE hProtocolBindingContext,
   IN PNET_PNP_EVENT pNetPnPEvent
)
/*++

Routine Description:
   This is called by NDIS to notify us of a PNP event related to a lower
   binding. Based on the event, this dispatches to other helper routines.

   NDIS 5.1: forward this event to the upper protocol(s) by calling
   NdisIMNotifyPnPEvent.

Arguments:
   hProtocolBindingContext - Pointer to our binding structure. Can be NULL
      for "global" notifications.
   pNetPnPEvent - Pointer to the PNP event to be processed.

Return Value:
   NDIS_STATUS code indicating status of event processing.

--*/
{
   BINDING* pBinding  = (BINDING*)hProtocolBindingContext;
   NDIS_STATUS status  = NDIS_STATUS_SUCCESS;
   PsDMUX  pDMUXIf = NULL;
   DWORD i;


   DBGPRINT((
      "==>DMUXMINI::ProtocolPnPHandler: pBinding %p, Event %d\n", pBinding, 
      pNetPnPEvent->NetEvent
   ));

   switch (pNetPnPEvent->NetEvent) {
   case NetEventSetPower:
      status = ProtocolPnPNetEventSetPower(pBinding, pNetPnPEvent);
      break;

   case NetEventReconfigure:
      status = ProtocolPnPNetEventReconfigure(pBinding, pNetPnPEvent);
      break;

   default:
#ifdef NDIS51
      //
      // Pass on this notification to protocol(s) above, before
      // doing anything else with it.
      //
	  if (pBinding != NULL)
	  {
		  for (i=0;i<pBinding->sPASSDMUXSet.dwDMUXnos;++i)
		  {
			  pDMUXIf = pBinding->sPASSDMUXSet.ArrayDMUX[i];
			  if (pDMUXIf->hMPBinding != NULL)
			  {
				  status = NdisIMNotifyPnPEvent(pDMUXIf->hMPBinding, pNetPnPEvent);
			  }
		  }
	  }

#else
      status = NDIS_STATUS_SUCCESS;
#endif // NDIS51
      break;
   }

   DBGPRINT(("<==DMUXMINI::ProtocolPnPHandler: status %x", status));
   
   return status;
}

//------------------------------------------------------------------------------

extern
VOID
ProtocolUnload(
   VOID
)
{
   DBGPRINT(("===DMUXMINI::ProtocolUnload"));
   return;
}

//------------------------------------------------------------------------------
