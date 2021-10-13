//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
// Copyright 2001, Cisco Systems, Inc.  All rights reserved.
// No part of this source, or the resulting binary files, may be reproduced,
// transmitted or redistributed in any form or by any means, electronic or
// mechanical, for any purpose, without the express written permission of Cisco.
//
//debug.cpp
#include "NDISVER.h"
extern "C"{
	#include	<ndis.h>
}
#include "support.h"
#include "memory.h"
#include "Airodef.h"
#include "HWX500P.h"
#include "debug.h"

USHORT X500ReadWord(PCARD card, USHORT offset)
{
	USHORT	usWord;
	NdisRawReadPortUshort(card->m_IOBase+offset, &usWord );
	return usWord;
}

void X500WriteWord(PCARD card, USHORT offset, USHORT val)
{
	NdisRawWritePortUshort(card->m_IOBase+offset, val);
}

void
X500ReadBytes(PCARD card, void *vP, UINT offset, USHORT len)
{
	UCHAR *pbuf = (UCHAR *)vP;
	while (len >= 2) {
		*(USHORT*)pbuf = X500ReadWord(card,(USHORT)offset);
		pbuf	+= 2;
		offset	+= 2;
		len		-= 2;
	}
	if (len) 
		*pbuf = (u8)(X500ReadWord(card, (USHORT)offset)&0xff);

}

void
X500WriteBytes(PCARD card, void *vP, UINT offset, USHORT len)
{
	USHORT val;
	UCHAR *pbuf = (UCHAR *)vP;

	while (len >= 2) {
		X500WriteWord(card, (USHORT)offset, *(USHORT*)pbuf);
		pbuf += 2;
		offset += 2;
		len -= 2;
	}
	if (len) {
		val = (X500ReadWord(card, (USHORT)offset) & 0xFF00) + *pbuf;
		X500WriteWord(card, (USHORT)offset, val);
	}
}
//
//==================================================================
//
USHORT X500ReadAuxWord(PCARD card, UINT offset)
{
	USHORT	usWord;
	NdisRawWritePortUshort(card->m_IOBase+REG_AUX_PAGE, offset/0x80);
	NdisRawWritePortUshort(card->m_IOBase+REG_AUX_OFF, offset&0x7E);
	NdisRawReadPortUshort(card->m_IOBase+REG_AUX_DATA, &usWord );
	return usWord;
}

void X500WriteAuxWord(PCARD card, UINT offset, USHORT val)
{
	NdisRawWritePortUshort(card->m_IOBase+REG_AUX_PAGE, offset/0x80);
	NdisRawWritePortUshort(card->m_IOBase+REG_AUX_OFF, offset&0x7E);
	NdisRawWritePortUshort(card->m_IOBase+REG_AUX_DATA, val );
}

void
X500ReadAuxBytes(PCARD card, void *vP, UINT offset, USHORT len)
{
	UCHAR *pbuf = (UCHAR *)vP;

	if (offset & 1) {
		*pbuf++ = (UCHAR)(X500ReadAuxWord(card, offset++)>>8);
		len--;
	}
	while (len >= 2) {
		*(USHORT*)pbuf = X500ReadAuxWord(card, offset);
		pbuf += 2;
		offset += 2;
		len -= 2;
	}
	if (len) {
		*pbuf = (u8)(X500ReadAuxWord(card, offset)&0xff);
	}

}

void
X500WriteAuxBytes(PCARD card, void *vP, UINT offset, USHORT len)
{
	USHORT val;
	UCHAR *pbuf = (UCHAR *)vP;

	if (offset & 1) {
		val = (X500ReadAuxWord(card, offset) & 0xFF) + ( ((USHORT)(*pbuf++)) << 8);
		X500WriteAuxWord(card, offset++, val);
		len--;
	}
	while (len >= 2) {
		X500WriteAuxWord(card, offset, *(USHORT*)pbuf);
		pbuf += 2;
		offset += 2;
		len -= 2;
	}
	if (len) {
		val = (X500ReadAuxWord(card, offset) & 0xFF00) + *pbuf;
		X500WriteAuxWord(card, offset, val);
	}
}

