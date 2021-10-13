//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __REQUEST_RECEIVE_STOP_H
#define __REQUEST_RECEIVE_STOP_H

//------------------------------------------------------------------------------

#include "Request.h"

//------------------------------------------------------------------------------

class CRequestReceiveStop : public CRequest
{
public:
   CRequestReceiveStop(CBinding* pBinding = NULL);
   virtual NDIS_STATUS Execute();
};

//------------------------------------------------------------------------------

#endif
