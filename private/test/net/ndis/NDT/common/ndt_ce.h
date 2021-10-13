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
#ifndef __NDT_CE_H
#define __NDT_CE_H

//------------------------------------------------------------------------------

typedef struct {
   WCHAR szShareName[17];
   DWORD dwShareSize;
} NDT_IOCONTROL_REQUEST;

//------------------------------------------------------------------------------

typedef struct {
  WCHAR szEventName[17];               // Name of event set when async IO end
  NDIS_STATUS status;                  // Result of IO
  DWORD dwInputSize;
  DWORD dwInputOffset;
  DWORD dwOutputSize;
  DWORD dwActualOutputSize;
  DWORD dwOutputOffset;
} NDT_IOCONTROL_SHARE;

//------------------------------------------------------------------------------

#endif
