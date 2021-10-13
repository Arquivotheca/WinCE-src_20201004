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

/***************************************************************************\
|* Copyright (c) 1996  Aironet                                      *|
|*                                                                          *|
|* This file is part of the Aironet PC4500 Miniport Driver.                 *|
\***************************************************************************/

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Module Name:

    version.h

Abstract:

    This file contains version definitions for the Miniport driver file.



    Ned Nassar

Environment:

    Include this file at the top of each module in the driver to make sure
    each module gets rebuilt when the version changes.

Revision History:

---------------------------------------------------------------------------*/

#ifndef _VERSION_H
#define _VERSION_H
#include "ndisver.h"


#define DRIVER_MAJOR_VERSION        07    
#define DRIVER_MINOR_VERSION        41
#define DRIVER_MINOR_MINOR_VERSION  0
#ifndef MICROSOFT
#define VER_FILEVERSION_STR         "7.41.00"
#else
#define VER_FILEVERSION_STR         "7.41.00 Firmware"
#endif
#define VER_FILEVERSION             DRIVER_MAJOR_VERSION,DRIVER_MINOR_VERSION,DRIVER_MINOR_MINOR_VERSION
#undef  VER_PRODUCTVERSION_STR
#define VER_PRODUCTVERSION_STR      VER_FILEVERSION_STR
#undef  VER_COMPANYNAME_STR
#define VER_COMPANYNAME_STR         "Cisco Systems"
#define VER_INTERNALNAME_STR        "PCX500.SYS"
#define VER_LEGALCOPYRIGHT_STR      "Copyright \251 2001 " VER_COMPANYNAME_STR
#define VER_ORIGINALFILENAME_STR    VER_INTERNALNAME_STR
#undef  VER_PRODUCTNAME_STR
#define VER_PRODUCTNAME_STR         "Cisco Systems Wireless LAN Adapter"

#if NDISVER == 3
#   define VER_FILEDESCRIPTION_STR     "NDIS3 Miniport Driver for 32 bit Windows"
#elif NDISVER == 4
#   define VER_FILEDESCRIPTION_STR     "NDIS4 Miniport Driver for 32 bit Windows"
#elif NDISVER == 41
#   define VER_FILEDESCRIPTION_STR     "NDIS4 Miniport Driver for 32 bit Windows"
#elif NDISVER == 5
#   define VER_FILEDESCRIPTION_STR     "NDIS5 Miniport Driver for 32 bit Windows"
#else
#   define VER_FILEDESCRIPTION_STR     "NDIS Miniport Driver for 32 bit Windows"
#endif
//
/*
// Used when registering MAC with NDIS wrapper.
// i.e. What NDIS version do we expect.
*/

#endif

