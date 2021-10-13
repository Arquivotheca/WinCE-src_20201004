//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __PROTOCOL_H
#define __PROTOCOL_H

//------------------------------------------------------------------------------

#define NDT_CMD_PORT          5000

//------------------------------------------------------------------------------

typedef enum {
   NDT_CMD_OPEN,
   NDT_CMD_CLOSE,
   NDT_CMD_LOAD_ADAPTER, 
   NDT_CMD_UNLOAD_ADAPTER,
   NDT_CMD_QUERY_ADAPTERS,
   NDT_CMD_QUERY_PROTOCOLS,
   NDT_CMD_QUERY_BINDING,
   NDT_CMD_BIND_PROTOCOL, 
   NDT_CMD_UNBIND_PROTOCOL,
   NDT_CMD_WRITE_VERIFY_FLAG, 
   NDT_CMD_DELETE_VERIFY_FLAG,
   NDT_CMD_START_IOCONTROL, 
   NDT_CMD_STOP_IOCONTROL, 
   NDT_CMD_WAIT_IOCONTROL
} NDT_CMD;

//------------------------------------------------------------------------------

#define NDT_CMD_INP_LOAD_ADAPTER             _T("S")
#define NDT_CMD_INP_UNLOAD_ADAPTER           _T("S")
#define NDT_CMD_INP_QUERY_ADAPTERS           _T("")
#define NDT_CMD_OUT_QUERY_ADAPTERS           _T("M")
#define NDT_CMD_INP_QUERY_PROTOCOLS          _T("")
#define NDT_CMD_OUT_QUERY_PROTOCOLS          _T("M")
#define NDT_CMD_INP_QUERY_BINDING            _T("S")
#define NDT_CMD_OUT_QUERY_BINDING            _T("M")
#define NDT_CMD_INP_BIND_PROTOCOL            _T("SS")
#define NDT_CMD_INP_UNBIND_PROTOCOL          _T("SS")
#define NDT_CMD_INP_WRITE_VERIFY_FLAG        _T("SD")
#define NDT_CMD_INP_DELETE_VERIFY_FLAG       _T("S")
#define NDT_CMD_INP_START_IOCONTROL          _T("DDY")
#define NDT_CMD_OUT_START_IOCONTROL          _T("VY")
#define NDT_CMD_INP_STOP_IOCONTROL           _T("V")
#define NDT_CMD_INP_WAIT_IOCONTROL           _T("VD")
#define NDT_CMD_OUT_WAIT_IOCONTROL           _T("Y")

//------------------------------------------------------------------------------
// PKT_HEADER STRUCTURE
//

typedef struct 
{
   ULONG   ulPacketId;
   ULONG   ulCommand;
   HRESULT hr;
   ULONG   ulLength;
} NDT_CMD_HEADER, UNALIGNED *PNDT_CMD_HEADER;

//------------------------------------------------------------------------------

HRESULT DispatchCommand(
   ULONG ulCommand, PVOID* ppvInpBuffer, DWORD* pcbInpBuffer, 
   PVOID* ppvOutBuffer, DWORD* pcbOutBuffer
);

//------------------------------------------------------------------------------

#endif
