//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//
//
//  Module Name:  
//  
//      cenet.h
//  
//  Abstract:  
//
//      Common network macros
//      
//------------------------------------------------------------------------------

#ifndef __CENET_H__
#define __CENET_H__

#ifdef __cplusplus
extern "C" {
#endif

#define net_short(s) _byteswap_ushort((USHORT)(s))
#define net_long(l)  _byteswap_ulong((ULONG)(l))

#define ntohl(l)    _byteswap_ulong((ULONG)(l))
#define ntohs(s)    _byteswap_ushort((USHORT)(s))
#define htonl(l)    _byteswap_ulong((ULONG)(l))
#define htons(s)    _byteswap_ushort((USHORT)(s))

#define BYTE_SWAP(s)        _byteswap_ushort((USHORT)(s))
#define BYTE_SWAP_ULONG(l)  _byteswap_ulong((ULONG)(l))

#ifdef RtlUshortByteSwap
#undef RtlUshortByteSwap
#endif
#ifdef RtlUlongByteSwap
#undef RtlUlongByteSwap
#endif

#define RtlUshortByteSwap(s)        _byteswap_ushort((USHORT)(s))
#define RtlUlongByteSwap(l)         _byteswap_ulong((ULONG)(l))

#define RtlConvertEndianShort(s)    _byteswap_ushort((USHORT)(s))
#define RtlConvertEndianLong(l)     _byteswap_ulong((ULONG)(l))


#ifdef __cplusplus
}
#endif

#endif __CENET_H__
