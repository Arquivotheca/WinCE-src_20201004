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
//---------------------------------------------------------------------------
// profile.h
//---------------------------------------------------------------------------
// Description:
//
// Revision History:
//
// Date        
//---------------------------------------------------------------------------
// 11/10/00    jbeaujon       -Initial Revision
//
//---------------------------------------------------------------------------

#ifndef __PROFILE_H
#define __PROFILE_H

extern "C"{
    #include    <ndis.h>
	#include	<string.h>
}
#include "card.h"
#include "hwX500.h"

//---------------------------------------------------------------------------
// Default profile names (used if we have no profiles or a name is not specified).
//---------------------------------------------------------------------------
#define DEFAULT_ENTERPRISE_PROFILE_NAME     "Enterprise"
#define DEFAULT_HOME_PROFILE_NAME           "Home"


STPROFILE*      findProfileByName   (char               *name,
                                     STPROFILE          *profiles,
                                     USHORT             count,
                                     USHORT             *index);

BOOLEAN         getProfiles         (NDIS_HANDLE        ConfigHandle, 
                                     STPROFILE          **profiles, 
                                     PUSHORT            count);

STPROFILE*      addProfile          (PCARD              card,
                                     PROFILE_PROPERTIES *profileProps);

STPROFILE*      addProfile          (PCARD              card,
                                     PROFILE            *profile);

#ifdef DBG
void            listProfiles        (STPROFILE          *profile,
                                     int                count);

void            dumpProfile         (STPROFILE          *profile,
                                     BOOLEAN            detailed = FALSE);
#endif


#endif // __PROFILE_H
