//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __TAPIINFO_H__
#define __TAPIINFO_H__

#ifdef __cplusplus
extern "C" {
#endif

LPTSTR FormatLineCallback(LPTSTR szOutputBuffer,
  DWORD dwDevice, DWORD dwMsg, DWORD dwCallbackInstance, 
  DWORD dwParam1, DWORD dwParam2, DWORD dwParam3);
LPTSTR FormatLineError(long lLineError, LPTSTR lpszOutputBuffer);

#ifdef __cplusplus
}
#endif

#endif
