//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __NDISTEST_REQUEST_H
#define __NDISTEST_REQUEST_H

//------------------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

//------------------------------------------------------------------------------

typedef enum {
   NDT_REQUEST_UNKNOWN = 0,
   NDT_REQUEST_BIND,
   NDT_REQUEST_UNBIND,
   NDT_REQUEST_RESET,
   NDT_REQUEST_GET_COUNTER,
   NDT_REQUEST_REQUEST,
   NDT_REQUEST_SEND,
   NDT_REQUEST_RECEIVE,
   NDT_REQUEST_RECEIVE_STOP,
   NDT_REQUEST_SET_ID,
   NDT_REQUEST_SET_OPTIONS
} NDT_ENUM_REQUEST_TYPE;

//------------------------------------------------------------------------------

#define NDT_MARSHAL_INP_DEFAULT           _T("V")
#define NDT_MARSHAL_OUT_DEFAULT           _T("T")

#define NDT_MARSHAL_INP_BIND              _T("IASI")
#define NDT_MARSHAL_OUT_BIND              _T("TTIV")
#define NDT_MARSHAL_INP_UNBIND            NDT_MARSHAL_INP_DEFAULT
#define NDT_MARSHAL_OUT_UNBIND            NDT_MARSHAL_OUT_DEFAULT
#define NDT_MARSHAL_INP_RESET             NDT_MARSHAL_INP_DEFAULT
#define NDT_MARSHAL_OUT_RESET             NDT_MARSHAL_OUT_DEFAULT
#define NDT_MARSHAL_INP_GET_COUNTER       _T("VL")
#define NDT_MARSHAL_OUT_GET_COUNTER       _T("L")
#define NDT_MARSHAL_INP_REQUEST           _T("VROBL")
#define NDT_MARSHAL_OUT_REQUEST           _T("TIIB")
#define NDT_MARSHAL_INP_SEND              _T("VBXCCLLII")
#define NDT_MARSHAL_OUT_SEND              _T("TLLLLLLLL")
#define NDT_MARSHAL_INP_RECEIVE           _T("V")
#define NDT_MARSHAL_OUT_RECEIVE           _T("TLLLLLL")
#define NDT_MARSHAL_INP_RECEIVE_STOP      NDT_MARSHAL_INP_DEFAULT
#define NDT_MARSHAL_OUT_RECEIVE_STOP      NDT_MARSHAL_OUT_DEFAULT
#define NDT_MARSHAL_INP_SET_ID            _T("VEE")
#define NDT_MARSHAL_OUT_SET_ID            NDT_MARSHAL_OUT_DEFAULT
#define NDT_MARSHAL_INP_SET_OPTIONS       _T("II")
#define NDT_MARSHAL_OUT_SET_OPTIONS       NDT_MARSHAL_OUT_DEFAULT

//------------------------------------------------------------------------------

#define NDT_MARSHAL_VOID_REF              _T('V')
#define NDT_MARSHAL_NDIS_STATUS           _T('T')
#define NDT_MARSHAL_UCHAR                 _T('C')
#define NDT_MARSHAL_UINT                  _T('I')
#define NDT_MARSHAL_USHORT                _T('E')
#define NDT_MARSHAL_DWORD                 _T('D')
#define NDT_MARSHAL_ULONG                 _T('L')
#define NDT_MARSHAL_STRING                _T('S')
#define NDT_MARSHAL_MEDIUM_ARRAY          _T('A')
#define NDT_MARSHAL_NDIS_OID              _T('O')
#define NDT_MARSHAL_BYTE_ARRAY            _T('B')
#define NDT_MARSHAL_BYTE_ARRAY2           _T('X')
#define NDT_MARSHAL_NDIS_REQUEST_TYPE     _T('R')
#define NDT_MARSHAL_MULTISTRING           _T('M')
#define NDT_MARSHAL_BYTE_BUFFER           _T('Y')

//------------------------------------------------------------------------------

#ifdef __cplusplus
}
#endif

//------------------------------------------------------------------------------

#endif
