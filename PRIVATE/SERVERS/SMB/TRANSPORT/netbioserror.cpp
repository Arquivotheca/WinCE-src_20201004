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
#include <windows.h>
#include <ncb.h>

#include "SMB_Globals.h"
#include "nb.h"
#include "NetBiosTransport.h"


TCHAR * NCB_Errors[] = {
    TEXT("NRC_GOODRET"),
    TEXT("NRC_BUFLEN"),
    TEXT("NRC_BFULL"),
    TEXT("NRC_ILLCMD"),
    TEXT("Unknown"),
    TEXT("NRC_CMDTMO"),
    TEXT("NRC_INCOMP"),
    TEXT("NRC_BADDR"),
    TEXT("NRC_SNUMOUT"),
    TEXT("NRC_NORES"),
    TEXT("NRC_SCLOSED"),
    TEXT("NRC_CMDCAN"),
    TEXT("NRC_DMAFAIL"),
    TEXT("NRC_DUPNAME"),
    TEXT("NRC_NAMTFUL"),
    TEXT("NRC_ACTSES"),
    TEXT("NRC_INVALID"),
    TEXT("NRC_LOCTFUL"),
    TEXT("NRC_REMTFUL"),
    TEXT("NRC_ILLNN"),
    TEXT("NRC_NOCALL"),
    TEXT("NRC_NOWILD"),
    TEXT("NRC_INUSE"),
    TEXT("NRC_NAMERR"),
    TEXT("NRC_SABORT"),
    TEXT("NRC_NAMCONF"),
    TEXT("NRC_BADREM"),
    TEXT("NRC_IFBUSY"),
    TEXT("Unknown"),
    TEXT("Unknown"),
    TEXT("Unknown"),
    TEXT("Unknown"),
    TEXT("Unknown"),
    TEXT("Unknown"),
    TEXT("NRC_TOOMANY"),
    TEXT("NRC_BRIDGE"),
    TEXT("NRC_CANOCCR"),
    TEXT("NRC_RESNAME"),
    TEXT("NRC_CANCEL"),
    TEXT("Unknown"),
    TEXT("Unknown"),
    TEXT("Unknown"),
    TEXT("Unknown"),
    TEXT("Unknown"),
    TEXT("Unknown"),
    TEXT("Unknown"),
    TEXT("Unknown"),
    TEXT("Unknown"),
    TEXT("NRC_DUPENV"),
    TEXT("Unknown"),
    TEXT("Unknown"),
    TEXT("NRC_MULT"),
    TEXT("NRC_ENVNOTDEF"),
    TEXT("NRC_OSRESNOTAV"),
    TEXT("NRC_MAXAPPS"),
    TEXT("NRC_NOSAPS"),
    TEXT("NRC_NORESOURCES"),
    TEXT("NRC_INVADDRESS"),
    TEXT("Unknown"),
    TEXT("NRC_INVDDID"),
    TEXT("NRC_LOCKFAIL"),
    TEXT("Unknown"),
    TEXT("Unknown"),
    TEXT("NRC_OPENERR"),
    TEXT("NRC_SYSTEM"),
    TEXT("NRC_ROM "),
    TEXT("NRC_RAM "),
    TEXT("NRC_DLF "),
    TEXT("NRC_ALF "),
    TEXT("NRC_IFAIL"),
    TEXT("Unknown"),
    TEXT("Unknown"),
    TEXT("Unknown"),
    TEXT("Unknown"),
    TEXT("Unknown"),
    TEXT("Unknown"),
    TEXT("Unknown"),
    TEXT("Unknown"),
    TEXT("Unknown"),
    TEXT("Unknown"),
    TEXT("NRC_ADPTMALFN")
};

LPTSTR
NETBIOS_TRANSPORT::NCBError(UCHAR ncb_err)
{
    if (ncb_err == NRC_PENDING) {
        return TEXT("NRC_PENDING");
    }
    if (ncb_err > NRC_ADPTMALFN) {
        return TEXT("Unknown");
    }
    return NCB_Errors[ncb_err];
}

