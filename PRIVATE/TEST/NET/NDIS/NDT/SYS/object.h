//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __OBJECT_H
#define __OBJECT_H

//------------------------------------------------------------------------------

#define NDT_MAGIC_OBJECT                     0x17016400
#define NDT_MAGIC_DRIVER                     0x17016401
#define NDT_MAGIC_PROTOCOL                   0x17016402
#define NDT_MAGIC_BINDING                    0x17016403
#define NDT_MAGIC_PACKET                     0x17016404
#define NDT_MAGIC_MEDIUM_802_3               0x17016405
#define NDT_MAGIC_MEDIUM_802_5               0x17016406

#define NDT_MAGIC_REQUEST_BIND               0x17016451
#define NDT_MAGIC_REQUEST_UNBIND             0x17016452
#define NDT_MAGIC_REQUEST_RESET              0x17016453
#define NDT_MAGIC_REQUEST_GET_COUNTER        0x17016454
#define NDT_MAGIC_REQUEST_REQUEST            0x17016455
#define NDT_MAGIC_REQUEST_SEND               0x17016456
#define NDT_MAGIC_REQUEST_RECEIVE            0x17016457
#define NDT_MAGIC_REQUEST_RECEIVE_STOP       0x17016458
#define NDT_MAGIC_REQUEST_SET_ID             0x17016459
#define NDT_MAGIC_REQUEST_SET_OPTIONS        0x1701645A
#define NDT_MAGIC_REQUEST_LISTEN             0x1701645B
#define NDT_MAGIC_REQUEST_LISTEN_STOP        0x1701645C

//------------------------------------------------------------------------------

class CObject
{
   friend class CObjectList;

protected:   
   LONG m_nRefCount;                      // Reference counter
   CObject* m_pFLink;                     // Forward link for a list
   CObject* m_pBLink;                     // Backward link for a list
   
public:
   DWORD m_dwMagic;                       // Magic value

public:
   CObject();
   virtual ~CObject();
   
   LONG AddRef();
   LONG Release();
};

//------------------------------------------------------------------------------

#endif
