//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __REQUEST_RESET_H
#define __REQUEST_RESET_H

//------------------------------------------------------------------------------

#include "Request.h"

//------------------------------------------------------------------------------

class CRequestReset : public CRequest
{
public:
   CRequestReset(CBinding* pBinding = NULL);
   virtual NDIS_STATUS Execute();
};

//------------------------------------------------------------------------------

#endif

