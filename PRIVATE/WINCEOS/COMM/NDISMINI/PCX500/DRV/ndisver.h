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
|* Copyright (c) 1996  Aironet, Inc                                        *|
|*                                                                         *|
|* This file is part of the Aironet Wireless Miniport Driver.             *|
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

    07/23/01        spb026      - If we aren't using ndis5, then we can't
                                  Use the MICROSOFT define.  Just make sure
                                  here.
---------------------------------------------------------------------------*/

#ifndef __NDISVER_H__
#define __NDISVER_H__

#if NDISVER==5 
#   define NDIS_MAJOR_VERSION       0x05
#   define NDIS_MINOR_VERSION       0x00
//-----------------------------------------
#   define NDIS50_MINIPORT_DRIVER
#   define NDIS50_MINIPORT  1 
#   define NDIS5_SUPPORT    
//-----------------------------------------
#elif NDISVER==41 
#   undef  NDISVER
#   undef  MICROSOFT            //spb026
#   define NDISVER 3 
#   define NDIS_MAJOR_VERSION       0x04
#   define NDIS_MINOR_VERSION       0x00
//-----------------------------------------
#   define NDIS40_MINIPORT 
#   define NDIS4_SUPPORT
//
#elif NDISVER==4 
#   define SOFTEX  
#   undef  NDISVER
#   undef  MICROSOFT            //spb026
#   define NDISVER 3 
#   define NDIS_MAJOR_VERSION       0x04
#   define NDIS_MINOR_VERSION       0x00
//-----------------------------------------
#   define NDIS40_MINIPORT 
#   define NDIS4_SUPPORT
//
#elif defined   NDIS41_MINIPORT 
#   define NDIS4_SUPPORT
#   undef  MICROSOFT            //spb026
//-----------------------------------------
#else
#   undef  MICROSOFT            //spb026
#   define NDIS_MAJOR_VERSION       0x03
#   define NDIS_MINOR_VERSION       0x00
#endif

#endif

