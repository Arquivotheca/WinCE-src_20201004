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
#pragma once


#ifndef BIG_ENDIAN
//  we're little-endian and need to swap bytes around

#define NTOH_LL(ll)                                             \
                    ( (((ll) & 0xFF00000000000000) >> 56)  |    \
                      (((ll) & 0x00FF000000000000) >> 40)  |    \
                      (((ll) & 0x0000FF0000000000) >> 24)  |    \
                      (((ll) & 0x000000FF00000000) >> 8)   |    \
                      (((ll) & 0x00000000FF000000) << 8)   |    \
                      (((ll) & 0x0000000000FF0000) << 24)  |    \
                      (((ll) & 0x000000000000FF00) << 40)  |    \
                      (((ll) & 0x00000000000000FF) << 56) )

#define NTOH_L(l)                                       \
                    ( (((l) & 0xFF000000) >> 24)    |   \
                      (((l) & 0x00FF0000) >> 8)     |   \
                      (((l) & 0x0000FF00) << 8)     |   \
                      (((l) & 0x000000FF) << 24) )

#define NTOH_S(s)                                   \
                    ( (((s) & 0xFF00) >> 8)     |   \
                      (((s) & 0x00FF) << 8) )

#else   //  BIG_ENDIAN
//  we're already big-endian, so we do nothing

#define NTOH_LL(ll)     (ll)
#define NTOH_L(l)       (l)
#define NTOH_S(s)       (s)

#endif  //  BIG_ENDIAN

//  ----------------------------------------------------------------------------
//  pack header parsing
//  pb points to pack_start_code
#define PACK_START_CODE_VALUE(pb)                       NTOH_L(DWORD_VALUE(pb,0))
#define PACK_HEADER_SCR_BASE(pb)                        (((NTOH_LL(ULONGLONG_VALUE(pb,1)) & 0x000000000003FFF8) >> 3) | \
                                                         ((NTOH_LL(ULONGLONG_VALUE(pb,1)) & 0x00000003FFF80000) >> 4) | \
                                                         ((NTOH_LL(ULONGLONG_VALUE(pb,1)) & 0x0000003800000000) >> 5))
#define PACK_HEADER_SCR_EXT(pb)                         ((NTOH_LL(ULONGLONG_VALUE(pb,2)) & 0x00000000000003FE) >> 1)
#define PACK_PROGRAM_MUX_RATE(pb)                       (((NTOH_L(DWORD_VALUE(pb,10)) >> 8) & 0xFFFFFC0) >> 2)
#define PACK_STUFFING_LENGTH(pb)                        (BYTE_VALUE(pb,13) & 0x03)


//  start codes, etc...
#define START_CODE_PREFIX                               0x000001
#define MPEG2_PACK_START_CODE                           0xBA
#define MPEG2_SYSTEM_HEADER_START_CODE                  0xBB
#define PS_PMT_START_CODE                               0XBC
#define PS_DIRECTORY_START_CODE                         0XFF
#define PROGRAM_END_CODE                                0xB9
#define MPEG2_SEQUENCE_HEADER_START_CODE                0xB3
#define MPEG2_EXTENSION_START_CODE                      0xB5

//  i 0-based
#define BIT_VALUE(v,b)              ((v & (0x00 | (1 << b))) >> b)
#define BYTE_OFFSET(pb,i)           (& BYTE_VALUE((pb),i))
#define BYTE_VALUE(pb,i)            (((BYTE *) (pb))[i])
#define WORD_VALUE(pb,i)            (* (UNALIGNED WORD *) BYTE_OFFSET((pb),i))
#define DWORD_VALUE(pb,i)           (* (UNALIGNED DWORD *) BYTE_OFFSET((pb),i))
#define ULONGLONG_VALUE(pb,i)       (* (UNALIGNED ULONGLONG *) BYTE_OFFSET((pb),i))


__inline
BYTE
StartCode (
    IN BYTE *   pbStartCode
    )
{
    return pbStartCode [3] ;
}

__inline
BOOL
IsStartCodePrefix (
    IN  BYTE *  pbBuffer
    )
{
    return (pbBuffer [0] == 0x00 &&
            pbBuffer [1] == 0x00 &&
            pbBuffer [2] == 0x01) ;
}

__inline
LONGLONG
PackSCR (
    IN  BYTE *  pbPackHeader
    )
{
    LONGLONG    llSCR ;
    LONGLONG    system_clock_reference_base ;
    LONGLONG    system_clock_reference_extension ;

    ASSERT (pbPackHeader) ;

    if (IsStartCodePrefix (pbPackHeader) &&
        StartCode (pbPackHeader) == MPEG2_PACK_START_CODE) {

        //  get the first SCR
        system_clock_reference_base         = PACK_HEADER_SCR_BASE  (pbPackHeader) ;
        system_clock_reference_extension    = PACK_HEADER_SCR_EXT   (pbPackHeader) ;

        //  equation 2-1, H.222.0
        llSCR = system_clock_reference_base * 300 + system_clock_reference_extension ;
    }
    else {
        llSCR = (LONGLONG) -1 ;
    }

    return llSCR ;
}

__inline
REFERENCE_TIME
PCRToDShow (
    IN  LONGLONG    llPCR
    )
{
    //  safe to use since the top 16 bits are never set
    ASSERT ((llPCR & 0xffff000000000000) == 0x0000000000000000) ;
    return (REFERENCE_TIME) llMulDiv (llPCR, 10, 27, 0) ;
}

__inline
REFERENCE_TIME
SCRToDShow (
    IN  LONGLONG    llSCR
    )
{
    return PCRToDShow (llSCR) ;
}

#define MAX_SCR_VALUE   ((LONGLONG)(((LONGLONG)1 << 48) - (LONGLONG)1))

typedef struct
{
	BYTE PackStartCode[4];
	BYTE SCR[6];
	BYTE ProgramMuxRate[3];
	BYTE PackStuffing; // note the first 5 bits must be 11111b.

} DVD_PACK_HEADER, *PDVD_PACK_HEADER;

typedef struct
{
	DVD_PACK_HEADER			PackHeader;
	BYTE					StartCode[3];
	BYTE					StreamID;
	BYTE					Unknown[2030];
} GENERIC_PCK, *PGENERIC_PCK;
typedef struct
{
	BYTE	Marker1 : 1;
	BYTE	PTS3230 : 3;
	BYTE	Marker2 : 4;
	BYTE			: 0;
	BYTE	PTS2922;
	BYTE	Marker3 : 1;
	BYTE	PTS2115 : 7;
	BYTE			: 0;
	BYTE	PTS147;
	BYTE	Marker4 : 1;
	BYTE	PTS60	: 7;
} DVD_PTS, *PDVD_PTS;

typedef struct
{
	DVD_PACK_HEADER			PackHeader;
	BYTE					StartCode[3];
	BYTE					StreamID;
	BYTE					PESPacketLength[2];
	BYTE					Flags[3];
	DVD_PTS					PTS;
	BYTE					Unknown[2020];
} GENERIC_PCK_WITH_PTS, *PGENERIC_PCK_WITH_PTS;

/*  Inlines to get PTSs from MPEG2 packets */
inline BOOL MPEG2PacketHasPTS(const BYTE * pbPacket)
{
    /*  Just check if the PTS_DTS_flags are 10 or 11 (ie the first
        bit is one)
    */
    return 0 != (pbPacket[7] & 0x80);
}


STDMETHODIMP ConvertPTStoUINT(DVD_PTS PTS, UINT32 *puiPTS)
{
	UINT32 uiLocalPTS;
	uiLocalPTS = 0x7F & PTS.PTS60;
	uiLocalPTS |= 0x7F80 & (((UINT32)PTS.PTS147) <<7);
	uiLocalPTS |= 0x3F8000 & (((UINT32)PTS.PTS2115) <<15);
	uiLocalPTS |= 0x3FC00000 & (((UINT32)PTS.PTS2922) <<22);	
	uiLocalPTS |= 0xC0000000 & (((UINT32)PTS.PTS3230) <<30);
	// todo: gabego need to swap here?
	*puiPTS = uiLocalPTS;
	return S_OK;
}

STDMETHODIMP GetPTS(BYTE *pBuf, UINT32 *puiPTS)
{
	*puiPTS = 0;
	PGENERIC_PCK pPck = (PGENERIC_PCK)(pBuf);
	if (pPck->StreamID == 0xE0)
	{
		// use the first one that has PTS (should be an I-frame)
		// Now we know that we are on a video pack.
		// Get past the header and any DTS/PTS
		if (MPEG2PacketHasPTS(pPck->StartCode))
		{
			PGENERIC_PCK_WITH_PTS pPTSPck = (PGENERIC_PCK_WITH_PTS) pPck;
			HRESULT hr = ConvertPTStoUINT(pPTSPck->PTS,puiPTS);
			if (FAILED(hr))
			{
				ASSERT(0);
				return hr;
			}
			return S_OK;
		}
	}
	return S_FALSE;
}
