//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//
#include "FileServer.h"

FileStream *GetNewFileStream(TIDState *pMyState) 
{
    RETAILMSG(1, (L"SMB_SRV:  The SMB File server has not been installed!"));
    ASSERT(FALSE);
    return NULL;
}

