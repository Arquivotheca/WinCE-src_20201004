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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
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
   NDT_REQUEST_RECEIVE_EX,
   NDT_REQUEST_RECEIVE_STOP_EX,
   NDT_REQUEST_SET_ID,
   NDT_REQUEST_SET_OPTIONS,
   NDT_REQUEST_STATUS_START,
   NDT_REQUEST_CLEAR_COUNTERS,
   NDT_REQUEST_HAL_EN_WAKE,
   NDT_REQUEST_OPEN,
   NDT_REQUEST_CLOSE,
   NDT_REQUEST_KILL
} NDT_ENUM_REQUEST_TYPE;

//------------------------------------------------------------------------------

#define NDT_MARSHAL_INP_DEFAULT           _T("V")
#define NDT_MARSHAL_OUT_DEFAULT           _T("T")

#define NDT_MARSHAL_INP_OPEN              _T("S")
#define NDT_MARSHAL_OUT_OPEN              _T("TV")
#define NDT_MARSHAL_INP_CLOSE             _T("V")
#define NDT_MARSHAL_OUT_CLOSE            NDT_MARSHAL_OUT_DEFAULT
#define NDT_MARSHAL_INP_BIND              _T("IASIV")
#define NDT_MARSHAL_OUT_BIND              _T("TIV")
#define NDT_MARSHAL_INP_UNBIND            NDT_MARSHAL_INP_DEFAULT
#define NDT_MARSHAL_OUT_UNBIND            NDT_MARSHAL_OUT_DEFAULT
#define NDT_MARSHAL_INP_RESET             NDT_MARSHAL_INP_DEFAULT
#define NDT_MARSHAL_OUT_RESET             NDT_MARSHAL_OUT_DEFAULT
#define NDT_MARSHAL_INP_GET_COUNTER       _T("VL")
#define NDT_MARSHAL_OUT_GET_COUNTER       _T("L")
#define NDT_MARSHAL_INP_REQUEST           _T("VROBLLLL")
#define NDT_MARSHAL_OUT_REQUEST           _T("TIIB")
#define NDT_MARSHAL_INP_SEND              _T("VBXCCLLLII")
#define NDT_MARSHAL_OUT_SEND              _T("TLLLLLNLL")
#define NDT_MARSHAL_INP_RECEIVE           _T("VL")
#define NDT_MARSHAL_OUT_RECEIVE           _T("TLLLNLL")
#define NDT_MARSHAL_INP_RECEIVE_EX        _T("VLX")
#define NDT_MARSHAL_OUT_RECEIVE_EX		  _T("TX")
#define NDT_MARSHAL_INP_RECEIVE_STOP      NDT_MARSHAL_INP_DEFAULT
#define NDT_MARSHAL_OUT_RECEIVE_STOP      NDT_MARSHAL_OUT_DEFAULT
#define NDT_MARSHAL_INP_RECEIVE_STOP_EX   NDT_MARSHAL_INP_DEFAULT
#define NDT_MARSHAL_OUT_RECEIVE_STOP_EX   NDT_MARSHAL_OUT_DEFAULT
#define NDT_MARSHAL_INP_SET_ID            _T("VEE")
#define NDT_MARSHAL_OUT_SET_ID            NDT_MARSHAL_OUT_DEFAULT
#define NDT_MARSHAL_INP_SET_OPTIONS       _T("VII")
#define NDT_MARSHAL_OUT_SET_OPTIONS       NDT_MARSHAL_OUT_DEFAULT
#define NDT_MARSHAL_INP_STATUS_START      _T("VL")
#define NDT_MARSHAL_OUT_STATUS_START	_T("B")
#define NDT_MARSHAL_INP_CLEAR_COUNTERS            NDT_MARSHAL_INP_DEFAULT
#define NDT_MARSHAL_OUT_CLEAR_COUNTERS             NDT_MARSHAL_OUT_DEFAULT
#define NDT_MARSHAL_INP_HAL_SETUP_WAKE       _T("III")
#define NDT_MARSHAL_OUT_HAL_SETUP_WAKE       NDT_MARSHAL_OUT_DEFAULT


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
#define NDT_MARSHAL_ULONGLONG		_T('N')

//------------------------------------------------------------------------------

#ifdef __cplusplus
}
#endif

//------------------------------------------------------------------------------

#endif
