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
