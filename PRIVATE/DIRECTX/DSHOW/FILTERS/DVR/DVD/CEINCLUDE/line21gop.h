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
#ifndef _INC_LINE21GOP_H
#define _INC_LINE21GOP_H

#pragma pack(push, 1)

//
//  DVD Line21 data come as a user data packet from the GOP header.
//  A struct for per frame/field based data and a valid flag definition.
//
//  From DVD specifications...
#define AM_L21_GOPUD_HDR_STARTCODE      0x000001B2
#define AM_L21_GOPUD_HDR_INDICATOR      0x4343
#define AM_L21_GOPUD_HDR_RESERVED       0x01F8
#define AM_L21_GOPUD_HDR_TOPFIELD_FLAG  0x1
#define AM_L21_GOPUD_ELEM_MARKERBITS    0x7F
#define AM_L21_GOPUD_ELEM_VALIDFLAG     0x1
// There can be max 63 frames/fields' worth data per packet as there are 
// 6 bits to represent this number in the packet.
#define AM_L21_GOPUD_ELEMENT_MAX        63

typedef struct _AM_L21_GOPUD_ELEMENT {
    BYTE        bMarker_Switch ;
    BYTE        chFirst ;
    BYTE        chSecond ;
} AM_L21_GOPUD_ELEMENT, *PAM_L21_GOPUD_ELEMENT ;

typedef struct _AM_L21_GOPUD_HEADER {
    BYTE        abL21StartCode[4] ;
    BYTE        abL21Indicator[2] ;
    BYTE        abL21Reserved[2] ;
    BYTE        bTopField_Rsrvd_NumElems ;
} AM_L21_GOPUD_HEADER, *PAM_L21_GOPUD_HEADER ;

typedef struct _AM_L21_GOPUD_PACKET {
    AM_L21_GOPUD_HEADER   Header ;
    AM_L21_GOPUD_ELEMENT  aElements[AM_L21_GOPUD_ELEMENT_MAX] ;
} AM_L21_GOPUD_PACKET, *PAM_L21_GOPUD_PACKET ;

#define GETGOPUD_NUMELEMENTS(pGOPUDPacket) ((pGOPUDPacket)->Header.bTopField_Rsrvd_NumElems & 0x3F)
#define GETGOPUD_PACKETSIZE(pGOPUDPacket)  (LONG)(sizeof(AM_L21_GOPUD_HEADER) + GETGOPUD_NUMELEMENTS(pGOPUDPacket) * sizeof(AM_L21_GOPUD_ELEMENT))
#define GETGOPUDPACKET_ELEMENT(pGOPUDPacket, i) ((pGOPUDPacket)->aElements[i])
#define GETGOPUD_ELEM_MARKERBITS(Elem)     ((((Elem).bMarker_Switch & 0xFE) >> 1) & 0x7F)
#define GETGOPUD_ELEM_SWITCHBITS(Elem)     ((Elem).bMarker_Switch & 0x01)

#define GETGOPUD_L21STARTCODE(Header)             \
    ( (DWORD)((Header).abL21StartCode[0]) << 24 | \
    (DWORD)((Header).abL21StartCode[1]) << 16 | \
    (DWORD)((Header).abL21StartCode[2]) <<  8 | \
(DWORD)((Header).abL21StartCode[3]) )
#define GETGOPUD_L21INDICATOR(Header)             \
    ( (DWORD)((Header).abL21Indicator[0]) << 8 |  \
(DWORD)((Header).abL21Indicator[1]) )
#define GETGOPUD_L21RESERVED(Header)              \
    ( (DWORD)((Header).abL21Reserved[0]) << 8  |  \
(DWORD)((Header).abL21Reserved[1]) )

#define GOPUD_HEADERLENGTH   (4+2+2+1)
#define GETGOPUD_ELEMENT(pGOPUDPkt, i)  (pGOPUDPkt + GOPUD_HEADERLENGTH + 3 * i)
#define ISGOPUD_TOPFIELDFIRST(pGOPUDPacket)  ((pGOPUDPacket)->Header.bTopField_Rsrvd_NumElems & 0x80)

#define AM_L21_INFO_FIELDBASED          0x0001
#define AM_L21_INFO_TOPFIELDFIRST       0x0003
#define AM_L21_INFO_BOTTOMFIELDFIRST    0x0005

typedef struct _AM_LINE21INFO {
    DWORD       dwFieldFlags ;
    UINT        uWidth ;
    UINT        uHeight ;
    UINT        uBitDepth ;
    DWORD       dwAvgMSecPerSample ;
} AM_LINE21INFO, *PAM_LINE21INFO ;


#pragma pack(pop)

#endif // _INC_LINE21GOP_H
