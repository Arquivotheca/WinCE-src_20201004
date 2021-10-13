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
//pci.cp

/****************************************************************************
Module Name:
    pci.c

This driver runs on the following hardware:
    - aironet adapters

Environment:
    Kernel Mode - Or whatever is the equivalent on WinNT

*****************************************************************************/
#pragma code_seg("LCODE")
#include "NDISVER.h"
extern "C"{
	#include	<ndis.h>
}

#include "pci.h"
#include "aironet.h"
#include "Card.h"
#include "keywords.h"
#include "string.h"
#include "Support.h"
#include "AiroDef.h"
#include <AiroPCI.h>

//-----------------------------------------------------------------------------
// Procedure:   FindPciDeviceScan
//
// Description: This routine finds an adapter for the driver to load on
//              The critical piece to understanding this routine is that
//              the System will not let us read any information from PCI
//              space from any slot but the one that the System thinks
//              we should be using. The configuration manager rules this
//              land... The Slot number used by this routine is just a
//              placeholder, it could be zero even.
//
// Arguments:
//      Adapter - ptr to Adapter object instance
//      VendorID - Vendor ID of the adapter.
//      DeviceID - Device ID of the adapter.
//      PciCardsFound - A structure that contains an array of the IO addresses,
//                   IRQ, and node addresses of each PCI card that we find.
//
//    NOTE: due to NT 5's Plug and Play configuration manager
//          this routine will never return more than one device.
//
// Returns:
//      USHORT - Number of D100 based PCI adapters found in the scanned bus
//-----------------------------------------------------------------------------
extern USHORT Pci_Dev_Id;

BOOLEAN FindPciDeviceScan( PCARD card)
{
    NDIS_STATUS         Status;
	PCI_COMMON_CONFIG	pcicfg;
    PNDIS_RESOURCE_LIST  AssignedResources;
	int					size = sizeof(pcicfg);
	int					MemBank = 0;
    NdisReadPciSlotInformation(card->m_MAHandle,card->m_SlotNumber,0, &pcicfg, size);

    //if (AIRONET_PCI_VEN_ID != pcicfg.VendorID || Pci_Dev_Id != pcicfg.DeviceID )
    if (AIRONET_PCI_VEN_ID != pcicfg.VendorID )
		return FALSE;

    Status = NdisMPciAssignResources(card->m_MAHandle,card->m_SlotNumber, &AssignedResources);

    if (Status != NDIS_STATUS_SUCCESS)
		return FALSE;

    for (UINT i=0;i < AssignedResources->Count; i++ ){
		CM_PARTIAL_RESOURCE_DESCRIPTOR	*desc = &AssignedResources->PartialDescriptors[i];		
		switch (AssignedResources->PartialDescriptors[i].Type){

        case CmResourceTypePort:
			if (AssignedResources->PartialDescriptors[i].Flags & CM_RESOURCE_PORT_IO){

					if( 64==AssignedResources->PartialDescriptors[i].u.Port.Length ){
						//card->m_IOBase = 
						//	AssignedResources->PartialDescriptors[i].u.Port.Start.u.LowPart;
						card->m_InitialPort = 
							AssignedResources->PartialDescriptors[i].u.Port.Start.u.LowPart;
					}
					if( 16==AssignedResources->PartialDescriptors[i].u.Port.Length ){
						//card->m_IOBase8Bit = 
						//	AssignedResources->PartialDescriptors[i].u.Port.Start.u.LowPart;
						card->m_InitialPort8Bit = 
							AssignedResources->PartialDescriptors[i].u.Port.Start.u.LowPart;
							card->m_PortIOLen8Bit = 16;
					}
			}
			break;
            
		case CmResourceTypeInterrupt:
 			card->m_InterruptNumber = 
				(UCHAR)AssignedResources->PartialDescriptors[i].u.Interrupt.Level;
			break;
            
		case CmResourceTypeMemory:
			switch( MemBank ){
			case 0:
				card->m_ContolRegMemBase =
					(UCHAR *) AssignedResources->PartialDescriptors[i].u.Memory.Start.u.LowPart;
				break;
			
			case 1:	
				//card->IsAMCC = TRUE;
				//card->m_RadioMemBase =
				//	(UCHAR *) AssignedResources->PartialDescriptors[i].u.Memory.Start.u.LowPart;
				break;

			case 2:	
				card->IsAMCC = TRUE;
				card->m_pAttribMemBase =
					(UCHAR *) AssignedResources->PartialDescriptors[i].u.Memory.Start.u.LowPart;
				break;
			}
			++MemBank;
			break;
		}
	}
	if( ! card->IsAMCC ){
		card->m_ContolRegMemBase	= 0;
		card->m_pAttribMemBase		= 0;
	}
		
	return TRUE;
}

