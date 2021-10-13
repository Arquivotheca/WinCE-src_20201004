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
#pragma pack(push, 1)

#ifndef _CP
#define _CP(x) {if (!x) return E_POINTER;}
#endif

typedef struct DVD_PACK_HEADER1
{
	BYTE PackStartCode[4];
	BYTE SCR[6];
	BYTE ProgramMuxRate[3];
	BYTE PackStuffing; // note the first 5 bits must be 11111b.

} *PDVD_PACK_HEADER1;

typedef struct NV_PCK_SYSTEM_HEADER
{
	BYTE SystemHeaderStartCode[4];
	BYTE HeaderLength[2];
	BYTE MuxRate[3];
	BYTE AudioVideoBound[2];
	BYTE PacketRateRestriction;
	BYTE VideoStreamID;
	BYTE VideoBufBound[2];
	BYTE AudioStreamID;
	BYTE AudioBufBound[2];
	BYTE PrivateStream1StreamID;
	BYTE PrivateStream1BufBound[2];
	BYTE PrivateStream2StreamID;
	BYTE PrivateStream2BufBound[2];

} *PNV_PCK_SYSTEM_HEADER;
#pragma pack(pop)

UINT32 const g_cbMaxSizeVOBU	= 1572864;
UINT32 const g_cbDVDPackSize		= 2048;


BYTE const g_PackStartCode[4]				= {0x00,0x00,0x01,0xBA};
BYTE const g_ProgramMuxRate[3]				= {0x01,0x89,0xC3};

// TEMPORARY
BYTE const g_416TempComp[9]					= {0x00,0xDB,0xBB,0xFA,0xFF,0xFF,0x00,0x00,0x00};
// END TEMPORARY

BYTE const g_PackStuffingNoStuffing			= {0xF8};
BYTE const g_NVSystemHeader[24]				= {0x00,0x00,0x01,0xBB,0x00,0x12,0x80,0xC4,0xE1, // Just finished mux rate, next is num Audio Streams
												0x04,0xE1,0x7F,0xB9,0xE0,0xE8,0xB8,0xC0,0x20, //Just finished audio stream info, next is Private_Stream_1
												0xBD,0xE0,0x3A,0xBF,0xE0,0x02};
// How many bytes to compare in the system header
UINT32 const g_cbNVSystemHeaderMaskStart	= 9;

// How many bytes to compare at the end.  We skip the #of audio streams since this could be variable
UINT32 const g_cbNVSystemHeaderMaskEnd		= 13;
BYTE const g_PCIPacketHeader[6]				= {0x00,0x00,0x01,0xBF,0x03,0xD4};
BYTE const g_PCISubStreamID					= {0x00};
BYTE const g_DSIPacketHeader[6]				= {0x00,0x00,0x01,0xBF,0x03,0xFA};
BYTE const g_DSISubStreamID					= {0x01};
BYTE const g_PackHeaderMask[14]				= {0x00,0x00,0x01,0xBA,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x89,0xC3,0xF8};
// Packet rate restriction -- should this be 1 or 0?  currently 0x7F, which means 0.

HRESULT IsMPEGNavPack(BYTE *pBuf,UINT32 cbBufferSize, BOOL *pfOut)
{
	_CP(pfOut);
	if (cbBufferSize< 38)
	{
		return E_FAIL;
	}
	*pfOut = (memcmp(pBuf,g_PackStartCode,sizeof(g_PackStartCode)) == 0);
	if (!*pfOut)
	{
		return S_OK;	
	}

	// We're checking both the 416 incorrect output, and real nav packs, if either returns
	// true, then we return true.
	*pfOut = (memcmp(pBuf + 10,g_416TempComp,sizeof(g_416TempComp)) == 0);

	if (*pfOut)
	{
		return S_OK;
	}
	// This is the real check
	
	*pfOut = ((memcmp(pBuf + sizeof(DVD_PACK_HEADER1),g_NVSystemHeader,g_cbNVSystemHeaderMaskStart)) ==0);
	if (!*pfOut)
	{
		return S_OK;
	}
	*pfOut = ((memcmp(pBuf + sizeof(DVD_PACK_HEADER1)+sizeof(NV_PCK_SYSTEM_HEADER)-g_cbNVSystemHeaderMaskEnd ,g_NVSystemHeader +sizeof(NV_PCK_SYSTEM_HEADER)-g_cbNVSystemHeaderMaskEnd ,g_cbNVSystemHeaderMaskEnd)) ==0);
	if (!*pfOut)
	{
		return S_OK;
	}
	
	return S_OK;


}
