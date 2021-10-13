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
*  File:   irlmp.h
*
*  Description: IRLMP Protocol and control block definitions
*
*  Date:   4/15/95
*
*/

#define IRLMP_MAX_USER_DATA_LEN      53

// IrLMP Entry Points

VOID
IrlmpInitialize();

VOID
IrlmpOpenLink(OUT PNTSTATUS         Status,
              IN  PIRDA_LINK_CB     pIrdaLinkCb,  
              IN  UCHAR             *pDeviceName,
              IN  int               DeviceNameLen,
              IN  UCHAR             CharSet);
              
VOID
IrlmpCloseLink(IN PIRDA_LINK_CB     pIrdaLinkCb);              

UINT IrlmpUp(PIRDA_LINK_CB pIrdaLinkCb, PIRDA_MSG pIMsg);

UINT IrlmpDown(PVOID IrlmpContext, PIRDA_MSG pIrdaMsg);

VOID
IrlmpDeleteInstance(PVOID Context);

#if DBG
void IRLMP_PrintState();
#endif

// IAS

#define IAS_ASCII_CHAR_SET          0

// IAS Attribute value types
#define IAS_ATTRIB_VAL_MISSING      0
#define IAS_ATTRIB_VAL_INTEGER      1
#define IAS_ATTRIB_VAL_BINARY       2
#define IAS_ATTRIB_VAL_STRING       3

// IAS Operation codes
#define IAS_OPCODE_GET_VALUE_BY_CLASS   4   // The only one I do

extern const CHAR IasClassName_Device[];
extern const CHAR IasAttribName_DeviceName[];
extern const CHAR IasAttribName_IrLMPSupport[];
extern const CHAR IasAttribName_TTPLsapSel[];
extern const CHAR IasAttribName_IrLMPLsapSel[];
extern const CHAR IasAttribName_IrLMPLsapSel2[];


extern const UCHAR IasClassNameLen_Device;
extern const UCHAR IasAttribNameLen_DeviceName;
extern const UCHAR IasAttribNameLen_IRLMPSupport;
extern const UCHAR IasAttribNameLen_TTPLsapSel;
extern const UCHAR IasAttribNameLen_IrLMPLsapSel;


