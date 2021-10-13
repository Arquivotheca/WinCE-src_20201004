//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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


