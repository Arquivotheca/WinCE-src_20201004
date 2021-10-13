//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
