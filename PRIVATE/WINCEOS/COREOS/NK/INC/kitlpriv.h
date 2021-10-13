//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Module Name:  
    kitlpriv.h
    
Abstract:  
    Private interface to KITL library
    
Functions:

Notes: 

--*/
#ifndef __KITLPRIV_H__
#define __KITLPRIV_H__
#include <e_to_k.h>
#include <KitlProt.h>

//
//  These pointers are set in to point to the corresponding functions
//  in kitl.lib when KITLInit() is called.
//
typedef BOOL (*PFN_KITLSend)(UCHAR Id, UCHAR *pUserData, DWORD dwUserDataLen);
typedef BOOL (*PFN_KITLRecv)(UCHAR Id, UCHAR *pRecvBuf, DWORD *pdwLen, DWORD Timeout);
typedef BOOL (*PFN_KITLInitializeInterrupt)();
typedef BOOL (*PFN_KITLRegisterDfltClient)(UCHAR Service, UCHAR Flags, UCHAR **ppTxBuffer, UCHAR **ppRxBuffer);

extern PFN_KITLSend pKITLSend;
extern PFN_KITLRecv pKITLRecv;
extern PFN_KITLInitializeInterrupt pKITLInitializeInterrupt;
extern PFN_KITLRegisterDfltClient pKITLRegisterDfltClient;


#endif // __KITLPRIV_H__

