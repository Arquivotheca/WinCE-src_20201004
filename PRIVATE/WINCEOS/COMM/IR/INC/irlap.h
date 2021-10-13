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
*  File:   irlap.h
*
*  Description: IRLAP Protocol and control block definitions
*
*  Date:   4/15/95
*
*/

// Sequence number modulus
#define IRLAP_MOD                   8 
#define PV_TABLE_MAX_BIT            9

extern UINT vBaudTable[PV_TABLE_MAX_BIT+1];
extern UINT vMaxTATTable[PV_TABLE_MAX_BIT+1];
extern UINT vMinTATTable[PV_TABLE_MAX_BIT+1];
extern UINT vDataSizeTable[PV_TABLE_MAX_BIT+1];
extern UINT vWinSizeTable[PV_TABLE_MAX_BIT+1];
extern UINT vBOFSTable[PV_TABLE_MAX_BIT+1];
extern UINT vDiscTable[PV_TABLE_MAX_BIT+1];
extern UINT vThreshTable[PV_TABLE_MAX_BIT+1];
extern UINT vBOFSDivTable[PV_TABLE_MAX_BIT+1];

VOID IrlapOpenLink(
    OUT PNTSTATUS       Status,
    IN  PIRDA_LINK_CB   pIrdaLinkCb,
    IN  IRDA_QOS_PARMS  *pQos,
    IN  UCHAR           *pDscvInfo,
    IN  int             DscvInfoLen,
    IN  UINT            MaxSlot,
    IN  UCHAR           *pDeviceName,
    IN  int             DeviceNameLen,
    IN  UCHAR           CharSet);

UINT IrlapDown(IN PVOID Context,
               IN PIRDA_MSG);

VOID IrlapUp(IN PVOID Context,
             IN PIRDA_MSG);

VOID IrlapCloseLink(PIRDA_LINK_CB pIrdaLinkCb);

UINT IrlapGetQosParmVal(UINT[], UINT, UINT *);

VOID IrlapDeleteInstance(PVOID Context);

void IRLAP_PrintState();



typedef struct
{
    LIST_ENTRY      ListHead;
    int             Len;
} IRDA_MSG_LIST;

// I've exported these for the tester
UINT DequeMsgList(IRDA_MSG_LIST *, IRDA_MSG **);
UINT EnqueMsgList(IRDA_MSG_LIST *, IRDA_MSG *, int);
void InitMsgList(IRDA_MSG_LIST *);






