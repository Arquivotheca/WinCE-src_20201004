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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#ifndef __REQUEST_UNBIND_H
#define __REQUEST_UNBIND_H

//------------------------------------------------------------------------------

#include "Request.h"

//------------------------------------------------------------------------------

class CRequestUnbind : public CRequest
{
private:
       /**
   * The amount of time we wait for the bind/open to be completed, after we have
   * started binding. Currently we wait for this time, but we would not return
   * until the bind has completed.
   */
   static const ULONG ulUNBIND_COMPLETE_WAIT  = 60000;

public:
   CRequestUnbind(CBinding *pBinding = NULL);
   
   virtual NDIS_STATUS Execute();
   virtual void Complete();            // Complete request
};

//------------------------------------------------------------------------------

#endif
