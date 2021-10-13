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
#ifndef OBEXPACKETINFO_H
#define OBEXPACKETINFO_H

//
//	Obex packet structure and operation opcodes
//
//	<NF> == "netowrk format"
//
//	Packet:
//		1 byte			- opCode
//		2 bytes <NF>	- total length (headers + 3 bytes)
//		sequence of headers
//	Header:
//		1 byte			- header id + type
//		header data
//
//	Operation codes
//
#define OBEX_TYPE_UNICODE			0x00
#define OBEX_TYPE_BYTESEQ			0x40
#define OBEX_TYPE_BYTE				0x80
#define OBEX_TYPE_DWORD 			0xc0

#define OBEX_TYPE_MASK				0xc0

#define OBEX_HID_COUNT				(0x00 | OBEX_TYPE_DWORD)
#define OBEX_HID_NAME				(0x01 | OBEX_TYPE_UNICODE)
#define OBEX_HID_TYPE				(0x02 | OBEX_TYPE_BYTESEQ)
#define OBEX_HID_LENGTH				(0x03 | OBEX_TYPE_DWORD)
#define OBEX_HID_TIME_ISO			(0x04 | OBEX_TYPE_BYTESEQ)
#define OBEX_HID_TIME_COMPAT		(0x04 | OBEX_TYPE_DWORD)
#define OBEX_HID_DESCRIPTION		(0x05 | OBEX_TYPE_UNICODE)
#define OBEX_HID_TARGET				(0x06 | OBEX_TYPE_BYTESEQ)
#define OBEX_HID_HTTP				(0x07 | OBEX_TYPE_BYTESEQ)
#define OBEX_HID_BODY				(0x08 | OBEX_TYPE_BYTESEQ)
#define OBEX_HID_BODY_END			(0x09 | OBEX_TYPE_BYTESEQ)
#define OBEX_HID_WHO				(0x0a | OBEX_TYPE_BYTESEQ)
#define OBEX_HID_CONNECTIONID		(0x0b | OBEX_TYPE_DWORD)
#define OBEX_HID_APP_PARAMS			(0x0c | OBEX_TYPE_BYTESEQ)
#define OBEX_HID_AUTH_CHALLENGE		(0x0d | OBEX_TYPE_BYTESEQ)
#define OBEX_HID_AUTH_RESPONSE		(0x0e | OBEX_TYPE_BYTESEQ)
#define OBEX_HID_CLASS				(0x0f | OBEX_TYPE_BYTESEQ)

#define OBEX_OP_OPMASK				0x7f
#define OBEX_OP_ISFINAL				0x80

#define OBEX_OP_CONNECT				(0x00 | OBEX_OP_ISFINAL)
#define OBEX_OP_DISCONNECT			(0x01 | OBEX_OP_ISFINAL)
#define OBEX_OP_PUT					 0x02
#define OBEX_OP_GET					 0x03
#define OBEX_OP_SETPATH				(0x05 | OBEX_OP_ISFINAL)
#define OBEX_OP_ABORT				(0x7f | OBEX_OP_ISFINAL)

#define OBEX_STAT_CONTINUE			0x10

#define OBEX_STAT_OK				0x20
#define OBEX_STAT_CREATED			0x21
#define OBEX_STAT_ACCEPTED			0x22
#define OBEX_STAT_NONAUTHINFO		0x23
#define OBEX_STAT_NOCONTENT			0x24
#define OBEX_STAT_RESETCONTENT		0x25
#define OBEX_STAT_PARTIALCONTENT	0x26

#define OBEX_STAT_MULTIPLECHOICES	0x30
#define OBEX_STAT_MOVEDPERM			0x31
#define OBEX_STAT_MOVEDTEMP			0x32
#define OBEX_STAT_SEEOTHER			0x33
#define OBEX_STAT_NOTMODIFIED		0x34
#define OBEX_STAT_USEPROXY			0x35

#define OBEX_STAT_BADREQUEST		0x40
#define OBEX_STAT_UNAUTHORIZED		0x41
#define OBEX_STAT_PAYMENTREQD		0x42
#define OBEX_STAT_FORBIDDEN			0x43
#define OBEX_STAT_NOTFOUND			0x44
#define OBEX_STAT_NOTALLOWED		0x45
#define OBEX_STAT_NOTACCEPTABLE		0x46
#define OBEX_STAT_PROXYAUTHREQD		0x47
#define OBEX_STAT_REQUESTTIMEOUT	0x48
#define OBEX_STAT_CONFLICT			0x49
#define OBEX_STAT_GONE				0x4a
#define OBEX_STAT_LENGTHREQD		0x4b
#define OBEX_STAT_PRECONDITIONFAIL	0x4c
#define OBEX_STAT_REQENTITYTOOBIG	0x4d
#define OBEX_STAT_REQURLTOOBIG		0x4e
#define OBEX_STAT_UNSUPPORTEDMEDIA	0x4f

#define OBEX_STAT_INTERNALERROR		0x50
#define OBEX_STAT_NOTIMPLEMENTED	0x51
#define OBEX_STAT_BADGATEWAY		0x52
#define OBEX_STAT_SERVICEUNAVAIL	0x53
#define OBEX_STAT_GATEWAYTIMEOUT	0x54
#define OBEX_STAT_HTTPVUNSUPPORTED	0x55

#define OBEX_INVALID_CONNECTION		0xffffffff

#define OBEX_MAX_PACKET_SIZE		0xffff
#define OBEX_MIN_PACKET_SIZE		0xff

//
//	Requests
//
#define OBEX_REQ_REQUEST			0x00000001
#define OBEX_REQ_CLOSE				0x00000002
#define OBEX_REQ_INIT				0x00000003
#define OBEX_REQ_UNLOAD				0x00000004

//
//	Responses
//
#define OBEX_RESP_RESPOND			0x00000001
#define OBEX_RESP_OK				0x00000002
#define OBEX_RESP_ACCEPT			0x00000003
#define OBEX_RESP_REJECT			0x00000004
#define OBEX_RESP_DENY				0x00000005
#define OBEX_RESP_CONTINUE			0x00000006
#define OBEX_RESP_ABORT				0x00000007
#define OBEX_RESP_DISCONNECT		0x00000008
#define OBEX_RESP_HANGUP			0x00000009




#endif