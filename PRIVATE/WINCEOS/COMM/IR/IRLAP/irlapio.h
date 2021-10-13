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
/*****************************************************************************
* 
*
*  File:   irlapio.h 
*
*  Description: prototypes for IRLAP I/O routines 
*
*  Date:   4/25/95
*
*/
void SetMsgPointers(PIRLAP_CB, PIRDA_MSG);
VOID SendDscvXIDCmd(PIRLAP_CB);
VOID SendDscvXIDRsp(PIRLAP_CB);
VOID SendSNRM(PIRLAP_CB, BOOLEAN);
VOID SendUA(PIRLAP_CB, BOOLEAN);
VOID SendDM(PIRLAP_CB);
VOID SendRD(PIRLAP_CB);
VOID SendRR(PIRLAP_CB);
VOID SendRR_RNR(PIRLAP_CB);
VOID SendDISC(PIRLAP_CB);
VOID SendRNRM(PIRLAP_CB);
VOID SendIFrame(PIRLAP_CB, PIRDA_MSG, int, int);
VOID SendSREJ(PIRLAP_CB, int);
VOID SendREJ(PIRLAP_CB);
VOID SendFRMR(PIRLAP_CB, IRLAP_FRMR_FORMAT *);
VOID SendUIFrame(PIRLAP_CB, PIRDA_MSG);


