/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Copyright (c) 1996-2000 Microsoft Corporation

Module Name:
    tapiinfo.h

Abstract:
    Header file for TAPI error/message logging functions.

-------------------------------------------------------------------*/
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
