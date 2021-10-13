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
// PC4500.cpp
#pragma code_seg("LCODE")
#include "NDISVER.h"

extern "C"{
	#include	<ndis.h>
}
extern "C"{
	#include	<ndis.h>
	#define	WANTVXDWRAPS
#ifndef UNDER_CE
	#include <basedef.h>		
	#include <VMM.H>
	#include <vxdldr.h>
	#include <VXDWRAPS.H>
#endif
}
#include "version.h"
#include <AiroVxdWrap.H>
#include <AiroPCI.h>

//
UINT		FrequencyType		= 1;	//HOPPER		
UINT		CardType			= VXD_CARDTYPE_PC3500;		
UINT		DriverMajorVersion	= DRIVER_MAJOR_VERSION;  
UINT		DriverMinorVersion	= DRIVER_MINOR_VERSION;		
char		*Ver_FileVersionStr	= VER_FILEVERSION_STR;
char		*CardName			= "????????";		
char		*DriverName			= "PCX500.SYS";		
USHORT 		Pci_Dev_Id 			= 0x0001;
//

